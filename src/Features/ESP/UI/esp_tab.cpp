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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 0.0f));

    esp_sections::RenderCoreSection();

    if (g::espEnabled)
        esp_sections::RenderOptionsGrid();

    ImGui::PopStyleVar(2);
    ImGui::EndChild();

    ui::RenderEspPreview();
}
