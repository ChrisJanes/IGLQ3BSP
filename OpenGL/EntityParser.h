#pragma once

#include <map>
#include <vector>
#include <string>
#include "glm/glm.hpp"

struct entity
{
public:
	glm::vec3 origin;
	std::map<std::string, std::string> pairs;

	float get_float(std::string key);
	std::string get_string(std::string key);
	void get_vec3(std::string key, glm::vec3& vec);
};

class EntityParser
{
public:
	entity parse(std::string ent);
	void parse(std::string entities, std::vector<entity>& store);
};

