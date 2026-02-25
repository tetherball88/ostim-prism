#include <Windows.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "PCH.h"
#include "src/OstimNG-API-Thread.h"
#include "src/OStimDataProvider.h"
#include "src/PrismaUIManager.h"
#include "src/Settings.h"
#include "src/Papyrus.h"

using namespace SKSE;


namespace {
    void SetupLogging() {
        auto logDir = SKSE::log::log_directory();
        if (!logDir) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("OstimPrism: log directory unavailable");
            }
            return;
        }

        std::filesystem::path logPath = *logDir;
        if (!std::filesystem::is_directory(logPath)) {
            logPath = logPath.parent_path();
        }
        logPath /= "OstimPrism.log";

        std::error_code ec;
        std::filesystem::create_directories(logPath.parent_path(), ec);
        if (ec) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("OstimPrism: failed to create log folder (%s)", ec.message().c_str());
            }
            return;
        }

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto logger = std::make_shared<spdlog::logger>("OstimPrism", std::move(sink));
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::info);
        logger->set_pattern("[%H:%M:%S] [%l] %v");

        spdlog::set_default_logger(std::move(logger));
        spdlog::info("Logging to {}", logPath.string());
    }

    void PrintToConsole(std::string_view message) {
        SKSE::log::info("{}", message);
        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print("%s", message.data());
        }
    }
}

SKSEPluginLoad(const LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLogging();
    SKSE::log::info("OstimPrism plugin loading...");

    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(Papyrus::Register)) {
        SKSE::log::critical("Failed to register papyrus callback");
        return false;
    }

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* message) {
                switch (message->type) {
                    case SKSE::MessagingInterface::kPreLoadGame:
                        SKSE::log::info("PreLoadGame...");
                        break;

                    case SKSE::MessagingInterface::kPostLoadGame:
                    case SKSE::MessagingInterface::kNewGame:
                        SKSE::log::info("New game/Load...");
                        
                        break;

                    case SKSE::MessagingInterface::kDataLoaded: {
                        SKSE::log::info("Data loaded successfully.");

                        // Load settings
                        Settings::GetSingleton()->Load();

                        // Initialize OStim data provider and PrismaUI manager
                        OStimDataProvider::GetSingleton()->Initialize();

                        PrismaUIManager::GetSingleton()->Initialize();

                        // Register event callback with OStim Thread API
                        auto threadAPI = OstimNG_API::Thread::GetAPI("OStim Prism", REL::Version(1, 0, 0));
                        if (threadAPI) {
                            threadAPI->RegisterEventCallback(PrismaUIManager::OnThreadEvent, nullptr);
                            threadAPI->RegisterControlCallback(PrismaUIManager::OnControlInput, nullptr);
                            
                            // Only disable OStim's Flash UI if PrismaUI is actually available
                            if (PrismaUIManager::GetSingleton()->IsAvailable()) {
                                threadAPI->SetExternalUIEnabled(true);
                                SKSE::log::info("Registered OStim callbacks and enabled external UI (PrismaUI available)");
                            } else {
                                SKSE::log::warn("PrismaUI not available, OStim Flash UI will be used");
                            }
                        } else {
                            SKSE::log::error("Failed to get OStim Thread API");
                        }

                        if (auto* console = RE::ConsoleLog::GetSingleton()) {
                            console->Print("OstimPrism: Ready");
                        }
                        break;
                    }

                    default:
                        break;
                }
            })) {
            SKSE::log::critical("Failed to register messaging listener.");
            return false;
        }
    } else {
        SKSE::log::critical("Messaging interface unavailable.");
        return false;
    }

    return true;
}
