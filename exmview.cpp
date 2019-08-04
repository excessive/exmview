#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
//doesn't work right, disabled
//#define NANOVG_LINEAR_GAMMA
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/src/nanovg.h"
#include "nanovg/src/nanovg_gl.h"
#include <string>
#include <algorithm>

#include "widgets.hpp"
#include "gui.hpp"
#define OUI_IMPLEMENTATION
#include "oui.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>
#endif

#include "tinyfx.h"

// used by widgets
bool g_focus = true;

static void reshape_ui() {
	// this section does not have to be regenerated on frame; a good
	// policy is to invalidate it on events, as this usually alters
	// structure and layout.

	// begin new UI declarations
	uiBeginLayout();

	int w, h;
	glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);
	tfx_reset(w, h, TFX_RESET_MAX_ANISOTROPY);
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

tfx_transient_buffer draw_quad(float x, float y, float w, float h, float z = 0.0f) {
	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 0, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_add(&fmt, 1, 2, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	tfx_transient_buffer tb = tfx_transient_buffer_new(&fmt, 6);
	float *fdata = (float *)tb.data;

	const float cw = 1.0f;
	const float a[4] = { floorf(x), floorf(y), 0.0f, 1.0f - cw }; // BL
	const float b[4] = { floorf(x + w), floorf(y), 1.0f, 1.0f - cw }; // BR
	const float c[4] = { floorf(x + w), floorf(y + h), 1.0f, cw }; // TR
	const float d[4] = { floorf(x), floorf(y + h), 0.0f, cw }; // TL
	const float *v[4] = { a, b, c, d };
	const uint8_t indices[6] = { 0, cw < 0.5f ? 1 : 3, 2, 0, 2, cw < 0.5f ? 3 : 1 };

	for (unsigned i = 0; i < 6; i++) {
		uint8_t index = indices[i];
		fdata[i * 5 + 0] = v[index][0];
		fdata[i * 5 + 1] = v[index][1];
		fdata[i * 5 + 2] = z;
		fdata[i * 5 + 3] = v[index][2];
		fdata[i * 5 + 4] = v[index][3];
	}

	return tb;
}

static bool loaded = false;
static tfx_program flat_textured;
static tfx_uniform albedo, m, w, light;
static tfx_texture bg_gradient;

#include "shaders.hpp"

static std::string decash(std::string input) {
	std::string output(input);
	std::replace(output.begin(), output.end(), '$', '#');
	return output;
}

static tfx_program load(ShaderType shader) {
	tfx_program ret = 0;
	BuiltInShader def = shaders[shader];
	if (def.is_cs && def.cs != NULL) {
		std::string src = decash(std::string(def.cs));
		ret = tfx_program_cs_new(src.c_str());
	}
	else if (def.vs_fs != NULL && def.attribs != NULL) {
		std::string src = decash(std::string(def.vs_fs));
		const char *csrc = src.c_str();
		ret = tfx_program_new(csrc, csrc, def.attribs);
	}
	assert(ret != 0);
	return ret;
}

static unsigned swap32(unsigned v) {
	return ((v >> 24) & 0x000000ff)
	     | ((v <<  8) & 0x00ff0000)
	     | ((v >>  8) & 0x0000ff00)
	     | ((v << 24) & 0xff000000)
	;
}

static void gui_redraw(NVGcontext *vg) {
	if (!loaded) {
		flat_textured = load(SHADER_GRADIENT);
		uint32_t bg_pixels9[] = {
			swap32(0x5F1F3FFF), swap32(0x5F1F3FFF), swap32(0x5F1F3FFF),
			swap32(0x8F2F5FFF), swap32(0xA7376FFF), swap32(0x8F2F5FFF),
			swap32(0xBF3F7FFF), swap32(0xBF3F7FFF), swap32(0xBF3F7FFF),
		};
		uint32_t bg_pixels2[] = {
			swap32(0x5F1F3FFF),
			swap32(0xA7376FFF),
			//swap32(0xBF3F7FFF)
		};
		//float c0[4] = { 0.75 * 0.5, 0.25 * 0.5, 0.5 * 0.5, 1.0 };
		//float c1[4] = { 0.75, 0.25, 0.5, 1.0 };
		bg_gradient = tfx_texture_new(1, 2, 1, bg_pixels2, TFX_FORMAT_RGBA8, TFX_TEXTURE_FILTER_LINEAR);
		albedo = tfx_uniform_new("s_albedo", TFX_UNIFORM_INT, 1);
		m = tfx_uniform_new("u_local_to_screen", TFX_UNIFORM_MAT4, 1);
		w = tfx_uniform_new("u_local_to_world", TFX_UNIFORM_MAT4, 1);
		light = tfx_uniform_new("u_light", TFX_UNIFORM_VEC4, 1);
		loaded = true;
	}

	auto hwnd = glfwGetCurrentContext();
	int w, h, bw, bh;
	glfwGetFramebufferSize(hwnd, &w, &h);
	glfwGetWindowSize(hwnd, &bw, &bh);

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

	tfx_view_set_clear_color(0, 0xAA607FFF);
	tfx_view_set_clear_depth(0, 1.0);
	tfx_view_set_depth_test(0, TFX_DEPTH_TEST_LT);
	tfx_touch(0);

	//float aspect = fmaxf((float)w / h, (float)h / w);
	float proj[16] = {
		2.0f / w, 0.0f, 0.0f, 0.0f,
		0.0f, -2.0f / h, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 1.0f
	};
	tfx_set_uniform(&m, proj, 1);

	tfx_set_texture(&albedo, &bg_gradient, 0);
	tfx_set_transient_buffer(draw_quad(0, 0, w, h, 0));
	tfx_set_state(TFX_STATE_DEFAULT);
	tfx_submit(0, flat_textured, false);

	//tfx_set_uniform(&m, mvp, 1);
	//tfx_set_uniform(&w, model, 1);
	//tfx_set_vertices(&vbo, vert_count);
	//tfx_set_indices(&ibo, count, 0);
	//tfx_set_state(TFX_STATE_DEFAULT);
	//tfx_submit(0, shaded, false);

	tfx_frame();

	// i don't think any os even supports mismatched x/y dpi scaling,
	// but averaging them is trivial.
	float ratio = 0.5f * ((float)w / (float)bw + (float)h / (float)bh);
	nvgBeginFrame(vg, w, h, ratio);

	nvgSave(vg);

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
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, true);
	glfwWindowHint(GLFW_SRGB_CAPABLE, true);
	auto hwnd = glfwCreateWindow(w, h, "exmview", NULL, NULL);
	glfwSetWindowSizeLimits(hwnd, 256, 256, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwSetWindowAttrib(hwnd, GLFW_RESIZABLE, true);
	glfwMakeContextCurrent(hwnd);
	glfwSwapInterval(-1);

	tfx_platform_data pd;
	memset(&pd, 0, sizeof(tfx_platform_data));
	pd.context_version = 43;
	pd.use_gles = false;
	pd.gl_get_proc_address = [](const char *name) {
		return (void*)glfwGetProcAddress(name);
	};
	tfx_set_platform_data(pd);

	tfx_reset(w, h, TFX_RESET_MAX_ANISOTROPY);

	gl3wInit2(glfwGetProcAddress);
	auto vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

#ifdef _MSC_VER
	//load_font(vg, "regular", "Inter-Regular.ttf");
	//load_font(vg, "heading", "Inter-Medium.ttf");
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

	glfwSetWindowFocusCallback(hwnd, [](GLFWwindow *, int focus) {
		g_focus = focus == GLFW_TRUE;
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
	tfx_shutdown();
	glfwDestroyWindow(hwnd);
	glfwTerminate();
	return 0;
}
