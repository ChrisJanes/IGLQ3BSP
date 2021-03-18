#include "MD3Loader.h"
#include "physfs/physfs.h"

#include <vector>
#include <SOIL2/SOIL2.h>

int    LongSwap(int l)
{
	unsigned char    b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

Model MD3Loader::Load(std::string filename)
{
	PHYSFS_File* handle = PHYSFS_openRead(filename.c_str());

	MD3Header header;
	PHYSFS_readBytes(handle, &header, sizeof(MD3Header));

	std::vector<Surface> surfaces;

	PHYSFS_seek(handle, header.surfaces_offset);

	int offset = header.surfaces_offset;

	for (int i = 0; i < header.num_surfaces; ++i)
	{
		Surface surface;
		PHYSFS_readBytes(handle, &surface.header, sizeof(MD3SurfaceHeader));

		offset += surface.header.end_offset;

		if (surface.header.ident[0] != 'I' && surface.header.ident[1] != 'D'
			&& surface.header.ident[2] != 'P' && surface.header.ident[3] != '3') continue;

		PHYSFS_seek(handle, header.surfaces_offset + surface.header.triangles_offset);
		surface.triangles.resize(surface.header.num_triangles);
		PHYSFS_readBytes(handle, &surface.triangles[0], surface.header.num_triangles * sizeof(MD3Triangle));

		std::vector<MD3Shader> shaders;
		PHYSFS_seek(handle, header.surfaces_offset + surface.header.shaders_offset);
		shaders.resize(surface.header.num_shaders);
		PHYSFS_readBytes(handle, &shaders[0], surface.header.num_shaders * sizeof(MD3Shader));

		for (int j = 0; j < shaders.size(); j++)
		{
			surface.shaders.push_back(Shader{ shaders[j].name, shaders[j].index, -1 });
		}

		PHYSFS_seek(handle, header.surfaces_offset + surface.header.st_offset);
		surface.texcoords.resize(surface.header.num_verts);
		PHYSFS_readBytes(handle, &surface.texcoords[0], surface.header.num_verts * sizeof(MD3TexCoord));

		// we won't handle animation quite yet, so just one frames worth of verts
		PHYSFS_seek(handle, header.surfaces_offset + surface.header.vertex_offset);
		surface.vertices.resize(surface.header.num_verts);
		PHYSFS_readBytes(handle, &surface.vertices[0], surface.header.num_verts * sizeof(MD3Vertex));

		PHYSFS_seek(handle, offset);
		surfaces.push_back(surface);
	}

	PHYSFS_close(handle);

	return Model{ surfaces };
}

void Model::LoadSurfaceAssets()
{
	// load the textures for each surface.
	// load the texture file
	for (auto surface : surfaces)
	{
		for (auto shader : surface.shaders)
		{
			std::string file = shader.name;
			std::string path = "/data/" + file;

			auto handle = PHYSFS_openRead(path.c_str());
			if (handle != NULL)
			{
				int length = PHYSFS_fileLength(handle);

				unsigned char* tex_data = new unsigned char[length];

				PHYSFS_readBytes(handle, &tex_data[0], length);
				PHYSFS_close(handle);

				shader.texId = SOIL_load_OGL_texture_from_memory(tex_data, length, 3, 0, SOIL_FLAG_POWER_OF_TWO | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS);
				delete[] tex_data;
				tex_data = nullptr;
			}
		}
	}
}
