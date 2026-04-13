/*
 * clay_drop_down.h
 *
 * Drop down widget for Clay UI — C99 / C++20 compatible.
 *
 */

#ifndef CLAY_DROP_DOWN_H
#define CLAY_DROP_DOWN_H

#ifndef CLAY_H
#include <clay.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint8_t CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH;
// GCC marks the above CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH as an unused variable for files that include clay.h but don't declare any layout
// This is to suppress that warning
static inline void Clay__SuppressUnusedDropDownLatchDefinitionVariableWarning(void) { (void) CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH; }

#define CLAY_DROP_DOWN(id, ...)                                                                                                                     \
    for (                                                                                                                                           \
        CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH = (Clay__OpenDropDownWithId(id, CLAY__CONFIG_WRAPPER(Clay_ElementDeclaration, __VA_ARGS__)), 0);   \
        CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH < 1;                                                                                               \
        CLAY__DROP_DOWN_ELEMENT_DEFINITION_LATCH=1, Clay__CloseDropDown()                                                                           \
    )

CLAY_DLL_EXPORT void Clay__OpenDropDownWithId(Clay_ElementId elementId);
CLAY_DLL_EXPORT void Clay__ConfigureOpenDropDown(const Clay_ElementDeclaration config);
CLAY_DLL_EXPORT void Clay__CloseDropDown(void);

#ifdef __cplusplus
}
#endif

#endif /* CLAY_DROP_DOWN_H */

/* ═══════════════════════════════════════════════════════════════════════════
 * IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */
#define CLAY_DROP_DOWN_IMPLEMENTATION
#ifdef CLAY_DROP_DOWN_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct Clay__DropDownState {
    bool opened;
    Clay_ElementDeclaration config;
} Clay__DropDownState;

static Clay__DropDownState clay__currentDropDown = {0};

void Clay__OpenDropDownWithId(Clay_ElementId elementId, const Clay_ElementDeclaration config) {
    Clay_Context* context = Clay_GetCurrentContext();
    if (clay__currentDropDown.opened) {
        context->errorHandler.errorHandlerFunction(CLAY__INIT(Clay_ErrorData) {
                .errorType = CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE,
                .errorText = CLAY_STRING("The previous Drop down element was not closed."),
                .userData = context->errorHandler.userData });
        return;
    }
    // The main elemenet
    Clay__OpenElementWithId(elementId);
    Clay__ConfigureOpenElementPtr(&config);
    // The container which will be "floating"..
    Clay__OpenElement();

    clay__currentDropDown.opened = true;
    clay__currentDropDown.config = config;
}

void Clay__CloseDropDown(void) {
    Clay_Context* context = Clay_GetCurrentContext();
    if (context->booleanWarnings.maxElementsExceeded) {
        clay__currentDropDown = CLAY__INIT(Clay__DropDownState){0};
        return;
    }
    if (!clay__currentDropDown.opened) {
        context->errorHandler.errorHandlerFunction(CLAY__INIT(Clay_ErrorData) {
                .errorType = CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE,
                .errorText = CLAY_STRING("There is no Drop down element opened."),
                .userData = context->errorHandler.userData });
        return;
    }
    
    
    // Closing the floating container
    Clay__CloseElement();
    // Closing the main drop-down element
    Clay__CloseElement();
    clay__currentDropDown = CLAY__INIT(Clay__DropDownState){0};
}

#endif /* CLAY_DROP_DOWN_IMPLEMENTATION */