#include "Features/WebRadar/UI/webradar_tab.h"

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/menu_utils.h"
#include "app/UI/MenuShell/tab_page.h"
#include "Features/WebRadar/UI/webradar_sections.h"
#include "app/Core/globals.h"
#include "Features/WebRadar/webradar.h"
#include "Features/WebRadar/web_remote.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <imgui.h>

namespace
{
    std::string TrimCopy(std::string value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
            value.erase(value.begin());
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
            value.pop_back();
        return value;
    }

    std::string BuildRemoteRadarLink()
    {
        const std::string host = TrimCopy(g::webRadarRemoteHost);
        if (host.empty())
            return {};
        const int port = std::clamp(g::webRadarRemoteWebPort, 1, 65535);
        return "http://" + host + ":" + std::to_string(port);
    }
}

const char* ui::tabs::WebRadarTab::Label() const
{
    return "WEBRadar";
}

void ui::tabs::WebRadarTab::Render(MenuState& state, IStatusSink& statusSink)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));
    ImGui::BeginChild("##webradarchild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    const webradar::RuntimeStats wr = webradar::GetRuntimeStats();
    const uint16_t effectivePort = wr.listenPort != 0
        ? wr.listenPort
        : static_cast<uint16_t>(std::clamp(g::webRadarPort, 1025, 65535));

    const std::vector<std::string> lanLinks = ui::menu_utils::BuildLanRadarLinks(effectivePort);
    const std::string localLink = ui::menu_utils::BuildLocalRadarLink(effectivePort);
    const std::string localQrLink = lanLinks.empty() ? localLink : lanLinks.front();
    const std::string webLink = BuildRemoteRadarLink();

    webradar_sections::RenderConnectionSection(state, localLink, webLink, statusSink);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    webradar_sections::RenderRemoteSection(state, statusSink);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    webradar_sections::RenderQrSection(localQrLink, webLink);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    webradar_sections::RenderDebugSection(wr, effectivePort);
    webradar_sections::RenderRemoteDebugSection(webradar::remote::GetStats());

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
