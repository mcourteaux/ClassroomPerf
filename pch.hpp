#pragma once

#define CXXOPTS_NO_REGEX true
#define STORE_LEADERBOARD 0
#include <httplib.h>
#if STORE_LEADERBOARD
#include <nlohmann/json.hpp>
#endif
#include <cxxopts.hpp>
#include <regex>
