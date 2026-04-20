#pragma once

#include <string>

namespace ui {
struct MenuState;

class IStatusSink {
public:
    virtual ~IStatusSink() = default;
    virtual void SetStatus(const std::string& text) = 0;
};

class ITabPage {
public:
    virtual ~ITabPage() = default;
    virtual const char* Label() const = 0;
    virtual void Render(MenuState& state, IStatusSink& statusSink) = 0;
};
}
