#pragma once

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"

namespace ui::tabs::settings_sections {

void RenderProfilesSection(MenuState& state, IStatusSink& statusSink);
void RenderControlsSection(MenuState& state, IStatusSink& statusSink);
void RenderScreenSection(IStatusSink& statusSink);
void RenderDebugWindow(bool* open);

} 
