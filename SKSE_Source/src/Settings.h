#pragma once

#include <vector>
#include <string>

class Settings {
public:
    static Settings* GetSingleton();

    void Load();

    // Hotkeys
    std::vector<uint32_t> toggleFocusKeys;
    std::vector<uint32_t> toggleInspectorKeys;

    // UI Behaviour
    int idleTimeoutSec = 30;
    std::vector<std::string> trackedMenus;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;
};
