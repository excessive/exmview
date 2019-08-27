R"(
$if VERTEX
in vec3 v_position;
in vec2 v_coords;
in vec3 v_normal;
in vec4 v_color;

out vec2 f_coords;
out vec3 f_normal;
out vec4 f_color;

uniform mat4 u_local_to_screen;
uniform mat4 u_local_to_world;

void main() {
	f_coords = v_coords;
	f_normal = mat3(u_local_to_world) * v_normal;
	f_color = v_color;
	gl_Position = u_local_to_screen * vec4(v_position.xyz, 1.0);
}
$endif

$if PIXEL
in vec2 f_coords;
in vec3 f_normal;
in vec4 f_color;

out vec4 fs_out;

uniform sampler2D s_albedo;
uniform vec4 u_light;

void main() {
	vec3 n = normalize(f_normal);
	vec3 l = normalize(u_light.xyz);
	fs_out = u_light.z * max(0.0, dot(n, l)) * texture(s_albedo, f_coords) * f_color;
	fs_out.rgb *= fs_out.a;
}
$endif
)";
