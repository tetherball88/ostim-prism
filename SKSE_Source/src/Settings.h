#pragma once

#include <vector>

class Settings {
public:
    static Settings* GetSingleton();

    void Load();

    // Hotkeys
    std::vector<uint32_t> toggleFocusKeys;
    std::vector<uint32_t> toggleInspectorKeys;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;
};
