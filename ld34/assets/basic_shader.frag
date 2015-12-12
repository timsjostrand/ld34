#version 400

in vec2 texcoord;
out vec4 frag_color;

uniform float time;
uniform vec4 color;
uniform sampler2D tex;

uniform vec2 view_size;
uniform vec2 view_offset;
uniform vec2 window_size;
uniform vec3 player_pos;
uniform vec3 player_sprite_pos;

void main() {
    frag_color = texture(tex, vec2(texcoord.x, texcoord.y)) * color;
}
