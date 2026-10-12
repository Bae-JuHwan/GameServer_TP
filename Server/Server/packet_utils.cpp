#include "packet_utils.h"

#include <algorithm>
#include <cstring>

void CopyText(char* destination, int destinationSize, const std::string& text)
{
	std::memset(destination, 0, destinationSize);
	const int copySize = std::min(destinationSize - 1, static_cast<int>(text.size()));
	std::memcpy(destination, text.c_str(), copySize);
}

std::string PacketText(const char* text, int maxSize)
{
	int size = 0;
	while (size < maxSize && text[size] != '\0') {
		++size;
	}

	return std::string(text, size);
}

bool SendAll(SOCKET socket, const void* data, int size)
{
	const char* bytes = static_cast<const char*>(data);
	int sent = 0;

	while (sent < size) {
		const int result = send(socket, bytes + sent, size - sent, 0);
		if (result == SOCKET_ERROR || result == 0) {
			return false;
		}
		sent += result;
	}

	return true;
}