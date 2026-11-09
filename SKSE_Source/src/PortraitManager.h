#pragma once
#include "PCH.h"
#include "OStimDataProvider.h"
#include "Phantasm_API.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class PortraitManager {
public:
    static PortraitManager* GetSingleton() {
        static PortraitManager singleton;
        return &singleton;
    }

    void Initialize();
    bool IsAvailable() const;

    // Clear cached portraits and cancel pending requests on save load / new game
    void OnGameLoad();

    // Notify that an OStim thread started so the initial settle timer resets
    void OnSceneChanged();

    // Ensure portraits are requested/synced for the current thread's actors
    void SyncActors(const std::vector<OStimDataProvider::ActorData>& actors, bool forceResendToUI = false);

private:
    struct CacheEntry {
        uint64_t equipSignature = 0;
        std::string dataUrl;
        bool classicPose = false;
        bool sentToUI = false;
        int classicRetries = 0;
        std::chrono::steady_clock::time_point capturedAt{};
    };

    struct PendingState {
        bool inFlight = false;
        uint32_t requestId = 0;
        uint64_t inFlightEquipSig = 0;
        bool isClassicRetry = false;

        bool hasObservedEquip = false;
        uint64_t observedEquipSig = 0;
        std::chrono::steady_clock::time_point equipStableSince{};

        int failCount = 0;
        std::chrono::steady_clock::time_point retryAfter{};
    };

    struct StillCallbackContext {
        PortraitManager* manager = nullptr;
        uint32_t formId = 0;
        uint64_t equipSignature = 0;
        uint64_t sessionGen = 0;
        bool isClassicRetry = false;
    };

    const PHANTASM_API::Interface1* phantasm = nullptr;
    PHANTASM_API::ClientId clientId = 0;
    std::atomic<uint64_t> sessionGeneration{1};

    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point sceneStartTime_{};
    std::unordered_map<uint32_t, CacheEntry> cache_;
    std::unordered_map<uint32_t, PendingState> pending_;

    // Background worker for linear-light downscaling + WIC PNG encoding
    std::mutex workerMutex_;
    std::condition_variable workerWake_;
    std::deque<std::function<void()>> workerJobs_;
    bool workerStarted_ = false;

    void PostWorkerJob(std::function<void()> job);
    static void WorkerLoop(PortraitManager* self);

    void RequestActorPortraitLocked(
        uint32_t formId,
        uint64_t equipSignature,
        bool isClassicRetry);

    void OnEncodedPortraitReady(
        uint32_t formId,
        uint64_t equipSignature,
        uint64_t sessionGen,
        bool classicPose,
        bool isClassicRetry,
        std::string dataUrl);

    static void OnPhantasmEvent(const PHANTASM_API::Event* event, void* user);
    static void OnStillDone(const PHANTASM_API::Still* still, void* user);

    PortraitManager() = default;
    ~PortraitManager() = default;
    PortraitManager(const PortraitManager&) = delete;
    PortraitManager& operator=(const PortraitManager&) = delete;
};
