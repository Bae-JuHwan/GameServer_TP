#include "asset_paths.h"

std::array<std::string, 4> AssetCandidates(const std::string& relativePath)
{
	return {
		"../../Asset/" + relativePath,
		"../../../Asset/" + relativePath,
		"Asset/" + relativePath,
		"../Asset/" + relativePath
	};
}