#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <iostream>

#include <GL\glew.h>
#include <glm\glm.hpp>

#include "physfs/physfs.h"
#include "MD3Loader.h"

// Q3 BSP format reference: http://www.mralligator.com/q3/

namespace FaceTypes {
	const int Polygon = 1;
	const int Patch = 2;
	const int Model = 3;
	const int Billboard = 4;
}

#pragma region SurfaceFlags
const int CONTENTS_SOLID = 0x1;
const int CONTENTS_LAVA = 0x8;
const int CONTENTS_SLIME = 0x10;
const int CONTENTS_WATER = 0x20;
const int CONTENTS_FOG = 0x40;
const int CONTENTS_NOTTEAM1 = 0x80;
const int CONTENTS_NOTTEAM2 = 0x100;
const int CONTENTS_NOBOTCLIP = 0x200;
const int CONTENTS_AREAPORTAL = 0x8000;
const int CONTENTS_PLAYERCLIP = 0x10000;
const int CONTENTS_MONSTERCLIP = 0x20000;
const int CONTENTS_TELEPORTER = 0x40000;
const int CONTENTS_JUMPPAD = 0x80000;
const int CONTENTS_CLUSTERPORTAL = 0x100000;
const int CONTENTS_DONOTENTER = 0x200000;
const int CONTENTS_BOTCLIP = 0x400000;
const int CONTENTS_MOVER = 0x800000;
const int CONTENTS_ORIGIN = 0x1000000;
const int CONTENTS_BODY = 0x2000000;
const int CONTENTS_CORPSE = 0x4000000;
const int CONTENTS_DETAIL = 0x8000000;
const int CONTENTS_STRUCTURAL = 0x10000000;
const int CONTENTS_TRANSLUCENT = 0x20000000;
const int CONTENTS_TRIGGER = 0x40000000;
const int CONTENTS_NODROP = 0x80000000;

const int SURF_NODAMAGE = 0x1;
const int SURF_SLICK = 0x2;
const int SURF_SKY = 0x4;
const int SURF_LADDER = 0x8;
const int SURF_NOIMPACT = 0x10;
const int SURF_NOMARKS = 0x20;
const int SURF_FLESH = 0x40;
const int SURF_NODRAW = 0x80;
const int SURF_HINT = 0x100;
const int SURF_SKIP = 0x200;
const int SURF_NOLIGHTMAP = 0x400;
const int SURF_POINTLIGHT = 0x800;
const int SURF_METALSTEPS = 0x1000;
const int SURF_NOSTEPS = 0x2000;
const int SURF_NONSOLID = 0x4000;
const int SURF_LIGHTFILTER = 0x8000;
const int SURF_ALPHASHADOW = 0x10000;
const int SURF_NODLIGHT = 0x20000;
const int SURF_SURFDUST = 0x40000;
#pragma endregion

#pragma region BSPStructs

typedef unsigned char ubyte;

struct visdata
{
	int n_vecs;
	int sz_vecs;
	std::vector<ubyte*> vecs;
};

struct lightvol
{
	ubyte ambient[3];
	ubyte directional[3];
	ubyte dir[2];
};

struct LightMap {
	GLuint id;
};

struct lightmap
{
	ubyte map[128*128*3];
};

struct face
{
	int texture;
	int effect;
	int type;
	int vertex;
	int n_vertexes;
	int meshvert;
	int n_meshverts;
	int lm_index;
	int lm_start[2];
	int lm_size[2];
	float lm_origin[3];
	float lm_vecs[2][3];
	float normal[3];
	int size[2];
};

struct effect
{
	char name[64];
	int brush;
	int unknown;
};

struct meshvert
{
	int offset;
};

struct vertex
{
	glm::vec3 position;
	glm::vec2 dtexcoord;
	glm::vec2 lmtexcoord;
	glm::vec3 normal;

	ubyte colour[4];
};

struct brushside
{
	int plane;
	int texture;
};

struct brush
{
	int brushside;
	int n_brushsides;
	int texture;
};

struct model
{
	float mins[3];
	float maxs[3];
	int face;
	int n_faces;
	int brush;
	int n_brushes;
};

struct leafbrush
{
	int brush;
};

struct leafface
{
	int face;
};

struct leaf
{
	int cluster;
	int area;
	int mins[3];
	int maxs[3];
	int leaffaces;
	int n_leaffaces;
	int leafbrush;
	int n_leafbrushes;
};

struct node
{
	int plane;
	int children[2];
	int mins[3];
	int maxs[3];
};

struct plane
{
	float normal[3];
	float dist;
};

struct texture
{
	char name[64];
	int flags;
	int contents;
};

struct shader
{
	bool transparent;
	bool solid;
	bool render;

	std::string name;
	GLuint id;
};

struct entities
{
	char* ents;
};

struct direntry
{
	int offset;
	int length;
};

struct Directory
{
	char magic[4];
	int version;
	direntry direntries[17];
};

#pragma endregion

class BSPLoader
{
public:
	BSPLoader(std::string filename, bool single) : file{filename}, single_draw{single}
	{
		load_file();
	}

	BSPLoader(bool single) : single_draw(single) {}

	void SetBSPFile(std::string filename)
	{
		file = filename;
		load_file();
	}

	std::vector<vertex> get_vertex_data() const { return file_vertices; }
	face get_face(int index) const { return file_faces[index]; }
	shader get_shader(int index) const { return shaders[index]; }
	GLuint get_lightmap_tex(int index) const { return lightmaps[index].id; }
	GLuint get_default_lightmap() const { return (GLuint)file_lightmaps.size(); }
	int get_face_count() const { return (int)file_faces.size(); }
	std::vector<unsigned int> get_indices();
	meshvert get_meshvert(int index) const { return file_meshverts[index]; }
	GLuint get_lm_id() const { return lmap_id; }

	bool is_loaded() const { return loaded; }
private:
	void get_lump_position(int index, int& offset, int& length);

	void build_indices();

	void process_textures();
	void process_lightmaps();

	void combine_lightmaps();
	void update_lm_coords();

	void clear_memory();

	void tesselate(int controlOffset, int controlWidth, int vOffset, int iOffset);
	void tesselate_patches();
	void load_models();

	GLuint lmap_id;
	std::vector<LightMap> lightmaps;
	std::vector<shader> shaders;

	template<class T>
	void read_lump(int index, std::vector<T>& storage, PHYSFS_File* handle);

	void load_file();
	std::string file;
	bool single_draw;

	int offset, length;

	bool loaded{ false };

	std::vector<unsigned int> indices;

	Directory file_directory;
	entities file_entities;
	int texture_count;
	std::vector < texture > file_textures;
	std::vector < plane > file_planes;
	std::vector < node > file_nodes;
	std::vector < leaf > file_leafs;
	std::vector < leafface > file_leaffaces;
	std::vector < leafbrush > file_leafbrushes;
	std::vector < model > file_models;
	std::vector < brush > file_brushes;
	std::vector < brushside > file_brushsides;
	std::vector < vertex > file_vertices;
	std::vector < meshvert > file_meshverts;
	std::vector < effect > file_effects;
	std::vector < face > file_faces;
	std::vector < lightmap > file_lightmaps;
	std::vector < lightvol > file_lightvols;
	visdata file_visdata;

	std::vector<Model> models;
};

// generic function to read lumps that are sizeof/length style.
template<class T>
inline void BSPLoader::read_lump(int index, std::vector<T> &storage, PHYSFS_File *handle)
{
	get_lump_position(index, offset, length);

	int count = length / sizeof(T);
	storage.resize(count);

	int _index = 0;

	if (length > 0)
	{
		PHYSFS_seek(handle, offset);
		PHYSFS_readBytes(handle, (char*)&storage[0], length);
	}
}
