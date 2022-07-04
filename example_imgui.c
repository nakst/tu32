#define TU_IMMEDIATE
#include <stdio.h>
#include "tu32.c"

const char *labelText = "Hello, world! This a IMGUI-style wrapper for the Win32 GUI.";
uint32_t panelFlags = 0;
bool toggle;

void Refresh(TUElement *root) {
	bool repeat = true;

	while (repeat) {
		repeat = false;

		TURefreshStart(root);
		{
			TUPanel *panel = TUPanelNextWithSpacing(root, panelFlags, -1, TURectangleMake(20, 20, 20, 20), 10);
			TURefreshStart(&panel->e);
			{
				TU_LABEL_IMMEDIATE(&panel->e, TU_ELEMENT_H_FILL, -1, labelText, -1);

				if (TU_BUTTON_IMMEDIATE(&panel->e, 0, -1, "Push button", -1)) {
					labelText = "You clicked the button!";
					repeat = true;
					panelFlags |= TU_PANEL_HORIZONTAL;
				}

				TU_CHECKBOX_IMMEDIATE(&panel->e, 0, -1, "Toggle", -1, &toggle);
			}
			TURefreshEnd(&panel->e);
		}
		TURefreshEnd(root);
	}
}

int MyWindowMessage(TUElement *element, TUMessage message, int di, void *dp) {
	(void) di;
	(void) dp;

	if (message == TU_MSG_WINDOW_UPDATE_START) {
		Refresh(element);
	}

	return 0;
}

int WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
	(void) instance;
	(void) previousInstance;
	(void) commandLine;

	TUInitialise();
	TUWindow *window = TUWindowCreate(0, "Example", 640, 480, showCommand);
	window->e.messageUser = MyWindowMessage;
	return TUMessageLoop();
}
