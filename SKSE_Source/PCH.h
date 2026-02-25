#pragma once

#define WIN32_LEAN_AND_MEAN

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <shared_mutex>
#include <set>
#include <vector>
#include <string>
#include <algorithm>

#include <nlohmann/json.hpp>

using namespace std::literals;
using json = nlohmann::json;
