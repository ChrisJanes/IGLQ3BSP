#pragma once

#include <string>
#include <glm/glm.hpp>

const int MAX_QPATH = 64;
const int MD3_MAX_FRAMES = 1024;
const int MD3_MAX_TAGS = 16;
const int MD3_MAX_SURFACES = 32;

class Model {

};

class MD3Vertex {
	short x;
	short y;
	short z;
	short normal;
};

class MD3TexCoord {
	glm::vec3 st;
};

class MD3Triangle {
	int indices[3];
};

class MD3Shader {
	char* name;
	int index;
};

class MD3Surface {
	int surface_offset;
	int ident;
	char* name;
	int flags;
	int num_frames;
	int num_shaders;
	int num_verts;
	int num_triangles;
	int triangles_offset;
	int shaders_offset;
	int st_offset;
	int vertex_offset;
	int end_offset;
};

class MD3Tag {
	char* name;
	glm::vec3 origin;
	glm::mat3x3 axis;
};

class MD3Frame {
	glm::vec3 min_bounds;
	glm::vec3 max_bounds;
	glm::vec3 local_origin;
	float radius;
	char* name;
};

class MD3Header {
	int md3_offset;
	int ident;
	int version;
	char* name;
	int flags;
	int num_frames;
	int num_tags;
	int num_surfaces;
	int num_skins;
	int frames_offset;
	int tags_offset;
	int surfaces_offset;
	int end_offset;
};

class MD3Loader {
	void Load(std::string filename);
};