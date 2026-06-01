#pragma once

#include <string>

namespace thegame {

constexpr const char *kLogsTxt = "logs.txt";
constexpr const char *kNetlogsTxt = "netlogs.txt";
constexpr const char *kProudlogsTxt = "proudlogs.txt";

std::string game_directory();
std::string main_log_path();
std::string net_log_path();
std::string proud_log_path();

} // namespace thegame
