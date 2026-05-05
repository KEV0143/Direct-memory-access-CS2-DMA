#pragma once

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"
#include "Features/WebRadar/webradar.h"
#include "Features/WebRadar/web_remote.h"

#include <cstdint>
#include <string>

namespace ui::tabs::webradar_sections {

void RenderConnectionSection(MenuState& state, const std::string& localLink, const std::string& webLink, IStatusSink& statusSink);
void RenderRemoteSection(MenuState& state, IStatusSink& statusSink);

void RenderQrSection(const std::string& localLink, const std::string& webLink);
void RenderDebugSection(const webradar::RuntimeStats& runtimeStats, uint16_t effectivePort);
void RenderRemoteDebugSection(const webradar::remote::Stats& runtimeStats);

} 
