#include "Features/WebRadar/UI/webradar_tab.h"

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/menu_utils.h"
#include "app/UI/MenuShell/tab_page.h"
#include "Features/WebRadar/UI/webradar_sections.h"
#include "app/Core/globals.h"
#include "Features/WebRadar/webradar.h"

#include <algorithm>
#include <string>
#include <vector>

#include <imgui.h>

const char* ui::tabs::WebRadarTab::Label() const
{
    return "WEBRadar";
}

void ui::tabs::WebRadarTab::Render(MenuState& state, IStatusSink& statusSink)
{
    ImGui::BeginChild("##webradarchild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    const webradar::RuntimeStats wr = webradar::GetRuntimeStats();
    const uint16_t effectivePort = wr.listenPort != 0
        ? wr.listenPort
        : static_cast<uint16_t>(std::clamp(g::webRadarPort, 1025, 65535));

    const std::vector<std::string> lanLinks = ui::menu_utils::BuildLanRadarLinks(effectivePort);
    const std::string localLink = ui::menu_utils::BuildLocalRadarLink(effectivePort);
    
    const std::string radarLink = lanLinks.empty() ? localLink : lanLinks.front();

    webradar_sections::RenderConnectionSection(state, radarLink, statusSink);
    webradar_sections::RenderQrSection(radarLink);
    webradar_sections::RenderDebugSection(wr, effectivePort);

    ImGui::EndChild();
}
