// --- START OF FILE TutorialWidgetInterface.h ---
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TutorialWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable) // Make it Blueprintable so Blueprints can implement it
class UTutorialWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for tutorial widgets that need progress updates from the StudyGameController.
 */
class HCI_PRAKTIKUM_VR_API_API ITutorialWidgetInterface // <-- Make sure your API macro matches your project (e.g., HCI_PRAKTIKUM_VR_API_API)
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	/**
	 * Called by the StudyGameController to update the widget's displayed progress.
	 * Implement this event in your Blueprint tutorial widgets.
	 * @param CurrentSuccesses The number of successful interactions completed for the current step.
	 * @param RequiredSuccesses The total number of successful interactions required for the current step.
	 * @param HoldProgress A value from 0.0 to 1.0 indicating the progress of the current hold action (if applicable).
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Tutorial UI")
	void UpdateProgressDisplay(int32 CurrentSuccesses, int32 RequiredSuccesses, float HoldProgress);
};
// --- END OF FILE TutorialWidgetInterface.h ---