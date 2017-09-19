#version 330 core

in vec3 vert_col;

out vec4 color;

void main() {
  color = vec4(vert_col, 1.0);
}
