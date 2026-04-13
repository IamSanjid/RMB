#pragma once

#include "View.h"

#include "clay/clay_renderer_gles3.h"
#include "clay/clay_text_input.h"
#include "clay/clay_button.h"

#include "Config.h"

#define FONT_ID_ROBOTO_REGULAR 0
#define FONT_ID_INTER_REGULAR 1
#define FONT_ID_INTER_MEDIUM 2
#define FONT_ID_INTER_BOLD 3

#define IMAGE_ID_ICON_PRIMARY_KEYBOARD 0
#define IMAGE_ID_ICON_LABEL_WINDOW 1
#define IMAGE_ID_ICON_HEADLINE_STRAIGHTEN 2
#define IMAGE_ID_ICON_HEADLINE_HIDE_SOURCE 3

const Clay_Color COLOR_PRIMARY = { 56, 189, 248, 255 };
const Clay_Color COLOR_SECONDARY = { 71, 85, 105, 255 };
const Clay_Color COLOR_TERTIARY = { 129, 140, 248, 255 };
const Clay_Color COLOR_NEUTRAL = { 15, 23, 42, 255 };
const Clay_Color COLOR_BG = { 3, 7, 23, 255 };
const Clay_Color COLOR_BG2 = { 6, 18, 38, 255 };
const Clay_Color COLOR_CARD_BG = { 16, 28, 51, 255 };

const Clay_Color COLOR_LIGHT = { 224, 215, 210, 255 };
const Clay_Color COLOR_RED = { 168, 66, 28, 255 };
const Clay_Color COLOR_ORANGE = { 225, 138, 50, 255 };

const Clay_Color COLOR_HEADLINE = { 222, 229, 255, 255 };
const Clay_Color COLOR_BODY = { 163, 170, 196, 255 };
const Clay_Color COLOR_LABEL = { 163, 170, 196, 255 };

static Clay_TI_ResizeResult resize_realloc(char *old, int oldCap, int minCap, void *ud) {
    (void)ud;
    /* Round up to next multiple of 64 to avoid per-character reallocs. */
    int newCap = ((minCap + 63) / 64) * 64;
    char *buf  = (char *)realloc(old, (size_t)newCap);
    if (!buf) return CLAY__INIT(Clay_TI_ResizeResult){ .buf = NULL, .capacity = 0 };
    return CLAY__INIT(Clay_TI_ResizeResult){ .buf = buf, .capacity = newCap };
}

static void LabelWithImage(Clay_String text, Clay_TextElementConfig textConfig, float imageHeight, void *imageData) {
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIT(0)
            },
            .padding = CLAY_PADDING_ALL(2),
            .childGap = 8,
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
        }
    }) {
        CLAY_AUTO_ID({
            .layout = {.sizing = { .width = CLAY_SIZING_FIT(imageHeight), .height = CLAY_SIZING_FIT(imageHeight) }},
            .cornerRadius = { imageHeight * 0.5f, imageHeight * 0.5f, imageHeight * 0.5f, imageHeight * 0.5f },
            .image = { .imageData = imageData },
        }) {}

        CLAY_TEXT(text, textConfig);
    }
}

const Clay_TextInputConfig DEFAULT_TEXT_INPUT_CONFIG = {
    .sizing = {
        .width = CLAY_SIZING_GROW(0),
        .height = CLAY_SIZING_GROW(18 * 1.2f + 4)
    },
    .padding = CLAY_PADDING_ALL(4),
    .textConfig = {
        .textColor = COLOR_HEADLINE,
        .fontId = FONT_ID_INTER_REGULAR,
        .fontSize = 18
    },
    .placeholder = CLAY_STRING("Target Window!"),
    .colorPlaceholder = COLOR_SECONDARY,
    .colorBorder = COLOR_SECONDARY,
    .colorBorderFocus = COLOR_PRIMARY,
    .colorSelection = COLOR_PRIMARY,
    .colorCursor = COLOR_SECONDARY,
    .cornerRadius = CLAY_CORNER_RADIUS(8),
    .borderWidth = CLAY_BORDER_ALL(2),
    .cursorBlinkPeriod = 0.53,
    .onResize = resize_realloc
};

class ClayMainView : public View {
public:
    ClayMainView() {
        const std::string& target_window_name = Config::Current()->TARGET_NAME;
        const size_t target_window_name_len = target_window_name.length();
        if (!Clay_TextInputState_InitDynamic(&ti_target_window_, target_window_name_len, CLAY_ID("TargetWindow"), resize_realloc, nullptr))
            return;
        Clay_TextInputState_Insert(&ti_target_window_, 0, target_window_name.c_str(), target_window_name_len);
    }

    ~ClayMainView() {
        Clay_TextInputState_Destroy(&ti_target_window_, free);
    }

    void SetSize(uint32_t width, uint32_t height) override {}
    void SetPos(uint32_t x, uint32_t y) override {}
    void Show() override {}
    void OnKeyPress(int key, int scancode, int mods) override {
        (void)key;
        (void)scancode;
        (void)mods;
    }
    void OnKeyRelease(int key, int scancode, int mods) override {
        (void)key;
        (void)scancode;
        (void)mods;
    }

    virtual ViewResult Update() {
        const Clay_Sizing EXPANDED_SIZING = {
            .width = CLAY_SIZING_GROW(0),
            .height = CLAY_SIZING_GROW(0)
        };
        const Clay_Sizing FIT_SIZING = {
            .width = CLAY_SIZING_FIT(0),
            .height = CLAY_SIZING_FIT(0)
        };

        Clay_BeginLayout();

        CLAY(CLAY_ID("OuterContainer"), { 
            .layout = { 
                .sizing = EXPANDED_SIZING,
                .padding = CLAY_PADDING_ALL(12),
                .childGap = 10,
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            },
            .backgroundColor = COLOR_BG
        }) {
            CLAY(CLAY_ID("HeaderBar"), {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_FIT(0)
                    },
                    .padding = { 
                        .left = 16,
                        .right = 16,
                        .top = 8,
                        .bottom = 8
                    },
                    .childGap = 16,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
                }
            }) {
                CLAY_TEXT(CLAY_STRING("RMB"), {
                    .textColor = COLOR_PRIMARY,
                    .fontId = FONT_ID_INTER_BOLD,
                    .fontSize = 24
                });

                CLAY(CLAY_ID("Seperator"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}}}) {}

                if (CLAY_BUTTON(CLAY_ID("SaveConfigBtn"), CLAY_STRING("Save Config"), {
                    .actionOn = CLAY_BUTTON_ACTION_ON_RELEASE,
                    .sizing = {
                        .width = CLAY_SIZING_FIT(120),
                        .height = CLAY_SIZING_FIT(0)
                    },
                    .padding = CLAY_PADDING_ALL(8),
                    .textConfig = {
                        .textColor = COLOR_PRIMARY,
                        .fontId = FONT_ID_INTER_MEDIUM,
                        .fontSize = 20
                    },
                    .hoverColor = { 20, 31, 56, 255 },
                    .hoverTextColor = COLOR_PRIMARY,
                    .backgroundColor = { 20, 31, 56, 255 },
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                    .border = { 
                        .color = { 20, 31, 56, 255 },
                        .width = CLAY_BORDER_ALL(2)
                    },
                    .hoverBorderColor = COLOR_PRIMARY
                })) {
                    printf("SaveConfigBtn clicked!\n");
                }

                if (CLAY_BUTTON(CLAY_ID("StartBtn"), CLAY_STRING("Start"), {
                    .actionOn = CLAY_BUTTON_ACTION_ON_RELEASE,
                    .sizing = {
                        .width = CLAY_SIZING_FIT(100),
                        .height = CLAY_SIZING_FIT(0)
                    },
                    .padding = CLAY_PADDING_ALL(8),
                    .textConfig = {
                        .textColor = COLOR_BG,
                        .fontId = FONT_ID_INTER_MEDIUM,
                        .fontSize = 20
                    },
                    .hoverColor = COLOR_LIGHT,
                    .hoverTextColor = COLOR_BG,
                    .backgroundColor = COLOR_PRIMARY,
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                    .border = { 
                        .color = COLOR_PRIMARY,
                        .width = CLAY_BORDER_ALL(2)
                    },
                    .hoverBorderColor = COLOR_LIGHT
                })) {
                    printf("StartBtn clicked!\n");
                }
            }
            
            CLAY(CLAY_ID("MainContent"), {
                .layout = {
                    .sizing = EXPANDED_SIZING,
                    .padding = CLAY_PADDING_ALL(10),
                    .childGap = 8,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = COLOR_BG2
            }) {
                CLAY(CLAY_ID("TopContainer"), {
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_FIT(0)
                        },
                        .padding = CLAY_PADDING_ALL(2),
                        .childGap = 8
                    }
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT(0)
                            },
                            .padding = CLAY_PADDING_ALL(10),
                            .childGap = 8,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                        .backgroundColor = COLOR_CARD_BG,
                        .cornerRadius = CLAY_CORNER_RADIUS(8)
                    }) {
                        LabelWithImage(CLAY_STRING("TARGET WINDOW STRING"), {
                            .textColor = COLOR_LABEL,
                            .fontId = FONT_ID_INTER_BOLD,
                            .fontSize = 18
                        }, 20, &icon_label_window_);
                        CLAY_TEXT_INPUT(CLAY_ID("TargetWindow"), &ti_target_window_, &DEFAULT_TEXT_INPUT_CONFIG);
                    }

                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT(0)
                            },
                            .padding = CLAY_PADDING_ALL(10),
                            .childGap = 8,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                        .backgroundColor = COLOR_CARD_BG,
                        .cornerRadius = CLAY_CORNER_RADIUS(8)
                    }) {
                        LabelWithImage(CLAY_STRING("Panning Toggle Hotkeys"), {
                            .textColor = COLOR_HEADLINE,
                            .fontId = FONT_ID_INTER_BOLD,
                            .fontSize = 18
                        }, 24, &icon_primary_keyboard_);
                    }
                }

                CLAY(CLAY_ID("MiddleContainer"), {
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_FIT(0)
                        },
                        .padding = CLAY_PADDING_ALL(2),
                        .childGap = 8
                    }
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT(0)
                            },
                            .padding = CLAY_PADDING_ALL(10),
                            .childGap = 8,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                        .backgroundColor = COLOR_CARD_BG,
                        .cornerRadius = CLAY_CORNER_RADIUS(8)
                    }) {
                        LabelWithImage(CLAY_STRING("Deadzone"), {
                            .textColor = COLOR_HEADLINE,
                            .fontId = FONT_ID_INTER_BOLD,
                            .fontSize = 18
                        }, 20, &icon_headline_hide_source_);

                        LabelWithImage(CLAY_STRING("Range"), {
                            .textColor = COLOR_HEADLINE,
                            .fontId = FONT_ID_INTER_BOLD,
                            .fontSize = 18
                        }, 24, &icon_headline_straighten_);
                    }
                }
            }
            
        }

        return {
            .action = ViewActionType::Render,
            .commands = Clay_EndLayout(0),
        };
    }

private:
    Gles3_ImageConfig icon_primary_keyboard_{
        .textureToUse = IMAGE_ID_ICON_PRIMARY_KEYBOARD,
        .u0 = 0.f,
        .v0 = 0.f,
        .u1 = 1.f,
        .v1 = 1.f
    };
    Gles3_ImageConfig icon_label_window_{
        .textureToUse = IMAGE_ID_ICON_LABEL_WINDOW,
        .u0 = 0.f,
        .v0 = 0.f,
        .u1 = 1.f,
        .v1 = 1.f
    };
    Gles3_ImageConfig icon_headline_hide_source_{
        .textureToUse = IMAGE_ID_ICON_HEADLINE_HIDE_SOURCE,
        .u0 = 0.f,
        .v0 = 0.f,
        .u1 = 1.f,
        .v1 = 1.f
    };
    Gles3_ImageConfig icon_headline_straighten_{
        .textureToUse = IMAGE_ID_ICON_HEADLINE_STRAIGHTEN,
        .u0 = 0.f,
        .v0 = 0.f,
        .u1 = 1.f,
        .v1 = 1.f
    };
    Clay_TextInputState ti_target_window_;
};