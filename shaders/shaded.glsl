R"(
$if VERTEX
layout(std430, binding = 0) readonly buffer InPosition { float in_position[]; };
layout(std430, binding = 1) readonly buffer InCoords { float in_coord[]; };
layout(std430, binding = 2) readonly buffer InNormals { float in_normal[]; };
layout(std430, binding = 3) readonly buffer InColors { uint in_color[]; };

out vec2 f_coords;
out vec3 f_normal;
out vec4 f_color;
out vec3 view_dir_ws;

uniform mat4 u_screen_from_local;
uniform mat4 u_world_from_local;
uniform vec4 u_color;

vec3 read_pos() {
	int idx3 = gl_VertexID * 3;
	return vec3(
		in_position[idx3],
		in_position[idx3+1],
		in_position[idx3+2]
	);
}

vec2 read_uv() {
	int idx2 = gl_VertexID * 2;
	return vec2(
		in_coord[idx2],
		in_coord[idx2+1]
	);
}

vec3 read_normal() {
	int idx3 = gl_VertexID * 3;
	return vec3(
		in_normal[idx3],
		in_normal[idx3+1],
		in_normal[idx3+2]
	);
}

vec4 read_color() {
	uint color = in_color[gl_VertexID];
	return vec4(
		// explicit cast to int for gles
		float(int(color >>  0) & 255) / 255.0,
		float(int(color >>  8) & 255) / 255.0,
		float(int(color >> 16) & 255) / 255.0,
		float(int(color >> 24) & 255) / 255.0
	);
}

void main() {
	vec3 v_position = read_pos();
	vec2 v_coords = read_uv();
	vec3 v_normal = read_normal();
	vec4 v_color = pow(read_color(), vec4(vec3(2.2), 1.0));

	vec3 view_dir_vs = -(u_world_from_local * vec4(v_position, 1.0)).xyz;
	view_dir_ws = transpose(mat3(u_world_from_local)) * -view_dir_vs;

	f_coords = v_coords;
	f_normal = mat3(1.0) * v_normal;
	f_color = v_color * vec4(u_color.rgb, 1.0);
	gl_Position = u_screen_from_local * vec4(v_position.xyz, 1.0);
}
$endif

$if PIXEL
in vec2 f_coords;
in vec3 f_normal;
in vec4 f_color;
in vec3 view_dir_ws;

out vec4 fs_out;

//uniform sampler2D s_albedo;
uniform vec4 u_light;

uniform vec4 u_color;
uniform vec4 u_color_mixes;
#define u_material_mix (u_color_mixes.x)
#define u_backface_mix (u_color_mixes.y)
#define u_uv_mix (u_color_mixes.z)
#define u_vcol_mix (u_color_mixes.w)

float square(float s) { return s * s; }

vec3 hueGradient(float t) {
	vec3 p = abs(fract(t + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return (clamp(p - 1.0, 0.0, 1.0));
}

vec3 rainbowGradient(float t) {
	vec3 c = 1.0 - pow(abs(vec3(t) - vec3(0.65, 0.5, 0.2)) * vec3(3.0, 3.0, 5.0), vec3(1.5, 1.3, 1.7));
	c.r = max((0.15 - square(abs(t - 0.04) * 5.0)), c.r);
	c.g = (t < 0.5) ? smoothstep(0.04, 0.45, t) : c.g;
	return clamp(c, 0.0, 1.0);
}

float cube(float v) { return v*v*v; }

vec3 extra_cheap_atmosphere(vec3 i_ws, vec3 sun_ws, float sdi) {
	sun_ws.z = max(sun_ws.z, -0.07);
	float special_trick = 1.0 / (i_ws.z * 1.0 + 0.125);
	float special_trick2 = 1.0 / (sun_ws.z * 11.0 + 1.0);
	float raysundt = square(abs(sdi));
	float sundt = pow(max(0.0, sdi), 8.0);
	float mymie = sundt * special_trick * 0.025;
	vec3 suncolor = mix(vec3(1.0), max(vec3(0.0), vec3(1.0) - vec3(5.5, 13.0, 22.4) / 22.4), special_trick2);
	vec3 bluesky= vec3(5.5, 13.0, 22.4) / 22.4 * suncolor;
	vec3 bluesky2 = max(vec3(0.0), bluesky - vec3(5.5, 13.0, 22.4) * 0.002 * (special_trick + -6.0 * sun_ws.z * sun_ws.z));
	bluesky2 *= special_trick * (0.4 + raysundt * 0.4);
	return max(vec3(0.0), bluesky2 * (1.0 + 1.0 * cube(1.0 - i_ws.z)) + mymie * suncolor);
}

vec3 sky_approx(vec3 i_ws, vec3 sun_ws) {
	float sdi = dot(sun_ws, i_ws);
	vec3 final = extra_cheap_atmosphere(i_ws, sun_ws, sdi);
	final = mix( // earth shadow
		final,
		vec3(3.0 * sun_ws.z) * vec3(0.3, 0.6, 1.0),
		smoothstep(0.25, -0.1, i_ws.z)
	);
	return final;
}

float schlick_ior_fresnel(float ior, float ldh) {
	float f0 = (ior-1.0)/(ior+1.0);
	f0 *= f0;
	float x = clamp(1.0-ldh, 0.0, 1.0);
	float x2 = x*x;
	return f0 + (1.0 - f0) * (x2*x2*x);
}

void main() {
	vec3 n = normalize(f_normal);
	vec3 l = normalize(u_light.xyz);
	vec3 i_ws = normalize(view_dir_ws);
	float ndl = dot(n, l);
	float ndi = max(0.0, dot(n, -i_ws));
	vec3 refl = sky_approx(i_ws, l) * mix(0.05, schlick_ior_fresnel(1.45, ndi), 0.9) * exp2(-2.0);
	float shade = pow(max(abs(-1.0-ndl)*0.05+0.05, ndl), 0.454545) * exp2(1.0);
	fs_out.a = 1.0;

	// vcols
	fs_out.rgb = mix(vec3(1.0), f_color.rgb, u_vcol_mix) * shade;

	// uv debug
	float coord_range = 1.0;
	vec2 range = vec2(0.0, 1.0);
	if (any(lessThan(f_coords, range.xx)) || any(greaterThan(f_coords, range.yy))) {
		coord_range = 0.0;
	}
	fs_out.rgb = mix(fs_out.rgb, vec3(mod(f_coords + 0.0001 /* slight bias to avoid artifacts */, vec2(1.0)), coord_range), u_uv_mix);

	// material display
	fs_out.rgb = mix(fs_out.rgb, hueGradient(u_color.a), u_material_mix);

	fs_out.rgb += refl;

	// backface highlight (mixed last, for clarity)
	if (!gl_FrontFacing) {
		fs_out.rgb = mix(fs_out.rgb, vec3(5.0, 0.0, 0.0), u_backface_mix);
	}
	fs_out.rgb *= fs_out.a;
}
$endif
)";
