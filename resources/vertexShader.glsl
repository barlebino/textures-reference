#version 330 core

layout(location = 0) in vec4 vertPos;

uniform mat4 perspective;
uniform mat4 placement;

out vec3 vert_col;

void main() {
  gl_Position = perspective * placement * vertPos;
  vert_col = vec3(1.f, 0.f, 0.f);
  // vert_col = vertPos.xyz;
}
