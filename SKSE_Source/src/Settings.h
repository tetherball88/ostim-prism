#pragma once

#include <vector>
#include <string>

class Settings {
public:
    static Settings* GetSingleton();

    void Load();
    void SaveLayout(int mainX, int mainY, int mainScale, int exciteX, int exciteY, int exciteScale, int cols, int rows, bool enableGrid, bool enablePortraitsVal);

    // Hotkeys
    std::vector<uint32_t> toggleFocusKeys;
    std::vector<uint32_t> toggleInspectorKeys;

    // UI Behaviour
    int idleTimeoutSec = 30;
    bool enablePortraits = true;
    bool enableGridMenu = true;
    int gridColumns = 4;
    int gridRows = 4;
    int mainBlockX = -1;
    int mainBlockY = -1;
    int mainBlockScale = 100;
    int excitementBlockX = -1;
    int excitementBlockY = -1;
    int excitementBlockScale = 100;
    std::vector<std::string> trackedMenus;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;
};
