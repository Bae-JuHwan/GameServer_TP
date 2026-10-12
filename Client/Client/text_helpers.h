#pragma once

#include <SFML/System/String.hpp>

#include <string>

std::string ToUtf8(const sf::String& text);
sf::String FromUtf8(const std::string& text);
bool CanAppendChatChar(const sf::String& currentText, sf::Uint32 unicode);
sf::String BeginnerZoneNotice();
sf::String IntermediateZoneNotice();
sf::String AdvancedZoneNotice();