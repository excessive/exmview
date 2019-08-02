#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
//doesn't work right, disabled
//#define NANOVG_LINEAR_GAMMA
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/src/nanovg.h"
#include "nanovg/src/nanovg_gl.h"
#include <string>

#include "widgets.hpp"
#include "gui.hpp"
#define OUI_IMPLEMENTATION
#include "oui.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>
#endif

static void reshape_ui() {
	// this section does not have to be regenerated on frame; a good
	// policy is to invalidate it on events, as this usually alters
	// structure and layout.

	// begin new UI declarations
	uiBeginLayout();

	int w, h;
	glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);
	// - UI setup code goes here -
	gui_layout(w, h);

	// layout UI
	uiEndLayout();
}

static int last_w = -1;
static int last_h = -1;

static void draw_widgets(NVGcontext *vg, int item, bool _first, bool _last) {
	// retrieve custom data and cast it to Data; we assume the first member
	// of every widget data item to be a Data field.
	Data *head = (Data *)uiGetHandle(item);

	// if a handle is set, this is a specialized widget
	if (head) {
		draw_widget(vg, head, item, _first, _last);
	}

	// iterate through all children and draw
	int kid = uiFirstChild(item);
	bool first = true;
	while (kid != -1) {
		bool last = kid == uiLastChild(kid);
		draw_widgets(vg, kid, first, last);
		kid = uiNextSibling(kid);
		first = false;
	}
}

static void ui_handler(int item, UIevent e) {
	Data *data = (Data *)uiGetHandle(item);
	if (data && data->handler) {
		data->handler(item, e);
	}
}

static void gui_redraw(NVGcontext *vg) {
	auto hwnd = glfwGetCurrentContext();
	int w, h, bw, bh;
	glfwGetFramebufferSize(hwnd, &w, &h);
	glfwGetWindowSize(hwnd, &bw, &bh);
	glViewport(0, 0, w, h);
	glClearColor(0.75f, 0.25f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// i don't think any os even supports mismatched x/y dpi scaling,
	// but averaging them is trivial.
	float ratio = 0.5f * ((float)w / (float)bw + (float)h / (float)bh);
	float aspect = fmaxf((float)w / h, (float)h / w);
	nvgBeginFrame(vg, w, h, ratio);

	nvgSave(vg);

	// update position of mouse cursor; the ui can also be updated
	// from received events.
	double mx, my;
	glfwGetCursorPos(hwnd, &mx, &my);
	uiSetCursor((int)mx, (int)my);

	// update button state
	for (int i = 0; i < 3; ++i)
		uiSetButton(i, 0, glfwGetMouseButton(hwnd, i) == GLFW_PRESS);

	if (last_w != w || last_h != h) {
		reshape_ui();
		last_w = w;
		last_h = h;
	}

	draw_widgets(vg, 0, true, true);

	// update states and fire handlers
	uiProcess((int)glfwGetTime());

	nvgRestore(vg);

#ifdef NANOVG_LINEAR_GAMMA
	glEnable(GL_FRAMEBUFFER_SRGB);
#endif
	nvgEndFrame(vg);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glfwSwapBuffers(hwnd);
}

#ifndef _MSC_VER
std::string exec(const char* cmd) {
	char buffer[128];
	std::string result = "";
	FILE* pipe = popen(cmd, "r");
	if (!pipe) {
		return result;
	}
	while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
			result += buffer;
	}
	pclose(pipe);
	return result;
}
#endif

static void load_font(NVGcontext *vg, const char *name, const char *filename) {
#if defined _MSC_VER
	char buf[1024];
	SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, 0, buf);
	std::string better_path(buf);
	better_path += "\\";
	better_path += filename;
	if (better_path.length() > 0) {
		nvgCreateFont(vg, name, better_path.c_str());
	}
#else
	std::string cmd = "fc-match --format=%{file} ";
	cmd += name;
	std::string path = exec(cmd.c_str());
	if (path.length() > 0) {
		nvgCreateFont(vg, name, path.c_str());
	}
#endif
}

int main(int argc, char **argv) {
	glfwInit();

	int w = 800;
	int h = 800;
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, true);
	glfwWindowHint(GLFW_SRGB_CAPABLE, true);
	auto hwnd = glfwCreateWindow(w, h, "exmview", NULL, NULL);
	glfwSetWindowSizeLimits(hwnd, 256, 256, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwSetWindowAttrib(hwnd, GLFW_RESIZABLE, true);
	glfwMakeContextCurrent(hwnd);
	glfwSwapInterval(-1);
	gl3wInit2(glfwGetProcAddress);
	auto vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

#ifdef _MSC_VER
	load_font(vg, "regular", "segoeui.ttf");
	load_font(vg, "heading", "seguisb.ttf");
#else
	load_font(vg, "regular", "Ubuntu-R.ttf");
	load_font(vg, "heading", "Ubuntu-M.ttf");
#endif

	UIcontext *oui = uiCreateContext(4096, 1 << 20);
	uiMakeCurrent(oui);
	uiSetHandler(ui_handler);

	static NVGcontext *tmp_context = NULL;
	static int spin = 1;
	glfwSetWindowRefreshCallback(hwnd, [](GLFWwindow *){
		spin = 0;
		gui_redraw(tmp_context);
	});

	while (!glfwWindowShouldClose(hwnd)) {
		if (spin > 0) {
			gui_redraw(vg);
			spin -= 1;
		}
		// hack around inability to do capture for callbacks
		tmp_context = vg;
		if (spin > 0) {
			glfwPollEvents();
		}
		else {
			glfwWaitEvents();
			// HACK: we have to spin, apparently, 3 times, to register mouse
			// press events in the UI. this can't be right, and means rendering
			// more frames than otherwise needed.
			spin = 3;
		}
		//glfwWaitEventsTimeout(1.0/60.0);
		tmp_context = NULL;
	}

	uiDestroyContext(oui);

	nvgDeleteGL3(vg);
	glfwDestroyWindow(hwnd);
	glfwTerminate();
	return 0;
}
