/*
 * clay_button_input.h
 *
 * Text input widget for Clay UI — C99 / C++20 compatible.
 *
 */

#ifndef CLAY_BUTTON_H
#define CLAY_BUTTON_H

#ifndef CLAY_H
#include <clay.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef CLAY_PACKED_ENUM {
    CLAY_BUTTON_ACTION_ON_RELEASE = 0,
    CLAY_BUTTON_ACTION_ON_PRESS
} Clay_ButtonActionOn;

typedef void (*Clay_ButtonActionFn)(Clay_ElementId elementId, void *userData);

typedef struct Clay_ButtonConfig {
    void *userData;
    Clay_ButtonActionFn onAction;
    Clay_ButtonActionOn actionOn;
    Clay_Sizing sizing;
    Clay_Padding padding;
    Clay_TextElementConfig textConfig;
    Clay_Color hoverColor;
    Clay_Color hoverTextColor;
    Clay_Color backgroundColor;
    Clay_CornerRadius cornerRadius;
    Clay_FloatingElementConfig floating;
    Clay_BorderElementConfig border;
    Clay_Color hoverBorderColor;
} Clay_ButtonElementConfig;

CLAY__WRAPPER_STRUCT(Clay_ButtonElementConfig);

CLAY_DLL_EXPORT bool Clay__OpenButtonElement(Clay_ElementId elementId, Clay_String text, Clay_ButtonConfig buttonConfig);

#define CLAY_BUTTON(elementId, text, ...) Clay__OpenButtonElement(elementId, text, CLAY__CONFIG_WRAPPER(Clay_ButtonElementConfig, __VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif /* CLAY_BUTTON_H */


/* ═══════════════════════════════════════════════════════════════════════════
 * IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifdef CLAY_BUTTON_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static Clay_ElementId g_clicked_on_id; /* Current clicked on button */

bool Clay__OpenButtonElement(Clay_ElementId elementId, Clay_String text, Clay_ButtonConfig buttonConfig) {
    Clay_Context* context = Clay_GetCurrentContext();

    Clay_PointerData pointerData = context->pointerInfo;
    bool possibleAction = buttonConfig.actionOn == CLAY_BUTTON_ACTION_ON_PRESS ? 
                            pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME :
                            pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME;
    bool handleAction = false;

    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME && Clay_PointerOver(elementId)) {
        g_clicked_on_id = elementId;
    } else if (!possibleAction && g_clicked_on_id.id == elementId.id) {
        g_clicked_on_id = CLAY__INIT(Clay_ElementId){};
    }

    Clay_Color backgroundColor = buttonConfig.backgroundColor;
    Clay_TextElementConfig textConfig = buttonConfig.textConfig;
    Clay_BorderElementConfig border = buttonConfig.border;
    if (g_clicked_on_id.id == elementId.id && possibleAction && Clay_PointerOver(elementId)) {
        if (buttonConfig.onAction)
            buttonConfig.onAction(elementId, buttonConfig.userData);
        else
            handleAction = true;
    } else if (Clay_PointerOver(elementId) && pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        backgroundColor = buttonConfig.hoverColor;
        textConfig.textColor = buttonConfig.hoverTextColor;
        border.color = buttonConfig.hoverBorderColor;
    }
    
    CLAY(elementId, {
        .layout = {
            .sizing         = buttonConfig.sizing,
            .padding        = buttonConfig.padding,
            .childAlignment = { 
                .x = CLAY_ALIGN_X_CENTER,
                .y = CLAY_ALIGN_Y_CENTER
            },
        },
        .backgroundColor = backgroundColor,
        .cornerRadius = buttonConfig.cornerRadius,
        .floating = buttonConfig.floating,
        .border = border,
        .userData = buttonConfig.userData
    }) {
        CLAY_TEXT(text, textConfig);
    };

    return handleAction;
}

#endif /* CLAY_BUTTON_IMPLEMENTATION */