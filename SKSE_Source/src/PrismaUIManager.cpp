#include "PrismaUIManager.h"
#include "Settings.h"
#include <thread>
#include <chrono>
#include <Windows.h>
#include <algorithm>

void PrismaUIManager::StartListeningInput() {
    if (isListeningInput) return;
    auto deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (deviceManager) {
        deviceManager->AddEventSink(this);
        isListeningInput = true;
        SKSE::log::info("Started listening for input events");
    }
}

void PrismaUIManager::StopListeningInput() {
    if (!isListeningInput) return;
    auto deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (deviceManager) {
        deviceManager->RemoveEventSink(this);
        isListeningInput = false;
        SKSE::log::info("Stopped listening for input events");
    }
}

void PrismaUIManager::SetTextInputFocus(bool focused) {
    SKSE::log::info("Setting text input focus: {}", focused);
    auto controlMap = RE::ControlMap::GetSingleton();
    if (!controlMap) {
        SKSE::log::error("ControlMap singleton not available");
        return;
    }

    if (focused && !isTextInputFocused) {
        controlMap->AllowTextInput(true);
        isTextInputFocused = true;
        SKSE::log::info("Text input focused: suppressing game hotkeys");
    } else if (!focused && isTextInputFocused) {
        controlMap->AllowTextInput(false);
        isTextInputFocused = false;
        SKSE::log::info("Text input unfocused: restoring game hotkeys");
    }
}

RE::BSEventNotifyControl PrismaUIManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) {
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Suppress unused parameter warning
    (void)a_source;

    if (!IsViewValid()) {
        SKSE::log::warn("ProcessEvent called but PrismaUI view is not valid");
        return RE::BSEventNotifyControl::kContinue;
    }

    // While a text input field is focused, suppress all hotkey handling
    // if (isTextInputFocused) {
    //     return RE::BSEventNotifyControl::kContinue;
    // }

    for (auto event = *a_event; event; event = event->next) {
        if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        auto button = event->AsButtonEvent();
        if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
            continue;
        }

        // Toggle Focus (Configurable)
        auto key = button->GetIDCode();
        auto& focusKeys = Settings::GetSingleton()->toggleFocusKeys;
        if (std::find(focusKeys.begin(), focusKeys.end(), key) != focusKeys.end()) {
            if (button->IsDown()) {
                if (prismaUI && view) {
                    if (prismaUI->HasFocus(view)) {
                        prismaUI->Unfocus(view);
                    } else {
                        prismaUI->Focus(view);
                    }
                } else {
                     SKSE::log::warn("Focus key pressed but prismaUI or view is invalid.");
                }
            } 
        }

        // Toggle Inspector (Configurable)
        auto& inspectorKeys = Settings::GetSingleton()->toggleInspectorKeys;
        if (std::find(inspectorKeys.begin(), inspectorKeys.end(), key) != inspectorKeys.end()) {
            if (button->IsDown()) {
                if (prismaUI && view) {
                    if (!inspectorCreated) {
                        prismaUI->CreateInspectorView(view);
                        prismaUI->SetInspectorBounds(view, 100.0f, 100.0f, 800, 600);
                        inspectorCreated = true;
                    }
                    bool visible = prismaUI->IsInspectorVisible(view);
                    prismaUI->SetInspectorVisibility(view, !visible);
                } else {
                    SKSE::log::warn("Inspector key pressed but prismaUI or view is invalid.");
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

void PrismaUIManager::Initialize() {
    if (initialized) {
        return;
    }

    auto api = PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1);
    if (!api) {
        SKSE::log::info("PrismaUI not available");
        return;
    }

    prismaUI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(api);
    initialized = true;
    SKSE::log::info("PrismaUI initialized successfully for OStim Prism");
}

void PrismaUIManager::Show() {
    if (!prismaUI) {
        return;
    }

    if (view && prismaUI->IsValid(view)) {
        prismaUI->Show(view);
        return;
    }

    auto newView = prismaUI->CreateView("OStim/index.html", OnDomReady);

    if (!newView) {
        SKSE::log::error("Failed to create PrismaUI view");
        return;
    }

    view = newView;
    StartPolling();
    SKSE::log::info("PrismaUI view created: {}", newView);
}

void PrismaUIManager::Hide() {
    if (!prismaUI || !view) {
        return;
    }

    if (prismaUI->IsValid(view)) {
        prismaUI->Hide(view);
    }
}

void PrismaUIManager::Destroy() {
    if (!prismaUI || !view) {
        return;
    }

    StopPolling();
    StopListeningInput();

    // Ensure text input mode is released if a field was focused when destroyed
    SetTextInputFocus(false);

    // Hide immediately
    if (prismaUI->IsValid(view)) {
        prismaUI->Unfocus(view);
        prismaUI->Hide(view);
        SKSE::log::info("Hiding PrismaUI view: {}; focused={}", view, prismaUI->HasFocus(view));
    }
    
    // Defer destruction to avoid potential deadlocks if called from certain contexts
    // or if the UI is busy.
    // Making a copy of view ID to capture in lambda
    PrismaView viewToDestroy = view;
    view = 0; // Mark as invalid immediately
    inspectorCreated = false;

    SKSE::GetTaskInterface()->AddTask([this, viewToDestroy]() {
        if (prismaUI && prismaUI->IsValid(viewToDestroy)) {
            if (prismaUI->HasFocus(viewToDestroy)) {
                SKSE::log::info("View still focused at destroy time, unfocusing: {}", viewToDestroy);
                prismaUI->Unfocus(viewToDestroy);
            }
        }
        // Poll until focus is released, then destroy
        std::thread([this, viewToDestroy]() {
            auto stillFocused = std::make_shared<std::atomic<bool>>(true);
            int maxWaitMs = 2000;
            int waited = 0;
            while (waited < maxWaitMs) {
                SKSE::GetTaskInterface()->AddTask([this, viewToDestroy, stillFocused]() {
                    stillFocused->store(prismaUI && prismaUI->IsValid(viewToDestroy) && prismaUI->HasFocus(viewToDestroy));
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                waited += 100;
                if (!stillFocused->load()) break;
                SKSE::log::info("Waiting for unfocus... ({}ms)", waited);
            }
            SKSE::GetTaskInterface()->AddTask([this, viewToDestroy]() {
                if (prismaUI && prismaUI->IsValid(viewToDestroy)) {
                    SKSE::log::info("Destroying PrismaUI view after unfocus: {}", viewToDestroy);
                    prismaUI->Destroy(viewToDestroy);
                }
            });
        }).detach();
    });
}

void PrismaUIManager::StartPolling() {
    if (isPolling) return;
    isPolling = true;
    
    // Ensure the polling loop starts on the main thread
    SKSE::GetTaskInterface()->AddTask([this]() {
        if (isPolling) {
            PollUpdate();
        }
    });
    
    SKSE::log::info("Started polling for excitement updates (100ms interval)");
}

void PrismaUIManager::StopPolling() {
    isPolling = false;
    SKSE::log::info("Stopped polling for excitement updates");
}

void PrismaUIManager::PollUpdate() {
    // Check polling flag first
    if (!isPolling) return;

    // Check thread ID validity
    if (currentThreadID == INVALID_THREAD_ID) {
        isPolling = false;
        return;
    }

    UpdateExcitements();

    // Sleep in a background thread to avoid blocking the main game loop
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Re-schedule PollUpdate on the main thread
        // Use a weak check logic here: capture by value or verify singleton?
        // Capturing 'this' is safe as long as singleton is alive (which it is for static/global duration)
        if (isPolling) {
            SKSE::GetTaskInterface()->AddTask([this]() {
                if (isPolling) {
                    PollUpdate();
                }
            });
        }
    }).detach();
}

void PrismaUIManager::UpdateKeys()
{
    if (!prismaUI || !view) return;

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) return;

    auto keys = dataProvider->GetKeyData();

    json j = {        
        {"keyUp", keys.keyUp},
        {"keyDown", keys.keyDown},
        {"keyLeft", keys.keyLeft},
        {"keyRight", keys.keyRight},
        {"keyYes", keys.keyYes},
        {"keyEnd", keys.keyEnd},
        {"keyToggle", keys.keyToggle},
        {"keySearch", keys.keySearch},
        {"keyAlignment", keys.keyAlignment},
        {"keySceneStart", keys.keySceneStart},
        {"keyNpcSceneStart", keys.keyNpcSceneStart},
        {"keySpeedUp", keys.keySpeedUp},
        {"keySpeedDown", keys.keySpeedDown},
        {"keyPullOut", keys.keyPullOut},
        {"keyAutoMode", keys.keyAutoMode},
        {"keyFreeCam", keys.keyFreeCam},
        {"keyHideUI", keys.keyHideUI}
    };
    
    std::string script = "updateKeys(" + j.dump() + ")";
    prismaUI->Invoke(view, script.c_str());
    SKSE::log::info("Sent key update to UI");
}

void PrismaUIManager::UpdateExcitements() {
    // SKSE::log::info("UpdateExcitements called");

    if (!prismaUI || !view) {
        SKSE::log::warn("UpdateExcitements early return: prismaUI={}, view={}",
            (void*)prismaUI, view);
        return;
    }

    // Also refresh thread status (speed, control flags) on every excitement poll
    UpdateThreadStatus();

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    // currentThreadID is 0 for player thread (validated in OnThreadEvent)
    auto actors = dataProvider->GetActorExcitements(currentThreadID);
    
    json actorsJson = json::array();
    for (const auto& actor : actors) {
        actorsJson.push_back({
            {"name", actor.name},
            {"excitementProgress", actor.excitementProgress},
            {"staminaProgress", actor.staminaProgress},
            {"gender", actor.gender},
            {"timesClimaxed", actor.timesClimaxed},
            {"additionalProgress", actor.additionalProgress}
        });

        // SKSE::log::info("Actor {}: excitement={}, stamina={}%, gender={}",
        //    actor.name, actor.excitementProgress, actor.staminaProgress, actor.gender);
    }

    std::string script = "updateExcitements(" + actorsJson.dump() + ")";
    prismaUI->Invoke(view, script.c_str());
}

void PrismaUIManager::UpdateNavigation() {
    SKSE::log::info("UpdateNavigation called");

    if (!prismaUI || !view) {
        SKSE::log::warn("UpdateNavigation early return: prismaUI={}, view={}",
            (void*)prismaUI, view);
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    // currentThreadID is 0 for player thread (validated in OnThreadEvent)
    auto options = dataProvider->GetNavigationOptions(currentThreadID);

    json navJson = json::array();
    for (const auto& nav : options) {
        navJson.push_back({
            {"id", nav.id},
            {"iconPath", nav.iconPath},
            {"iconData", nav.iconData},
            {"description", nav.description},
            {"destination", nav.destination}
        });
    }

    std::string jsonStr = navJson.dump();
    std::string script = "updateNavigation(" + jsonStr + ")";
    SKSE::log::info("UpdateNavigation sending {} options", options.size());
    prismaUI->Invoke(view, script.c_str());
}

void PrismaUIManager::UpdateOptions() {
    SKSE::log::info("UpdateOptions called");

    if (!prismaUI || !view) {
        SKSE::log::warn("UpdateOptions early return: prismaUI={}, view={}",
            (void*)prismaUI, view);
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    auto items = dataProvider->GetSceneOptions();

    json optionsJson = json::array();
    for (const auto& item : items) {
        optionsJson.push_back({
            {"id", item.id},
            {"iconPath", item.icon}, // UI expects iconPath
            {"iconData", item.iconData},
            {"border", item.border},
            {"description", item.description}
        });
    }

    std::string script = "updateOptions(" + optionsJson.dump() + ")";
    SKSE::log::info("UpdateOptions sending {} items", items.size());
    prismaUI->Invoke(view, script.c_str());
}

void PrismaUIManager::UpdateThreadStatus() {
    if (!prismaUI || !view) {
        SKSE::log::warn("UpdateThreadStatus early return: prismaUI={}, view={}",
            (void*)prismaUI, view);
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    auto status = dataProvider->GetThreadStatus(currentThreadID);

    json statusJson = {
        {"autoControl",   status.autoControl},
        {"manualControl", status.manualControl},
        {"locked",        status.locked},
        {"currentSpeed",  status.currentSpeed},
        {"minSpeed",      status.minSpeed},
        {"maxSpeed",      status.maxSpeed}
    };

    std::string statusScript = "updateThreadStatus(" + statusJson.dump() + ")";
    prismaUI->Invoke(view, statusScript.c_str());
}

void PrismaUIManager::OnThreadEvent(OstimNG_API::Thread::ThreadEvent eventType, uint32_t threadID, void* /*userData*/) {
    // Dispatch to main thread to avoid deadlocks/crashes when calling UI or OStim APIs
    SKSE::GetTaskInterface()->AddTask([eventType, threadID]() {
        auto manager = GetSingleton();
        if (!manager) return;

        SKSE::log::info("OnThreadEvent (Main Thread): event={}, threadID={}", static_cast<int>(eventType), threadID);

        // Only handle player thread (threadID == 0)
        if (threadID != 0) {
            return;
        }

        switch (eventType) {
            case OstimNG_API::Thread::ThreadEvent::ThreadStarted: {
                SKSE::log::info("Thread started: {}", threadID);
                manager->currentThreadID = threadID;
                bool viewReused = manager->view != 0 && manager->prismaUI && manager->prismaUI->IsValid(manager->view);
                manager->Show();
                if (viewReused) {
                    // View survived from the previous thread (rapid restart case).
                    // Show() just made it visible but didn't restart polling or refresh data.
                    SKSE::log::info("Thread restarted with existing view — refreshing UI state");
                    if (!manager->isPolling) manager->StartPolling();
                    manager->UpdateNavigation();
                    manager->UpdateExcitements();
                    manager->UpdateOptions();
                    manager->UpdateThreadStatus();
                    manager->UpdateKeys();
                }
                break;
            }

            case OstimNG_API::Thread::ThreadEvent::ThreadEnded:
                SKSE::log::info("Thread ended: {}", threadID);
                if (manager->currentThreadID == threadID) {
                    manager->StopPolling();
                    manager->currentThreadID = INVALID_THREAD_ID;
                    // Destroy asynchronously to allow polling tasks to drain.
                    // Guard against rapid restart: if a new thread has already started
                    // by the time this runs, skip the destroy so we don't kill the new view.
                    std::thread([manager]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        SKSE::GetTaskInterface()->AddTask([manager]() {
                            if (manager->currentThreadID == INVALID_THREAD_ID) {
                                manager->Destroy();
                            } else {
                                SKSE::log::info("Skipping deferred Destroy: new thread started before destroy ran");
                            }
                        });
                    }).detach();
                }
                break;

            case OstimNG_API::Thread::ThreadEvent::NodeChanged:
                SKSE::log::info("Node changed: {}", threadID);
                if (manager->currentThreadID == threadID) {
                    manager->UpdateNavigation();
                    manager->UpdateThreadStatus();
                }
                break;
        }
    });
}

void PrismaUIManager::OnControlInput(OstimNG_API::Thread::Controls controlType, uint32_t threadID, void* /*userData*/) {
    SKSE::GetTaskInterface()->AddTask([controlType, threadID]() {
        auto manager = GetSingleton();
        if (!manager || !manager->prismaUI || !manager->view) {
            return;
        }

        // Only handle player thread (threadID == 0)
        if (threadID != 0) {
            return;
        }

        if (manager->currentThreadID != threadID) {
            return;
        }

        // Convert Controls to string for JS
        const char* controlStr = nullptr;
        switch (controlType) {
            case OstimNG_API::Thread::Controls::Up: controlStr = "up"; break;
            case OstimNG_API::Thread::Controls::Down: controlStr = "down"; break;
            case OstimNG_API::Thread::Controls::Left: controlStr = "left"; break;
            case OstimNG_API::Thread::Controls::Right: controlStr = "right"; break;
            case OstimNG_API::Thread::Controls::Toggle: controlStr = "toggle"; break;
            case OstimNG_API::Thread::Controls::Yes: controlStr = "yes"; break;
            case OstimNG_API::Thread::Controls::No: controlStr = "no"; break;
            case OstimNG_API::Thread::Controls::Menu: controlStr = "menu"; break;
            case OstimNG_API::Thread::Controls::KEY_HIDE: {
                SKSE::log::info("Toggle UI visibility");
                if(manager->prismaUI->IsHidden(manager->view)) {
                    SKSE::log::info("Showing UI");
                    manager->prismaUI->Show(manager->view);
                } else {
                    SKSE::log::info("Hiding UI");
                    manager->prismaUI->Hide(manager->view);
                }
                return;
            }
            case OstimNG_API::Thread::Controls::AlignMenu: {
                SKSE::log::info("Opening Alignment Menu");
                std::string script = "showMenu('alignMenu')";
                manager->prismaUI->Invoke(manager->view, script.c_str());
                return;
            }
            case OstimNG_API::Thread::Controls::SearchMenu: {
                SKSE::log::info("Opening Search Menu");
                std::string searchScript = "showMenu('searchMenu')";
                manager->prismaUI->Invoke(manager->view, searchScript.c_str());
                return;
            }
            default: return;
        }

        std::string script = "handleControl('" + std::string(controlStr) + "')";
        manager->prismaUI->Invoke(manager->view, script.c_str());
    });
}

void PrismaUIManager::OnDomReady(PrismaView view) {
    SKSE::log::info("PrismaUI DOM ready for view: {}", view);

    auto manager = GetSingleton();
    if (!manager || !manager->prismaUI) {
        return;
    }

    manager->view = view;

    manager->prismaUI->Invoke(view, "console.log('OStim Prism UI loaded')");
    manager->prismaUI->RegisterJSListener(view, "sendAction", OnAction);

    manager->UpdateNavigation();
    manager->UpdateExcitements();
    manager->UpdateOptions();
    manager->UpdateThreadStatus();
    manager->UpdateKeys();

    std::string gameReadyScript = "setGameReady()";
    manager->prismaUI->Invoke(view, gameReadyScript.c_str());

    // Start listening for input
    manager->StartListeningInput();
}

void PrismaUIManager::OnAction(const char* data) {
    SKSE::log::info("PrismaUI action received: {}", data);

    auto manager = GetSingleton();
    auto dataProvider = OStimDataProvider::GetSingleton();

    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("Cannot handle action: OStim not connected");
        return;
    }

    try {
        json actionData = json::parse(data);

        std::string action = actionData.value("action", "");
        if (action.empty()) {
            SKSE::log::warn("PrismaUI action missing 'action' field");
            return;
        }

        auto sendAlignmentUpdate = [&](uint32_t threadID, uint32_t actorIndex) {
            if (!manager || !manager->prismaUI || !manager->view) {
                return;
            }

            uint32_t actorCount = dataProvider->GetActorCount(threadID);
            if (actorCount == 0) {
                actorCount = 1;
            }

            if (actorIndex >= actorCount) {
                actorIndex = actorCount - 1;
            }

            // Get scene info
            OStimDataProvider::SearchResult sceneInfo{};
            if (!dataProvider->GetCurrentSceneInfo(threadID, sceneInfo)) {
                SKSE::log::warn("Failed to fetch scene info (threadID={})", threadID);
            }

            // Get actor info
            auto actors = dataProvider->GetActorExcitements(threadID);
            std::string actorName = "Unknown";
            std::string actorGender = "unknown";
            if (actorIndex < actors.size()) {
                actorName = actors[actorIndex].name;
                actorGender = actors[actorIndex].gender;
            }

            OStimDataProvider::AlignmentData data{};
            if (!dataProvider->GetActorAlignment(threadID, actorIndex, data)) {
                SKSE::log::warn("Failed to fetch alignment data (actorIndex={})", actorIndex);
                return;
            }

            json payload = {
                {"actorIndex", actorIndex},
                {"actorCount", actorCount},
                {"sceneId", sceneInfo.sceneId},
                {"sceneName", sceneInfo.name},
                {"actorName", actorName},
                {"actorGender", actorGender},
                {"data", {
                    {"offsetX", data.offsetX},
                    {"offsetY", data.offsetY},
                    {"offsetZ", data.offsetZ},
                    {"scale", data.scale},
                    {"rotation", data.rotation},
                    {"sosBend", data.sosBend}
                }}
            };

            SKSE::log::info("Sending alignment update: actorIndex={}, sceneId={}, actorName={}",
                actorIndex, sceneInfo.sceneId, actorName);

            std::string script = "updateAlignment(" + payload.dump() + ")";
            manager->prismaUI->Invoke(manager->view, script.c_str());
        };

        if (action == "navigationSelect") {
            auto payload = actionData.value("payload", json::object());
            std::string destination = payload.value("destination", "");

            if (destination.empty()) {
                SKSE::log::warn("PrismaUI navigationSelect missing destination");
                return;
            }

            // Only navigate if we're on player thread (threadID == 0)
            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->NavigateToScene(threadID, destination);
            } else {
                SKSE::log::warn("Cannot navigate: not on player thread (threadID={})", threadID);
            }
        } else if (action == "searchQuery") {
            if (!manager || !manager->prismaUI || !manager->view) {
                return;
            }

            auto payload = actionData.value("payload", json::object());
            std::string query = payload.value("query", "");

            if (query.empty()) {
                manager->prismaUI->Invoke(manager->view, "updateSearchResults([])");
                return;
            }

            auto results = dataProvider->SearchScenes(query, 999);
            json resultsJson = json::array();
            for (const auto& item : results) {
                resultsJson.push_back({
                    {"sceneId", item.sceneId},
                    {"name", item.name},
                    {"actorCount", item.actorCount}
                });
            }

            std::string script = "updateSearchResults(" + resultsJson.dump() + ")";
            manager->prismaUI->Invoke(manager->view, script.c_str());
        } else if (action == "searchSelect") {
            auto payload = actionData.value("payload", json::object());
            std::string sceneId = payload.value("sceneId", "");

            if (sceneId.empty()) {
                SKSE::log::warn("PrismaUI searchSelect missing sceneId");
                return;
            }

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->NavigateToSearchResult(threadID, sceneId);
                // dataProvider->NavigateToScene(threadID, sceneId);
            } else {
                SKSE::log::warn("Cannot navigate search result: not on player thread (threadID={})", threadID);
            }
        } else if (action == "selectOption") {
            auto payload = actionData.value("payload", json::object());
            int32_t index = payload.value("index", -1);

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                // Returns true if still in options menu
                bool stillOpen = dataProvider->SelectOptionsItem(index);

                if(!stillOpen) {
                    SKSE::log::info("Options menu closed after selecting index {}", index);
                    std::string script = "showMenu('navigation')";
                    manager->prismaUI->Invoke(manager->view, script.c_str());
                } else {
                    // Refresh the options list (could be new page or empty if closed)
                    manager->UpdateOptions();
                }
            } else {
                SKSE::log::warn("Cannot select option: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentInit") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = payload.value("actorIndex", 0u);

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                sendAlignmentUpdate(threadID, actorIndex);
            } else {
                SKSE::log::warn("Cannot init alignment: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentSelectActor") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = payload.value("actorIndex", 0u);

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                sendAlignmentUpdate(threadID, actorIndex);
            } else {
                SKSE::log::warn("Cannot select alignment actor: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentSet") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = payload.value("actorIndex", 0u);
            std::string field = payload.value("field", "");
            float value = payload.value("value", 0.0f);

            if (field.empty()) {
                SKSE::log::warn("alignmentSet missing field");
                return;
            }

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID != 0) {
                SKSE::log::warn("Cannot set alignment: not on player thread (threadID={})", threadID);
                return;
            }

            OStimDataProvider::AlignmentData alignData{};
            if (!dataProvider->GetActorAlignment(threadID, actorIndex, alignData)) {
                SKSE::log::warn("Failed to fetch alignment data for set (actorIndex={})", actorIndex);
                return;
            }

            if (field == "offsetX") alignData.offsetX = value;
            else if (field == "offsetY") alignData.offsetY = value;
            else if (field == "offsetZ") alignData.offsetZ = value;
            else if (field == "scale") alignData.scale = value;
            else if (field == "rotation") alignData.rotation = value;
            else if (field == "sosBend") alignData.sosBend = value;
            else {
                SKSE::log::warn("alignmentSet unknown field: {}", field);
                return;
            }

            if (!dataProvider->SetActorAlignment(threadID, actorIndex, alignData)) {
                SKSE::log::warn("Failed to apply alignment set (actorIndex={})", actorIndex);
                return;
            }

            // sendAlignmentUpdate(threadID, actorIndex);
        } else if (action == "setSpeed") {
            auto payload = actionData.value("payload", json::object());
            int32_t speed = payload.value("speed", 1);

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->SetSpeed(threadID, speed);
                SKSE::log::info("setSpeed: applied speed {} on thread {}", speed, threadID);
                // Echo updated status back to UI immediately
                if (manager) manager->UpdateThreadStatus();
            } else {
                SKSE::log::warn("Cannot set speed: not on player thread (threadID={})", threadID);
            }
        } else if (action == "focus") {
            auto payload = actionData.value("payload", json::object());
            bool shouldFocus = payload.value("shouldFocus", false);
            if (manager->prismaUI && manager->view) {
                if (shouldFocus) {
                    manager->prismaUI->Focus(manager->view);
                } else {
                    manager->prismaUI->Unfocus(manager->view);
                }
            }
        } else if (action == "setTextInputFocus") {
            auto payload = actionData.value("payload", json::object());
            bool focused = payload.value("focused", false);
            SKSE::GetTaskInterface()->AddTask([manager, focused]() {
                manager->SetTextInputFocus(focused);
            });
        } else {
            SKSE::log::warn("PrismaUI unknown action: {}", action);
        }
    } catch (const json::parse_error& e) {
        SKSE::log::error("PrismaUI failed to parse action JSON: {}", e.what());
    }
}
