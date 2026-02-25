#include "OStimDataProvider.h"
#include <algorithm>
#include <cmath>
#include "PrismaUIManager.h"
#include "OstimNG-API-Thread.h"
#include "UIUtil.h"

void OStimDataProvider::Initialize() {
    ostimAPI = OstimNG_API::Thread::GetAPI("OStim Prism", REL::Version(1, 0, 0));

    if (ostimAPI) {
        SKSE::log::info("OStim Thread API connected successfully");
    } else {
        SKSE::log::warn("OStim Thread API not available - OStim may not be loaded");
    }
}

uint32_t OStimDataProvider::GetCurrentThreadID() {
    if (!ostimAPI) return PrismaUIManager::INVALID_THREAD_ID;
    return ostimAPI->GetPlayerThreadID();
}

OStimDataProvider::KeyData OStimDataProvider::GetKeyData() {
    KeyData result{};
    if (ostimAPI) {
        OstimNG_API::Thread::KeyData keys;
        ostimAPI->GetKeyData(&keys);
        
        result.keyUp = keys.keyUp;
        result.keyDown = keys.keyDown;
        result.keyLeft = keys.keyLeft;
        result.keyRight = keys.keyRight;
        result.keyYes = keys.keyYes;
        result.keyEnd = keys.keyEnd;
        result.keyToggle = keys.keyToggle;
        result.keySearch = keys.keySearch;
        result.keyAlignment = keys.keyAlignment;
        result.keySceneStart = keys.keySceneStart;
        result.keyNpcSceneStart = keys.keyNpcSceneStart;
        result.keySpeedUp = keys.keySpeedUp;
        result.keySpeedDown = keys.keySpeedDown;
        result.keyPullOut = keys.keyPullOut;
        result.keyAutoMode = keys.keyAutoMode;
        result.keyFreeCam = keys.keyFreeCam;
        result.keyHideUI = keys.keyHideUI;
    }
    return result;
}

std::vector<OStimDataProvider::ActorData> OStimDataProvider::GetActorExcitements(uint32_t threadID) {
    std::vector<ActorData> result;
    
    if (!ostimAPI) return result;

    constexpr uint32_t MAX_ACTORS = 10;
    OstimNG_API::Thread::ActorData buffer[MAX_ACTORS];

    uint32_t count = ostimAPI->GetActors(threadID, buffer, MAX_ACTORS);

    for (uint32_t i = 0; i < count; ++i) {
        // Get actor form from FormID
        auto* actorForm = RE::TESForm::LookupByID<RE::Actor>(buffer[i].formID);
        if (!actorForm) continue;

        ActorData data;
        
        data.name = actorForm->GetDisplayFullName();
        data.excitementProgress = buffer[i].excitement;
        data.timesClimaxed = buffer[i].timesClimaxed;
        
        // Calculate stamina percentage from actor
        auto* actorValueOwner = actorForm->AsActorValueOwner();
        if (actorValueOwner) {
            float currentStamina = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
            float maxStamina = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
            data.staminaProgress = maxStamina > 0 ? (currentStamina / maxStamina) * 100.0f : 0.0f;
        } else {
            data.staminaProgress = 0.0f;
        }
        
        // Determine gender string
        if (buffer[i].isFemale) {
            data.gender = buffer[i].hasSchlong ? "neither" : "female";
        } else {
            data.gender = "male";
        }
        
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            auto it = additionalProgressMap.find(buffer[i].formID);
            if (it != additionalProgressMap.end()) {
                data.additionalProgress = it->second;
            } else {
                data.additionalProgress = -1.0f;
            }
        }
        
        result.push_back(std::move(data));
    }

    return result;
}

uint32_t OStimDataProvider::GetActorCount(uint32_t threadID) {
    if (!ostimAPI) return 0;
    return ostimAPI->GetActorCount(threadID);
}

std::vector<OStimDataProvider::NavOption> OStimDataProvider::GetNavigationOptions(uint32_t threadID) {
    std::vector<NavOption> result;

    if (!ostimAPI) return result;

    constexpr uint32_t MAX_OPTIONS = 50;
    OstimNG_API::Thread::NavigationData buffer[MAX_OPTIONS];

    uint32_t count = ostimAPI->GetNavigationOptions(threadID, buffer, MAX_OPTIONS);

    for (uint32_t i = 0; i < count; ++i) {
        NavOption option;
        
        option.id = buffer[i].sceneId;
        option.iconPath = buffer[i].icon;
        option.description = buffer[i].description;
        option.destination = option.id;
        
        // Convert icon path to base64 for PrismaUI
        option.iconData = UIUtil::getIconBase64(option.iconPath);

        result.push_back(std::move(option));
    }

    return result;
}

std::vector<OStimDataProvider::SceneOptionItem> OStimDataProvider::GetSceneOptions() {
    std::vector<SceneOptionItem> result;

    if (!ostimAPI) return result;

    constexpr uint32_t MAX_ITEMS = 50;
    OstimNG_API::Thread::OptionsMenuItem buffer[MAX_ITEMS];

    uint32_t count = ostimAPI->GetOptionsItems(buffer, MAX_ITEMS);

    for (uint32_t i = 0; i < count; ++i) {
        SceneOptionItem item;

        item.id = buffer[i].id;
        item.title = buffer[i].title;
        item.icon = buffer[i].icon;
        item.border = buffer[i].border;
        item.description = buffer[i].description;
        item.iconData = UIUtil::getIconBase64(item.icon);

        result.push_back(std::move(item));
    }

    return result;
}

std::vector<OStimDataProvider::SearchResult> OStimDataProvider::SearchScenes(const std::string& query, uint32_t maxResults) {
    std::vector<SearchResult> result;

    if (!ostimAPI || query.empty() || maxResults == 0) {
        return result;
    }

    const uint32_t cappedMax = std::min<uint32_t>(maxResults, 999);
    std::vector<OstimNG_API::Thread::SceneSearchResult> buffer(cappedMax);

    uint32_t count = ostimAPI->SearchScenes(query.c_str(), buffer.data(), cappedMax);
    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        SearchResult item;
        item.sceneId = buffer[i].sceneId;
        item.name = buffer[i].name;
        item.actorCount = buffer[i].actorCount;
        result.push_back(std::move(item));
    }

    return result;
}

bool OStimDataProvider::GetActorAlignment(uint32_t threadID, uint32_t actorIndex, AlignmentData& outData) {
    if (!ostimAPI) return false;

    OstimNG_API::Thread::ActorAlignmentData data;
    if (!ostimAPI->GetActorAlignment(threadID, actorIndex, &data)) {
        return false;
    }

    outData.offsetX = data.offsetX;
    outData.offsetY = data.offsetY;
    outData.offsetZ = data.offsetZ;
    outData.scale = data.scale;
    outData.rotation = data.rotation;
    outData.sosBend = data.sosBend;
    return true;
}

bool OStimDataProvider::SetActorAlignment(uint32_t threadID, uint32_t actorIndex, const AlignmentData& data) {
    if (!ostimAPI) return false;

    OstimNG_API::Thread::ActorAlignmentData apiData{};
    apiData.offsetX = data.offsetX;
    apiData.offsetY = data.offsetY;
    apiData.offsetZ = data.offsetZ;
    apiData.scale = data.scale;
    apiData.rotation = data.rotation;
    apiData.sosBend = data.sosBend;

    auto result = ostimAPI->SetActorAlignment(threadID, actorIndex, &apiData);
    return result == OstimNG_API::Thread::APIResult::OK;
}

bool OStimDataProvider::NavigateToScene(uint32_t threadID, const std::string& sceneID) {
    if (!ostimAPI) return false;
    auto result = ostimAPI->NavigateToScene(threadID, sceneID.c_str());
    return result == OstimNG_API::Thread::APIResult::OK;
}

bool OStimDataProvider::NavigateToSearchResult(uint32_t threadID, const std::string& sceneID) {
    if (!ostimAPI) return false;
    auto result = ostimAPI->NavigateToSearchResult(threadID, sceneID.c_str());
    return result == OstimNG_API::Thread::APIResult::OK;
}

bool OStimDataProvider::GetCurrentSceneInfo(uint32_t threadID, SearchResult& outInfo) {
    if (!ostimAPI) return false;
    
    auto sceneID = ostimAPI->GetCurrentSceneID(threadID);
    if (!sceneID) return false;

    OstimNG_API::Thread::SceneSearchResult apiInfo{};
    if (ostimAPI->GetSceneInfo(sceneID, &apiInfo)) {
        outInfo.sceneId = apiInfo.sceneId;
        outInfo.name = apiInfo.name;
        outInfo.actorCount = apiInfo.actorCount;
        return true;
    }
    return false;
}

bool OStimDataProvider::SelectOptionsItem(int32_t index) {
    if (!ostimAPI) return false;
    return ostimAPI->SelectOptionsItem(index);
}

OStimDataProvider::ThreadStatusData OStimDataProvider::GetThreadStatus(uint32_t threadID) {
    ThreadStatusData result{};
    result.minSpeed = 1;
    result.maxSpeed = 1;
    result.currentSpeed = 1;
    result.autoControl = false;
    result.manualControl = false;
    result.locked = false;

    if (!ostimAPI) return result;

    result.currentSpeed = ostimAPI->GetCurrentSpeed(threadID);
    result.maxSpeed = ostimAPI->GetMaxSpeed(threadID);
    if (result.maxSpeed < 1) result.maxSpeed = 1;
    if (result.currentSpeed < 1) result.currentSpeed = 1;
    result.minSpeed = 1; // OStim animation speeds start at 1

    result.locked = ostimAPI->IsPlayerControlDisabled(threadID);
    result.autoControl = ostimAPI->IsAutoMode(threadID);
    result.manualControl = !result.autoControl && !result.locked;

    return result;
}

bool OStimDataProvider::SetSpeed(uint32_t threadID, int32_t speed) {
    if (!ostimAPI) return false;
    return ostimAPI->SetSpeed(threadID, speed) == OstimNG_API::Thread::APIResult::OK;
}

void OStimDataProvider::SetActorAdditionalProgress(RE::FormID actorID, float progress) {
    if (!actorID) return;
    std::lock_guard<std::mutex> lock(mapMutex);

    SKSE::log::info("Setting additional progress for actor {} to {}", actorID, progress);
    
    // If progress is effectively zero, remove from map to save space
    if (std::abs(progress) < 0.001f) {
        additionalProgressMap.erase(actorID);
    } else {
        additionalProgressMap[actorID] = progress;
    }
}
