#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/command.h"
#include <cmath>
#include <algorithm>

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace Amplitron {

// Helper: draw a dashed line
static void DrawDashedLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, 
                          float thickness = 1.0f, float dash_len = 4.0f, 
                          float gap_len = 4.0f) {
    float dist = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) + 
                           (p2.y - p1.y) * (p2.y - p1.y));
    if (dist <= 0.0f) {
    return;
}                       
    ImVec2 dir = ImVec2((p2.x - p1.x) / dist, (p2.y - p1.y) / dist);
    
    float cycle = dash_len + gap_len;
    for (float d = 0.0f; d < dist; d += cycle) {
        ImVec2 start = ImVec2(p1.x + dir.x * d, p1.y + dir.y * d);
        float end_d = std::min(d + dash_len, dist);
        ImVec2 end = ImVec2(p1.x + dir.x * end_d, p1.y + dir.y * end_d);
        dl->AddLine(start, end, col, thickness);
    }
}

void PedalBoard::render_signal_chain() {
    std::vector<int> visible;
    for (int idx : visible_indices_) {
        if (idx >= 0 && idx < static_cast<int>(widgets_.size())) {
            visible.push_back(idx);
        }
    }

    if (visible.empty()) {
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetWindowWidth() / 2 - 150,
            ImGui::GetWindowHeight() / 2 - 30
        ));
        ImGui::TextColored(Theme::TextDim(),
            "No pedals in chain.\nClick '+ Add Pedal' to get started.");
        return;
    }

    // Add vertical padding at the top of the child window to make room for the flow line
    ImGui::Dummy(ImVec2(0, 30.0f));
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // Configuration for the signal flow line
    // Now y is measured AFTER the dummy, so we draw in the padding area.
    const float line_y = origin.y - 15.0f; 
    const float line_thickness = 3.0f;
    const float pedal_spacing = 195.0f;
    const float pedal_start_x = origin.x + 20.0f;
    
    // Animation pulse based on time and audio level
    float level = engine_.get_output_level();
    float time = (float)ImGui::GetTime();
    float pulse = 0.6f + 0.4f * std::sin(time * 5.0f) * (0.5f + level * 2.0f);
    bool is_running = engine_.is_running();

    float total_chain_width = 20.0f + static_cast<float>(visible.size()) * pedal_spacing;

    // 1. Draw the input jack
    dl->AddCircleFilled(ImVec2(origin.x + 5, line_y), 7, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(origin.x + 5, line_y), 7, Theme::BORDER_DARK, 0, 1.5f);

    float current_x = origin.x + 5;
    int remove_idx = -1;
    bool needs_rebuild = false;

    for (int vi = 0; vi < static_cast<int>(visible.size()); ++vi) {
        int i = visible[vi];
        float next_pedal_x = pedal_start_x + vi * pedal_spacing;
        
        // --- Segment from previous point to this pedal (The "Cable") ---
        // Added a slight "slack" (downward curve) to make it look less mechanical/stiff.
        ImVec2 p1 = ImVec2(current_x, line_y);
        ImVec2 p2 = ImVec2(next_pedal_x, line_y);
        
        ImU32 cable_col = is_running ? Theme::ACCENT_GOLD_DIM : Theme::CHAIN_LINE;
        if (is_running) {
            ImU32 r = (cable_col >> 0) & 0xFF;
            ImU32 g = (cable_col >> 8) & 0xFF;
            ImU32 b = (cable_col >> 16) & 0xFF;
            cable_col = IM_COL32(r, g, b, (int)(200 * (0.8f + 0.2f * pulse)));
        }

        // Slight Bezier for slack
        ImVec2 cp1 = ImVec2(p1.x + (p2.x - p1.x) * 0.35f, p1.y + 4.0f);
        ImVec2 cp2 = ImVec2(p1.x + (p2.x - p1.x) * 0.65f, p1.y + 4.0f);
        dl->AddBezierCubic(p1, cp1, cp2, p2, cable_col, line_thickness);
        
        // Add moving "signal" highlight along the cable if running
        if (is_running && level > 0.01f) {
            float t_off = std::fmod(time * 2.0f, 1.0f);
            // Draw a small bright segment moving along the curve
            for (int step = 0; step < 5; ++step) {
                float t = std::fmod(t_off + step * 0.02f, 1.0f);
                // Simple linear interp for the highlight (approximation of the curve)
                ImVec2 h_pos = ImVec2(p1.x + (p2.x - p1.x) * t, p1.y + 4.0f * std::sin(t * 3.14159f));
                dl->AddCircleFilled(h_pos, 2.0f, Theme::ACCENT_GOLD_HOT);
            }
        }

        // --- Render the pedal widget ---
        ImVec2 pedal_min = ImVec2(next_pedal_x, origin.y);
        ImGui::SetCursorScreenPos(pedal_min);
        
        char dnd_id[32];
        snprintf(dnd_id, sizeof(dnd_id), "##dnd_%d", i);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(dnd_id, ImVec2(Theme::PEDAL_WIDTH, Theme::PEDAL_HEIGHT));

        bool is_amp = std::strcmp(widgets_[i]->get_effect()->name(), "Amp Sim") == 0;
        if (!is_amp) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("PEDAL_REORDER", &i, sizeof(int));
                ImGui::Text("Move %s", widgets_[i]->get_effect()->name());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PEDAL_REORDER")) {
                    int source_idx = *static_cast<const int*>(payload->Data);
                    if (source_idx != i) {
                        history_.execute(std::make_unique<ReorderEffectCommand>(engine_, source_idx, i));
                        needs_rebuild = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::SetCursorScreenPos(pedal_min);
        if (widgets_[i]->render()) {
            remove_idx = i;
        }

        // --- Segment OVER the pedal ---
        bool enabled = widgets_[i]->get_effect()->is_enabled();
        float pedal_end_x = next_pedal_x + Theme::PEDAL_WIDTH;
        
        if (enabled) {
            const auto* colors = get_effect_color(widgets_[i]->get_effect()->name());
            ImU32 pedal_accent = ImGui::ColorConvertFloat4ToU32(colors->led_color);
            
            ImU32 r = (pedal_accent >> 0) & 0xFF;
            ImU32 g = (pedal_accent >> 8) & 0xFF;
            ImU32 b = (pedal_accent >> 16) & 0xFF;
            
            // Draw multiple layers for a rich "neon" glow effect
            dl->AddLine(ImVec2(next_pedal_x, line_y), ImVec2(pedal_end_x, line_y), 
                        IM_COL32(r, g, b, (int)(60 * pulse)), line_thickness + 6.0f);
            dl->AddLine(ImVec2(next_pedal_x, line_y), ImVec2(pedal_end_x, line_y), 
                        IM_COL32(r, g, b, (int)(120 * pulse)), line_thickness + 2.0f);
            dl->AddLine(ImVec2(next_pedal_x, line_y), ImVec2(pedal_end_x, line_y), 
                        IM_COL32(255, 255, 255, (int)(200 * pulse)), 1.5f);
        } else {
            // Bypassed style: thinner, darker, dashed
            DrawDashedLine(dl, ImVec2(next_pedal_x, line_y), ImVec2(pedal_end_x, line_y), 
                           IM_COL32(80, 80, 80, 180), 1.5f, 6.0f, 4.0f);
        }

        // Tooltip on pedal hover
        ImVec2 pedal_max = ImVec2(next_pedal_x + Theme::PEDAL_WIDTH, origin.y + Theme::PEDAL_HEIGHT);
        if (ImGui::IsMouseHoveringRect(pedal_min, pedal_max)) {
            ImGui::SetTooltip("%s (%s)", widgets_[i]->get_effect()->name(), enabled ? "Active" : "Bypassed");
        }

        current_x = pedal_end_x;
    }

    // --- Final segment to output jack ---
    float output_jack_x = origin.x + total_chain_width;
    ImU32 final_col = is_running ? Theme::ACCENT_GOLD_DIM : Theme::CHAIN_LINE;
    dl->AddLine(ImVec2(current_x, line_y), ImVec2(output_jack_x, line_y), final_col, line_thickness);

    // --- Output jack ---
    dl->AddCircleFilled(ImVec2(output_jack_x, line_y), 7, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(output_jack_x, line_y), 7, Theme::BORDER_DARK, 0, 1.5f);

    if (remove_idx >= 0) {
        visible_indices_.erase(remove_idx);
        history_.execute(std::make_unique<RemoveEffectCommand>(engine_, remove_idx));
        needs_rebuild = true;
    }

    if (needs_rebuild) {
        rebuild_widgets();
    }

    // Space for scrolling
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(ImVec2(total_chain_width + 40.0f, Theme::PEDAL_HEIGHT + 40.0f));
}

} // namespace Amplitron
