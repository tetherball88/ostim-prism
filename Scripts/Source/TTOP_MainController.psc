ScriptName TTOP_MainController Extends Quest

OCumScript OCum
OSexIntegrationMain OStim

Event OnInit()
    Maintenance()
EndEvent

Function Maintenance()
    MiscUtil.PrintConsole("TTOP: Maintenance function called")
    if(Game.GetModByName("OCum.esp") != 255)
        RegisterForModEvent("ostim_thread_start", "OStimStart")
        RegisterForModEvent("ostim_thread_end", "OStimEnd")
        RegisterForModEvent("ocum_play_cum_shoot_effect", "OCumFire")
        OCum = Game.GetFormFromFile(0x800, "OCum.esp") as OCumScript
    endif

    ; disable stamina bars from OEndurance if it's present to avoid conflicts with the OStim Prism bars
    if(Game.GetModByName("OEndurance.esp") != 255)
        OEnduranceLibs OELibs = Game.GetFormFromFile(0x2d68, "OEndurance.esp") as OEnduranceLibs
        OElibs.ShowStaminaBarPlayer = false
	    OElibs.ShowStaminaBarNpc = false
    endif

    OStim = OUtils.GetOStim()

    ; disable OStim's built in bars since we'll be using the OStim Prism bars instead
    OStim.EnablePlayerBar = false
	OStim.EnableNpcBar = false
EndFunction

Event OnUpdate()
    if(OThread.IsRunning(0))
        UpdateActors()
    endif
EndEvent

Event OCumFire(string eventName, string strArg, float numArg, Form sender)
	Actor orgasmer = sender as Actor
    MiscUtil.PrintConsole("TTOP: OCumFire event received for actor " + orgasmer.GetDisplayName() + " with numArg " + numArg * 100)
    OStimPrism_API.SetActorAdditionalProgress(orgasmer, numArg * 100)
    
EndEvent

Event OStimStart(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    if(ThreadID != 0)
        return
    endif
    UpdateActors()
EndEvent

Function UpdateActors()
    Actor[] actors = OThread.GetActors(0)
    MiscUtil.PrintConsole("TTOP: Updating " + actors.Length + " actors for OCum progress")
    int i = 0
    while(i < actors.Length)
        Actor target = actors[i]
        if(!OStim.IsFemale(target))
            UpdateActor(actors[i])
        endif
        i += 1
    EndWhile
    RegisterForSingleUpdate(5)
EndFunction

Function UpdateActor(Actor akActor)
    float current = OCum.GetCumStoredAmount(akActor)
    float max = OCum.GetMaxCumStoragePossible(akActor)
    float progress = (current / max) * 100.0    
    OStimPrism_API.SetActorAdditionalProgress(akActor, progress)
EndFunction