#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <string>

bool LoadTextureFromCandidates(sf::Texture& texture, const std::array<std::string, 4>& paths, const char* name);
bool LoadFontFromCandidates(sf::Font& font, const std::array<std::string, 4>& paths, const char* name);