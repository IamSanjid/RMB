#pragma once

#include <cstdint>
#include "clay/clay.h"

enum class ViewActionType {
    None,
    Render,
    NavigateTo,
    NavigateBack,
};

class View;

struct ViewResult {
    ViewActionType action = ViewActionType::None;
    union {
        Clay_RenderCommandArray commands;
        View* view = nullptr;
    };
};

class View {
public:
    virtual ~View() = default;

    virtual void SetSize(uint32_t width, uint32_t height) = 0;
    virtual void SetPos(uint32_t x, uint32_t y) = 0;
    virtual void Show() = 0;
    virtual void OnKeyPress(int key, int scancode, int mods) {
        (void)key;
        (void)scancode;
        (void)mods;
    }
    virtual void OnKeyRelease(int key, int scancode, int mods) {
        (void)key;
        (void)scancode;
        (void)mods;
    }

    virtual ViewResult Update() {
        return ViewResult();
    }

protected:
    const char* name_ = "View";
};