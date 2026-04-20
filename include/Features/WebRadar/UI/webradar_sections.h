#pragma once

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"
#include "Features/WebRadar/webradar.h"

#include <cstdint>
#include <string>

namespace ui::tabs::webradar_sections {

void RenderConnectionSection(MenuState& state, const std::string& radarLink, IStatusSink& statusSink);

void RenderQrSection(const std::string& radarLink);
void RenderDebugSection(const webradar::RuntimeStats& runtimeStats, uint16_t effectivePort);

} 
