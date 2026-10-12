#pragma once

#include <string>

std::string PromptInput(const std::string& prompt, const std::string& defaultValue);
std::string PromptIpAddress(const std::string& prompt);
std::string PromptRequiredInput(const std::string& prompt);