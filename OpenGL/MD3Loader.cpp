#include "MD3Loader.h"
#include "physfs/physfs.h"

void MD3Loader::Load(std::string filename)
{
	PHYSFS_File *handle = PHYSFS_openRead(filename.c_str());


}