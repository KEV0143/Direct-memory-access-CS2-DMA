#pragma once

#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"

#include <memory>
#include <vector>

namespace ui {
class MenuController final : public IStatusSink {
public:
    MenuController();
    void Render();
    void SetStatus(const std::string& text) override;

private:
    void EnsureInitialized();
    void HandleMenuKeyCapture();

    MenuState state_;
    std::vector<std::unique_ptr<ITabPage>> tabs_;
};
}
