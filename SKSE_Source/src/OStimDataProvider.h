#pragma once
#include "PCH.h"
#include <map>
#include <mutex>

// Forward declare the API interface
namespace OstimNG_API::Thread {
    class IThreadInterface;
}

class OStimDataProvider {
public:
    static OStimDataProvider* GetSingleton() {
        static OStimDataProvider singleton;
        return &singleton;
    }

    void Initialize();
    bool IsConnected() const { return ostimAPI != nullptr; }

    // Get current thread ID
    uint32_t GetCurrentThreadID();

    // Key mappings
    struct KeyData
    {
        int keyUp;
        int keyDown;
        int keyLeft;
        int keyRight;
        int keyYes;
        int keyEnd;
        int keyToggle;
        int keySearch;
        int keyAlignment;
        int keySceneStart; // 10
        int keyNpcSceneStart;
        int keySpeedUp;
        int keySpeedDown;
        int keyPullOut;
        int keyAutoMode;
        int keyFreeCam;
        int keyHideUI;
    };
    
    // Get key mappings
    KeyData GetKeyData();

    // Actor data structure for UI
    struct ActorData {
        std::string name;
        float excitementProgress;
        float staminaProgress;
        std::string gender;
        int timesClimaxed = 0;
        float additionalProgress = 0.0f;
    };

    // Navigation option for UI
    struct NavOption {
        std::string id;
        std::string iconPath;
        std::string iconData;
        std::string description;
        std::string destination;
    };

    // Scene options menu item for UI
    struct SceneOptionItem {
        std::string id;
        std::string title;
        std::string icon;
        std::string iconData;
        std::string border;
        std::string description;
    };

    struct SearchResult {
        std::string sceneId;
        std::string name;
        uint32_t actorCount;
    };

    struct AlignmentData {
        float offsetX;
        float offsetY;
        float offsetZ;
        float scale;
        float rotation;
        float sosBend;
    };

    // Thread status (speed, control mode)
    struct ThreadStatusData {
        bool autoControl;    // Thread is in auto mode (player-toggled automatic progression)
        bool manualControl;  // Player has manual control
        bool locked;         // Player control is disabled
        int32_t currentSpeed;
        int32_t minSpeed;
        int32_t maxSpeed;
    };

    // Get actor excitement/stamina data
    std::vector<ActorData> GetActorExcitements(uint32_t threadID);

    // Get actor count in thread
    uint32_t GetActorCount(uint32_t threadID);

    // Get navigation options
    std::vector<NavOption> GetNavigationOptions(uint32_t threadID);

    // Get scene options menu items for current page
    std::vector<SceneOptionItem> GetSceneOptions();

    // Search scenes by name
    std::vector<SearchResult> SearchScenes(const std::string& query, uint32_t maxResults = 50);

    // Get actor alignment data
    bool GetActorAlignment(uint32_t threadID, uint32_t actorIndex, AlignmentData& outData);

    // Set actor alignment data
    bool SetActorAlignment(uint32_t threadID, uint32_t actorIndex, const AlignmentData& data);

    // Navigate to scene
    bool NavigateToScene(uint32_t threadID, const std::string& sceneID);

    // Navigate to a scene found via search
    bool NavigateToSearchResult(uint32_t threadID, const std::string& sceneID);

    // Get info about the current scene
    bool GetCurrentSceneInfo(uint32_t threadID, SearchResult& outInfo);

    // Select an option from the options menu
    bool SelectOptionsItem(int32_t index);

    // Get thread status (speed/control state)
    ThreadStatusData GetThreadStatus(uint32_t threadID);

    // Set animation speed
    bool SetSpeed(uint32_t threadID, int32_t speed);

    // Set additional progress for an actor (exposed to Papyrus)
    void SetActorAdditionalProgress(RE::FormID actorID, float progress);

private:
    OstimNG_API::Thread::IThreadInterface* ostimAPI = nullptr;
    
    // Map to store additional progress values set by Papyrus
    std::map<RE::FormID, float> additionalProgressMap;
    std::mutex mapMutex;

    OStimDataProvider() = default;
    ~OStimDataProvider() = default;
    OStimDataProvider(const OStimDataProvider&) = delete;
    OStimDataProvider& operator=(const OStimDataProvider&) = delete;
};
