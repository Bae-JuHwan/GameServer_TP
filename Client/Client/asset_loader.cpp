#include "asset_loader.h"
#include <iostream>

bool LoadTextureFromCandidates(sf::Texture& texture, const std::array<std::string, 4>& paths, const char* name)
{
	for (const std::string& path : paths) {
		if (path.empty()) {
			continue;
		}
		if (texture.loadFromFile(path)) {
			return true;
		}
	}

	return false;
}

bool LoadFontFromCandidates(sf::Font& font, const std::array<std::string, 4>& paths, const char* name)
{
	for (const std::string& path : paths) {
		if (path.empty()) {
			continue;
		}
		if (font.loadFromFile(path)) {
			return true;
		}
	}

	return false;
}