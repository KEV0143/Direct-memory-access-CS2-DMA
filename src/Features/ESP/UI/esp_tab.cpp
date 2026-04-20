#include "Features/ESP/UI/esp_tab.h"

#include "Features/ESP/UI/esp_sections.h"
#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"

#include <imgui.h>

namespace ui {
void RenderEspPreview();
}

const char* ui::tabs::EspTab::Label() const
{
    return "ESP";
}

void ui::tabs::EspTab::Render(MenuState& state, IStatusSink& statusSink)
{
    (void)state;
    (void)statusSink;

    ImGui::BeginChild("##espchild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    
    const float indent = 8.0f;
    ImGui::Indent(indent);

    ImGui::Checkbox("Enable ESP", &g::espEnabled);
    ImGui::SameLine();
    ImGui::Checkbox("ESP Preview", &g::espPreviewOpen);
    ImGui::Spacing();

    if (g::espEnabled) {
        esp_sections::RenderGeneralSection();
        esp_sections::RenderWeaponSection();
        esp_sections::RenderPlayerVisualsSection();
        esp_sections::RenderFlagsSection();

        
        
        
        
        constexpr bool kShowWorldItemSections = false;
        if (kShowWorldItemSections) {
            esp_sections::RenderItemSection();
            esp_sections::RenderWorldSection();
        }
    }

    ImGui::Unindent(indent);
    ImGui::EndChild();

    ui::RenderEspPreview();
}
