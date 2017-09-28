#version 330 core

uniform sampler2D texCol;

in vec2 vert_texCoord;
in vec3 vert_col;

out vec4 color;

void main() {
  color = texture(texCol, vert_texCoord);
}
