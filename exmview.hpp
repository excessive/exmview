#pragma once

struct State {
	bool wireframe = false;
	bool show_backfaces = true;
	float highlight_backfaces = 1.0f;
	float show_materials = 0.0f;
	float show_uv = 0.0f;
	float show_vcols = 1.0f;
};

extern State g_state;
