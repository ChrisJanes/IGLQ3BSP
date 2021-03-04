#include "BSPLoader.h"

#include "physfs/physfs.h"
#include <SOIL2/SOIL2.h>

std::vector<unsigned int> BSPLoader::get_indices()
{
	return indices;
}

void BSPLoader::get_lump_position(int index, int& offset, int& length)
{
	offset = file_directory.direntries[index].offset;
	length = file_directory.direntries[index].length;
}

void BSPLoader::build_indices()
{
	// loop all the faces
	for (int i = 0; i < file_faces.size(); ++i)
	{
		auto& face = file_faces[i];

		// only handle polygon + mesh types at the moment, not patches or billboards.
		if (face.type == 1 || face.type == 3)
		{
			int ind = indices.size();

			// add to the list of indicies based on the meshvert data
			for (int j = 0; j < face.n_meshverts; ++j)
			{
				// meshIndexArray[face.meshIndexOffset + i] += face.vertexOffset;
				// meshvert list should translate directly into triangles.
				int vertIndex = face.meshvert + j;
				int index = face.vertex + file_meshverts[vertIndex].offset;

				indices.push_back(index);
			}

			face.meshvert = ind;
		}
	}
}

void BSPLoader::process_textures()
{
	for (int i = 0; i < file_textures.size(); i++)
	{
		texture texture = file_textures[i];

		shader _shader;
		_shader.render = true;
		_shader.solid = true;
		_shader.transparent = false;
		_shader.name = file_textures[i].name;
		if (texture.flags & SURF_NONSOLID) _shader.solid = false;
		if (texture.flags & SURF_SKY) _shader.render = false;
		if (texture.contents & CONTENTS_PLAYERCLIP) _shader.solid = true;
		if (texture.contents & CONTENTS_TRANSLUCENT) _shader.transparent = true;
		if (texture.contents & CONTENTS_LAVA  || texture.contents & CONTENTS_WATER ||
			texture.contents & CONTENTS_SLIME || texture.contents & CONTENTS_FOG )
				_shader.render = false;
		if (_shader.name == "noshader") _shader.render = false;
		if (_shader.name == "textures/common/caulk") _shader.render = false;

		if (_shader.render)
		{
			// load the texture file
			std::string file = texture.name;
			std::string path = "/data/" + file;

			if (PHYSFS_exists((path + ".tga").c_str()))
				path += ".tga";
			else if (PHYSFS_exists((path + ".jpg").c_str()))
				path += ".jpg";

			auto handle = PHYSFS_openRead(path.c_str());
			if (handle != NULL)
			{

				int length = PHYSFS_fileLength(handle);

				unsigned char* tex_data = new unsigned char[length];

				PHYSFS_readBytes(handle, &tex_data[0], length);
				PHYSFS_close(handle);

				_shader.id = SOIL_load_OGL_texture_from_memory(tex_data, length, 3, 0, SOIL_FLAG_POWER_OF_TWO | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS);
				delete[] tex_data;
				tex_data = nullptr;
			}
		}

		shaders.push_back(_shader);
	}
}

void BSPLoader::process_lightmaps()
{
	for (int i = 0; i < file_lightmaps.size(); ++i)
	{
		const int size = (128 * 128 * 3);
		LightMap map;
		glGenTextures(1, &map.id);
		glBindTexture(GL_TEXTURE_2D, map.id);
		GLenum format = GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, 128, 128, 0, format, GL_UNSIGNED_BYTE, file_lightmaps[i].map);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		lightmaps.push_back(map);
	}

	// generate a "grey" lightmap for faces with -1 lm index.
	ubyte* data = new ubyte[128 * 128 * 3];
	for (unsigned int i = 0; i < 128 * 128 * 3; ++i)
	{
		data[i] = (char)64;
	}

	LightMap map;
	glGenTextures(1, &map.id);
	glBindTexture(GL_TEXTURE_2D, map.id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	lightmaps.push_back(map);

	combine_lightmaps();
}

void BSPLoader::combine_lightmaps()
{
	// get how many lightmaps there are
	int map_count = file_lightmaps.size();

	if (map_count >= 2)
	{
		int width = 128 * map_count;
		long size = width * 128 * 3;
		ubyte* target = new ubyte[size];

		int targetX = 0;
		for (int i = 0; i < map_count; ++i)
		{
			ubyte* source = file_lightmaps[i].map;

			for (int sourceY = 0; sourceY < 128; ++sourceY) {
				for (int sourceX = 0; sourceX < 128; ++sourceX) {
					int from = (sourceY * 128 * 3) + (sourceX * 3); // 3 bytes per pixel (assuming RGB)
					int to = ((sourceY)*width * 3) + ((targetX + sourceX) * 3); // same format as source

					for (int channel = 0; channel < 3; ++channel) {
						target[to + channel] = source[from + channel];
					}
				}
			}
			targetX += 128;
		}

		glGenTextures(1, &lmap_id);
		glBindTexture(GL_TEXTURE_2D, lmap_id);
		GLenum format = GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, 128, 0, format, GL_UNSIGNED_BYTE, target);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (single_draw)
			update_lm_coords();
	}
}

void BSPLoader::update_lm_coords()
{
	int lm_count = file_lightmaps.size();
	// loop the faces
	// for each face, loop the verts
	// re-scale the lm u coord to a new 0 - 1 range based on the lm index, v stays the same.
	for (int i = 0; i < file_faces.size(); ++i)
	{
		face _face = file_faces[i];
		for (int j = 0; j < _face.n_meshverts; ++j)
		{
			// meshvert list should translate directly into triangles.
			if (_face.lm_index < 0) _face.lm_index = lm_count;
			int vertIndex = _face.meshvert + j;
			int index = indices[vertIndex];
			float coord = file_vertices[index].lmtexcoord[0];

			// rescale u coord to fit the atlas.
			coord = (coord + _face.lm_index) / lm_count;

			file_vertices[index].lmtexcoord[0] = coord;
		}
	}
}

void BSPLoader::load_file()
{
	// open saved file for reading as binary
	std::ifstream fs{ file, std::fstream::in | std::fstream::binary };

	// read directory block
	fs.read( (char*)&file_directory, sizeof(Directory));

	// then read each of the data lumps in "order"
	get_lump_position(0, offset, length);

	file_entities.ents = new char[length];

	fs.seekg(offset);
	fs.read(file_entities.ents, length);

	int lightmapCount = file_directory.direntries[14].length / sizeof(lightmap);

	// 1 to 15 are array based lumps
	read_lump<texture>(1, file_textures, fs);
	read_lump<plane>(2, file_planes, fs);
	read_lump<node>(3, file_nodes, fs);
	read_lump<leaf>(4, file_leafs, fs);
	read_lump<leafface>(5, file_leaffaces, fs);
	read_lump<leafbrush>(6, file_leafbrushes, fs);
	read_lump<model>(7, file_models, fs);
	read_lump<brush>(8, file_brushes, fs);
	read_lump<brushside>(9, file_brushsides, fs);
	read_lump<vertex>(10, file_vertices, fs);
	read_lump<meshvert>(11, file_meshverts, fs);
	read_lump<effect>(12, file_effects, fs);
	read_lump<face>(13, file_faces, fs);
	read_lump<lightmap>(14, file_lightmaps, fs);
	read_lump<lightvol>(15, file_lightvols, fs);

	// 16 is vis data
	get_lump_position(16, offset, length);

	fs.seekg(offset, std::ios_base::beg);
	fs.read((char*)&file_visdata, sizeof(int) * 2);
	int nvecs = file_visdata.n_vecs;
	int sz_vecs = file_visdata.sz_vecs;
	int sz = nvecs * sz_vecs;
	file_visdata.vecs.resize(sz);
	fs.read((char*)&file_visdata.vecs[0], sz);
	fs.close();

	build_indices();
	process_textures();
	process_lightmaps();
}

