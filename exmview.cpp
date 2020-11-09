#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
//doesn't work right, disabled
//#define NANOVG_LINEAR_GAMMA
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/src/nanovg.h"
#include "nanovg/src/nanovg_gl.h"
#include <string>
#include <algorithm>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include <unordered_map>

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
#include "iqm.hpp"
#include "exmview.hpp"

// used by widgets
bool g_focus = true;

static int last_w = -1;
static int last_h = -1;

static bool loaded = false;
static tfx_program flat_textured, shaded, post, sky;
static tfx_uniform albedo, world_from_local, screen_from_local, light, color, color_mixes;
static tfx_texture bg_gradient;
static tfx_canvas back;

State g_state;

struct Model {
	unsigned total_indices;
	std::unordered_map<int, tfx_buffer> buffers;
	std::vector<MeshChunk> chunks;
	tfx_buffer ibo;
};
static std::vector<Model> g_models;
struct v3 { float x, y, z; };

static bool spinning = false;
static float pitch = M_PI_4 * -0.25f;
static float yaw = M_PI_2;

static v3 view_pos = { 0.0f, -5.0f, 1.7f };
static v3 view_dir = { 0.0f, 0.0f, 0.0f };

#include "shaders.hpp"

static void reshape_ui() {
	// this section does not have to be regenerated on frame; a good
	// policy is to invalidate it on events, as this usually alters
	// structure and layout.

	// begin new UI declarations
	uiBeginLayout();

	int w, h;
	glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);
	tfx_reset(w, h, (tfx_reset_flags)(TFX_RESET_MAX_ANISOTROPY));
	// | TFX_RESET_REPORT_GPU_TIMINGS | TFX_RESET_DEBUG_OVERLAY | TFX_RESET_DEBUG_OVERLAY_STATS));
	// - UI setup code goes here -
	gui_layout(w, h);

	// layout UI
	uiEndLayout();
}

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
		ret = tfx_program_new(csrc, csrc, def.attribs, -1);
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

tfx_transient_buffer screen_triangle(float depth = 1.0f) {
	tfx_vertex_format fmt = tfx_vertex_format_start();
	tfx_vertex_format_add(&fmt, 0, 3, false, TFX_TYPE_FLOAT);
	tfx_vertex_format_end(&fmt);

	tfx_transient_buffer tb = tfx_transient_buffer_new(&fmt, 3);
	v3 *vdata = (v3*)tb.data;
	vdata[0] = { -1.0f, 3.0f, depth };
	vdata[1] = { -1.0f, -1.0f, depth };
	vdata[2] = { 3.0f, -1.0f, depth };
	return tb;
}

static void update_dir() {
	view_dir.x = cosf(yaw);
	view_dir.y = sinf(yaw);
	view_dir.z = sinf(pitch);
}

static void gui_redraw(NVGcontext *vg) {
	if (!loaded) {
		flat_textured = load(SHADER_GRADIENT);
		shaded = load(SHADER_SHADED);
		post = load(SHADER_POST);
		sky = load(SHADER_SKY);
		assert(shaded);
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
		screen_from_local = tfx_uniform_new("u_screen_from_local", TFX_UNIFORM_MAT4, 1);
		world_from_local = tfx_uniform_new("u_world_from_local", TFX_UNIFORM_MAT4, 1);
		// world_from_screen = tfx_uniform_new("u_world_from_screen", TFX_UNIFORM_MAT4, 1);
		light = tfx_uniform_new("u_light", TFX_UNIFORM_VEC4, 1);
		color = tfx_uniform_new("u_color", TFX_UNIFORM_VEC4, 1);
		color_mixes = tfx_uniform_new("u_color_mixes", TFX_UNIFORM_VEC4, 1);
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

		tfx_canvas_free(&back);
		tfx_texture attachments[2] = {
			tfx_texture_new(w, h, 1, NULL, TFX_FORMAT_RGBA16F, TFX_TEXTURE_MSAA_X4),
			tfx_texture_new(w, h, 1, NULL, TFX_FORMAT_D24, TFX_TEXTURE_MSAA_X4)
		};
		back = tfx_canvas_attachments_new(true, 2, attachments);
	}

	const float flight[4] = { 1.0f, -1.0f, 1.0f, 1.0f };
	tfx_set_uniform(&light, flight, 1);

	const bool wireframe = g_state.wireframe;
	const bool culling = !g_state.show_backfaces;
	const float show_materials = g_state.show_materials;
	const float highlight_backfaces = g_state.highlight_backfaces;
	const float show_uv = g_state.show_uv;
	const float show_vcols = g_state.show_vcols;
	const float colmix[4] = {
		show_materials,
		highlight_backfaces,
		show_uv,
		show_vcols
	};
	tfx_set_uniform(&color_mixes, colmix, 1);

	tfx_view_set_clear_color(0, 0xAA607FFF);
	tfx_view_set_clear_depth(0, 1.0);
	tfx_view_set_depth_test(0, TFX_DEPTH_TEST_LT);
	tfx_view_set_name(0, "Background");
	tfx_view_set_canvas(0, &back, 0);
	tfx_touch(0);

	tfx_view_set_depth_test(1, TFX_DEPTH_TEST_LT);
	tfx_view_set_name(1, "Main");
	tfx_view_set_canvas(1, &back, 0);
	tfx_touch(1);

	tfx_view_set_name(2, "Resolve");
	tfx_set_transient_buffer(screen_triangle(1.0f));
	tfx_set_state(TFX_STATE_RGB_WRITE | TFX_STATE_CULL_CCW);
	tfx_texture tex = tfx_get_texture(&back, 0);
	tfx_set_texture(&albedo, &tex, 0);
	tfx_submit(2, post, false);

	//float aspect = fmaxf((float)w / h, (float)h / w);
	const float proj[16] = {
		2.0f / w, 0.0f, 0.0f, 0.0f,
		0.0f, -2.0f / h, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 1.0f
	};
	tfx_set_uniform(&screen_from_local, proj, 1);

	tfx_set_texture(&albedo, &bg_gradient, 0);
	tfx_set_transient_buffer(draw_quad(0, 0, w, h, 0));
	tfx_set_state(TFX_STATE_RGB_WRITE | TFX_STATE_CULL_CCW);
	tfx_submit(0, flat_textured, false);

	update_dir();

	const v3 eye = { view_pos.x, view_pos.y, view_pos.z };
	const v3 at = { eye.x + view_dir.x, eye.y + view_dir.y, eye.z + view_dir.z };
	const v3 up = { 0.0f, 0.0f, 1.0f };
	const auto dot = [](v3 a, v3 b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	};
	const auto normalize = [](v3 v) {
		float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
		return v3 { v.x / len, v.y / len, v.z / len };
	};
	const auto cross = [](v3 a, v3 b) {
		return v3 {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	};
	const v3 forward = normalize(v3 { at.x - eye.x, at.y - eye.y, at.z - eye.z });
	const v3 side = normalize(cross(forward, up));
	const v3 new_up = cross(side, forward);
	const float local[16] = {
		side.x, new_up.x, -forward.x, 0.0f,
		side.y, new_up.y, -forward.y, 0.0f,
		side.z, new_up.z, -forward.z, 0.0f,
		-dot(side, eye), -dot(new_up, eye), dot(forward, eye), 1.0f
	};

	const float fovy = 55.0f;
	const float aspect = (float)w / h;
	const float _near = 0.1;
	const float _far = 1000.0f;
	const float pi = 3.141592653589793f;
	const float t = tanf(fovy * pi / 180.0f * 0.5);
	const float persp[16] = {
		1.0f / (t * aspect), 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / t, 0.0f, 0.0f,
		0.0f, 0.0f, -(_far + _near) / (_far - _near), -1.0f,
		0.0f, 0.0f, -(2.0f * _far * _near) / (_far - _near), 0.0f
	};

	const float *a = persp;
	const float *b = local;
	const float mvp[16] = {
		b[0] * a[0] + b[1] * a[4] + b[2] * a[8] + b[3] * a[12],
		b[0] * a[1] + b[1] * a[5] + b[2] * a[9] + b[3] * a[13],
		b[0] * a[2] + b[1] * a[6] + b[2] * a[10] + b[3] * a[14],
		b[0] * a[3] + b[1] * a[7] + b[2] * a[11] + b[3] * a[15],
		b[4] * a[0] + b[5] * a[4] + b[6] * a[8] + b[7] * a[12],
		b[4] * a[1] + b[5] * a[5] + b[6] * a[9] + b[7] * a[13],
		b[4] * a[2] + b[5] * a[6] + b[6] * a[10] + b[7] * a[14],
		b[4] * a[3] + b[5] * a[7] + b[6] * a[11] + b[7] * a[15],
		b[8] * a[0] + b[9] * a[4] + b[10] * a[8] + b[11] * a[12],
		b[8] * a[1] + b[9] * a[5] + b[10] * a[9] + b[11] * a[13],
		b[8] * a[2] + b[9] * a[6] + b[10] * a[10] + b[11] * a[14],
		b[8] * a[3] + b[9] * a[7] + b[10] * a[11] + b[11] * a[15],
		b[12] * a[0] + b[13] * a[4] + b[14] * a[8] + b[15] * a[12],
		b[12] * a[1] + b[13] * a[5] + b[14] * a[9] + b[15] * a[13],
		b[12] * a[2] + b[13] * a[6] + b[14] * a[10] + b[15] * a[14],
		b[12] * a[3] + b[13] * a[7] + b[14] * a[11] + b[15] * a[15]
	};
	tfx_set_uniform(&screen_from_local, mvp, 1);
	tfx_set_uniform(&world_from_local, local, 1);

	tfx_set_transient_buffer(screen_triangle(1.0f));
	tfx_set_state(TFX_STATE_RGB_WRITE | TFX_STATE_CULL_CCW | TFX_STATE_BLEND_ALPHA);
	tfx_submit(0, sky, false);

	float i = 0.0f;
	for (auto &model : g_models) {
		for (auto &chunk : model.chunks) {
			for (auto &buf : model.buffers) {
				tfx_set_buffer(&buf.second, buf.first, false);
			}
			float fcolor[4] = { 1.0f, 1.0f, 1.0f, fmodf(i / model.chunks.size(), 1.0f) };
			tfx_set_uniform(&color, fcolor, 1);
			tfx_set_state(0
				| TFX_STATE_RGB_WRITE
				| TFX_STATE_DEPTH_WRITE
				| TFX_STATE_BLEND_ALPHA
				| TFX_STATE_MSAA
				| (wireframe ? TFX_STATE_WIREFRAME : 0)
				| (culling ? TFX_STATE_CULL_CCW : 0)
			);
			tfx_set_indices(&model.ibo, chunk.num_indices, chunk.offset * 4);
			tfx_submit(1, shaded, false);
			i += 1.0f;
		}
	}
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
	update_dir();

	for (int i = 1; i < argc; i++) {
		MeshData md;
		if (iqm_read_data(&md, argv[i], true)) {
			Model model;
			for (auto &l : md.layers) {
				if (l.data.size() == 0) {
					continue;
				}
				int slot = -1;
				switch (l.component) {
					case MC_POSITION: slot = 0; break;
					case MC_TEXCOORD: slot = 1; break;
					case MC_NORMAL: slot = 2; break;
					case MC_COLOR: slot = 3;  break;
					case MC_TANGENT: break;
					case MC_BONE: break;
					case MC_WEIGHT: break;
					case MC_NONE: break;
					default: assert(false);
				}
				if (slot >= 0) {
					model.buffers[slot] = tfx_buffer_new(
						l.data.data(),
						l.bytes,
						NULL,
						TFX_BUFFER_NONE
					);
				}
			}
			model.ibo = tfx_buffer_new(
				md.indices.data(),
				md.indices_bytes,
				NULL,
				TFX_BUFFER_INDEX_32
			);
			model.total_indices = md.indices.size();
			for (const auto &chunk : md.chunks) {
				model.chunks.push_back(chunk);
			}
			g_models.push_back(model);
		}
	}

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

	glfwSetMouseButtonCallback(hwnd, [](GLFWwindow *wnd, int button, int action, int mods) {
		if (button == 0) {
			spinning = (action == GLFW_PRESS);
		}
		if (spinning) {
			glfwSetInputMode(wnd, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode(wnd, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	});
	glfwSetScrollCallback(hwnd, [](GLFWwindow *, double x, double y) {
		update_dir();
		view_pos.x += view_dir.x * y;
		view_pos.y += view_dir.y * y;
		view_pos.z += view_dir.z * y;
	});
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(hwnd, GLFW_RAW_MOUSE_MOTION, 1);
	}
	glfwSetCursorPosCallback(hwnd, [](GLFWwindow *, double x, double y) {
		double div = 250.0;
		static double lx = x;
		static double ly = y;
		if (spinning) {
			yaw += (lx - x) / div;
			pitch += (ly - y) / div;
			// yaw = fmodf(yaw, M_PI * 2.0f);
			pitch = fmaxf(fminf(pitch, M_PI_2), -M_PI_2);
		}
		lx = x;
		ly = y;
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
