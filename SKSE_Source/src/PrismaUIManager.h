#pragma once
#include "PCH.h"
#include "MagelightUI_API.h"
#include "OStimDataProvider.h"
#include "OstimNG-API-Thread.h"
#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>

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

    void UpdateExcitements(bool forceResendPortraits = false);
    void UpdateNavigation();
    void UpdateOptions();
    void UpdateThreadStatus();
    void UpdateKeys();
    void UpdateSettings();
    void SendActorPortrait(uint32_t formId, const std::string& dataUrl);
    void ClearActorPortraits();
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
    static void OnControlEvent(OstimNG_API::Thread::Controls controlType, uint32_t threadID, void* userData);

    static constexpr uint32_t INVALID_THREAD_ID = 0xFFFFFFFF;

private:
    const MAGELIGHT_API::MagelightApi4* magelight = nullptr;
    MAGELIGHT_API::ModId modId = 0;
    MAGELIGHT_API::ViewId view = 0;
    std::atomic<bool> hasUIMode{false};
    std::atomic<bool> hasKeyFocus{false};

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
    std::array<std::atomic<int64_t>, 16> lastDirectControlMs{};
    std::atomic<int64_t> lastFocusToggleMs{0};
    std::atomic<int64_t> lastInspectorToggleMs{0};
    int64_t lastExcitementPollMs = 0;
    bool wasFocusedLastPoll = false;
    std::unordered_map<uint32_t, bool> prevPolledKeyDown;
    uint32_t prevXInputMask = 0xFFFFFFFFu;
    int xinputRepeatDir = 0;
    int64_t xinputNextRepeatMs = 0;
    float xinputRightStickAcc = 0.0f;
    int64_t lastXInputProbeMs = 0;
    int activeXInputSlot = -1;

    void NoteDirectControl(OstimNG_API::Thread::Controls control);
    bool WasRecentlyHandledDirectly(OstimNG_API::Thread::Controls control, int64_t eventTimeMs) const;
    void ToggleFocusDebounced();
    void ToggleInspectorDebounced();
    bool IsMagelightCapturingGamepad() const;
    void PollFocusedInput(int64_t nowMs);
    void ResetFocusedInputState();

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
