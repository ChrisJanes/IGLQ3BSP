#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

const int MAX_QPATH = 64;
const int MD3_MAX_FRAMES = 1024;
const int MD3_MAX_TAGS = 16;
const int MD3_MAX_SURFACES = 32;

struct MD3Vertex {
	glm::u8vec3 vert;
	short normal;
};

struct MD3TexCoord {
	glm::vec2 st;
};

struct MD3Triangle {
	int indexes[3];
};

struct MD3Shader {
	char name[MAX_QPATH];
	int index;
};
	
struct MD3SurfaceHeader {
	//int surface_offset;
	char ident[4];
	char name[MAX_QPATH];
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

struct Shader {
	std::string name;
	int index;
	int texId;
};

struct Surface {
	MD3SurfaceHeader header;
	std::vector<Shader> shaders;
	std::vector<MD3Triangle> triangles;
	std::vector<MD3TexCoord> texcoords;
	std::vector<MD3Vertex> vertices;
};

struct MD3Tag {
	char name[MAX_QPATH];
	glm::vec3 origin;
	glm::mat3x3 axis;
};

struct MD3Frame {
	glm::vec3 min_bounds;
	glm::vec3 max_bounds;
	glm::vec3 local_origin;
	float radius;
	char name[MAX_QPATH];
};

struct MD3Header {
	//int md3_offset;
	char ident[4];
	int version;
	char name[MAX_QPATH];
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

class Model {
public:
	Model() {};
	Model(std::vector<Surface> surfs) : surfaces{ surfs } { }
	void LoadSurfaceAssets();
	std::vector<Surface> GetSurfaces() { return surfaces; }

private:
	std::vector<Surface> surfaces;
};

class MD3Loader {
public:
	static bool Load(Model& model, std::string filename);
};