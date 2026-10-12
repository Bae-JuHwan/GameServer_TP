#include "input_helpers.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
	std::string WideToUtf8(const std::wstring& text)
	{
		if (text.empty()) {
			return "";
		}

		const int utf8Size = WideCharToMultiByte(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			nullptr,
			0,
			nullptr,
			nullptr);
		if (utf8Size <= 0) {
			return "";
		}

		std::string utf8(utf8Size, '\0');
		WideCharToMultiByte(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			&utf8[0],
			utf8Size,
			nullptr,
			nullptr);
		return utf8;
	}

	std::string ReadConsoleLineUtf8()
	{
		HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
		DWORD consoleMode = 0;
		if (inputHandle != INVALID_HANDLE_VALUE && GetConsoleMode(inputHandle, &consoleMode)) {
			wchar_t buffer[256]{};
			DWORD charsRead = 0;
			if (ReadConsoleW(inputHandle, buffer, static_cast<DWORD>((sizeof(buffer) / sizeof(buffer[0])) - 1), &charsRead, nullptr)) {
				std::wstring input(buffer, charsRead);
				while (!input.empty() && (input.back() == L'\n' || input.back() == L'\r')) {
					input.pop_back();
				}
				return WideToUtf8(input);
			}
		}

		std::string input;
		std::getline(std::cin, input);
		return input;
	}

	bool IsValidIpv4Address(const std::string& address)
	{
		std::stringstream stream(address);
		std::string part;
		std::vector<std::string> parts;

		while (std::getline(stream, part, '.')) {
			parts.push_back(part);
		}

		if (parts.size() != 4) {
			return false;
		}

		for (const std::string& octet : parts) {
			if (octet.empty() || octet.size() > 3) {
				return false;
			}

			for (char ch : octet) {
				if (!std::isdigit(static_cast<unsigned char>(ch))) {
					return false;
				}
			}

			const int value = std::stoi(octet);
			if (value < 0 || value > 255) {
				return false;
			}
		}

		return true;
	}
}

std::string PromptIpAddress(const std::string& prompt)
{
	while (true) {
		std::cout << prompt << ": ";

		std::string input;
		std::getline(std::cin, input);
		if (IsValidIpv4Address(input)) {
			return input;
		}

		std::cout << "Invalid IP address. Please enter an IPv4 address like 127.0.0.1.\n";
	}
}

std::string PromptRequiredInput(const std::string& prompt)
{
	while (true) {
		std::cout << prompt << ": ";

		const std::string input = ReadConsoleLineUtf8();
		if (!input.empty()) {
			return input;
		}

		std::cout << prompt << " is required.\n";
	}
}