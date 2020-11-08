R"(
$if VERTEX
in vec3 v_position;
in vec2 v_coords;
out vec2 f_coords;
uniform mat4 u_screen_from_local;
void main() {
	f_coords = v_coords;
	gl_Position = u_screen_from_local * vec4(v_position.xyz, 1.0);
}
$endif

$if PIXEL
in vec2 f_coords;
out vec4 fs_out;

uniform sampler2D s_albedo;

void main() {
	fs_out = texture(s_albedo, f_coords);
	fs_out.rgb *= fs_out.a;
}
$endif
)"
