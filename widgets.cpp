#include "widgets.hpp"
#include <math.h>
#include <stddef.h>

#define APP_WIDGET_HEIGHT 20
#define APP_WIDGET_FONT_SIZE 16.0f
#define APP_WIDGET_FILL_COLOR nvgRGBA(212, 208, 200, 255)
#define APP_WIDGET_FONT_COLOR nvgRGBA(0, 0, 5, 255)
#define APP_WIDGET_HOT_COLOR  nvgRGBA(0, 220, 80, 255)

#define APP_WIDGET_BUTTON_ROUNDING 0
#define APP_WIDGET_BUTTON_SHADOW_OX 0
#define APP_WIDGET_BUTTON_SHADOW_OY 1
#define APP_WIDGET_BUTTON_SHADOW_INSET 0
#define APP_WIDGET_BUTTON_SHADOW_SPREAD 2

#define APP_WIDGET_BUTTON_TEXT_NORMAL nvgRGBA(0, 0, 0, 255)
#define APP_WIDGET_BUTTON_TEXT_DISABLED nvgRGBA(0x44, 0x44, 0x44, 255)
#define APP_WIDGET_BUTTON_BG_DISABLED nvgRGBA(192, 188, 180, 255)
#define APP_WIDGET_BUTTON_BG_ACTIVE nvgRGBA(232, 218, 210, 255)
#define APP_WIDGET_BUTTON_BG_INACTIVE nvgRGBA(212, 208, 200, 255)
#define APP_WIDGET_BUTTON_BG_HOT nvgRGBA(222, 218, 210, 255)
#define APP_WIDGET_BUTTON_HIGHLIGHT_LOW nvgRGBA(0, 0, 0, 0)
#define APP_WIDGET_BUTTON_HIGHLIGHT_HIGH nvgRGBA(255, 255, 255, 255)

#define APP_WIDGET_BUTTON_SHADOW_COLOR nvgRGBA(0, 0, 0, 0x60)

struct ItemRounding {
	float tl;
	float tr;
	float bl;
	float br;
};

static ItemRounding round_item_corners(int item, bool _first, bool _last, float radius) {
	ItemRounding r;
	r.tl = 0.0f;
	r.tr = 0.0f;
	r.bl = 0.0f;
	r.br = 0.0f;

	unsigned box = uiGetBox(item);
	if ((_first && _last) || !(box & (UI_ROW | UI_COLUMN))) {
		r.tl = radius;
		r.tr = radius;
		r.bl = radius;
		r.br = radius;
	}
	else if ((box & UI_ROW) == UI_ROW) {
		if (_first) {
			r.tl = radius;
			r.bl = radius;
		}
		else {
			r.tr = radius;
			r.br = radius;
		}
	}
	else {
		if (_first) {
			r.tl = radius;
			r.tr = radius;
		}
		else {
			r.bl = radius;
			r.br = radius;
		}
	}

	return r;
}

static void auto_rect(NVGcontext *vg, NVGpaint col, float x, float y, float w, float h, ItemRounding *r = NULL, float pad = 0.0f, float round = 0.0f) {
	nvgBeginPath(vg);;
	if (r) {
		nvgRoundedRectVarying(vg, x + pad, y + pad, w - pad * 2, h - pad * 2, r->tl, r->tr, r->br, r->bl);
	}
	else {
		nvgRoundedRect(vg, x + pad, y + pad, w - pad * 2, h - pad * 2, round);
	}
	nvgFillPaint(vg, col);
	nvgFill(vg);
}

static void auto_rect(NVGcontext *vg, NVGcolor col, float x, float y, float w, float h, ItemRounding *r = NULL, float pad = 0.0f, float round = 0.0f) {
	nvgBeginPath(vg);
	if (r) {
		nvgRoundedRectVarying(vg, x + pad, y + pad, w - pad * 2, h - pad * 2, r->tl, r->tr, r->br, r->bl);
	}
	else {
		nvgRoundedRect(vg, x + pad, y + pad, w - pad * 2, h - pad * 2, round);
	}
	nvgFillColor(vg, col);
	nvgFill(vg);
}

struct BoxShading {
	NVGcolor bg;
	NVGcolor high;
	NVGcolor low;
	float grad_start;
	float grad_end;
};

static void shaded_box(NVGcontext *vg, float x, float y, float w, float h, BoxShading col, ItemRounding *corners){
	float r = APP_WIDGET_BUTTON_ROUNDING;
	float r2 = r;
	float ox = APP_WIDGET_BUTTON_SHADOW_OX;
	float oy = APP_WIDGET_BUTTON_SHADOW_OY;
	float inset = APP_WIDGET_BUTTON_SHADOW_INSET;
	float spread = APP_WIDGET_BUTTON_SHADOW_SPREAD;
	auto shadow = nvgBoxGradient(vg,
		x + inset + ox, y + inset + oy,
		w - inset * 2, h - inset * 2,
		r2, spread,
		APP_WIDGET_BUTTON_SHADOW_COLOR, nvgRGBA(0, 0, 0, 0)
	);
	auto highlight = nvgLinearGradient(vg, x, y + col.grad_start, x, y + col.grad_end, col.high, col.low);
	auto_rect(vg, shadow, x - spread + ox, y - spread + oy, w + 2 * spread, h + 2 * spread, corners, 0, 0);
	auto_rect(vg, col.bg, x, y, w, h, corners, 0, r);
	auto_rect(vg, highlight, x, y, w, h, corners, 0, r);
}

static void draw_hot(NVGcontext *vg, UIrect rect, int item) {
	unsigned char heat = 0;
	unsigned state = uiGetState(item);
	if (state == UI_HOT) {
		heat = 100;
	}
	else if (state == UI_BUTTON0_CAPTURE || state == UI_ACTIVE) {
		heat = 150;
	}
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, rect.w, rect.h);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, heat));
	nvgFill(vg);
}

static void draw_window(NVGcontext *vg, UIrect rect) {
	nvgBeginPath(vg);
	float spread = 10.0f;
	float ox = 0.0f;
	float oy = 2.0f;
	float r = 0.0f;
	nvgFillPaint(vg, nvgBoxGradient(vg, ox, oy, rect.w, rect.h, r, spread, nvgRGBA(0, 0, 0, 50), nvgRGBA(0, 0, 0, 0)));
	nvgRoundedRect(vg, ox-spread, oy-spread, rect.w + spread * 2, rect.h + spread * 2, r);
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, rect.w, rect.h, r);
	nvgFillColor(vg, APP_WIDGET_FILL_COLOR);
	nvgFill(vg);
}

static void draw_label(NVGcontext *vg, UIrect rect, ButtonData *data) {
	nvgFontSize(vg, APP_WIDGET_FONT_SIZE);
	nvgFontFace(vg, data->head.type == WT_HEADING ? "heading" : "regular");
	nvgFillColor(vg, APP_WIDGET_FONT_COLOR);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, 0, (int)(APP_WIDGET_HEIGHT / 2), data->label, NULL);
}

static void draw_checkbox(NVGcontext *vg, UIrect rect, CheckBoxData *data, int item) {
	// get the widget's current state
	int state = uiGetState(item);
	// if the value is set, the state is always active
	if (*data->checked) {
		state = UI_ACTIVE;
	}
	
	// checkbox
	int size = APP_WIDGET_HEIGHT - 4;
	nvgBeginPath(vg);
	nvgRoundedRect(vg, rect.w - size - 2, 2.0f, size, size, 3.0f);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	nvgFill(vg);

	// fill
	nvgBeginPath(vg);
	nvgRoundedRect(vg, rect.w - size, 4, size - 4, size - 4, 2);
	nvgFillColor(vg, nvgTransRGBA(APP_WIDGET_HOT_COLOR, state == UI_ACTIVE ? 255 : 0));
	nvgFill(vg);

	// label
	nvgFontFace(vg, "regular");
	nvgFontSize(vg, APP_WIDGET_FONT_SIZE);
	nvgFillColor(vg, APP_WIDGET_FONT_COLOR);

	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, 4, (int)(APP_WIDGET_HEIGHT / 2), data->label, NULL);

	//nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	//nvgText(vg, (int)(rect.w - size - 5), (int)(APP_WIDGET_HEIGHT / 2), data->label, NULL);
}

static BoxShading hot_colors() {
	BoxShading col;
	col.high = APP_WIDGET_BUTTON_HIGHLIGHT_HIGH;
	col.low = APP_WIDGET_BUTTON_HIGHLIGHT_LOW;
	col.bg = APP_WIDGET_BUTTON_BG_HOT;
	col.grad_start = -10;
	col.grad_end = 25;
	return col;
}

static BoxShading active_colors(float h) {
	BoxShading col;
	col.high = APP_WIDGET_BUTTON_HIGHLIGHT_LOW;
	col.low = nvgRGBA(255, 255, 255, 0x30);
	col.bg = APP_WIDGET_BUTTON_BG_ACTIVE;
	col.grad_start = h - 30;
	col.grad_end = h;
	return col;
}

static BoxShading disabled_colors() {
	BoxShading col;
	col.high = nvgRGBA(0xBB, 0xBB, 0xBB, 255);
	col.low = APP_WIDGET_BUTTON_HIGHLIGHT_LOW;
	col.bg = APP_WIDGET_BUTTON_BG_DISABLED;
	col.grad_start = -10;
	col.grad_end = 15;
	return col;
}

static void draw_button(NVGcontext *vg, UIrect rect, ButtonData *data, int item, bool _first, bool _last) {
	BoxShading col;
	col.bg = APP_WIDGET_BUTTON_BG_INACTIVE;
	col.high = APP_WIDGET_BUTTON_HIGHLIGHT_HIGH;
	col.low = APP_WIDGET_BUTTON_HIGHLIGHT_LOW;
	col.grad_start = -10;
	col.grad_end = 25;

	unsigned state = uiGetState(item);
	bool disabled = data->head.handler == NULL;
	if (disabled) {
		col = disabled_colors();
	}
	else if (state == UI_HOT) {
		col = hot_colors();
	}
	else if (state == UI_BUTTON0_CAPTURE || state == UI_ACTIVE) {
		col = active_colors(rect.h);
	}

	ItemRounding corners = round_item_corners(item, _first, _last, APP_WIDGET_BUTTON_ROUNDING);
	shaded_box(vg, 0, 0, rect.w, rect.h, col, &corners);

	nvgFontFace(vg, "regular");
	nvgFontSize(vg, APP_WIDGET_FONT_SIZE);
	nvgFillColor(vg, disabled ? APP_WIDGET_BUTTON_TEXT_DISABLED : APP_WIDGET_BUTTON_TEXT_NORMAL);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgText(vg, (int)(rect.w * 0.5), (int)(APP_WIDGET_HEIGHT * 0.5), data->label, NULL);
}

static void draw_hbox(NVGcontext *vg, UIrect rect) {
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, rect.w, rect.h);
	nvgFillColor(vg, nvgRGBAf(0.0f, 1.0f, 1.0f, 0.5f));
	nvgFill(vg);
}

static void draw_vbox(NVGcontext *vg, UIrect rect) {
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, rect.w, rect.h);
	nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 0.0f, 0.5f));
	nvgFill(vg);
}

static void draw_separator(NVGcontext *vg, UIrect rect) {
	float margin = 5.0f;
	nvgBeginPath(vg);
	nvgRect(vg, margin, floorf(rect.h / 2 - 0.5), rect.w - margin * 2, 1);
	nvgFillColor(vg, nvgRGBA(0xa0, 0xa0, 0xa0, 255));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRect(vg, margin, ceilf(rect.h / 2), rect.w - margin * 2, 1);
	nvgFillColor(vg, nvgRGBA(0xf0, 0xf0, 0xf0, 255));
	nvgFill(vg);
}

void draw_widget(NVGcontext *vg, Data *head, int item, bool _first, bool _last) {
	// get the widgets absolute rectangle
	UIrect rect = uiGetRect(item);

	nvgSave(vg);
	nvgFontFace(vg, "regular");
	nvgFontSize(vg, APP_WIDGET_FONT_SIZE);
	nvgTranslate(vg, rect.x, rect.y);
	//nvgScissor(vg, 0, 0, rect.w, rect.h);

	// this affects anything that responds to mouse
	draw_hot(vg, rect, item);

	switch (head->type) {
		default: break;
		case WT_WINDOW: draw_window(vg, rect); break;
		case WT_HEADING:
		case WT_LABEL:  draw_label(vg, rect, (ButtonData *)head); break;
		case WT_HBOX:   draw_hbox(vg, rect); break;
		case WT_VBOX:   draw_vbox(vg, rect); break;
		case WT_BUTTON: draw_button(vg, rect, (ButtonData *)head, item, _first, _last); break;
		case WT_CHECKBOX: draw_checkbox(vg, rect, (CheckBoxData *)head, item); break;
		case WT_SEPARATOR: draw_separator(vg, rect); break;
	}

	nvgRestore(vg);
}
