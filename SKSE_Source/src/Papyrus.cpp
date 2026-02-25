#include "PCH.h"
#include "Papyrus.h"
#include "OStimDataProvider.h"

namespace Papyrus {
    void SetActorAdditionalProgress(RE::StaticFunctionTag*, RE::Actor* actor, float progress) {
        if (!actor) return;
        SKSE::log::info("Papyrus call: SetActorAdditionalProgress for actor {} with progress {}", actor->GetFormID(), progress);
         OStimDataProvider::GetSingleton()->SetActorAdditionalProgress(actor->GetFormID(), progress);
    }
    
    bool Register(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("SetActorAdditionalProgress", "OStimPrism_API", SetActorAdditionalProgress);
        return true;
    }
}
