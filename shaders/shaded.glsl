R"(
$if VERTEX
//layout(location=0) in vec3 v_position;
//layout(location=1) in vec2 v_coords;
//layout(location=2) in vec3 v_normal;
//layout(location=3) in vec4 v_color;

layout(std430, binding = 0) readonly buffer InPosition { float in_position[]; };
layout(std430, binding = 1) readonly buffer InCoords { float in_coord[]; };
layout(std430, binding = 2) readonly buffer InNormals { float in_normal[]; };
layout(std430, binding = 3) readonly buffer InColors { float in_color[]; };

out vec2 f_coords;
out vec3 f_normal;
out vec4 f_color;

uniform mat4 u_screen_from_local;
uniform mat4 u_world_from_local;

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

void main() {
	vec3 v_position = read_pos();
	vec2 v_coords = read_uv();
	vec3 v_normal = read_normal();
	vec4 v_color = vec4(1.0);

	f_coords = v_coords;
	f_normal = mat3(u_world_from_local) * v_normal;
	f_color = v_color;
	gl_Position = u_screen_from_local * vec4(v_position.xyz, 1.0);
}
$endif

$if PIXEL
in vec2 f_coords;
in vec3 f_normal;
in vec4 f_color;

out vec4 fs_out;

//uniform sampler2D s_albedo;
//uniform vec4 u_light;
const vec4 u_light = vec4(1.0);

void main() {
	vec3 n = normalize(f_normal);
	vec3 l = normalize(u_light.xyz);
	fs_out = f_color * u_light.z * max(0.0, dot(n, l));// * texture(s_albedo, f_coords) * f_color;
	fs_out.a = 1.0;
	if (!gl_FrontFacing) {
		fs_out *= vec4(1.0, 0.25, 0.25, 1.0);
	}
	fs_out.rgb = pow(fs_out.rgb, vec3(0.454545));
	fs_out.rgb *= fs_out.a;
}
$endif
)";
