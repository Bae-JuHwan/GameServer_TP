#include "text_helpers.h"

#include <iterator>

#include "protocol_2026.h"

std::string ToUtf8(const sf::String& text)
{
	const std::basic_string<sf::Uint8> utf8 = text.toUtf8();
	return std::string(utf8.begin(), utf8.end());
}

sf::String FromUtf8(const std::string& text)
{
	return sf::String::fromUtf8(text.begin(), text.end());
}

bool CanAppendChatChar(const sf::String& currentText, sf::Uint32 unicode)
{
	sf::String nextText = currentText;
	nextText += unicode;
	return ToUtf8(nextText).size() < MAX_CHAT_MSG_LEN;
}

sf::String BeginnerZoneNotice()
{
	static constexpr sf::Uint32 text[] = {
		0xCD08, 0xBCF4, 0xC790, 0x0020, 0xB9C8, 0xC744,
		0x0028, 0x0031, 0x007E, 0x0039, 0xB819, 0x0020,
		0xAD8C, 0xC7A5, 0x0029
	};
	return sf::String::fromUtf32(std::begin(text), std::end(text));
}

sf::String IntermediateZoneNotice()
{
	static constexpr sf::Uint32 text[] = {
		0xC911, 0xAE09, 0xC790, 0x0020, 0xB9C8, 0xC744,
		0x0028, 0x0031, 0x0030, 0x007E, 0x0031, 0x0039,
		0xB819, 0x0020, 0xAD8C, 0xC7A5, 0x0029
	};
	return sf::String::fromUtf32(std::begin(text), std::end(text));
}

sf::String AdvancedZoneNotice()
{
	static constexpr sf::Uint32 text[] = {
		0xC0C1, 0xAE09, 0xC790, 0x0020, 0xB9C8, 0xC744,
		0x0028, 0x0032, 0x0030, 0x007E, 0x0032, 0x0039,
		0xB819, 0x0020, 0xAD8C, 0xC7A5, 0x0029
	};
	return sf::String::fromUtf32(std::begin(text), std::end(text));
}