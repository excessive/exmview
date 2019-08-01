#include "gui.hpp"
#include "widgets.hpp"
#include <stddef.h>

// creates a checkbox control for a pointer to a boolean
int checkbox(const char *label, bool *checked) {
	// create new ui item
	int item = uiItem();

	// set minimum size of wiget; horizontal size is dynamic, vertical is fixed
	uiSetSize(item, 0, APP_WIDGET_HEIGHT);

	// store some custom data with the checkbox that we use for rendering
	// and value changes.
	CheckBoxData *data = (CheckBoxData *)uiAllocHandle(item, sizeof(CheckBoxData));

	// assign a custom typeid to the data so the renderer knows how to
	// render this control, and our event handler
	data->head.type = WT_CHECKBOX;
	// called when the item is clicked
	data->head.handler = [](int item, UIevent event) {
		CheckBoxData *data = (CheckBoxData *)uiGetHandle(item);
		if (event == UI_BUTTON0_HOT_UP) {
			*data->checked = !(*data->checked);
		}
	};
	data->label = label;
	data->checked = checked;

	// set to fire as soon as the left button is pressed and released
	uiSetEvents(item, UI_BUTTON0_HOT_UP);
	//uiSetEvents(item, UI_BUTTON0_DOWN);

	return item;
}

int button(const char *label, UIhandler handler) {
	int item = uiItem();
	uiSetSize(item, 0, APP_WIDGET_HEIGHT);
	ButtonData *data = (ButtonData *)uiAllocHandle(item, sizeof(ButtonData));
	data->head.type = WT_BUTTON;
	data->head.handler = handler;
	data->label = label;

	uiSetEvents(item, UI_BUTTON0_HOT_UP);
	//uiSetEvents(item, UI_BUTTON0_DOWN);

	return item;
}

int label(const char *label, bool heading) {
	int item = uiItem();
	uiSetSize(item, 0, APP_WIDGET_HEIGHT);
	ButtonData *data = (ButtonData *)uiAllocHandle(item, sizeof(ButtonData));
	data->head.type = heading ? WT_HEADING : WT_LABEL;
	data->head.handler = NULL;
	data->label = label;
	return item;
}

int hbox() {
	int item = uiItem();
	Data *data = (Data *)uiAllocHandle(item, sizeof(Data));
	data->type = WT_HBOX;
	data->handler = NULL;
	uiSetBox(item, UI_ROW);
	return item;
}

int vbox() {
	int item = uiItem();
	Data *data = (Data *)uiAllocHandle(item, sizeof(Data));
	data->type = WT_VBOX;
	data->handler = NULL;
	uiSetBox(item, UI_COLUMN);
	return item;
}

int column_append(int parent, int item) {
	uiInsert(parent, item);
	// fill parent horizontally, anchor to previous item vertically
	uiSetLayout(item, UI_HFILL);
	uiSetMargins(item, 0, 1, 0, 0);
	return item;
}

int column() {
	int item = uiItem();
	uiSetBox(item, UI_COLUMN);
	return item;
}

int vgroup_append(int parent, int item) {
	uiInsert(parent, item);
	// fill parent horizontally, anchor to previous item vertically
	uiSetLayout(item, UI_HFILL);
	return item;
}

int vgroup() {
	int item = uiItem();
	uiSetBox(item, UI_COLUMN);
	return item;
}

int hgroup_append(int parent, int item) {
	uiInsert(parent, item);
	uiSetLayout(item, UI_HFILL);
	return item;
}

int hgroup_append_fixed(int parent, int item) {
	uiInsert(parent, item);
	return item;
}

int hgroup() {
	int item = uiItem();
	uiSetBox(item, UI_ROW);
	return item;
}

int row_append(int parent, int item) {
	uiInsert(parent, item);
	uiSetLayout(item, UI_HFILL);
	return item;
}

int row() {
	int item = uiItem();
	uiSetBox(item, UI_ROW);
	return item;
}

int separator() {
	int item = uiItem();
	uiSetSize(item, 0, 10);
	Data *data = (Data *)uiAllocHandle(item, sizeof(Data));
	data->type = WT_SEPARATOR;
	data->handler = NULL;
	return item;
}

static void layout_window(int x, int y, int w, int h) {
	// create root item; the first item always has index 0
	// this is the window frame
	int root = uiItem();
	// assign fixed size
	uiSetMargins(root, x, y, 0, 0);
	uiSetSize(root, w, 0);

	WindowData *data = (WindowData *)uiAllocHandle(root, sizeof(WindowData));
	data->head.type = WT_WINDOW;
	data->head.handler = NULL;

	// next, the content region
	int col = uiInsert(root, column());
	int pad = 5;
	uiSetMargins(col, pad, pad, pad, pad);
	uiSetLayout(col, UI_TOP | UI_HFILL);

	int box = column_append(col, hbox());
	hgroup_append(box, button("Flat", [](int item, UIevent e) {}));
	hgroup_append(box, button("Shaded", [](int item, UIevent e) {}));
	hgroup_append(box, button("Fancy", NULL));

	column_append(col, separator());

	static bool grid = false;
	static bool wireframe = false;
	column_append(col, checkbox("Show Grid", &grid));
	column_append(col, checkbox("Show Wireframe", &wireframe));
	//column_append(col, button("button", [](int item, UIevent e) {}));

	//static bool checked = false;
	// add a checkbox to the same parent as item; this is faster than
	// calling uiInsert on the same parent repeatedly.
	//item = uiAppend(item, checkbox("Checked:", &checked));
	// set a fixed height for the checkbox
	//uiSetSize(item, 0, APP_WIDGET_HEIGHT);
	// span the checkbox in the same way as the label
	//uiSetLayout(item, UI_HFILL);
}

static int imaxf(int a, int b) {
	return a > b ? a : b;
}

static int iminf(int a, int b) {
	return a < b ? a : b;
}

void gui_layout(int w, int h) {
	//layout_window(10, 10, 200, 400);
	layout_window(0, 0, iminf(w, 300), h);
}
