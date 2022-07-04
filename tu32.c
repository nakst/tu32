// TODO comctl32.dll wrappers: 
// 	- combo boxes
// 	- date and time pickers
// 	- hot keys
// 	- hyperlinks
// 	- IP address controls
// 	- list boxes
// 	- list views
// 	- month calendars
// 	- rich textboxes
// 	- scroll bars (cropping HWNDs?)
// 	- sliders
// 	- status bars
// 	- tabs
// 	- toolbars
// 	- tooltips
// 	- tree views
// 	- up-down controls
// TODO user32.dll wrappers:
// 	- clipboard
// 	- dialogs
// 	- menus
// TODO mctrl wrappers:
// 	- chart
// 	- expand
// 	- grid
// 	- HTML
// 	- image view
// 	- tree-list view
// TODO Better radiobox support.
// TODO Rich text display?
// TODO Flicker-free repainting?

#define UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellscalingapi.h>
#include <shlwapi.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

typedef HRESULT (*GetDpiForMonitorType)(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);
typedef BOOL (*SetProcessDpiAwarenessContextType)(DPI_AWARENESS_CONTEXT value);

/////////////////////////////////////////
// Definitions.
/////////////////////////////////////////

#define TU_UPDATE_HOVERED (1)
#define TU_UPDATE_PRESSED (2)
#define TU_UPDATE_FOCUSED (3)

// For TUDrawString.
#define TU_ALIGN_LEFT (1)
#define TU_ALIGN_RIGHT (2)
#define TU_ALIGN_CENTER (3)

#define TU_CALLOC(x) HeapAlloc(tuGlobal.heap, HEAP_ZERO_MEMORY, (x))
#define TU_FREE(x) HeapFree(tuGlobal.heap, 0, (x))
#define TU_MALLOC(x) HeapAlloc(tuGlobal.heap, 0, (x))
#define TU_REALLOC _TUHeapReAlloc

#define TU_CURSOR_ARROW (0)
#define TU_CURSOR_TEXT (1)
#define TU_CURSOR_SPLIT_V (2)
#define TU_CURSOR_SPLIT_H (3)
#define TU_CURSOR_FLIPPED_ARROW (4)
#define TU_CURSOR_CROSS_HAIR (5)
#define TU_CURSOR_HAND (6)
#define TU_CURSOR_RESIZE_UP (7)
#define TU_CURSOR_RESIZE_LEFT (8)
#define TU_CURSOR_RESIZE_UP_RIGHT (9)
#define TU_CURSOR_RESIZE_UP_LEFT (10)
#define TU_CURSOR_RESIZE_DOWN (11)
#define TU_CURSOR_RESIZE_RIGHT (12)
#define TU_CURSOR_RESIZE_DOWN_RIGHT (13)
#define TU_CURSOR_RESIZE_DOWN_LEFT (14)
#define TU_CURSOR_COUNT (15)

typedef enum TUMessage {
	TU_MSG_PAINT, // dp = paint context
	TU_MSG_LAYOUT,
	TU_MSG_DESTROY,
	TU_MSG_UPDATE, // di = TU_UPDATE_... constant
	TU_MSG_GET_WIDTH, // di = height (if known); return width
	TU_MSG_GET_HEIGHT, // di = width (if known); return height
	TU_MSG_UPDATE_FONT,
	TU_MSG_GET_CHILD_STABILITY, // dp = child element; return stable axes, 1 (width) | 2 (height)

	// Input events.
	TU_MSG_INPUT_EVENTS_START, // not sent to disabled elements
	TU_MSG_LEFT_DOWN,
	TU_MSG_LEFT_UP,
	TU_MSG_MIDDLE_DOWN,
	TU_MSG_MIDDLE_UP,
	TU_MSG_RIGHT_DOWN,
	TU_MSG_RIGHT_UP,
	TU_MSG_KEY_TYPED, // dp = pointer to TUKeyTyped; return 1 if handled
	TU_MSG_MOUSE_MOVE,
	TU_MSG_MOUSE_DRAG,
	TU_MSG_MOUSE_WHEEL, // di = delta; return 1 if handled
	TU_MSG_CLICKED,
	TU_MSG_GET_CURSOR, // return cursor code
	TU_MSG_INPUT_EVENTS_END,

	// Buttons.
	TU_MSG_DROPDOWN, // for TU_BUTTON_SPLIT

	// Textboxes.
	TU_MSG_TEXTBOX_MODIFIED,
	TU_MSG_TEXTBOX_START_FOCUS,
	TU_MSG_TEXTBOX_END_FOCUS,
	TU_MSG_TEXTBOX_KEY_DOWN, // di = virtual key code; return 1 to prevent default handling
				 
	// Windows.
	TU_MSG_WINDOW_CLOSE, // return 1 to prevent application exit
	TU_MSG_WINDOW_DROP_FILES, // di = count, dp = char ** of paths
	TU_MSG_WINDOW_ACTIVATE,
	TU_MSG_WINDOW_DEACTIVATE,
	TU_MSG_WINDOW_UPDATE_START,
	TU_MSG_WINDOW_UPDATE_BEFORE_DESTROY,
	TU_MSG_WINDOW_UPDATE_BEFORE_LAYOUT,
	TU_MSG_WINDOW_UPDATE_END,

	TU_MSG_USER,
} TUMessage;

typedef struct TUKeyTyped {
	char *text;
	int textBytes;
	intptr_t code;
} TUKeyTyped;

typedef struct TURectangle {
	int l, r, t, b;
} TURectangle;

typedef struct TUShortcut {
	int32_t vkCode;
	bool ctrl, shift, alt;
	void (*invoke)(void *cp);
	void *cp;
} TUShortcut;

struct TUElement;
typedef int (*TUMessageHandler)(struct TUElement *element, TUMessage message, int di, void *dp);

#ifdef TU_IMMEDIATE
typedef struct TUInteraction {
	struct TUElement *element;
	bool clicked, dropdown, modified, old /* element was not recreated this refresh */;
} TUInteraction;
#endif

typedef struct TUElement {
#define TU_HWND_BUTTON (1)
#define TU_HWND_EDIT   (2)
	HWND hwnd;
	uint32_t hwndType;

#define TU_ELEMENT_V_FILL (1 << 16)
#define TU_ELEMENT_H_FILL (1 << 17)
#define TU_ELEMENT_DISABLED (1 << 18)
#define TU_ELEMENT_RELAYOUT (1 << 28)
#define TU_ELEMENT_RELAYOUT_DESCENDENT (1 << 29)
#define TU_ELEMENT_DESTROY (1 << 30)
#define TU_ELEMENT_DESTROY_DESCENDENT (1 << 31)
	uint32_t flags; // First 16 bits are element specific.
	TURectangle bounds;

	struct TUElement *parent;
	struct TUElement **children;
	struct TUWindow *window;
	uint32_t childCount;

	void *cp; // Context pointer (for user).
	TUMessageHandler messageClass, messageUser;

	const char *cDebugName;

#ifdef TU_IMMEDIATE
	struct TUElement **oldChildren;
	ptrdiff_t discriminator;
	uint32_t oldChildCount;
	bool inRefresh;
	TUInteraction interaction, nextInteraction;
#endif
} TUElement;

typedef struct TULabel {
#define TU_LABEL_ALIGN_CENTER (1 << 0)
#define TU_LABEL_ALIGN_RIGHT  (1 << 1)
#define TU_LABEL_ELLIPSIS     (1 << 2)
	TUElement e;
	char *text;
	size_t textBytes;
	int cachedWidth, cachedHeight, cachedHeightDi;
} TULabel;

typedef struct TUButton {
#define TU_BUTTON_CHECKBOX    (1 << 0)
#define TU_BUTTON_RADIOBOX    (1 << 1)
#define TU_BUTTON_DIALOG_SIZE (1 << 2)
#define TU_BUTTON_SPLIT	      (1 << 3)
#define TU_BUTTON_DEFAULT     (1 << 4)
	TUElement e;
	char *text;
	size_t textBytes;
	int cachedWidth;
	void (*invoke)(void *cp);
	bool check;
} TUButton;

typedef struct TUPanel {
#define TU_PANEL_HORIZONTAL (1 << 0)
	TUElement e;
	TURectangle border;
	int gap;
} TUPanel;

typedef struct TUTextbox {
#define TU_TEXTBOX_READ_ONLY (1 << 0)
#define TU_TEXTBOX_PASSWORD  (1 << 1)
#define TU_TEXTBOX_MULTILINE (1 << 2)
#define TU_TEXTBOX_NUMBER    (1 << 3)
#define TU_TEXTBOX_WORD_WRAP (1 << 4)
#define TU_TEXTBOX_NO_BORDER (1 << 5)
	TUElement e;
	WNDPROC oldWindowProcedure;
} TUTextbox;

typedef struct TUProgressBar {
	TUElement e;
	float position;
} TUProgressBar;

typedef struct TUWindow {
#define TU_WINDOW_NOT_RESIZABLE	  (1 << 0)
#define TU_WINDOW_NO_CLOSE_BUTTON (1 << 1)
#define TU_WINDOW_NO_TITLEBAR     (1 << 2) // Window won't appear in alt-tab or taskbar.
#define TU_WINDOW_NO_BORDER       (1 << 3) // Implies NO_TITLEBAR and NOT_RESIZABLE.
#define TU_WINDOW_TOOLBOX         (1 << 4) // Cannot have NO_CLOSE_BUTTON, NO_TITLEBAR or NO_BORDER.
	TUElement e;

	HFONT font;
	float scale;

	TUShortcut *shortcuts;
	size_t shortcutCount;

	TUElement *hovered, *pressed, *focused;
	int pressedButton, cursorStyle;
	int cursorX, cursorY;
	bool ctrl, shift, alt;
	bool trackingLeave;
} TUWindow;

typedef struct TUGlobalState {
	TUWindow **windows;
	size_t windowCount;
	HANDLE heap;
	HCURSOR cursors[TU_CURSOR_COUNT];
} TUGlobalState;

void TUInitialise();
int TUMessageLoop();

TUElement *TUElementCreate(size_t bytes, TUElement *parent, uint32_t flags, TUMessageHandler messageClass);
void TUElementDestroy(TUElement *element);
void TUElementRelayout(TUElement *element);
void TUElementRepaint(TUElement *element, TURectangle *region);
void TUElementMove(TUElement *element, TURectangle bounds, bool alwaysLayout);
int TUElementMessage(TUElement *element, TUMessage message, int di, void *dp);
void TUElementMeasurementsChanged(TUElement *element, int which); // 1 (width) | 2 (height)
void TUElementSetDisabled(TUElement *element, bool disabled);
#define TUElementSetEnabled(x, y) TUElementSetDisabled((x), !(y))

TUWindow *TUWindowCreate(uint32_t flags, const char *cTitle, int width, int height, int showCommand);
void TUWindowRegisterShortcut(TUWindow *window, int32_t vkCode, bool ctrl, bool shift, bool alt, void (*invoke)(void *cp), void *cp);
void TUWindowFocusElement(TUWindow *window, TUElement *element);

TUPanel *TUPanelCreate(TUElement *parent, uint32_t flags);

TULabel *TULabelCreate(TUElement *parent, uint32_t flags, const char *label, ptrdiff_t labelBytes);
void TULabelSetContent(TULabel *label, const char *content, ptrdiff_t byteCount);

TUButton *TUButtonCreate(TUElement *parent, uint32_t flags, const char *label, ptrdiff_t labelBytes);
void TUButtonSetContent(TUButton *button, const char *content, ptrdiff_t byteCount);
void TUButtonSetCheck(TUButton *button, bool check); // For TU_BUTTON_CHECKBOX and TU_BUTTON_RADIOBOX.

TUTextbox *TUTextboxCreate(TUElement *parent, uint32_t flags);
void TUTextboxReplaceSelection(TUTextbox *textbox, const char *text, ptrdiff_t textBytes, bool canUndo); // Warning: TU_MSG_TEXTBOX_MODIFIED may be sent!
int TUTextboxConvertCharacterToLine(TUTextbox *textbox, int character);
int TUTextboxConvertLineToCharacter(TUTextbox *textbox, int line);
void TUTextboxScrollToLine(TUTextbox *textbox, int line);
int TUTextboxGetFirstLineVisible(TUTextbox *textbox);
int TUTextboxGetLineCount(TUTextbox *textbox);
int TUTextboxGetLineLength(TUTextbox *textbox, int line);
char *TUTextboxGetLineText(TUTextbox *textbox, int line, size_t *bytes); // Free with TU_FREE. line = 0 is the first line.
void TUTextboxSetSelection(TUTextbox *textbox, int start, int end);
void TUTextboxGetSelection(TUTextbox *textbox, int *start, int *end);

TUProgressBar *TUProgressBarCreate(TUElement *parent, uint32_t flags);
void TUProgressBarSetPosition(TUProgressBar *progressBar, float position);
void TUProgressBarSetIndeterminate(TUProgressBar *progressBar, bool enabled);
#define TU_PROGRESS_BAR_STATE_DEFAULT (0)
#define TU_PROGRESS_BAR_STATE_ERROR   (1)
#define TU_PROGRESS_BAR_STATE_PAUSED  (2)
void TUProgressBarSetState(TUProgressBar *progressBar, int state);

TURectangle TURectangleMake(int l, int r, int t, int b);
TURectangle TURectangleIntersection(TURectangle a, TURectangle b);
TURectangle TURectangleBounding(TURectangle a, TURectangle b);
bool TURectangleValid(TURectangle a);
bool TURectangleEquals(TURectangle a, TURectangle b);
bool TURectangleContains(TURectangle a, int x, int y);

void TUStringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);

wchar_t *TUConvertToUTF16(const char *inputUTF8, ptrdiff_t inputCount);
char *TUConvertToUTF8(const wchar_t *inputUTF16, ptrdiff_t inputCount);

void TUDrawBlock(void *context, TURectangle rectangle, uint32_t color);
void TUDrawInvert(void *context, TURectangle rectangle);
void TUDrawLine(void *context, int x0, int y0, int x1, int y1, uint32_t color);
void TUDrawRectangle(void *context, TURectangle r, uint32_t mainColor, uint32_t borderColor, TURectangle borderSize);
void TUDrawBorder(void *context, TURectangle r, uint32_t borderColor, TURectangle borderSize);
void TUDrawString(void *context, TURectangle r, const char *string, ptrdiff_t bytes, uint32_t color, int align);

/////////////////////////////////////////
// Helper functions.
/////////////////////////////////////////

TUGlobalState tuGlobal;

void _TUWindowSetPressed(TUWindow *window, TUElement *element, int button);
void *_TUHeapReAlloc(void *pointer, size_t size);

TURectangle TURectangleMake(int l, int r, int t, int b) {
	TURectangle x;
	x.l = l, x.r = r, x.t = t, x.b = b;
	return x;
}

TURectangle TURectangleIntersection(TURectangle a, TURectangle b) {
	if (a.l < b.l) a.l = b.l;
	if (a.t < b.t) a.t = b.t;
	if (a.r > b.r) a.r = b.r;
	if (a.b > b.b) a.b = b.b;
	return a;
}

TURectangle TURectangleBounding(TURectangle a, TURectangle b) {
	if (a.l > b.l) a.l = b.l;
	if (a.t > b.t) a.t = b.t;
	if (a.r < b.r) a.r = b.r;
	if (a.b < b.b) a.b = b.b;
	return a;
}

bool TURectangleValid(TURectangle a) {
	return a.r > a.l && a.b > a.t;
}

bool TURectangleEquals(TURectangle a, TURectangle b) {
	return a.l == b.l && a.r == b.r && a.t == b.t && a.b == b.b;
}

bool TURectangleContains(TURectangle a, int x, int y) {
	return a.l <= x && a.r > x && a.t <= y && a.b > y;
}

wchar_t *TUConvertToUTF16(const char *inputUTF8, ptrdiff_t inputCount) {
	if (!inputUTF8) return NULL;
	int count = MultiByteToWideChar(CP_UTF8, 0, inputUTF8, inputCount, NULL, 0);
	if (!count) return NULL;
	wchar_t *out = (wchar_t *) TU_MALLOC(count * 2 + 2);
	out[MultiByteToWideChar(CP_UTF8, 0, inputUTF8, inputCount, out, count)] = 0;
	return out;
}

char *TUConvertToUTF8(const wchar_t *inputUTF16, ptrdiff_t inputCount) {
	if (!inputUTF16) return NULL;
	int count = WideCharToMultiByte(CP_UTF8, 0, inputUTF16, inputCount, NULL, 0, NULL, NULL);
	if (!count) return NULL;
	char *out = (char *) TU_MALLOC(count + 1);
	out[WideCharToMultiByte(CP_UTF8, 0, inputUTF16, inputCount, out, count, NULL, NULL)] = 0;
	return out;
}

void TUStringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes) {
	if (sourceBytes == -1) sourceBytes = source ? strlen(source) : 0;
	*destination = (char *) TU_REALLOC(*destination, sourceBytes);
	*destinationBytes = sourceBytes;

	for (ptrdiff_t i = 0; i < sourceBytes; i++) {
		(*destination)[i] = source[i];
	}
}

/////////////////////////////////////////
// GDI drawing wrappers.
/////////////////////////////////////////

void TUDrawBlock(void *context, TURectangle rectangle, uint32_t color) {
	HDC dc = (HDC) context;
	HBRUSH brush = GetStockObject(DC_BRUSH);
	COLORREF oldColor = SetDCBrushColor(dc, color);
	RECT r = { rectangle.l, rectangle.t, rectangle.r, rectangle.b };
	FillRect(dc, &r, brush);
	SetDCBrushColor(dc, oldColor);
}

void TUDrawInvert(void *context, TURectangle rectangle) {
	HDC dc = (HDC) context;
	RECT r = { rectangle.l, rectangle.t, rectangle.r, rectangle.b };
	InvertRect(dc, &r);
}

void TUDrawLine(void *context, int x0, int y0, int x1, int y1, uint32_t color) {
	HDC dc = (HDC) context;
	POINT oldPoint;
	MoveToEx(dc, x0, y0, &oldPoint);
	HGDIOBJ oldPen = SelectObject(dc, GetStockObject(DC_PEN));
	COLORREF oldColor = SetDCPenColor(dc, color);
	LineTo(dc, x1, y1);
	MoveToEx(dc, oldPoint.x, oldPoint.y, NULL);
	SetDCBrushColor(dc, oldColor);
	SelectObject(dc, oldPen);
}

void TUDrawRectangle(void *context, TURectangle r, uint32_t mainColor, uint32_t borderColor, TURectangle borderSize) {
	TUDrawBorder(context, r, borderColor, borderSize);
	TUDrawBlock(context, TURectangleMake(r.l + borderSize.l, r.r - borderSize.r, r.t + borderSize.t, r.b - borderSize.b), mainColor);
}

void TUDrawBorder(void *context, TURectangle r, uint32_t borderColor, TURectangle borderSize) {
	TUDrawBlock(context, TURectangleMake(r.l, r.r, r.t, r.t + borderSize.t), borderColor);
	TUDrawBlock(context, TURectangleMake(r.l, r.l + borderSize.l, r.t + borderSize.t, r.b - borderSize.b), borderColor);
	TUDrawBlock(context, TURectangleMake(r.r - borderSize.r, r.r, r.t + borderSize.t, r.b - borderSize.b), borderColor);
	TUDrawBlock(context, TURectangleMake(r.l, r.r, r.b - borderSize.b, r.b), borderColor);
}

void TUDrawString(void *context, TURectangle rectangle, const char *string, ptrdiff_t bytes, uint32_t color, int align) {
	HDC dc = (HDC) context;
	RECT r = { rectangle.l, rectangle.t, rectangle.r, rectangle.b };
	UINT flags = DT_NOPREFIX | DT_VCENTER | DT_END_ELLIPSIS;
	if (align == TU_ALIGN_LEFT)   flags |= DT_LEFT;
	if (align == TU_ALIGN_CENTER) flags |= DT_CENTER;
	if (align == TU_ALIGN_RIGHT)  flags |= DT_RIGHT;
	int oldMode = SetBkMode(dc, TRANSPARENT);
	COLORREF oldColor = SetTextColor(dc, color);
	wchar_t *cwString = TUConvertToUTF16(string, bytes);
	DrawText(dc, cwString, -1, &r, flags);
	TU_FREE(cwString);
	SetTextColor(dc, oldColor);
	SetBkMode(dc, oldMode);
}

/////////////////////////////////////////
// Core user interface logic.
/////////////////////////////////////////

bool _TUElementDestroyNow(TUElement *element) {
	if (element->flags & TU_ELEMENT_DESTROY_DESCENDENT) {
		element->flags &= ~TU_ELEMENT_DESTROY_DESCENDENT;

		for (uintptr_t i = 0; i < element->childCount; i++) {
			if (_TUElementDestroyNow(element->children[i])) {
				memmove(&element->children[i], &element->children[i + 1], sizeof(TUElement *) * (element->childCount - i - 1));
				element->childCount--, i--;
			}
		}
	}

	if (element->flags & TU_ELEMENT_DESTROY) {
		TUElementMessage(element, TU_MSG_DESTROY, 0, 0);

		if (element->window->pressed == element) {
			_TUWindowSetPressed(element->window, NULL, 0);
		}

		if (element->window->hovered == element) {
			element->window->hovered = &element->window->e;
		}

		if (element->window->focused == element) {
			element->window->focused = NULL;
		}

		TU_FREE(element->children);
		TU_FREE(element);
		return true;
	} else {
		return false;
	}
}

void _TUUpdate() {
	for (uintptr_t i = 0; i < tuGlobal.windowCount; i++) {
		TUWindow *window = tuGlobal.windows[i];

		TUElementMessage(&window->e, TU_MSG_WINDOW_UPDATE_START, 0, 0);
		TUElementMessage(&window->e, TU_MSG_WINDOW_UPDATE_BEFORE_DESTROY, 0, 0);

		if (_TUElementDestroyNow(&window->e)) {
			tuGlobal.windows[i] = tuGlobal.windows[tuGlobal.windowCount - 1];
			tuGlobal.windowCount--, i--;
		} else {
			TUElementMessage(&window->e, TU_MSG_WINDOW_UPDATE_BEFORE_LAYOUT, 0, 0);
			TUElementMove(&window->e, window->e.bounds, false);
			TUElementMessage(&window->e, TU_MSG_WINDOW_UPDATE_END, 0, 0);
		}
	}
}

void _TUElementPaint(TUElement *element, void *context, TURectangle updateRect) {
	if (TURectangleValid(TURectangleIntersection(element->bounds, updateRect))) {
		TUElementMessage(element, TU_MSG_PAINT, 0, context);

		for (uintptr_t i = 0; i < element->childCount; i++) {
			_TUElementPaint(element->children[i], context, updateRect);
		}
	}
}

void TUElementSetDisabled(TUElement *element, bool disabled) {
	if (disabled) {
		element->flags |= TU_ELEMENT_DISABLED;
	} else {
		element->flags &= ~TU_ELEMENT_DISABLED;
	}

	if (element->hwnd) {
		EnableWindow(element->hwnd, !disabled);
	}
}

void TUElementMove(TUElement *element, TURectangle bounds, bool layout) {
	bool moved = !TURectangleEquals(element->bounds, bounds);

	if (moved) {
		layout = true;

		RECT oldRect = { element->bounds.l, element->bounds.t, element->bounds.r, element->bounds.b };
		RedrawWindow(element->window->e.hwnd, &oldRect, NULL, RDW_ERASE | RDW_INVALIDATE);

		if (element->hwnd && element->parent) {
			MoveWindow(element->hwnd, bounds.l, bounds.t, bounds.r - bounds.l, bounds.b - bounds.t, TRUE);
		}

		RECT newRect = { bounds.l, bounds.t, bounds.r, bounds.b };
		RedrawWindow(element->window->e.hwnd, &newRect, NULL, RDW_INVALIDATE);

		element->bounds = bounds;
	}

	if (element->flags & TU_ELEMENT_RELAYOUT) {
		layout = true;
		element->flags &= ~TU_ELEMENT_RELAYOUT;
	}

	if (layout) {
		TUElementMessage(element, TU_MSG_LAYOUT, 0, 0);
	} else if (element->flags & TU_ELEMENT_RELAYOUT_DESCENDENT) {
		for (uint32_t i = 0; i < element->childCount; i++) {
			TUElementMove(element->children[i], element->children[i]->bounds, false);
		}
	}

	element->flags &= ~TU_ELEMENT_RELAYOUT_DESCENDENT;
}

void TUElementDestroy(TUElement *element) {
	if (element->flags & TU_ELEMENT_DESTROY) {
		return;
	}

	element->flags |= TU_ELEMENT_DESTROY;
	TUElement *ancestor = element->parent;

	while (ancestor) {
		ancestor->flags |= TU_ELEMENT_DESTROY_DESCENDENT;
		ancestor = ancestor->parent;
	}

	for (uintptr_t i = 0; i < element->childCount; i++) {
		TUElementDestroy(element->children[i]);
	}

	if (element->parent) {
		TUElementRelayout(element->parent);
		TUElementMeasurementsChanged(element->parent, 3);
	}
}

void TUElementRepaint(TUElement *element, TURectangle *region) {
	if (!region) region = &element->bounds;
	TURectangle r2 = TURectangleIntersection(element->bounds, *region);
	RECT r = { r2.l, r2.t, r2.r, r2.b };
	RedrawWindow(element->window->e.hwnd, &r, NULL, RDW_ERASE | RDW_INVALIDATE);
}

void TUElementRelayout(TUElement *element) {
	if (element->flags & TU_ELEMENT_RELAYOUT) {
		return;
	}

	element->flags |= TU_ELEMENT_RELAYOUT;
	TUElement *ancestor = element->parent;

	while (ancestor) {
		ancestor->flags |= TU_ELEMENT_RELAYOUT_DESCENDENT;
		ancestor = ancestor->parent;
	}
}

void TUElementMeasurementsChanged(TUElement *element, int which) {
	if (!element->parent) {
		return; // This is the window element.
	}

	while (true) {
		which &= ~TUElementMessage(element->parent, TU_MSG_GET_CHILD_STABILITY, which, element);
		if (!which) break;
		element->flags |= TU_ELEMENT_RELAYOUT;
		element = element->parent;
	}

	TUElementRelayout(element);
}

int TUElementMessage(TUElement *element, TUMessage message, int di, void *dp) {
	if (message != TU_MSG_DESTROY && (element->flags & TU_ELEMENT_DESTROY)) {
		return 0;
	}

	if (message >= TU_MSG_INPUT_EVENTS_START && message <= TU_MSG_INPUT_EVENTS_END && (element->flags & TU_ELEMENT_DISABLED)) {
		return 0;
	}

	if (element->messageUser) {
		int result = element->messageUser(element, message, di, dp);

		if (result) {
			return result;
		}
	}

	if (element->messageClass) {
		return element->messageClass(element, message, di, dp);
	} else {
		return 0;
	}
}

TUElement *TUElementCreate(size_t bytes, TUElement *parent, uint32_t flags, TUMessageHandler messageClass) {
	TUElement *element = (TUElement *) TU_CALLOC(bytes);
	element->flags = flags;
	element->messageClass = messageClass;

	if (parent) {
		element->window = parent->window;
		element->parent = parent;
		parent->childCount++;
		parent->children = TU_REALLOC(parent->children, sizeof(TUElement *) * parent->childCount);
		parent->children[parent->childCount - 1] = element;
		TUElementRelayout(parent);
		TUElementMeasurementsChanged(parent, 3);
	}

	return element;
}

/////////////////////////////////////////
// Layout panels.
/////////////////////////////////////////

int _TUPanelCalculatePerFill(TUPanel *panel, bool horizontal, int hSpace, int vSpace, int *_count) {
	int available = horizontal ? hSpace : vSpace;
	if (available <= 0) return 0;
	int fill = 0, perFill = 0, count = 0;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		count++;

		if (horizontal && (panel->e.children[i]->flags & TU_ELEMENT_H_FILL)) {
			fill++;
		} else if (!horizontal && (panel->e.children[i]->flags & TU_ELEMENT_V_FILL)) {
			fill++;
		}
	}

	if (!count || !fill) return 0;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		if (panel->e.children[i]->flags & TU_ELEMENT_DESTROY) continue;

		if (horizontal && !(panel->e.children[i]->flags & TU_ELEMENT_H_FILL) && available > 0) {
			available -= TUElementMessage(panel->e.children[i], TU_MSG_GET_WIDTH, vSpace, 0);
		} else if (!horizontal && !(panel->e.children[i]->flags & TU_ELEMENT_V_FILL) && available > 0) {
			available -= TUElementMessage(panel->e.children[i], TU_MSG_GET_HEIGHT, hSpace, 0);
		}
	}

	available -= (count - 1) * panel->gap;
	if (available > 0 && fill) perFill = available / fill;
	if (_count) *_count = count;
	return perFill;
}

int _TUPanelMeasure(TUPanel *panel, int di) {
	bool horizontal = panel->e.flags & TU_PANEL_HORIZONTAL;
	int perFill = _TUPanelCalculatePerFill(panel, horizontal, horizontal ? di : 0, horizontal ? 0 : di, NULL);
	int size = 0;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		if (panel->e.children[i]->flags & TU_ELEMENT_DESTROY) continue;
		int childSize = TUElementMessage(panel->e.children[i], horizontal ? TU_MSG_GET_HEIGHT : TU_MSG_GET_WIDTH, 
				(panel->e.children[i]->flags & (horizontal ? TU_ELEMENT_H_FILL : TU_ELEMENT_V_FILL)) ? perFill : 0, 0);
		if (childSize > size) size = childSize;
	}

	int border = horizontal ? panel->border.t + panel->border.b : panel->border.l + panel->border.r;
	return size + border;
}

int _TUPanelLayout(TUPanel *panel, TURectangle bounds, bool measure) {
	bool horizontal = panel->e.flags & TU_PANEL_HORIZONTAL;
	int position = horizontal ? panel->border.l : panel->border.t;
	int hSpace = bounds.r - bounds.l - panel->border.r - panel->border.l;
	int vSpace = bounds.b - bounds.t - panel->border.b - panel->border.t;
	int count;
	int perFill = _TUPanelCalculatePerFill(panel, horizontal, hSpace, vSpace, &count);
	int border2 = horizontal ? panel->border.t : panel->border.l;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		TUElement *child = panel->e.children[i];
		if (child->flags & TU_ELEMENT_DESTROY) continue;

		if (horizontal) {
			int height = (child->flags & TU_ELEMENT_V_FILL) ? vSpace : TUElementMessage(child, TU_MSG_GET_HEIGHT, (child->flags & TU_ELEMENT_H_FILL) ? perFill : 0, 0);
			int width = (child->flags & TU_ELEMENT_H_FILL) ? perFill : TUElementMessage(child, TU_MSG_GET_WIDTH, height, 0);
			TURectangle r = TURectangleMake(position + bounds.l, position + width + bounds.l, 
					border2 + (vSpace - height) / 2 + bounds.t, border2 + (vSpace + height) / 2 + bounds.t);
			if (!measure) TUElementMove(child, r, false);
			position += width + panel->gap;
		} else {
			int width = (child->flags & TU_ELEMENT_H_FILL) ? hSpace : TUElementMessage(child, TU_MSG_GET_WIDTH, (child->flags & TU_ELEMENT_V_FILL) ? perFill : 0, 0);
			int height = (child->flags & TU_ELEMENT_V_FILL) ? perFill : TUElementMessage(child, TU_MSG_GET_HEIGHT, width, 0);
			TURectangle r = TURectangleMake(border2 + (hSpace - width) / 2 + bounds.l, border2 + (hSpace + width) / 2 + bounds.l, 
					position + bounds.t, position + height + bounds.t);
			if (!measure) TUElementMove(child, r, false);
			position += height + panel->gap;
		}
	}

	return position - (count ? panel->gap : 0) + (horizontal ? panel->border.r : panel->border.b);
}

int _TUPanelMessage(TUElement *element, TUMessage message, int di, void *dp) {
	TUPanel *panel = (TUPanel *) element;
	bool horizontal = element->flags & TU_PANEL_HORIZONTAL;
	(void) dp;

	if (message == TU_MSG_LAYOUT) {
		_TUPanelLayout(panel, element->bounds, false);
	} else if (message == TU_MSG_GET_WIDTH) {
		return horizontal ? _TUPanelLayout(panel, TURectangleMake(0, 0, 0, di), true) : _TUPanelMeasure(panel, di);
	} else if (message == TU_MSG_GET_HEIGHT) {
		return horizontal ? _TUPanelMeasure(panel, di) : _TUPanelLayout(panel, TURectangleMake(0, di, 0, 0), true);
	} else if (message == TU_MSG_GET_CHILD_STABILITY) {
		TUElement *child = (TUElement *) dp;
		return ((child->flags & TU_ELEMENT_H_FILL) ? 1 : 0) | ((child->flags & TU_ELEMENT_V_FILL) ? 2 : 0);
	}

	return 0;
}

TUPanel *TUPanelCreate(TUElement *parent, uint32_t flags) {
	return (TUPanel *) TUElementCreate(sizeof(TUPanel), parent, flags, _TUPanelMessage);
}

/////////////////////////////////////////
// Labels.
/////////////////////////////////////////

int _TULabelMessage(TUElement *element, TUMessage message, int di, void *dp) {
	TULabel *label = (TULabel *) element;
	(void) di;
	(void) dp;
	
	if (message == TU_MSG_GET_HEIGHT || message == TU_MSG_GET_WIDTH) {
		if (message == TU_MSG_GET_WIDTH && label->cachedWidth != -1) return label->cachedWidth;
		if (message == TU_MSG_GET_HEIGHT && label->cachedHeight != -1 && label->cachedHeightDi == di) return label->cachedHeight;
		if (message == TU_MSG_GET_HEIGHT) label->cachedHeightDi = di;
		HDC dc = GetDC(element->hwnd);
		HGDIOBJ oldFont = SelectObject(dc, element->window->font);
		RECT rect = { 0 };
		rect.right = message == TU_MSG_GET_HEIGHT ? di : 0;
		wchar_t *cwText = TUConvertToUTF16(label->text, label->textBytes);
		DWORD flags = DT_CALCRECT | DT_WORDBREAK;
		if (element->flags & TU_LABEL_ELLIPSIS) flags = DT_CALCRECT | DT_SINGLELINE | DT_END_ELLIPSIS;
		DrawText(dc, cwText, -1, &rect, flags);
		TU_FREE(cwText);
		SelectObject(dc, oldFont);
		ReleaseDC(element->hwnd, dc);
		if (message == TU_MSG_GET_WIDTH) return (label->cachedWidth = rect.right);
		else return (label->cachedHeight = rect.bottom);
	} else if (message == TU_MSG_UPDATE_FONT) {
		SendMessage(label->e.hwnd, WM_SETFONT, (WPARAM) element->window->font, (LPARAM) TRUE);
		label->cachedWidth = label->cachedHeight = label->cachedHeightDi = -1;
	} else if (message == TU_MSG_DESTROY) {
		DestroyWindow(label->e.hwnd);
		TU_FREE(label->text);
	}

	return 0;
}

void TULabelSetContent(TULabel *label, const char *text, ptrdiff_t textBytes) {
	TUStringCopy(&label->text, &label->textBytes, text, textBytes);
	wchar_t *cwText = TUConvertToUTF16(text, textBytes);
	SetWindowText(label->e.hwnd, cwText ? cwText : L"");
	TU_FREE(cwText);
	label->cachedWidth = label->cachedHeight = label->cachedHeightDi = -1;
	TUElementMeasurementsChanged(&label->e, 3);
}

TULabel *TULabelCreate(TUElement *parent, uint32_t flags, const char *text, ptrdiff_t textBytes) {
	DWORD style = WS_CHILD | WS_VISIBLE;

	if (flags & TU_LABEL_ALIGN_CENTER) {
		style |= SS_CENTER;
	} else if (flags & TU_LABEL_ALIGN_RIGHT) {
		style |= SS_RIGHT;
	} else {
		style |= SS_LEFT;
	}

	if (flags & TU_LABEL_ELLIPSIS) {
		style |= SS_ENDELLIPSIS;
	}

	if (flags & TU_ELEMENT_DISABLED) {
		style |= WS_DISABLED;
	}

	TULabel *label = (TULabel *) TUElementCreate(sizeof(TULabel), parent, flags, _TULabelMessage);
	label->e.hwnd = CreateWindow(L"Static", L"", style, 0, 0, 0, 0, parent->window->e.hwnd, NULL, NULL, NULL);
	TUElementMessage(&label->e, TU_MSG_UPDATE_FONT, 0, 0);
	TULabelSetContent(label, text, textBytes);
	return label;
}

/////////////////////////////////////////
// Buttons.
/////////////////////////////////////////

int _TUButtonMessage(TUElement *element, TUMessage message, int di, void *dp) {
	TUButton *button = (TUButton *) element;
	(void) di;
	(void) dp;
	
	if (message == TU_MSG_GET_HEIGHT) {
		if (element->flags & TU_BUTTON_CHECKBOX) {
			return 17 * element->window->scale;
		} else if (element->flags & TU_BUTTON_RADIOBOX) {
			return 17 * element->window->scale;
		} else {
			return 23 * element->window->scale;
		}
	} else if (message == TU_MSG_GET_WIDTH) {
		if (element->flags & TU_BUTTON_DIALOG_SIZE) {
			return 75 * element->window->scale;
		} else {
			if (button->cachedWidth != -1) return button->cachedWidth;
			HDC dc = GetDC(element->hwnd);
			HGDIOBJ oldFont = SelectObject(dc, element->window->font);
			RECT rect = { 0 };
			wchar_t *cwText = TUConvertToUTF16(button->text, button->textBytes);
			DrawText(dc, cwText, -1, &rect, DT_CALCRECT | DT_SINGLELINE);
			TU_FREE(cwText);
			SelectObject(dc, oldFont);
			ReleaseDC(element->hwnd, dc);
			int extra = (element->flags & TU_BUTTON_SPLIT) ? 34 : 20;
			rect.right += extra * element->window->scale;
			return (button->cachedWidth = rect.right);
		}
	} else if (message == TU_MSG_UPDATE_FONT) {
		SendMessage(button->e.hwnd, WM_SETFONT, (WPARAM) element->window->font, (LPARAM) TRUE);
		button->cachedWidth = -1;
	} else if (message == TU_MSG_DESTROY) {
		DestroyWindow(button->e.hwnd);
		TU_FREE(button->text);
	} else if (message == TU_MSG_CLICKED) {
		if (button->invoke) {
			button->invoke(element->cp);
		}

#ifdef TU_IMMEDIATE
		button->e.nextInteraction.clicked = true;
#endif
	} else if (message == TU_MSG_DROPDOWN) {
#ifdef TU_IMMEDIATE
		button->e.nextInteraction.dropdown = true;
#endif
	}

	return 0;
}

void TUButtonSetCheck(TUButton *button, bool check) {
	if (button->check == check) return;
	button->check = check;
	SendMessage(button->e.hwnd, BM_SETCHECK, check ? BST_CHECKED : BST_UNCHECKED, 0);
}

void TUButtonSetContent(TUButton *button, const char *text, ptrdiff_t textBytes) {
	TUStringCopy(&button->text, &button->textBytes, text, textBytes);
	wchar_t *cwText = TUConvertToUTF16(text, textBytes);
	SetWindowText(button->e.hwnd, cwText ? cwText : L"");
	TU_FREE(cwText);
	button->cachedWidth = -1;
	TUElementMeasurementsChanged(&button->e, 3);
}

TUButton *TUButtonCreate(TUElement *parent, uint32_t flags, const char *text, ptrdiff_t textBytes) {
	DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;

	if (flags & TU_BUTTON_CHECKBOX) {
		style |= BS_CHECKBOX;
	} else if (flags & TU_BUTTON_RADIOBOX) {
		style |= BS_RADIOBUTTON;
	} else if (flags & TU_BUTTON_DEFAULT) {
		style |= (flags & TU_BUTTON_SPLIT) ? 0x000D /* BS_DEFDROPDOWN */ : BS_DEFPUSHBUTTON;
	} else {
		style |= (flags & TU_BUTTON_SPLIT) ? 0x000C /* BS_DROPDOWN */ : BS_PUSHBUTTON;
	}

	if (flags & TU_ELEMENT_DISABLED) {
		style |= WS_DISABLED;
	}

	TUButton *button = (TUButton *) TUElementCreate(sizeof(TUButton), parent, flags, _TUButtonMessage);
	button->e.hwnd = CreateWindow(L"Button", L"", style, 0, 0, 0, 0, parent->window->e.hwnd, NULL, NULL, NULL);
	button->e.hwndType = TU_HWND_BUTTON;
	SetWindowLongPtr(button->e.hwnd, GWLP_USERDATA, (LONG_PTR) button);
	TUElementMessage(&button->e, TU_MSG_UPDATE_FONT, 0, 0);
	TUButtonSetContent(button, text, textBytes);
	return button;
}

/////////////////////////////////////////
// Progress bars.
/////////////////////////////////////////

int _TUProgressBarMessage(TUElement *element, TUMessage message, int di, void *dp) {
	TUProgressBar *progressBar = (TUProgressBar *) element;
	(void) di;
	(void) dp;
	
	if (message == TU_MSG_GET_HEIGHT) {
		return 15 * element->window->scale;
	} else if (message == TU_MSG_GET_WIDTH) {
		return 160 * element->window->scale;
	} else if (message == TU_MSG_DESTROY) {
		DestroyWindow(progressBar->e.hwnd);
	}

	return 0;
}

void TUProgressBarSetPosition(TUProgressBar *progressBar, float position) {
	progressBar->position = position;
	SendMessage(progressBar->e.hwnd, PBM_SETPOS, (int) (position * 100), 0);
}

void TUProgressBarSetIndeterminate(TUProgressBar *progressBar, bool enabled) {
	LONG_PTR old = GetWindowLongPtr(progressBar->e.hwnd, GWL_STYLE);
	SetWindowLongPtr(progressBar->e.hwnd, GWL_STYLE, enabled ? (old | PBS_MARQUEE) : (old & ~PBS_MARQUEE));
	SendMessage(progressBar->e.hwnd, PBM_SETMARQUEE, enabled, 0);
}

void TUProgressBarSetState(TUProgressBar *progressBar, int state) {
	SendMessage(progressBar->e.hwnd, PBM_SETPOS, 100, 0); // Force repaint!
	if (state == TU_PROGRESS_BAR_STATE_ERROR) SendMessage(progressBar->e.hwnd, PBM_SETSTATE, PBST_ERROR, 0);
	if (state == TU_PROGRESS_BAR_STATE_PAUSED) SendMessage(progressBar->e.hwnd, PBM_SETSTATE, PBST_PAUSED, 0);
	if (state == TU_PROGRESS_BAR_STATE_DEFAULT) SendMessage(progressBar->e.hwnd, PBM_SETSTATE, PBST_NORMAL, 0);
	SendMessage(progressBar->e.hwnd, PBM_SETPOS, (int) (progressBar->position * 100), 0);
}

TUProgressBar *TUProgressBarCreate(TUElement *parent, uint32_t flags) {
	TUProgressBar *progressBar = (TUProgressBar *) TUElementCreate(sizeof(TUProgressBar), parent, flags, _TUProgressBarMessage);
	progressBar->e.hwnd = CreateWindow(PROGRESS_CLASS, L"", 
			WS_CHILD | WS_VISIBLE | PBS_SMOOTHREVERSE | ((flags & TU_ELEMENT_DISABLED) ? WS_DISABLED : 0), 
			0, 0, 0, 0, parent->window->e.hwnd, NULL, NULL, NULL);
	SetWindowLongPtr(progressBar->e.hwnd, GWLP_USERDATA, (LONG_PTR) progressBar);
	SendMessage(progressBar->e.hwnd, PBM_SETRANGE, 0, 100 << 16);
	return progressBar;
}

/////////////////////////////////////////
// Textboxes.
/////////////////////////////////////////

int _TUTextboxMessage(TUElement *element, TUMessage message, int di, void *dp) {
	TUTextbox *textbox = (TUTextbox *) element;
	(void) di;
	(void) dp;
	
	if (message == TU_MSG_GET_HEIGHT) {
		return 23 * element->window->scale;
	} else if (message == TU_MSG_GET_WIDTH) {
		return 210 * element->window->scale;
	} else if (message == TU_MSG_UPDATE_FONT) {
		SendMessage(textbox->e.hwnd, WM_SETFONT, (WPARAM) element->window->font, (LPARAM) TRUE);
	} else if (message == TU_MSG_DESTROY) {
		SetWindowLongPtr(textbox->e.hwnd, GWLP_WNDPROC, (LONG_PTR) textbox->oldWindowProcedure);
		DestroyWindow(textbox->e.hwnd);
#ifdef TU_IMMEDIATE
	} else if (message == TU_MSG_TEXTBOX_MODIFIED) {
		textbox->e.nextInteraction.modified = true;
#endif
	}

	return 0;
}

void TUTextboxReplaceSelection(TUTextbox *textbox, const char *text, ptrdiff_t textBytes, bool canUndo) {
	wchar_t *cwText = TUConvertToUTF16(text, textBytes);
	SendMessage(textbox->e.hwnd, EM_REPLACESEL, canUndo, (LPARAM) cwText);
	TU_FREE(cwText);
}

int TUTextboxConvertCharacterToLine(TUTextbox *textbox, int character) {
	return SendMessage(textbox->e.hwnd, EM_LINEFROMCHAR, character, 0);
}

int TUTextboxConvertLineToCharacter(TUTextbox *textbox, int line) {
	return SendMessage(textbox->e.hwnd, EM_LINEINDEX, line, 0);
}

void TUTextboxScrollToLine(TUTextbox *textbox, int line) {
	int firstVisibleLine = SendMessage(textbox->e.hwnd, EM_GETFIRSTVISIBLELINE, 0, 0);
	SendMessage(textbox->e.hwnd, EM_LINESCROLL, 0, line - firstVisibleLine);
}

int TUTextboxGetFirstLineVisible(TUTextbox *textbox) {
	return SendMessage(textbox->e.hwnd, EM_GETFIRSTVISIBLELINE, 0, 0);
}

int TUTextboxGetLineCount(TUTextbox *textbox) {
	return SendMessage(textbox->e.hwnd, EM_GETLINECOUNT, 0, 0);
}

int TUTextboxGetLineLength(TUTextbox *textbox, int line) {
	return SendMessage(textbox->e.hwnd, EM_LINELENGTH, SendMessage(textbox->e.hwnd, EM_LINEINDEX, line, 0), 0);
}

char *TUTextboxGetLineText(TUTextbox *textbox, int line, size_t *bytes) {
	int length = TUTextboxGetLineLength(textbox, line) + 16;
	wchar_t *buffer = (wchar_t *) TU_MALLOC(length * 2);
	buffer[0] = length;
	length = SendMessage(textbox->e.hwnd, EM_GETLINE, line, (LPARAM) buffer);
	char *result = TUConvertToUTF8(buffer, length);
	TU_FREE(buffer);
	if (bytes) *bytes = strlen(result);
	return result;
}

void TUTextboxSetSelection(TUTextbox *textbox, int start, int end) {
	SendMessage(textbox->e.hwnd, EM_SETSEL, start, end);
}

void TUTextboxGetSelection(TUTextbox *textbox, int *_start, int *_end) {
	DWORD start, end;
	SendMessage(textbox->e.hwnd, EM_GETSEL, (WPARAM) &start, (LPARAM) &end);
	*_start = start;
	*_end = end;
}

LRESULT _TUTextboxWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	TUTextbox *textbox = (TUTextbox *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (message == WM_KEYDOWN) {
		if (TUElementMessage(&textbox->e, TU_MSG_TEXTBOX_KEY_DOWN, wParam, 0)) {
			_TUUpdate();
			return 0;
		}
	}

	return CallWindowProc(textbox->oldWindowProcedure, hwnd, message, wParam, lParam);
}

TUTextbox *TUTextboxCreate(TUElement *parent, uint32_t flags) {
	DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
	DWORD exStyle = 0;
	if (  flags & TU_TEXTBOX_READ_ONLY)  style |= ES_READONLY;
	if (  flags & TU_TEXTBOX_PASSWORD)   style |= ES_PASSWORD;
	if (  flags & TU_TEXTBOX_MULTILINE)  style |= ES_AUTOVSCROLL | ES_WANTRETURN | ES_MULTILINE;
	if (  flags & TU_TEXTBOX_NUMBER)     style |= ES_NUMBER;
	if (  flags & TU_ELEMENT_DISABLED)   style |= WS_DISABLED;
	if (!(flags & TU_TEXTBOX_WORD_WRAP)) style |= ES_AUTOHSCROLL;
	if (!(flags & TU_TEXTBOX_NO_BORDER)) exStyle |= WS_EX_CLIENTEDGE;

	TUTextbox *textbox = (TUTextbox *) TUElementCreate(sizeof(TUTextbox), parent, flags, _TUTextboxMessage);
	textbox->e.hwnd = CreateWindowEx(exStyle, L"Edit", L"", style, 0, 0, 0, 0, parent->window->e.hwnd, NULL, NULL, NULL);
	textbox->e.hwndType = TU_HWND_EDIT;
	SetWindowLongPtr(textbox->e.hwnd, GWLP_USERDATA, (LONG_PTR) textbox);
	TUElementMessage(&textbox->e, TU_MSG_UPDATE_FONT, 0, 0);
	textbox->oldWindowProcedure = (WNDPROC) SetWindowLongPtr(textbox->e.hwnd, GWLP_WNDPROC, (LONG_PTR) _TUTextboxWindowProcedure);
	SHAutoComplete(textbox->e.hwnd, SHACF_AUTOSUGGEST_FORCE_OFF); // Enable Ctrl+Backspace.
	return textbox;
}

/////////////////////////////////////////
// Custom input handling.
/////////////////////////////////////////

void _TUWindowSetPressed(TUWindow *window, TUElement *element, int button) {
	TUElement *previous = window->pressed;
	window->pressed = element;
	window->pressedButton = button;
	if (previous) TUElementMessage(previous, TU_MSG_UPDATE, TU_UPDATE_PRESSED, 0);
	if (element) TUElementMessage(element, TU_MSG_UPDATE, TU_UPDATE_PRESSED, 0);
}

void TUWindowFocusElement(TUWindow *window, TUElement *element) {
	TUElement *previous = window->focused;
	if (previous == element) return;
	window->focused = element;
	if (previous) TUElementMessage(previous, TU_MSG_UPDATE, TU_UPDATE_FOCUSED, 0);

	if (element) {
		TUElementMessage(element, TU_MSG_UPDATE, TU_UPDATE_FOCUSED, 0);
		SetFocus(element->hwnd ? element->hwnd : window->e.hwnd);
	}
}

TUElement *_TUElementFindByPoint(TUElement *element, int x, int y) {
	for (uintptr_t i = 0; i < element->childCount; i++) {
		if (TURectangleContains(element->children[i]->bounds, x, y)) {
			return _TUElementFindByPoint(element->children[i], x, y);
		}
	}

	return element;
}

bool _TUWindowInputEvent(TUWindow *window, TUMessage message, int di, void *dp) {
	bool handled = true;

	if (window->pressed) {
		if (message == TU_MSG_MOUSE_MOVE) {
			TUElementMessage(window->pressed, TU_MSG_MOUSE_DRAG, di, dp);
		} else if (message == TU_MSG_LEFT_UP && window->pressedButton == 1) {
			if (window->hovered == window->pressed) {
				TUElementMessage(window->pressed, TU_MSG_CLICKED, di, dp);
			}

			TUElementMessage(window->pressed, TU_MSG_LEFT_UP, di, dp);
			_TUWindowSetPressed(window, NULL, 1);
		} else if (message == TU_MSG_MIDDLE_UP && window->pressedButton == 2) {
			TUElementMessage(window->pressed, TU_MSG_MIDDLE_UP, di, dp);
			_TUWindowSetPressed(window, NULL, 2);
		} else if (message == TU_MSG_RIGHT_UP && window->pressedButton == 3) {
			TUElementMessage(window->pressed, TU_MSG_RIGHT_UP, di, dp);
			_TUWindowSetPressed(window, NULL, 3);
		}
	}

	if (window->pressed) {
		bool inside = TURectangleContains(window->pressed->bounds, window->cursorX, window->cursorY);

		if (inside && window->hovered == &window->e) {
			window->hovered = window->pressed;
			TUElementMessage(window->pressed, TU_MSG_UPDATE, TU_UPDATE_HOVERED, 0);
		} else if (!inside && window->hovered == window->pressed) {
			window->hovered = &window->e;
			TUElementMessage(window->pressed, TU_MSG_UPDATE, TU_UPDATE_HOVERED, 0);
		}
	}

	if (!window->pressed) {
		TUElement *hovered = _TUElementFindByPoint(&window->e, window->cursorX, window->cursorY);

		if (message == TU_MSG_MOUSE_MOVE) {
			TUElementMessage(hovered, TU_MSG_MOUSE_MOVE, di, dp);

			int cursor = TUElementMessage(window->hovered, TU_MSG_GET_CURSOR, di, dp);

			if (cursor != window->cursorStyle) {
				window->cursorStyle = cursor;
				SetCursor(tuGlobal.cursors[cursor]);
			}
		} else if (message == TU_MSG_LEFT_DOWN) {
			_TUWindowSetPressed(window, hovered, 1);
			TUElementMessage(hovered, TU_MSG_LEFT_DOWN, di, dp);
		} else if (message == TU_MSG_MIDDLE_DOWN) {
			_TUWindowSetPressed(window, hovered, 2);
			TUElementMessage(hovered, TU_MSG_MIDDLE_DOWN, di, dp);
		} else if (message == TU_MSG_RIGHT_DOWN) {
			_TUWindowSetPressed(window, hovered, 3);
			TUElementMessage(hovered, TU_MSG_RIGHT_DOWN, di, dp);
		} else if (message == TU_MSG_MOUSE_WHEEL) {
			TUElement *element = hovered;

			while (element) {
				if (TUElementMessage(element, TU_MSG_MOUSE_WHEEL, di, dp)) {
					break;
				}

				element = element->parent;
			}
		} else if (message == TU_MSG_KEY_TYPED) {
			handled = false;

			if (window->focused) {
				TUElement *element = window->focused;

				while (element) {
					if (TUElementMessage(element, TU_MSG_KEY_TYPED, di, dp)) {
						handled = true;
						break;
					}

					element = element->parent;
				}
			} else {
				if (TUElementMessage(&window->e, TU_MSG_KEY_TYPED, di, dp)) {
					handled = true;
				}
			}
		}

		if (hovered != window->hovered) {
			TUElement *previous = window->hovered;
			window->hovered = hovered;
			TUElementMessage(previous, TU_MSG_UPDATE, TU_UPDATE_HOVERED, 0);
			TUElementMessage(window->hovered, TU_MSG_UPDATE, TU_UPDATE_HOVERED, 0);
		}
	}

	_TUUpdate();
	return handled;
}

/////////////////////////////////////////
// Windows.
/////////////////////////////////////////

void _TUWindowUpdateFontRecursively(TUElement *element) {
	for (uint32_t i = 0; i < element->childCount; i++) {
		_TUWindowUpdateFontRecursively(element->children[i]);
	}

	TUElementMessage(element, TU_MSG_UPDATE_FONT, 0, 0);
	element->flags |= TU_ELEMENT_RELAYOUT;
}

void _TUWindowUpdateFont(TUWindow *window) {
        HMONITOR monitor = MonitorFromWindow(window->e.hwnd, MONITOR_DEFAULTTOPRIMARY);
        UINT x, y;
        GetDpiForMonitorType getDpiForMonitor = (GetDpiForMonitorType) (void *) GetProcAddress(LoadLibrary(L"shcore.dll"), "GetDpiForMonitor");
        if (getDpiForMonitor) getDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y);
	else x = y = 96;
	NONCLIENTMETRICSW metrics = {};
	metrics.cbSize = sizeof(NONCLIENTMETRICSW);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	metrics.lfMessageFont.lfHeight = -((9 * y) / 72);
	if (window->font) DeleteObject(window->font);
	window->font = CreateFontIndirectW(&metrics.lfMessageFont);
	window->scale = y / 96.0f;
	_TUWindowUpdateFontRecursively(&window->e);
	TUElementRelayout(&window->e);
	RedrawWindow(window->e.hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

LRESULT CALLBACK _TUWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	TUWindow *window = (TUWindow *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (!window) {
		return DefWindowProc(hwnd, message, wParam, lParam);
	} else if (message == WM_CLOSE) {
		if (!TUElementMessage(&window->e, TU_MSG_WINDOW_CLOSE, 0, 0)) {
			PostQuitMessage(0);
		} else {
			_TUUpdate();
		}
	} else if (message == WM_ACTIVATE) {
		TUElementMessage(&window->e, wParam ? TU_MSG_WINDOW_ACTIVATE : TU_MSG_WINDOW_DEACTIVATE, 0, 0);
	} else if (message == WM_KILLFOCUS) {
		TUWindowFocusElement(window, NULL);
	} else if (message == WM_SIZE) {
		RECT client;
		GetClientRect(hwnd, &client);
		window->e.bounds = TURectangleMake(0, client.right, 0, client.bottom);
		TUElementRelayout(&window->e);
		_TUUpdate();
	} else if (message == WM_COMMAND) {
		TUElement *element = (TUElement *) GetWindowLongPtr((HWND) lParam, GWLP_USERDATA);

		if (!element) {
		} else if (HIWORD(wParam) == BN_CLICKED && element->hwndType == TU_HWND_BUTTON) {
			TUElementMessage(element, TU_MSG_CLICKED, 0, 0);
			_TUUpdate();
		} else if (HIWORD(wParam) == EN_SETFOCUS && element->hwndType == TU_HWND_EDIT) {
			TUElementMessage(element, TU_MSG_TEXTBOX_START_FOCUS, 0, 0);
			_TUUpdate();
		} else if (HIWORD(wParam) == EN_KILLFOCUS && element->hwndType == TU_HWND_EDIT) {
			TUElementMessage(element, TU_MSG_TEXTBOX_END_FOCUS, 0, 0);
			_TUUpdate();
		} else if (HIWORD(wParam) == EN_UPDATE && element->hwndType == TU_HWND_EDIT) {
			TUElementMessage(element, TU_MSG_TEXTBOX_MODIFIED, 0, 0);
			_TUUpdate();
		}
	} else if (message == WM_NOTIFY) {
		NMHDR *header = (NMHDR *) lParam;
		TUElement *element = (TUElement *) GetWindowLongPtr(header->hwndFrom, GWLP_USERDATA);

		if (!element) {
		} else if (header->code == (0U-1250U)+0x0002 /* BCN_DROPDOWN */ && element->hwndType == TU_HWND_BUTTON) {
			TUElementMessage(element, TU_MSG_DROPDOWN, 0, 0);
			_TUUpdate();
		}
	} else if (message == WM_DPICHANGED) {
		RECT *newBounds = (RECT *) lParam;
		_TUWindowUpdateFont(window);
		MoveWindow(hwnd, newBounds->left, newBounds->top, newBounds->right - newBounds->left, newBounds->bottom - newBounds->top, TRUE);
		_TUUpdate();
	} else if (message == WM_PAINT) {
		RECT updateRect;

		if (GetUpdateRect(hwnd, &updateRect, FALSE)) {
			PAINTSTRUCT paint;
			HDC dc = BeginPaint(hwnd, &paint);
			_TUElementPaint(&window->e, dc, TURectangleMake(updateRect.left, updateRect.right, updateRect.top, updateRect.bottom));
			EndPaint(hwnd, &paint);
		}
	} else if (message == WM_MOUSEMOVE) {
		if (!window->trackingLeave) {
			window->trackingLeave = true;
			TRACKMOUSEEVENT leave = { 0 };
			leave.cbSize = sizeof(TRACKMOUSEEVENT);
			leave.dwFlags = TME_LEAVE;
			leave.hwndTrack = hwnd;
			TrackMouseEvent(&leave);
		}

		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(hwnd, &cursor);
		window->cursorX = cursor.x;
		window->cursorY = cursor.y;
		_TUWindowInputEvent(window, TU_MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_MOUSELEAVE) {
		window->trackingLeave = false;

		if (!window->pressed) {
			window->cursorX = -1;
			window->cursorY = -1;
		}

		_TUWindowInputEvent(window, TU_MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_LBUTTONDOWN) {
		SetCapture(hwnd);
		_TUWindowInputEvent(window, TU_MSG_LEFT_DOWN, 0, 0);
	} else if (message == WM_LBUTTONUP) {
		if (window->pressedButton == 1) ReleaseCapture();
		_TUWindowInputEvent(window, TU_MSG_LEFT_UP, 0, 0);
	} else if (message == WM_MBUTTONDOWN) {
		SetCapture(hwnd);
		_TUWindowInputEvent(window, TU_MSG_MIDDLE_DOWN, 0, 0);
	} else if (message == WM_MBUTTONUP) {
		if (window->pressedButton == 2) ReleaseCapture();
		_TUWindowInputEvent(window, TU_MSG_MIDDLE_UP, 0, 0);
	} else if (message == WM_RBUTTONDOWN) {
		SetCapture(hwnd);
		_TUWindowInputEvent(window, TU_MSG_RIGHT_DOWN, 0, 0);
	} else if (message == WM_RBUTTONUP) {
		if (window->pressedButton == 3) ReleaseCapture();
		_TUWindowInputEvent(window, TU_MSG_RIGHT_UP, 0, 0);
	} else if (message == WM_MOUSEWHEEL) {
		int delta = (int) wParam >> 16;
		_TUWindowInputEvent(window, TU_MSG_MOUSE_WHEEL, -delta, 0);
	} else if (message == WM_KEYDOWN) {
		window->ctrl = GetKeyState(VK_CONTROL) & 0x8000;
		window->shift = GetKeyState(VK_SHIFT) & 0x8000;
		window->alt = GetKeyState(VK_MENU) & 0x8000;

		TUKeyTyped m = { 0 };
		m.code = wParam;
		_TUWindowInputEvent(window, TU_MSG_KEY_TYPED, 0, &m);
	} else if (message == WM_CHAR) {
		TUKeyTyped m = { 0 };
		char c = wParam;
		m.text = &c;
		m.textBytes = 1;
		_TUWindowInputEvent(window, TU_MSG_KEY_TYPED, 0, &m);
	} else if (message == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT) {
		POINT point = { window->cursorX, window->cursorY };

		if (hwnd == ChildWindowFromPoint(hwnd, point)) {
			SetCursor(tuGlobal.cursors[window->cursorStyle]);
			return 1;
		}
	} else if (message == WM_DROPFILES) {
		HDROP drop = (HDROP) wParam;
		int count = DragQueryFile(drop, 0xFFFFFFFF, NULL, 0);
		char **files = (char **) TU_MALLOC(sizeof(char *) * count);
		
		for (int i = 0; i < count; i++) {
			int length = DragQueryFile(drop, i, NULL, 0);
			wchar_t *wBuffer = (wchar_t *) TU_MALLOC(length * 2 + 2);
			DragQueryFile(drop, i, wBuffer, length + 1);
			files[i] = TUConvertToUTF8(wBuffer, length);
			TU_FREE(wBuffer);
		}
		
		TUElementMessage(&window->e, TU_MSG_WINDOW_DROP_FILES, count, files);
		for (int i = 0; i < count; i++) TU_FREE(files[i]);		
		TU_FREE(files);
		DragFinish(drop);
		_TUUpdate();
	} else {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

int _TUWindowMessage(TUElement *element, TUMessage message, int di, void *dp) {
	(void) di;
	(void) dp;

	TUWindow *window = (TUWindow *) element;

	if (message == TU_MSG_DESTROY) {
		SetWindowLongPtr(window->e.hwnd, GWLP_USERDATA, 0);
		DestroyWindow(window->e.hwnd);
	} else if (message == TU_MSG_LAYOUT && element->childCount) {
		TUElementMove(element->children[0], element->bounds, false);
	} else if (message == TU_MSG_GET_CHILD_STABILITY) {
		return 3; // Both width and height of the child element are ignored.
	}

	return 0;
}

void TUWindowRegisterShortcut(TUWindow *window, int32_t vkCode, bool ctrl, bool shift, bool alt, void (*invoke)(void *cp), void *cp) {
	TUShortcut shortcut = { 0 };
	shortcut.vkCode = vkCode;
	shortcut.ctrl = ctrl;
	shortcut.shift = shift;
	shortcut.alt = alt;
	shortcut.invoke = invoke;
	shortcut.cp = cp;
	window->shortcuts = (TUShortcut *) TU_REALLOC(window->shortcuts, (window->shortcutCount + 1) * sizeof(TUShortcut));
	window->shortcuts[window->shortcutCount++] = shortcut;
}

TUWindow *TUWindowCreate(uint32_t flags, const char *cTitle, int width, int height, int showCommand) {
	TUWindow *window = (TUWindow *) TUElementCreate(sizeof(TUWindow), NULL, flags, _TUWindowMessage);
	window->e.window = window;
	window->hovered = &window->e;
	tuGlobal.windowCount++;
	tuGlobal.windows = TU_REALLOC(tuGlobal.windows, sizeof(TUWindow *) * tuGlobal.windowCount);
	tuGlobal.windows[tuGlobal.windowCount - 1] = window;

	DWORD style = WS_CLIPSIBLINGS, exStyle = WS_EX_ACCEPTFILES;
	bool resizable = !(flags & TU_WINDOW_NOT_RESIZABLE),
	     toolbox = flags & TU_WINDOW_TOOLBOX,
	     closeButton = !(flags & TU_WINDOW_NO_CLOSE_BUTTON),
	     noBorder = flags & TU_WINDOW_NO_BORDER,
	     noTitlebar = flags & TU_WINDOW_NO_TITLEBAR;
	if (toolbox) exStyle |= WS_EX_TOOLWINDOW;
	if (resizable && !noBorder) style |= WS_THICKFRAME;
	if (resizable && !toolbox && !noBorder) style |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	if (closeButton || toolbox) style |= WS_SYSMENU;
	if (noTitlebar || noBorder) style |= WS_POPUP;
	if (noTitlebar && !noBorder && !resizable) style |= WS_DLGFRAME;

	wchar_t *cwTitle = TUConvertToUTF16(cTitle, -1);
	window->e.hwnd = CreateWindowEx(exStyle, L"TU32", cwTitle, style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	TU_FREE(cwTitle);

	SetWindowLongPtr(window->e.hwnd, GWLP_USERDATA, (LONG_PTR) window);
	_TUWindowUpdateFont(window);
	ShowWindow(window->e.hwnd, showCommand);
	PostMessage(window->e.hwnd, WM_SIZE, 0, 0);
	return window;
}

/////////////////////////////////////////
// Initialisation and message loop.
/////////////////////////////////////////

int TUMessageLoop() {
	MSG message = { 0 };

	while (GetMessage(&message, NULL, 0, 0)) {
		if (message.message == WM_KEYDOWN) {
			bool controlDown = GetKeyState(VK_CONTROL) >> 15;
			bool shiftDown = GetKeyState(VK_SHIFT) >> 15;
			bool altDown = GetKeyState(VK_MENU) >> 15;

			TUWindow *window = NULL;
			TUShortcut *shortcut = NULL;

			HWND target = GetAncestor(message.hwnd, GA_ROOT);
			
			for (uintptr_t i = 0; i < tuGlobal.windowCount; i++) {
				if (tuGlobal.windows[i]->e.hwnd == target) {
					window = tuGlobal.windows[i];
					break;
				}
			}
			
			if (window) {
				for (intptr_t i = window->shortcutCount - 1; i >= 0; i--) {
					TUShortcut *s = &window->shortcuts[i];

					if (s->vkCode == (int32_t) message.wParam && s->ctrl == controlDown && s->shift == shiftDown && s->alt == altDown) {
						shortcut = s;
					}
				}
			}

			if (shortcut) {
				shortcut->invoke(shortcut->cp);
				_TUUpdate();
				goto nextMessage;
			}
		}

		for (uintptr_t i = 0; i < tuGlobal.windowCount; i++) {
			if (IsDialogMessage(tuGlobal.windows[i]->e.hwnd, &message)) {
				goto nextMessage;
			}
		}
		
		TranslateMessage(&message);
		DispatchMessage(&message);

		nextMessage:;
	}

	return message.wParam;
}

void *_TUHeapReAlloc(void *pointer, size_t size) {
	if (pointer) {
		if (size) {
			return HeapReAlloc(tuGlobal.heap, 0, pointer, size);
		} else {
			TU_FREE(pointer);
			return NULL;
		}
	} else {
		if (size) {
			return TU_MALLOC(size);
		} else {
			return NULL;
		}
	}
}

void TUInitialise() {
	tuGlobal.heap = GetProcessHeap();

	SetProcessDpiAwarenessContextType setProcessDpiAwarenessContext = (SetProcessDpiAwarenessContextType) (void *) 
		GetProcAddress(LoadLibrary(L"user32.dll"), "SetProcessDpiAwarenessContext");
	if (setProcessDpiAwarenessContext) setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	tuGlobal.cursors[TU_CURSOR_ARROW] = LoadCursor(NULL, IDC_ARROW);
	tuGlobal.cursors[TU_CURSOR_TEXT] = LoadCursor(NULL, IDC_IBEAM);
	tuGlobal.cursors[TU_CURSOR_SPLIT_V] = LoadCursor(NULL, IDC_SIZENS);
	tuGlobal.cursors[TU_CURSOR_SPLIT_H] = LoadCursor(NULL, IDC_SIZEWE);
	tuGlobal.cursors[TU_CURSOR_FLIPPED_ARROW] = LoadCursor(NULL, IDC_ARROW);
	tuGlobal.cursors[TU_CURSOR_CROSS_HAIR] = LoadCursor(NULL, IDC_CROSS);
	tuGlobal.cursors[TU_CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
	tuGlobal.cursors[TU_CURSOR_RESIZE_UP] = LoadCursor(NULL, IDC_SIZENS);
	tuGlobal.cursors[TU_CURSOR_RESIZE_LEFT] = LoadCursor(NULL, IDC_SIZEWE);
	tuGlobal.cursors[TU_CURSOR_RESIZE_UP_RIGHT] = LoadCursor(NULL, IDC_SIZENESW);
	tuGlobal.cursors[TU_CURSOR_RESIZE_UP_LEFT] = LoadCursor(NULL, IDC_SIZENWSE);
	tuGlobal.cursors[TU_CURSOR_RESIZE_DOWN] = LoadCursor(NULL, IDC_SIZENS);
	tuGlobal.cursors[TU_CURSOR_RESIZE_RIGHT] = LoadCursor(NULL, IDC_SIZEWE);
	tuGlobal.cursors[TU_CURSOR_RESIZE_DOWN_LEFT] = LoadCursor(NULL, IDC_SIZENESW);
	tuGlobal.cursors[TU_CURSOR_RESIZE_DOWN_RIGHT] = LoadCursor(NULL, IDC_SIZENWSE);

	CoInitialize(NULL);
			
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASS windowClass = { 0 };
	windowClass.lpfnWndProc = _TUWindowProcedure;
	windowClass.lpszClassName = L"TU32";
	windowClass.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	RegisterClass(&windowClass);
}

/////////////////////////////////////////
// Immediate mode layer.
/////////////////////////////////////////

#ifdef TU_IMMEDIATE

void TURefreshStart(TUElement *element) {
	for (uint32_t i = 0; i < element->childCount; i++) {
		assert(!element->children[i]->inRefresh);
	}

	assert(!element->inRefresh);
	element->inRefresh = true;
	element->oldChildren = element->children;
	element->oldChildCount = element->childCount;
	element->children = NULL;
	element->childCount = 0;
}

void TURefreshEnd(TUElement *element) {
	assert(element->inRefresh);
	element->inRefresh = false;

	for (uint32_t i = 0; i < element->childCount; i++) {
		assert(!element->children[i]->inRefresh);
	}

	element->childCount += element->oldChildCount;
	element->children = TU_REALLOC(element->children, sizeof(TUElement *) * element->childCount);

	for (uint32_t i = 0; i < element->oldChildCount; i++) {
		assert(!element->oldChildren[i]->inRefresh);
		element->children[element->childCount - element->oldChildCount + i] = element->oldChildren[i];
		TUElementDestroy(element->oldChildren[i]);
	}

	TU_FREE(element->oldChildren);
}

TUElement *_TUInteractionClose(TUElement *element, ptrdiff_t discriminator) {
	element->discriminator = discriminator;
	element->interaction = element->nextInteraction;
	element->interaction.element = element;
	memset(&element->nextInteraction, 0, sizeof(TUInteraction));
	return element;
}

TUElement *_TURefreshSearch(TUElement *parent, TUMessageHandler messageClass, ptrdiff_t discriminator, 
		uint32_t newFlags, uint32_t synchronizableFlagsMask) {
	assert(parent->inRefresh);

	TUElement *element = NULL;
	uint32_t oldIndex = 0;

	synchronizableFlagsMask |= TU_ELEMENT_H_FILL | TU_ELEMENT_V_FILL | TU_ELEMENT_DISABLED;

	for (uint32_t i = 0; i < parent->oldChildCount; i++) {
		TUElement *candidate = parent->oldChildren[i];

		if (candidate->messageClass == messageClass
				&& candidate->discriminator == discriminator 
				&& (candidate->flags & ~synchronizableFlagsMask) == (newFlags & ~synchronizableFlagsMask)) {
			element = candidate;
			oldIndex = i;
			break;
		}
	}

	if (element) {
		TUElement *parent = element->parent;

		if ((element->flags & (TU_ELEMENT_H_FILL | TU_ELEMENT_V_FILL)) != (newFlags & (TU_ELEMENT_H_FILL | TU_ELEMENT_V_FILL))) {
			TUElementMeasurementsChanged(parent, 3);
		}

		if ((element->flags & TU_ELEMENT_DISABLED) != (newFlags & TU_ELEMENT_DISABLED)) {
			TUElementSetDisabled(element, newFlags & TU_ELEMENT_DISABLED);
		}

		parent->childCount++;
		parent->children = TU_REALLOC(parent->children, sizeof(TUElement *) * parent->childCount);
		parent->children[parent->childCount - 1] = parent->oldChildren[oldIndex];

		memmove(&parent->oldChildren[oldIndex], &parent->oldChildren[oldIndex + 1], (parent->oldChildCount - oldIndex - 1) * sizeof(TUElement *));
		parent->oldChildCount--;

		parent->children[parent->childCount - 1]->nextInteraction.old = true;
	}

	return element;
}

TULabel *TULabelNext(TUElement *parent, uint32_t flags, ptrdiff_t discriminator, const char *text, ptrdiff_t textBytes) {
	if (textBytes == -1) textBytes = text ? strlen(text) : 0;
	TULabel *label = (TULabel *) _TURefreshSearch(parent, _TULabelMessage, discriminator, flags, 0);

	if (!label) {
		label = TULabelCreate(parent, flags, text, textBytes);
	} else {
		if (label->textBytes != (size_t) textBytes || memcmp(label->text, text, textBytes)) {
			TULabelSetContent(label, text, textBytes);
		}
	}

	return (TULabel *) _TUInteractionClose(&label->e, discriminator);
}

TUButton *TUButtonNext(TUElement *parent, uint32_t flags, ptrdiff_t discriminator, const char *text, ptrdiff_t textBytes) {
	if (textBytes == -1) textBytes = text ? strlen(text) : 0;
	TUButton *button = (TUButton *) _TURefreshSearch(parent, _TUButtonMessage, discriminator, flags, 0);

	if (!button) {
		button = TUButtonCreate(parent, flags, text, textBytes);
	} else {
		if (button->textBytes != (size_t) textBytes || memcmp(button->text, text, textBytes)) {
			TUButtonSetContent(button, text, textBytes);
		}
	}

	return (TUButton *) _TUInteractionClose(&button->e, discriminator);
}

TUPanel *TUPanelNext(TUElement *parent, uint32_t flags, ptrdiff_t discriminator) {
	TUPanel *panel = (TUPanel *) _TURefreshSearch(parent, _TUPanelMessage, discriminator, flags, TU_PANEL_HORIZONTAL);

	if (!panel) {
		panel = TUPanelCreate(parent, flags);
	} else {
		if ((panel->e.flags & TU_PANEL_HORIZONTAL) != (flags & TU_PANEL_HORIZONTAL)) {
			panel->e.flags = (panel->e.flags & ~TU_PANEL_HORIZONTAL) | (flags & TU_PANEL_HORIZONTAL);
			TUElementRelayout(&panel->e);
			TUElementMeasurementsChanged(&panel->e, 3);
		}
	}

	return (TUPanel *) _TUInteractionClose(&panel->e, discriminator);
}

TUPanel *TUPanelNextWithSpacing(TUElement *parent, uint32_t flags, ptrdiff_t discriminator, TURectangle border, int gap) {
	TUPanel *panel = TUPanelNext(parent, flags, discriminator);

	if (!TURectangleEquals(panel->border, border) || panel->gap != gap) {
		panel->border = border;
		panel->gap = gap;
		TUElementRelayout(&panel->e);
		TUElementMeasurementsChanged(&panel->e, 3);
	}

	return panel;
}

TUTextbox *TUTextboxNext(TUElement *parent, uint32_t flags, ptrdiff_t discriminator) {
	TUTextbox *textbox = (TUTextbox *) _TURefreshSearch(parent, _TUTextboxMessage, discriminator, flags, TU_TEXTBOX_READ_ONLY);

	if (!textbox) {
		textbox = TUTextboxCreate(parent, flags);
	} else {
		if ((textbox->e.flags & TU_TEXTBOX_READ_ONLY) != (flags & TU_TEXTBOX_READ_ONLY)) {
			SendMessage(textbox->e.hwnd, EM_SETREADONLY, (flags & TU_TEXTBOX_READ_ONLY) ? TRUE : FALSE, 0);
		}
	}

	return (TUTextbox *) _TUInteractionClose(&textbox->e, discriminator);
}

TUProgressBar *TUProgressBarNext(TUElement *parent, uint32_t flags, ptrdiff_t discriminator) {
	TUProgressBar *progressBar = (TUProgressBar *) _TURefreshSearch(parent, _TUProgressBarMessage, discriminator, flags, 0);

	if (!progressBar) {
		progressBar = TUProgressBarCreate(parent, flags);
	}

	return (TUProgressBar *) _TUInteractionClose(&progressBar->e, discriminator);
}

TUElement *TUElementNext(size_t bytes, TUElement *parent, uint32_t flags, ptrdiff_t discriminator, TUMessageHandler messageClass) {
	TUElement *element = _TURefreshSearch(parent, messageClass, discriminator, flags, 0);
	return _TUInteractionClose(element ? element : TUElementCreate(bytes, parent, flags, messageClass), discriminator);
}

#define TU_LABEL_IMMEDIATE(parent, flags, discriminator, text, textBytes) \
	TULabelNext(parent, flags, discriminator, text, textBytes)

#define TU_BUTTON_IMMEDIATE(parent, flags, discriminator, text, textBytes) \
	TUButtonNext(parent, flags, discriminator, text, textBytes)->e.interaction.clicked

#define TU_CHECKBOX_IMMEDIATE(parent, flags, discriminator, text, textBytes, _value) do { \
	bool *value = _value; \
	TUButton *button = TUButtonNext(parent, (flags) | TU_BUTTON_CHECKBOX, discriminator, text, textBytes); \
	if (button->e.interaction.clicked) *value = !(*value); \
	TUButtonSetCheck(button, *value); \
} while (0)

#endif
