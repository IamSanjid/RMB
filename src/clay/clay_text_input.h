/*
 * clay_text_input.h
 *
 * Text input widget for Clay UI — C99 / C++20 compatible.
 *
 * Follows Clay's exact API conventions:
 *   - CLAY__INIT / CLAY__CONFIG_WRAPPER for C/C++ dual compatibility
 *   - Config structs are plain aggregates; fields are in declaration order
 *     (C++20 designated initializers require in-order initialisation)
 *   - A top-level macro CLAY_TEXT_INPUT(state, { ... config ... }) mirrors
 *     how CLAY() and CLAY_TEXT() work
 *   - Internal names use the Clay_TI__ double-underscore convention
 *
 * ── BUFFER MODES ────────────────────────────────────────────────────────────
 *
 *   STATIC   fixed char array you own; widget never allocates
 *   DYNAMIC  you supply a resize callback; widget calls it on overflow
 *
 * ── USAGE ───────────────────────────────────────────────────────────────────
 *
 *   // 1. Define implementation in exactly one translation unit:
 *   #define CLAY_TEXT_INPUT_IMPLEMENTATION
 *   #include "clay_text_input.h"
 *
 *   // 2. Include a platform adapter (or fill the struct yourself):
 *   #include "clay_text_input_glfw.h"
 *
 *   // 3. Init module once:
 *   Clay_TextInput_SetPlatform(Clay_TextInput_Platform_GLFW(window));
 *
 *   // 4. Declare states once (static lifetime is fine):
 *   static char nameBuf[128];
 *   static Clay_TextInputState nameState =
 *       Clay_TextInputState_Static(nameBuf, sizeof(nameBuf), CLAY_ID("Name"));
 *
 *   // 5. Each frame before Clay_BeginLayout():
 *   Clay_TextInput_Update(deltaTime);
 *   // feed platform events via Clay_TextInput_OnChar / OnKey / OnPointer
 *
 *   // 6. Inside layout:
 *   CLAY_TEXT_INPUT_WITH_CONFIG(&nameState, {
 *       .width            = CLAY_SIZING_FIXED(300),
 *       .height           = CLAY_SIZING_FIXED(36),
 *       .fontSize         = 18,
 *       .fontId           = FONT_ID,
 *       .placeholder      = CLAY_STRING("Enter name…"),
 *       .colorText        = { 220, 220, 220, 255 },
 *       .colorBackground  = {  30,  30,  30, 255 },
 *       .colorBorder      = {  80,  80,  80, 255 },
 *       .colorBorderFocus = { 100, 160, 255, 255 },
 *       .colorPlaceholder = { 100, 100, 100, 255 },
 *       .colorSelection   = {  60, 120, 210, 160 },
 *       .colorCursor      = { 220, 220, 220, 255 },
 *       .cornerRadius     = CLAY_CORNER_RADIUS(6),
 *       .borderWidth      = 1.5f,
 *       .padding          = CLAY_PADDING_ALL(8),
 *   });
 *
 * ── PLATFORM ADAPTERS ───────────────────────────────────────────────────────
 *   GLFW → clay_text_input_glfw.h
 *   SDL2 → clay_text_input_sdl2.h
 */

#ifndef CLAY_TEXT_INPUT_H
#define CLAY_TEXT_INPUT_H

#ifndef CLAY_H
#include <clay.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Key action ─────────────────────────────────────────────────── */
typedef CLAY_PACKED_ENUM {
    CLAY_TI_ACTION_RELEASE = 0,
    CLAY_TI_ACTION_PRESS,
    CLAY_TI_ACTION_REPEAT
} Clay_TI_Action;

/* ── Abstract key codes ───────────────────────────────────────────────────── */
typedef CLAY_PACKED_ENUM {
    CLAY_TI_KEY_UNKNOWN = 0,
    CLAY_TI_KEY_LEFT, CLAY_TI_KEY_RIGHT,
    CLAY_TI_KEY_HOME,  CLAY_TI_KEY_END,
    CLAY_TI_KEY_BACKSPACE, CLAY_TI_KEY_DELETE,
    CLAY_TI_KEY_ENTER, CLAY_TI_KEY_ESCAPE,
    CLAY_TI_KEY_A, CLAY_TI_KEY_C, CLAY_TI_KEY_V, CLAY_TI_KEY_X,
} Clay_TI_Key;

/* ── Modifier bitmask ─────────────────────────────────────────────────────── */
typedef CLAY_PACKED_ENUM {
    CLAY_TI_MOD_NONE  = 0,
    CLAY_TI_MOD_SHIFT = 1 << 0,
    CLAY_TI_MOD_CTRL  = 1 << 1,
    CLAY_TI_MOD_ALT   = 1 << 2,
    CLAY_TI_MOD_SUPER = 1 << 3,
} Clay_TI_Mod;

/* ── Platform interface ───────────────────────────────────────────────────── */
/*
 * Only clipboard I/O needs platform help; all other events are pushed in by
 * the caller via Clay_TextInput_On*().
 *
 * Field order is part of the public API (C++20 designated-initialiser rule).
 */
typedef struct Clay_TextInput_Platform {
    /* Return clipboard contents as null-terminated UTF-8; NULL if empty.
     * Pointer only needs to stay valid until the next call. */
    const char *(*getClipboardText)(void *userData);
    /* Write null-terminated UTF-8 to the system clipboard. */
    void        (*setClipboardText)(void *userData, const char *text);
    /* Should set the cursor to Ibeam */
    void        (*setIbeamCursor)(void *userData);
    /* Should reset the cursor back to normal */
    void        (*resetCursor)(void *userData);
    /* Passed verbatim to both functions; may be NULL. */
    void *userData;
} Clay_TextInput_Platform;

/* ── Resize callback ──────────────────────────────────────────────────────── */
/*
 * Called by the widget when the buffer is about to overflow (dynamic mode only).
 *
 *   oldBuf      — current buffer pointer; NULL on the very first allocation.
 *   oldCapacity — current bufferSize; 0 on first allocation.
 *   minCapacity — minimum size the returned buffer must have (incl. '\0').
 *   userData    — opaque value from Clay_TextInputConfig.resizeUserData.
 *
 * Return { newBuf, newCapacity } where newCapacity >= minCapacity.
 * Return { NULL, 0 } to signal failure; the triggering insert is dropped.
 *
 * RECOMMENDED growth policy (avoids O(n²) realloc):
 *   int newCap = oldCapacity > 0 ? oldCapacity * 2 : 64;
 *   if (newCap < minCapacity) newCap = minCapacity;
 */
typedef struct Clay_TI_ResizeResult {
    char *buf;
    int   capacity;
} Clay_TI_ResizeResult;

typedef Clay_TI_ResizeResult (*Clay_TI_ResizeFn)(
    char *oldBuf, int oldCapacity, int minCapacity, void *userData);

/* ── Char-filter callback ─────────────────────────────────────────────────── */
/*
 * Called before each codepoint is inserted.
 * Return true to accept, false to silently drop.
 *
 * Example — digits only:
 *   bool digits_only(uint32_t cp, void *ud) { return cp >= '0' && cp <= '9'; }
 */
typedef bool (*Clay_TI_CharFilterFn)(uint32_t codepoint, void *userData);

/* ── Changed callback ─────────────────────────────────────────────────────── */
/*
 * Called once after any edit that modifies the buffer.
 * Useful for live validation, search-as-you-type, etc.
 */
typedef void (*Clay_TI_ChangedFn)(const char *text, int textLen, void *userData);

/* ═══════════════════════════════════════════════════════════════════════════
 * Clay_TextInputConfig
 *
 * Passed to CLAY_TEXT_INPUT() each frame (immediate-mode style), so you can
 * change any field between frames — including toggling callbacks on/off.
 *
 * Field order is the public API contract. C++20 designated initialisation
 * requires fields to be listed in declaration order.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct Clay_TextInputConfig {
    /* ── Sizing & layout ──────────────────────────────────────────────── */
    Clay_Sizing     sizing;    /* same as Clay_LayoutConfig.sizing */
    Clay_Padding    padding;   /* inner padding; try CLAY_PADDING_ALL(8) */

    /* ── Text ─────────────────────────────────────────────────────────── */
    Clay_TextElementConfig textConfig; /* the text rendering config */
    /* Shown when the buffer is empty and the element is unfocused.
     * Use CLAY_STRING("…") or (Clay_String){ } to set. */
    Clay_String      placeholder;
    bool             passwordMode;  /* render '*' per codepoint           */

    /* ── Colours (all RGBA 0–255) ─────────────────────────────────────── */
    Clay_Color       colorPlaceholder;   /* placeholder color, it's seperate from textConfig color */
    Clay_Color       colorBackground;
    Clay_Color       colorBorder;        /* unfocused border               */
    Clay_Color       colorBorderFocus;   /* focused border                 */
    Clay_Color       colorSelection;     /* selection highlight            */
    Clay_Color       colorCursor;

    /* ── Shape ────────────────────────────────────────────────────────── */
    Clay_CornerRadius cornerRadius;  /* CLAY_CORNER_RADIUS(r) for uniform */
    Clay_BorderWidth  borderWidth;   /* CLAY_BORDER_*(w) for uniform */

    /* ── Extra Layout ────────────────────────────────────────────────────────── */
    Clay_FloatingElementConfig floating;

    /* ── Behaviour ────────────────────────────────────────────────────── */
    /* Maximum codepoints; 0 = limited only by bufferSize. */
    int              maxLength;
    /* Cursor blink half-period in seconds; 0 → default 0.53 s. */
    float            cursorBlinkPeriod;

    /* ── Callbacks (all optional; NULL disables) ──────────────────────── */

    /* Dynamic buffer growth.  NULL → static mode (no allocation). */
    Clay_TI_ResizeFn   onResize;
    void              *resizeUserData;

    /* Per-character filter.  NULL → accept all. */
    Clay_TI_CharFilterFn  onCharFilter;
    void                 *charFilterUserData;

    /* Post-edit notification. */
    Clay_TI_ChangedFn  onChanged;
    void              *changedUserData;
} Clay_TextInputConfig;

/* ═══════════════════════════════════════════════════════════════════════════
 * Clay_TextInputState
 *
 * Retains all per-input mutable data across frames.
 * Lives wherever the caller wants (static, stack, heap, arena — anything).
 *
 * Initialise with Clay_TextInputState_Static() or Clay_TextInputState_Dynamic()
 * (not with CLAY__INIT directly — the helpers zero-fill correctly).
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct Clay_TextInputState {
    /* Buffer — NEVER cache these pointers; in dynamic mode the widget may
     * update them after a successful resize callback. */
    char *text;
    int   bufferSize;   /* total bytes including null terminator    */
    int   textLen;      /* current strlen(text)                     */

    /* Editing */
    int   cursorPos;         /* byte offset of the insert point  */
    int   selectionAnchor;   /* byte offset; -1 = no selection   */

    /* Focus / blink */
    bool   focused;
    double cursorBlinkTimer;
    bool   cursorVisible;

    /* ── Internal ─────────────────────────────────────────────────────── */
    bool             _isDynamic;        /* true → widget may call onResize                                              */
    Clay_ElementId   _elementId;
    char            *_calcText;         /* calculation buffer grows with the text buffer for calculation and display    */
    int             _calcBufferSize;    /* total bytes for calculation including null terminator                        */
    /* Cached copy of config from the most recent CLAY_TEXT_INPUT() call.
     * Kept here so OnKey / OnChar can see callbacks without extra args. */
    Clay_TextInputConfig _cfg;
    uint64_t _lastClickFrame;
    bool     _lastClickWasDouble;
    bool     _dragSelecting;
    int      _dragAnchor;
} Clay_TextInputState;

/* ── State initialiser helpers ────────────────────────────────────────────── */
/*
 * These are inline functions rather than macros so that C++ gets proper
 * zero-initialisation without relying on compound-literal extensions.
 *
 * STATIC:
 *   static char buf[256] = "";
 *   static Clay_TextInputState s = Clay_TextInputState_Static(buf, 256, CLAY_ID("x"));
 *
 * DYNAMIC (allocates immediately; returns the state by value for convenience):
 *   Clay_TextInputState s;
 *   if (!Clay_TextInputState_InitDynamic(&s, 64, CLAY_ID("x"), myResize, NULL))
 *       // handle failure
 */

static inline Clay_TextInputState Clay_TextInputState_Static(
    char *buf, int bufferSize, char *calcBuf, int calcBufferSize,
    Clay_ElementId elementId)
{
    Clay_TextInputState s;
    /* Zero everything first — this is the C99-safe way to get default-zero
     * for all fields without listing every one. */
#ifdef __cplusplus
    s = Clay_TextInputState{};
#else
    s = (Clay_TextInputState){0};
#endif
    s.text                  = buf;
    s.bufferSize            = bufferSize;
    s._calcText             = calcBuf;
    s._calcBufferSize       = calcBufferSize;
    s.textLen               = 0; /* caller pre-fills or leaves empty */
    s.selectionAnchor       = -1;
    s.cursorVisible         = true;
    s._elementId            = elementId;
    s._isDynamic            = false;
    return s;
}

CLAY_DLL_EXPORT bool Clay_TextInputState_InitDynamic(Clay_TextInputState *state,
                                     int initialCapacity,
                                     Clay_ElementId elementId,
                                     Clay_TI_ResizeFn resizeFn,
                                     void *resizeUserData);

CLAY_DLL_EXPORT void Clay_TextInputState_Destroy(Clay_TextInputState *state,
                                 void (*freeFn)(void *));

CLAY_DLL_EXPORT void Clay_TextInputState_Insert(Clay_TextInputState *state,
                                int at, const char *s, int slen);

/* ── Module lifecycle ─────────────────────────────────────────────────────── */

CLAY_DLL_EXPORT void Clay_TextInput_SetPlatform(Clay_TextInput_Platform platform);
CLAY_DLL_EXPORT void Clay_TextInput_Update(double deltaTime);

/* ── Event feed ───────────────────────────────────────────────────────────── */

CLAY_DLL_EXPORT void Clay_TextInput_OnChar   (uint32_t codepoint);
CLAY_DLL_EXPORT void Clay_TextInput_OnKey    (Clay_TI_Key key, Clay_TI_Action action, Clay_TI_Mod mods);

/* ── Core element function (do not call directly — use the macro) ─────────── */

CLAY_DLL_EXPORT void Clay_TextInput__Element(Clay_TextInputState        *state,
                             const Clay_TextInputConfig *cfg);

CLAY_DLL_EXPORT void Clay_TextInput__Element_Auto(Clay_ElementId             id,
                                  Clay_TextInputState        *state,
                                  const Clay_TextInputConfig *cfg);

/* ═══════════════════════════════════════════════════════════════════════════
 * CLAY_TEXT_INPUT  —  the public-facing macro
 *
 * Usage mirrors CLAY_TEXT():
 *
 *   CLAY_TEXT_INPUT(&myState, {
 *       .width  = CLAY_SIZING_FIXED(300),
 *       .height = CLAY_SIZING_FIXED(36),
 *       .textConfig       = {},
 *       .placeholder      = CLAY_STRING("Enter text…"),
 *       .colorBackground  = {  30,  30,  30, 255 },
 *       .colorBorder      = {  80,  80,  80, 255 },
 *       .colorBorderFocus = { 100, 160, 255, 255 },
 *       .colorPlaceholder = { 100, 100, 100, 255 },
 *       .colorSelection   = {  60, 120, 210, 160 },
 *       .colorCursor      = { 220, 220, 220, 255 },
 *       .cornerRadius     = CLAY_CORNER_RADIUS(6),
 *       .borderWidth      = 1.5f,
 *       .padding          = CLAY_PADDING_ALL(8),
 *   });
 *
 * The second argument is the body of a Clay_TextInputConfig designated
 * initialiser.  Fields you omit are zero (colours default to transparent,
 * sizing defaults to zero — you must set at least width/height).
 *
 * You can also pass a pre-built config variable:
 *   Clay_TextInputConfig cfg = { ... };
 *   CLAY_TEXT_INPUT(CLAY_ID("ID"), &myState, &cfg);
 * ═══════════════════════════════════════════════════════════════════════════ */
#define CLAY_TEXT_INPUT(id, state, cfg)                                    \
    Clay_TextInput__Element_Auto(                                          \
        (id),                                                              \
        (state),                                                           \
        (cfg))

#define CLAY_TEXT_INPUT_WITH_STATE(state, cfg)                             \
    Clay_TextInput__Element(                                               \
        (state),                                                           \
        (cfg))

/*
 * Discouraged to use it, since the Clay_TextInputConfig is relatively big.
 * It creates a temporary Clay_TextInputConfig again and again and passes to CLAY_TEXT_INPUT,
 * it can be used with CLAY_TEXT_INPUT*:
 *   CLAY_TEXT_INPUT_CONFIG(CLAY_ID("ID"), &myState, { ... })
 */
#define CLAY_TEXT_INPUT_WITH_CONFIG(id, state, ...) {                                   \
        Clay_TextInputConfig config = (CLAY__INIT(Clay_TextInputConfig)__VA_ARGS__);    \
        CLAY_TEXT_INPUT((id), (state), &config);                                        \
    } while(0)

#define CLAY_TEXT_INPUT_WITH_STATE_CONFIG(state, ...) {                                 \
        Clay_TextInputConfig config = (CLAY__INIT(Clay_TextInputConfig)__VA_ARGS__);    \
        CLAY_TEXT_INPUT_WITH_STATE((state), &config);                                   \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* CLAY_TEXT_INPUT_H */


/* ═══════════════════════════════════════════════════════════════════════════
 * IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifdef CLAY_TEXT_INPUT_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── Module globals ───────────────────────────────────────────────────────── */

static Clay_TextInput_Platform g_cti_platform;
static bool                    g_cti_cursor_is_ibeam = false;
static Clay_TextInputState    *g_cti_focused         = NULL;
static uint64_t                g_cti_frame_count     = 0;

/* ── UTF-8 helpers ────────────────────────────────────────────────────────── */

static int cti_utf8_next(const char *t, int pos, int len) {
    if (pos >= len) return len;
    pos++;
    while (pos < len && (t[pos] & 0xC0) == 0x80) pos++;
    return pos;
}

static int cti_utf8_prev(const char *t, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (t[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

static int cti_utf8_cp_count(const char *t, int byteLen) {
    int n = 0, i = 0;
    while (i < byteLen) {
        unsigned char c = (unsigned char)t[i];
        if      ((c & 0xF8) == 0xF0) i += 4;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else                         i += 1;
        n++;
    }
    return n;
}

static int cti_word_left(const char *t, int pos) {
    while (pos > 0 && !isalnum((unsigned char)t[pos - 1])) pos--;
    while (pos > 0 &&  isalnum((unsigned char)t[pos - 1])) pos--;
    return pos;
}

static int cti_word_right(const char *t, int pos, int len) {
    while (pos < len && !isalnum((unsigned char)t[pos])) pos++;
    while (pos < len &&  isalnum((unsigned char)t[pos])) pos++;
    return pos;
}

/* ── Dynamic resize ───────────────────────────────────────────────────────── */

static bool cti_try_grow(Clay_TextInputState *s, int minCapacity) {
    if (!s->_cfg.onResize || s->bufferSize >= minCapacity) return s->bufferSize >= minCapacity;
    Clay_TI_ResizeResult r = s->_cfg.onResize(s->text, s->bufferSize,
                                               minCapacity, s->_cfg.resizeUserData);
    if (!r.buf || r.capacity < minCapacity) return false;
    Clay_TI_ResizeResult rCalc = s->_cfg.onResize(s->_calcText, s->_calcBufferSize,
                                               minCapacity, s->_cfg.resizeUserData);
    if (!rCalc.buf || rCalc.capacity < minCapacity) return false;
    s->text             = r.buf;
    s->bufferSize       = r.capacity;
    s->_calcText        = rCalc.buf;
    s->_calcBufferSize  = rCalc.capacity;
    return true;
}

/* ── Editing primitives ───────────────────────────────────────────────────── */

static void cti_delete_range(Clay_TextInputState *s, int lo, int hi) {
    if (lo < 0)          lo = 0;
    if (hi > s->textLen) hi = s->textLen;
    if (lo >= hi) return;
    int n = hi - lo;
    memmove(s->text + lo, s->text + hi, (size_t)(s->textLen - hi + 1));
    s->textLen -= n;
    if      (s->cursorPos > hi) s->cursorPos -= n;
    else if (s->cursorPos > lo) s->cursorPos  = lo;
    s->selectionAnchor = -1;
}

static void cti_delete_notify(Clay_TextInputState *s, int lo, int hi) {
    int before = s->textLen;
    cti_delete_range(s, lo, hi);
    if (s->textLen != before && s->_cfg.onChanged)
        s->_cfg.onChanged(s->text, s->textLen, s->_cfg.changedUserData);
}

static void cti_insert(Clay_TextInputState *s, const char *str, int strLen) {
    /* Replace selection */
    if (s->selectionAnchor >= 0 && s->selectionAnchor != s->cursorPos) {
        int lo = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
        int hi = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
        cti_delete_range(s, lo, hi);
        s->cursorPos = lo;
    }
    s->selectionAnchor = -1;

    /* Trim to maxLength (codepoints) */
    if (s->_cfg.maxLength > 0) {
        int rem = s->_cfg.maxLength - cti_utf8_cp_count(s->text, s->textLen);
        if (rem <= 0) return;
        int cp = 0, i = 0;
        while (i < strLen && cp < rem) {
            unsigned char c = (unsigned char)str[i];
            if      ((c & 0xF8) == 0xF0) i += 4;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else if ((c & 0xE0) == 0xC0) i += 2;
            else                         i += 1;
            cp++;
        }
        strLen = i;
    }
    if (strLen <= 0) return;

    /* Ensure capacity */
    int needed = s->textLen + strLen + 1;
    if (needed > s->bufferSize) {
        if (s->_isDynamic) {
            if (!cti_try_grow(s, needed)) return;
        } else {
            /* Static: hard-cap at buffer boundary, respecting UTF-8 boundaries */
            strLen = (s->bufferSize - 1) - s->textLen;
            if (strLen <= 0) return;
            while (strLen > 0 && (str[strLen] & 0xC0) == 0x80) strLen--;
            if (strLen <= 0) return;
        }
    }

    memmove(s->text + s->cursorPos + strLen,
            s->text + s->cursorPos,
            (size_t)(s->textLen - s->cursorPos + 1));
    memcpy(s->text + s->cursorPos, str, (size_t)strLen);
    s->textLen   += strLen;
    s->cursorPos += strLen;

    if (s->_cfg.onChanged)
        s->_cfg.onChanged(s->text, s->textLen, s->_cfg.changedUserData);
}

static void cti_copy_selection(const Clay_TextInputState *s) {
    if (!g_cti_platform.setClipboardText) return;
    if (s->selectionAnchor < 0 || s->selectionAnchor == s->cursorPos) return;
    int lo  = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
    int hi  = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
    int len = hi - lo;
    char *tmp = (char *)malloc((size_t)(len + 1));
    if (!tmp) return;
    memcpy(tmp, s->text + lo, (size_t)len);
    tmp[len] = '\0';
    g_cti_platform.setClipboardText(g_cti_platform.userData, tmp);
    free(tmp);
}

/* ── Focus helpers ────────────────────────────────────────────────────────── */

static void cti_focus(Clay_TextInputState *s) {
    if (g_cti_focused && g_cti_focused != s) {
        g_cti_focused->focused         = false;
        g_cti_focused->selectionAnchor = -1;
    }
    s->focused          = true;
    s->cursorBlinkTimer = 0.0;
    s->cursorVisible    = true;
    g_cti_focused       = s;
}

static void cti_unfocus(Clay_TextInputState *s) {
    s->focused         = false;
    s->selectionAnchor = -1;
    s->_dragSelecting  = false;
    if (g_cti_focused == s) g_cti_focused = NULL;
}

/* ── Public: lifecycle ────────────────────────────────────────────────────── */

void Clay_TextInput_SetPlatform(Clay_TextInput_Platform platform) {
    g_cti_platform    = platform;
    g_cti_focused     = NULL;
}

void Clay_TextInput_Update(double dt) {
    g_cti_frame_count++;
    if (!g_cti_focused) return;
    g_cti_focused->cursorBlinkTimer += dt;
    double period = g_cti_focused->_cfg.cursorBlinkPeriod > 0.0f
                    ? (double)g_cti_focused->_cfg.cursorBlinkPeriod : 0.53;
    while (g_cti_focused->cursorBlinkTimer >= period) {
        g_cti_focused->cursorBlinkTimer -= period;
        g_cti_focused->cursorVisible     = !g_cti_focused->cursorVisible;
    }
}

/* ── Public: state init ───────────────────────────────────────────────────── */

bool Clay_TextInputState_InitDynamic(Clay_TextInputState *state,
                                     int initialCapacity,
                                     Clay_ElementId elementId,
                                     Clay_TI_ResizeFn resizeFn,
                                     void *resizeUserData) {
#ifdef __cplusplus
    *state = Clay_TextInputState{};
#else
    *state = (Clay_TextInputState){0};
#endif
    state->selectionAnchor        = -1;
    state->cursorVisible          = true;
    state->_elementId             = elementId;
    state->_isDynamic             = true;
    state->_cfg.onResize          = resizeFn;
    state->_cfg.resizeUserData    = resizeUserData;

    Clay_TI_ResizeResult r = resizeFn(NULL, 0, initialCapacity, resizeUserData);
    if (!r.buf || r.capacity < initialCapacity) return false;
    Clay_TI_ResizeResult rCalc = resizeFn(NULL, 0, initialCapacity, resizeUserData);
    if (!rCalc.buf || rCalc.capacity < initialCapacity) return false;
    state->text             = r.buf;
    state->bufferSize       = r.capacity;
    state->text[0]          = '\0';
    state->_calcText        = rCalc.buf;
    state->_calcBufferSize  = rCalc.capacity;
    state->_calcText[0]     = '\0';
    return true;
}

void Clay_TextInputState_Destroy(Clay_TextInputState *state, void (*freeFn)(void *)) {
    if (state->_isDynamic && freeFn) {
        if (state->text)
            freeFn(state->text);
        if (state->_calcText)
            freeFn(state->_calcText);
    }
#ifdef __cplusplus
    *state = Clay_TextInputState{};
#else
    *state = (Clay_TextInputState){0};
#endif
}

void Clay_TextInputState_Insert(Clay_TextInputState *state,
                                int at, const char *s, int slen) {
    if (at < 0) at = 0;
    else if (at > state->textLen) at = state->textLen;
    int prevCursorPos = state->cursorPos;
    state->cursorPos = at;
    cti_insert(state, s, slen);
}

/* ── Public: event feed ───────────────────────────────────────────────────── */

void Clay_TextInput_OnChar(uint32_t cp) {
    if (!g_cti_focused) return;
    if (g_cti_focused->_cfg.onCharFilter)
        if (!g_cti_focused->_cfg.onCharFilter(cp, g_cti_focused->_cfg.charFilterUserData))
            return;

    char utf8[5] = {0}; int len = 0;
    if      (cp < 0x80)    { utf8[len++] = (char)cp; }
    else if (cp < 0x800)   { utf8[len++] = (char)(0xC0 | (cp >>  6));
                             utf8[len++] = (char)(0x80 | ( cp        & 0x3F)); }
    else if (cp < 0x10000) { utf8[len++] = (char)(0xE0 | (cp >> 12));
                             utf8[len++] = (char)(0x80 | ((cp >>  6) & 0x3F));
                             utf8[len++] = (char)(0x80 | ( cp        & 0x3F)); }
    else                   { utf8[len++] = (char)(0xF0 | (cp >> 18));
                             utf8[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                             utf8[len++] = (char)(0x80 | ((cp >>  6) & 0x3F));
                             utf8[len++] = (char)(0x80 | ( cp        & 0x3F)); }
    cti_insert(g_cti_focused, utf8, len);
    g_cti_focused->cursorBlinkTimer = 0.0;
    g_cti_focused->cursorVisible    = true;
}

void Clay_TextInput_OnKey(Clay_TI_Key key, Clay_TI_Action action, Clay_TI_Mod mods) {
    if (!g_cti_focused) return;
    if (action != CLAY_TI_ACTION_PRESS && action != CLAY_TI_ACTION_REPEAT) return;

    Clay_TextInputState *s    = g_cti_focused;
    bool ctrl  = (mods & CLAY_TI_MOD_CTRL)  != 0;
    bool shift = (mods & CLAY_TI_MOD_SHIFT) != 0;

#define CTI__SHIFT_ANCHOR() do { \
    if  (shift && s->selectionAnchor < 0) s->selectionAnchor = s->cursorPos; \
    else if (!shift)                      s->selectionAnchor = -1;            \
} while (0)

    switch (key) {
    case CLAY_TI_KEY_LEFT:
        CTI__SHIFT_ANCHOR();
        if (!shift && s->selectionAnchor >= 0) {
            s->cursorPos = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            s->selectionAnchor = -1;
        } else {
            s->cursorPos = ctrl ? cti_word_left(s->text, s->cursorPos)
                                : cti_utf8_prev(s->text, s->cursorPos);
        }
        break;
    case CLAY_TI_KEY_RIGHT:
        CTI__SHIFT_ANCHOR();
        if (!shift && s->selectionAnchor >= 0) {
            s->cursorPos = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            s->selectionAnchor = -1;
        } else {
            s->cursorPos = ctrl ? cti_word_right(s->text, s->cursorPos, s->textLen)
                                : cti_utf8_next(s->text, s->cursorPos, s->textLen);
        }
        break;
    case CLAY_TI_KEY_HOME: CTI__SHIFT_ANCHOR(); s->cursorPos = 0;           break;
    case CLAY_TI_KEY_END:  CTI__SHIFT_ANCHOR(); s->cursorPos = s->textLen;  break;

    case CLAY_TI_KEY_BACKSPACE: {
        int lo, hi;
        if (s->selectionAnchor >= 0 && s->selectionAnchor != s->cursorPos) {
            lo = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            hi = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            s->cursorPos = lo;
        } else if (s->cursorPos > 0) {
            lo = ctrl ? cti_word_left(s->text, s->cursorPos) : cti_utf8_prev(s->text, s->cursorPos);
            hi = s->cursorPos;
            s->cursorPos = lo;
        } else { break; }
        cti_delete_notify(s, lo, hi);
        s->selectionAnchor = -1;
        break;
    }
    case CLAY_TI_KEY_DELETE: {
        int lo, hi;
        if (s->selectionAnchor >= 0 && s->selectionAnchor != s->cursorPos) {
            lo = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            hi = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
            s->cursorPos = lo;
        } else if (s->cursorPos < s->textLen) {
            lo = s->cursorPos;
            hi = ctrl ? cti_word_right(s->text, s->cursorPos, s->textLen) : cti_utf8_next(s->text, s->cursorPos, s->textLen);
        } else { break; }
        cti_delete_notify(s, lo, hi);
        s->selectionAnchor = -1;
        break;
    }
    case CLAY_TI_KEY_A:
        if (ctrl) { s->selectionAnchor = 0; s->cursorPos = s->textLen; }
        break;
    case CLAY_TI_KEY_C:
        if (ctrl) cti_copy_selection(s);
        break;
    case CLAY_TI_KEY_X:
        if (ctrl) {
            cti_copy_selection(s);
            if (s->selectionAnchor >= 0 && s->selectionAnchor != s->cursorPos) {
                int lo = s->cursorPos < s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
                int hi = s->cursorPos > s->selectionAnchor ? s->cursorPos : s->selectionAnchor;
                cti_delete_notify(s, lo, hi);
                s->cursorPos = lo;
            }
        }
        break;
    case CLAY_TI_KEY_V:
        if (ctrl && g_cti_platform.getClipboardText) {
            const char *clip = g_cti_platform.getClipboardText(g_cti_platform.userData);
            if (clip) cti_insert(s, clip, (int)strlen(clip));
        }
        break;
    case CLAY_TI_KEY_ESCAPE:
    case CLAY_TI_KEY_ENTER:
        cti_unfocus(s);
        break;
    default: break;
    }

#undef CTI__SHIFT_ANCHOR

    s->cursorBlinkTimer = 0.0;
    s->cursorVisible    = true;
}

/* ── Layout helpers ───────────────────────────────────────────────────────── */

static int cti_display_str(const Clay_TextInputState *s,
                            const Clay_TextInputConfig *cfg) {
    if (cfg->passwordMode) {
        int out = 0, i = 0;
        while (i < s->textLen) {
            assert((out < s->_calcBufferSize - 1) && "Calc buffer overflown?");
            unsigned char c = (unsigned char)s->text[i];
            if      ((c & 0xF8) == 0xF0) i += 4;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else if ((c & 0xE0) == 0xC0) i += 2;
            else                         i += 1;
            s->_calcText[out++] = '*';
        }
        s->_calcText[out] = '\0';
        return out;
    }
    assert((s->textLen < s->_calcBufferSize - 1) && "Calc buffer overflown?");
    memcpy(s->_calcText, s->text, (size_t)s->textLen);
    s->_calcText[s->textLen] = '\0';
    return s->textLen;
}

static float cti_measure_to(const Clay_TextInputState *s,
                             const Clay_TextInputConfig *cfg,
                             int bytePos) {
    if (bytePos < 0) {
        bytePos = 0;
    }
    if (bytePos > s->textLen) {
        bytePos = s->textLen;
    }

    int dispLen;
    if (cfg->passwordMode) {
        dispLen = cti_utf8_cp_count(s->text, bytePos);
        assert((dispLen < s->_calcBufferSize - 1) && "Calc buffer overflown?");
        memset(s->_calcText, '*', (size_t)dispLen);
        s->_calcText[dispLen] = '\0';
    } else {
        dispLen = bytePos;
        assert((dispLen < s->_calcBufferSize - 1) && "Calc buffer overflown?");
        memcpy(s->_calcText, s->text, (size_t)dispLen);
        s->_calcText[dispLen] = '\0';
    }
    Clay_StringSlice slice = CLAY__INIT(Clay_StringSlice){ .length = dispLen, .chars = s->_calcText, .baseChars = s->_calcText };
    // Usually Clay__MeasureText shouldn't modify the Clay_TextElementConfig, it could cause UB, Clay should update
    // Clay__MeasureText argument types.
    return Clay__MeasureText(slice, (Clay_TextElementConfig*)&cfg->textConfig, Clay_GetCurrentContext()->measureTextUserData).width;
}

static int cti_byte_at_x(const Clay_TextInputState *s,
                         const Clay_TextInputConfig *cfg,
                         float x) {
    if (s->textLen <= 0 || x <= 0.0f) {
        return 0;
    }

    float totalWidth = cti_measure_to(s, cfg, s->textLen);
    if (x >= totalWidth) {
        return s->textLen;
    }

    int prev = 0;
    while (prev < s->textLen) {
        int next = cti_utf8_next(s->text, prev, s->textLen);
        float prevWidth = cti_measure_to(s, cfg, prev);
        float nextWidth = cti_measure_to(s, cfg, next);
        float mid = (prevWidth + nextWidth) * 0.5f;
        if (x < mid) {
            return prev;
        }
        prev = next;
    }

    return s->textLen;
}

static bool cti_byte_at_x_if_in_bounds(const Clay_TextInputState *s,
                                       const Clay_TextInputConfig *cfg,
                                       float x,
                                       int *outBytePos) {
    float totalWidth = cti_measure_to(s, cfg, s->textLen);
    if (x < 0.0f || x > totalWidth) {
        return false;
    }
    *outBytePos = cti_byte_at_x(s, cfg, x);
    return true;
}

/* ── Public: element ──────────────────────────────────────────────────────── */

void Clay_TextInput__Element(Clay_TextInputState        *state,
                             const Clay_TextInputConfig *cfg) {
    /* Cache config — OnKey / OnChar read s->_cfg for callbacks. */
    state->_cfg = *cfg;
    Clay_Context* context = Clay_GetCurrentContext();

    /* Click → focus / unfocus */
    Clay_PointerData pointerData = context->pointerInfo;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        if (Clay_PointerOver(state->_elementId)) {
            Clay_ElementData elementData = Clay_GetElementData(state->_elementId);
            cti_focus(state);

            bool isDoubleClick = state->_lastClickFrame > 0
                                 && !state->_lastClickWasDouble
                                 && (g_cti_frame_count - state->_lastClickFrame) <= 20;
            if (isDoubleClick) {
                state->selectionAnchor = 0;
                state->cursorPos       = state->textLen;
                state->_dragSelecting  = false;
                state->_lastClickWasDouble = true;
            } else {
                bool hasClickPos = false;
                int clickPos = state->cursorPos;
                if (elementData.found) {
                    float clickX = pointerData.position.x - elementData.boundingBox.x - (float)cfg->padding.left;
                    hasClickPos = cti_byte_at_x_if_in_bounds(state, cfg, clickX, &clickPos);
                }

                if (hasClickPos) {
                    state->cursorPos = clickPos;
                }
                state->selectionAnchor = -1;
                state->_dragAnchor     = state->cursorPos;
                state->_dragSelecting  = hasClickPos;
                state->_lastClickWasDouble = false;
            }

            state->_lastClickFrame = g_cti_frame_count;
            state->cursorBlinkTimer = 0.0;
            state->cursorVisible    = true;
        } else if (g_cti_focused == state) {
            cti_unfocus(state);
        }
    }

    if (state->focused && state->_dragSelecting && pointerData.state == CLAY_POINTER_DATA_PRESSED) {
        Clay_ElementData elementData = Clay_GetElementData(state->_elementId);
        if (elementData.found) {
            float dragX = pointerData.position.x - elementData.boundingBox.x - (float)cfg->padding.left;
            int dragPos = cti_byte_at_x(state, cfg, dragX);
            state->cursorPos = dragPos;
            if (dragPos == state->_dragAnchor) {
                state->selectionAnchor = -1;
            } else {
                state->selectionAnchor = state->_dragAnchor;
            }
            state->cursorBlinkTimer = 0.0;
            state->cursorVisible    = true;
        }
    }

    if (pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
        state->_dragSelecting = false;
    }

    if (g_cti_platform.setIbeamCursor && !g_cti_cursor_is_ibeam && Clay_PointerOver(state->_elementId)) {
        g_cti_cursor_is_ibeam = true;
        g_cti_platform.setIbeamCursor(g_cti_platform.userData);
    }
    else if (g_cti_platform.resetCursor && g_cti_cursor_is_ibeam && !Clay_PointerOver(state->_elementId)) {
        g_cti_cursor_is_ibeam = false;
        g_cti_platform.resetCursor(g_cti_platform.userData);
    }

    Clay_Color borderColor = state->focused ? cfg->colorBorderFocus : cfg->colorBorder;

    CLAY(state->_elementId, {
        .layout = {
            .sizing         = cfg->sizing,
            .padding        = cfg->padding,
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = cfg->colorBackground,
        .cornerRadius    = cfg->cornerRadius,
        .floating        = cfg->floating,
        .border = {
            .color = borderColor,
            .width = cfg->borderWidth,
        }
    }) {
        const char* TEST_WIDTH_CHAR = " ";
        const float visualXBias = Clay__MeasureText(CLAY__INIT(Clay_StringSlice){ .length = 1, .chars = TEST_WIDTH_CHAR, .baseChars = TEST_WIDTH_CHAR }, (Clay_TextElementConfig*)&cfg->textConfig, context->measureTextUserData).width * 0.33f;
        const float cursorH = cfg->textConfig.fontSize;
        const float cursorW = 2.0f;

        float textOffsetX = (float)cfg->padding.left;
        float cursorX = 0.0f;
        float selectionX = 0.0f;
        float selectionW = 0.0f;
        bool hasSelection = false;

        if (state->focused) {
            cursorX = cti_measure_to(state, cfg, state->cursorPos);
            if (state->selectionAnchor >= 0 && state->selectionAnchor != state->cursorPos) {
                int lo = state->cursorPos < state->selectionAnchor ? state->cursorPos : state->selectionAnchor;
                int hi = state->cursorPos > state->selectionAnchor ? state->cursorPos : state->selectionAnchor;
                float sx = cti_measure_to(state, cfg, lo);
                float sw = cti_measure_to(state, cfg, hi) - sx;
                selectionX = sx;
                selectionW = sw;
                hasSelection = true;
            }
        }

        /* Placeholder or text content */
        if (state->textLen == 0 && cfg->placeholder.length > 0 && !state->focused) {
            Clay_TextElementConfig placeholderConfig = cfg->textConfig;
            placeholderConfig.textColor = cfg->colorPlaceholder;
            CLAY_TEXT(cfg->placeholder, placeholderConfig);
        } else {
            int dispLen = cti_display_str(state, cfg);
            Clay_String ds = CLAY__INIT(Clay_String){ .length = dispLen, .chars = state->_calcText };
            CLAY_TEXT(ds, cfg->textConfig);
        }

        /* Cursor / selection (focused only) */
        if (state->focused) {
            /* Selection highlight */
            if (hasSelection) {
                CLAY_AUTO_ID({
                    .layout          = { .sizing = { CLAY_SIZING_FIXED(selectionW), CLAY_SIZING_FIXED(cursorH) } },
                    .backgroundColor = cfg->colorSelection,
                    .floating = {
                        .offset       = { textOffsetX + selectionX + visualXBias, 0 },
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_CENTER,
                            .parent  = CLAY_ATTACH_POINT_LEFT_CENTER,
                        },
                        .attachTo     = CLAY_ATTACH_TO_PARENT
                    }
                }) {}
            }

            /* Cursor bar */
            if (state->cursorVisible && !hasSelection) {
                CLAY_AUTO_ID({
                    .layout          = { .sizing = { CLAY_SIZING_FIXED(cursorW), CLAY_SIZING_FIXED(cursorH) } },
                    .backgroundColor = cfg->colorCursor,
                    .floating = {
                        .offset       = { textOffsetX + cursorX + 0.5f, 0 },
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_CENTER,
                            .parent  = CLAY_ATTACH_POINT_LEFT_CENTER,
                        },
                        .attachTo     = CLAY_ATTACH_TO_PARENT
                    }
                }) {}
            }
        }
    }
}

void Clay_TextInput__Element_Auto(Clay_ElementId        id,
                             Clay_TextInputState        *state,
                             const Clay_TextInputConfig *cfg) {
    if (state == NULL || state->text == NULL) {
        // Init the text input state
        if (cfg->onResize == NULL
            || !Clay_TextInputState_InitDynamic(state, 16, id, cfg->onResize, cfg->resizeUserData)) {
            Clay_Context* context = Clay_GetCurrentContext();
            context->errorHandler.errorHandlerFunction(CLAY__INIT(Clay_ErrorData) {
                .errorType = CLAY_ERROR_TYPE_INTERNAL_ERROR,
                .errorText = CLAY_STRING("CLAY_TEXT_INPUT or Clay_TextInput__Element_Auto needs to allocate text and display buffer dynamically, either the resize function failed or no resize function was provided."),
                .userData = context->errorHandler.userData });
            return;
        }
    }

    Clay_TextInput__Element(state, cfg);
}

#endif /* CLAY_TEXT_INPUT_IMPLEMENTATION */