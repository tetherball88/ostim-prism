#pragma once
#include "PCH.h"
#include "PrismaUI_API.h"
#include "OStimDataProvider.h"
#include "OstimNG-API-Thread.h"

class PrismaUIManager : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static PrismaUIManager* GetSingleton() {
        static PrismaUIManager singleton;
        return &singleton;
    }

    void Initialize();
    void Show();
    void Hide();
    void Destroy();

    void UpdateExcitements();
    void UpdateNavigation();
    void UpdateOptions();
    void UpdateThreadStatus();
    void UpdateKeys();
    void PollUpdate();

    bool IsAvailable() const { return prismaUI != nullptr; }
    bool IsViewValid() const { return prismaUI && prismaUI->IsValid(view); }

    void SetTextInputFocus(bool focused);

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) override;

    static void OnThreadEvent(OstimNG_API::Thread::ThreadEvent eventType, uint32_t threadID, void* userData);
    static void OnControlInput(OstimNG_API::Thread::Controls controlType, uint32_t threadID, void* userData);

    static constexpr uint32_t INVALID_THREAD_ID = 0xFFFFFFFF;

private:
    PRISMA_UI_API::IVPrismaUI1* prismaUI = nullptr;
    PrismaView view = 0;
    bool initialized = false;
    uint32_t currentThreadID = INVALID_THREAD_ID;
    bool isPolling = false;
    bool isListeningInput = false;
    bool inspectorCreated = false;
    bool isTextInputFocused = false;
    
    void StartPolling();
    void StopPolling();
    void StartListeningInput();
    void StopListeningInput();

    static void OnDomReady(PrismaView view);
    static void OnAction(const char* data);

    PrismaUIManager() = default;
    ~PrismaUIManager() = default;
    PrismaUIManager(const PrismaUIManager&) = delete;
    PrismaUIManager& operator=(const PrismaUIManager&) = delete;
};
