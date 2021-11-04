#pragma once

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

struct Stage
{
	std::string map;
	bool hasBlend{ false };
	GLuint BlendSrc{ GL_ONE };
	GLuint BlendDst{ GL_ONE };
	std::string rgb;
	std::string clamp;
	std::string alpha;
	std::string tcGen;
};

struct Shader
{
	std::string name;
	std::vector<Stage> stages;
};

class ShaderParser
{
public:
	void parse(std::string shader);
};

