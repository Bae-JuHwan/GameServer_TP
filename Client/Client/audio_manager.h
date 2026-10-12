#pragma once

#include <array>
#include <string>

bool PlayMp3WithMci(const std::array<std::string, 4>& candidates, const char* label);
void SetBackgroundMusicPaused(bool usingMciBackgroundMusic, bool shouldPause, bool& paused);
void CloseBackgroundMusic();