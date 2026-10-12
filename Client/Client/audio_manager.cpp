#include "audio_manager.h"

#include <fstream>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace {
	bool FileExists(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		return file.good();
	}
}

bool PlayMp3WithMci(const std::array<std::string, 4>& candidates, const char* label)
{
	for (const std::string& path : candidates) {
		if (!FileExists(path)) {
			continue;
		}

		mciSendStringA("close bgm", nullptr, 0, nullptr);
		const std::string openCommand = "open \"" + path + "\" type mpegvideo alias bgm";
		if (mciSendStringA(openCommand.c_str(), nullptr, 0, nullptr) != 0) {
			continue;
		}
		if (mciSendStringA("play bgm repeat", nullptr, 0, nullptr) != 0) {
			mciSendStringA("close bgm", nullptr, 0, nullptr);
			continue;
		}

		std::cout << "Loaded " << label << " with MCI: " << path << '\n';
		return true;
	}

	std::cout << "Could not load " << label << " with MCI.\n";
	return false;
}

void SetBackgroundMusicPaused(bool usingMciBackgroundMusic, bool shouldPause, bool& paused)
{
	if (!usingMciBackgroundMusic || paused == shouldPause) {
		return;
	}

	mciSendStringA(shouldPause ? "pause bgm" : "resume bgm", nullptr, 0, nullptr);
	paused = shouldPause;
}

void CloseBackgroundMusic()
{
	mciSendStringA("close bgm", nullptr, 0, nullptr);
}