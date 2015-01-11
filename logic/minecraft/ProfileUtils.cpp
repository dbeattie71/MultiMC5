#include "ProfileUtils.h"
#include "logic/minecraft/VersionFilterData.h"

namespace ProfileUtils
{
void removeLwjglFromPatch(VersionFilePtr patch)
{
	QList<RawLibraryPtr> filteredLibs;

	for (auto lib : patch->overwriteLibs)
	{
		if (!g_VersionFilterData.lwjglWhitelist.contains(lib->artifactPrefix()))
		{
			filteredLibs.append(lib);
		}
	}
	patch->overwriteLibs = filteredLibs;
}
}
