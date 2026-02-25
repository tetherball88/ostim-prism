Scriptname TTOP_PlayerRef extends ReferenceAlias

Event OnPlayerLoadGame()
    TTOP_MainController mainController = self.GetOwningQuest() as TTOP_MainController
    mainController.Maintenance()
EndEvent

