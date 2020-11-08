#pragma once

struct BuiltInShader {
	const char *vs_fs;
	const char **attribs;
	const char *cs;
	bool is_cs;
};

enum ShaderType {
	SHADER_GRADIENT,
	SHADER_SKY,
	SHADER_SHADED,
	SHADER_POST
};

static const char *sky_attribs[] = { "v_position", NULL };
static const char *sky_src =
#include "shaders/sky.glsl"
;

static const char *gradient_attribs[] = { "v_position", "v_coords", NULL };
static const char *gradient_src =
#include "shaders/gradient.glsl"
;

static const char *shaded_attribs[] = { "v_position", "v_coords", "v_normal", "v_color", NULL };
static const char *shaded_src =
#include "shaders/shaded.glsl"
;

static const char *post_attribs[] = { "v_position", NULL };
static const char *post_src =
#include "shaders/post.glsl"
;

static const BuiltInShader shaders[] = {
	{ gradient_src, gradient_attribs, NULL, false },
	{ sky_src, sky_attribs, NULL, false },
	{ shaded_src, shaded_attribs, NULL, false },
	{ post_src, post_attribs, NULL, false },
	NULL
};
