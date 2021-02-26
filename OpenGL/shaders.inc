const char* vertexSource = R"glsl(
#version 150 core

in vec3 position;
in vec4 colour;
in vec4 texcoord;
in vec2 lmcoord;

out vec4 Colour;
//out vec4 fragcoord;
out vec2 lightcoord;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 model;

void main()
{
    lightcoord = lmcoord;
	Colour = colour;
    gl_Position = proj * view * model * vec4(position, 1.0);
})glsl";

const char* fragmentSource = R"glsl(#version 150 core

in vec4 Colour;
//in vec4 fragcoord;
in vec2 lightcoord;

//uniform sampler2D tex;
uniform sampler2D lightmap;

out vec4 outColor;

void main()
{
    outColor = (texture(lightmap, lightcoord) * 3.0) * Colour;
}
)glsl";