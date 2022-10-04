#include "BSPLoader.h"

#include <SOIL2/SOIL2.h>

#include "EntityParser.h"
#include "MD3Loader.h"

#define _USE_MATH_DEFINES
#include <math.h>

constexpr auto MD3_XYZ_SCALE = (1.0/64);
const int bezierLevel = 10;

// addition operator for vertices
vertex operator+(const vertex& v1, const vertex& v2)
{
	return vertex{ 
		v1.position + v2.position,	   
		v1.dtexcoord + v2.dtexcoord,	   
		v1.lmtexcoord + v2.lmtexcoord,	   
		v1.normal + v2.normal
	};
}

vertex operator*(const vertex& v1, const float& d)
{
	return vertex{ 
		v1.position * d, 
		v1.dtexcoord * d, 
		v1.lmtexcoord * d, 
		v1.normal * d 
	};
}

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
		if (face.type == FaceTypes::Polygon || face.type == FaceTypes::Model)
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
		/*if (texture.contents & CONTENTS_LAVA  || texture.contents & CONTENTS_WATER ||
			texture.contents & CONTENTS_SLIME || texture.contents & CONTENTS_FOG )
				_shader.render = false;
		if (_shader.name == "noshader") _shader.render = false;*/
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

void BSPLoader::clear_memory()
{
	for (auto shader : shaders)
	{
		glDeleteTextures(1, &shader.id);
	}

	for (auto lm : lightmaps)
	{
		glDeleteTextures(1, &lm.id);
	}

	shaders.resize(0);
	lightmaps.resize(0);
	indices.resize(0);
}

void BSPLoader::load_models()
{
	// parse the entity lump
	EntityParser parser;

	std::vector<entity> entities;
	parser.parse(file_entities.ents, entities);

	// store the data in the models vector.
	for (int i = 0; i < entities.size(); ++i)
	{
		// pull out any misc_model entries
		if (entities[i].get_string("classname") != "misc_model") continue;
	
		// load the relevant md3
		std::string filename = entities[i].get_string("model");

		if (filename == "") continue;

		std::string path = "data/" + filename;

		Model model; 
		bool loaded = MD3Loader::Load(model, path);

		if (!loaded) continue;

		model.LoadSurfaceAssets();

		float angle = entities[i].get_float("angle");
		glm::vec3 origin;
		entities[i].get_vec3("origin", origin);
		angle = angle / 180 * M_PI;
		float angleCos = cos(angle);
		float angleSin = sin(angle);

		std::vector<Surface> md3surfaces = model.GetSurfaces();
		for (auto surface : md3surfaces)
		{
			// convert the models surfaces into bsp style faces...
			face _face;

			_face.texture = file_textures.size();

			for (auto shade : surface.shaders)
			{
				texture tex;
				shade.name.copy(tex.name, 64);
				tex.name[shade.name.size()] = '\0';
				tex.flags = 0;
				tex.contents = 0;
				file_textures.push_back(tex);
			}

			_face.type = 3;
			_face.effect = -1;
			_face.lm_index = -1;
			_face.n_vertexes = surface.vertices.size();
			_face.vertex = file_vertices.size();

			_face.n_meshverts = surface.triangles.size() * 3;
			_face.meshvert = file_meshverts.size();

			for (int i = 0; i < surface.triangles.size(); ++i)
			{
				meshvert vertA { surface.triangles[i].indexes[0] };
				meshvert vertB { surface.triangles[i].indexes[1] };
				meshvert vertC { surface.triangles[i].indexes[2] };
				file_meshverts.push_back(vertA);
				file_meshverts.push_back(vertB);
				file_meshverts.push_back(vertC);
			}

			for (int i = 0; i < _face.n_vertexes; ++i)
			{
				auto surfvert = surface.vertices[i];
				vertex vert;
				vert.colour[0] = 255;
				vert.colour[1] = 255;
				vert.colour[2] = 255;
				vert.colour[3] = 255;

				vert.dtexcoord[0] = surface.texcoords[i].st.r;
				vert.dtexcoord[1] = surface.texcoords[i].st.g;

				vert.lmtexcoord[0] = 0;
				vert.lmtexcoord[1] = 0;

				vert.position[0] = origin.x + MD3_XYZ_SCALE * (surfvert.vert.x * angleCos - surfvert.vert.y * angleSin);
				vert.position[1] = origin.y + MD3_XYZ_SCALE * (surfvert.vert.x * angleSin + surfvert.vert.y * angleCos);
				vert.position[2] = origin.z + MD3_XYZ_SCALE * (surfvert.vert.z);

				float lat = (surfvert.normal >> 8) & 0xff;
				float lng = (surfvert.normal & 0xff);
				lat *= M_PI / 128;
				lng *= M_PI / 128;

				glm::vec3 temp;
				temp.x = cos(lat) * sin(lng);
				temp.y = sin(lat) * sin(lng);
				temp.z = cos(lng);

				vert.normal[0] = temp.x * angleCos - temp.y * angleSin;
				vert.normal[1] = temp.x * angleSin + temp.y * angleCos;
				vert.normal[2] = temp.z;

				file_vertices.push_back(vert);
			}

			file_faces.push_back(_face);
		}
		models.push_back(model);
	}
}

void BSPLoader::tesselate(int controlOffset, int controlWidth, int vOffset, int iOffset)
{
	vertex controls[9];
	int cIndex = 0;
	for (int c = 0; c < 3; ++c)
	{
		int pos = c * controlWidth;
		controls[cIndex++] = file_vertices[controlOffset + pos];
		controls[cIndex++] = file_vertices[controlOffset + pos + 1];
		controls[cIndex++] = file_vertices[controlOffset + pos + 2];
	}

	int L1 = bezierLevel + 1;

	for (int j = 0; j <= bezierLevel; ++j)
	{
		float a = (float)j / bezierLevel;
		float b = 1.f - a;
		file_vertices[vOffset + j] = controls[0] * b * b + controls[3] * 2 * b * a + controls[6] * a * a;
	}

	for (int i = 1; i <= bezierLevel; ++i)
	{
		float a = (float)i / bezierLevel;
		float b = 1.f - a;

		vertex temp[3];

		for (int j = 0; j < 3; ++j)
		{
			int k = 3 * j;
			temp[j] = controls[k + 0] * b * b + controls[k + 1] * 2 * b * a + controls[k + 2] * a * a;
		}

		for (int j = 0; j <= bezierLevel; ++j)
		{
			float a = (float)j / bezierLevel;
			float b = 1.f - a;

			file_vertices[vOffset + i * L1 + j] = temp[0] * b * b + temp[1] * 2 * b * a + temp[2] * a * a;
		}
	}

	for (int i = 0; i <= bezierLevel; ++i)
	{
		for (int j = 0; j <= bezierLevel; ++j)
		{
			int offset = iOffset + (i * bezierLevel + j) * 6;
			//if(offset+5 >= indices.size()) break;
			indices[offset + 0] = (i    ) * L1 + (j    ) + vOffset;
			indices[offset + 1] = (i    ) * L1 + (j + 1) + vOffset;
			indices[offset + 2] = (i + 1) * L1 + (j + 1) + vOffset;
			indices[offset + 3] = (i + 1) * L1 + (j + 1) + vOffset;
			indices[offset + 4] = (i + 1) * L1 + (j    ) + vOffset;
			indices[offset + 5] = (i    ) * L1 + (j    ) + vOffset;
		}
	}
}

void BSPLoader::tesselate_patches()
{
	int bezierCount = 0;
	int bezierPatchSize = (bezierLevel + 1) * (bezierLevel + 1);
	int bezierIndexSize = bezierLevel * bezierLevel * 6;

	for (auto& face : file_faces)
	{
		if (face.type != FaceTypes::Patch) continue;
		int dimX = (face.size[0] - 1) / 2;
		int dimY = (face.size[1] - 1) / 2;
		int size = dimX * dimY;
		bezierCount += size;
	}

	int iOffset = indices.size();
	int vOffset = file_vertices.size();

	int meshIndexCount = file_directory.direntries[11].length / sizeof(int);
	int newICount = meshIndexCount + bezierCount * bezierIndexSize;

	file_vertices.resize(file_vertices.size() + (bezierPatchSize * bezierCount));
	indices.resize(newICount);

	for (auto& face : file_faces)
	{
		if (face.type != FaceTypes::Patch) continue;
		int dimX = (face.size[0] - 1) / 2;
		int dimY = (face.size[1] - 1) / 2;

		face.meshvert = iOffset;

		for (int x = 0, n = 0; n < dimX; n++, x = 2 * n)
		{
			for (int y = 0, m = 0; m < dimY; m++, y = 2 * m)
			{
				tesselate(face.vertex + x + face.size[0] * y, face.size[0], vOffset, iOffset);
				vOffset += bezierPatchSize;
				iOffset += bezierIndexSize;
			}
		}

		face.n_meshverts = iOffset - face.meshvert;
	}
}

void BSPLoader::load_file()
{
	if (loaded)
	{
		clear_memory();
	}

	// open saved file for reading as binary
	PHYSFS_File *handle = PHYSFS_openRead(file.c_str());

	if (handle == NULL)
	{ 
		std::cout << "BSPLoader error: " << PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) << '\n';
		return;
	}

	// read directory block
	PHYSFS_readBytes(handle, (char*)&file_directory, sizeof(Directory));

	// then read each of the data lumps in "order"
	get_lump_position(0, offset, length);

	file_entities.ents = new char[length];

	PHYSFS_seek(handle, offset);
	PHYSFS_readBytes(handle, file_entities.ents, length);
	//fs.read(file_entities.ents, length);

	int lightmapCount = file_directory.direntries[14].length / sizeof(lightmap);

	// 1 to 15 are array based lumps
	read_lump<texture>(1, file_textures, handle);
	read_lump<plane>(2, file_planes, handle);
	read_lump<node>(3, file_nodes, handle);
	read_lump<leaf>(4, file_leafs, handle);
	read_lump<leafface>(5, file_leaffaces, handle);
	read_lump<leafbrush>(6, file_leafbrushes, handle);
	read_lump<model>(7, file_models, handle);
	read_lump<brush>(8, file_brushes, handle);
	read_lump<brushside>(9, file_brushsides, handle);
	read_lump<vertex>(10, file_vertices, handle);
	read_lump<meshvert>(11, file_meshverts, handle);
	read_lump<effect>(12, file_effects, handle);
	read_lump<face>(13, file_faces, handle);
	read_lump<lightmap>(14, file_lightmaps, handle);
	read_lump<lightvol>(15, file_lightvols, handle);

	// 16 is vis data
	get_lump_position(16, offset, length);

	PHYSFS_seek(handle, offset);
	
	PHYSFS_readBytes(handle, (char*)&file_visdata, sizeof(int) * 2);
	int nvecs = file_visdata.n_vecs;
	int sz_vecs = file_visdata.sz_vecs;
	int sz = nvecs * sz_vecs;
	file_visdata.vecs.resize(sz);
	if (sz > 0)
		PHYSFS_readBytes(handle, (char*)&file_visdata.vecs[0], sz);
	
	PHYSFS_close(handle);

	load_models();
	build_indices();
	tesselate_patches();
	process_textures();
	process_lightmaps();	

	loaded = true;
}

