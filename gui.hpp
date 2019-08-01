#pragma once

#include "oui.h"

#define APP_WIDGET_HEIGHT 20
enum WidgetType {
	WT_WINDOW,
	WT_BUTTON,
	WT_CHECKBOX,
	WT_LABEL,
	WT_HEADING,
	WT_HBOX,
	WT_VBOX,
	WT_SEPARATOR
};

// a header for each widget
struct Data {
	int type;
	UIhandler handler;
};

struct CheckBoxData {
	Data head;
	const char *label;
	bool *checked;
};

struct ButtonData {
	Data head;
	const char *label;
};

struct WindowData {
	Data head;
};

void gui_layout(int w, int h);
