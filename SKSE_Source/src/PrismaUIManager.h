#pragma once
#include "PCH.h"
#include "MagelightUI_API.h"
#include "OStimDataProvider.h"
#include "OstimNG-API-Thread.h"
#include <atomic>
#include <chrono>
#include <string>

class PrismaUIManager
    : public RE::BSTEventSink<RE::InputEvent*>
    , public RE::BSTEventSink<SKSE::ModCallbackEvent>
    , public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static PrismaUIManager* GetSingleton() {
        static PrismaUIManager singleton;
        return &singleton;
    }

    void Initialize();
    void Show();
    void Hide();

    void UpdateExcitements();
    void UpdateNavigation();
    void UpdateOptions();
    void UpdateThreadStatus();
    void UpdateKeys();
    void PollUpdate(uint64_t generation);

    bool IsAvailable() const { return magelight != nullptr && modId != 0; }
    bool IsViewValid() const { return magelight && view != 0 && magelight->IsViewValid(view); }
    bool HasFocus() const;

    void SetFocus(bool shouldFocus);
    void ToggleInspector();
    void ToggleHideUI();

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) override;
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    static void OnThreadEvent(OstimNG_API::Thread::ThreadEvent eventType, uint32_t threadID, void* userData);

    static constexpr uint32_t INVALID_THREAD_ID = 0xFFFFFFFF;

private:
    const MAGELIGHT_API::MagelightApi4* magelight = nullptr;
    MAGELIGHT_API::ModId modId = 0;
    MAGELIGHT_API::ViewId view = 0;
    std::atomic<bool> hasUIMode{false};

    bool initialized = false;
    uint32_t currentThreadID = INVALID_THREAD_ID;
    bool isPolling = false;
    std::atomic<uint64_t> pollGeneration{0};
    bool isListeningInput = false;
    bool isListeningModEvents = false;
    bool isListeningMenuEvents = false;
    int openMenuCount = 0;
    bool hiddenByMenu = false;
    bool hiddenByUser = false;
    bool hiddenByIdle = false;
    bool isCurrentlyVisible = false;
    std::atomic<int64_t> lastActivityTime{0};
    OStimDataProvider::KeyData cachedKeys{};
    bool hasCachedKeys = false;

    void StartPolling();
    void StopPolling();
    void ApplyVisibility();
    void ResetIdleTimer();
    void CheckIdleTimeout();
    void StartListeningInput();
    void StopListeningInput();
    void StartListeningModEvents();
    void StartListeningMenuEvents();
    void StopListeningMenuEvents();
    void HandleActionOnGameThread(const std::string& data);

    static void OnMagelightEvent(const MAGELIGHT_API::EventData* ev, void* user);
    static void OnAction(const char* data);
    static void OnActionGameTask(void* user);

    PrismaUIManager() = default;
    ~PrismaUIManager() = default;
    PrismaUIManager(const PrismaUIManager&) = delete;
    PrismaUIManager& operator=(const PrismaUIManager&) = delete;
};
