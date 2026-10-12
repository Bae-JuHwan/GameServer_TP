#pragma once

#include "server_platform.h"

#include <string>

void CopyText(char* destination, int destinationSize, const std::string& text);
std::string PacketText(const char* text, int maxSize);
bool SendAll(SOCKET socket, const void* data, int size);