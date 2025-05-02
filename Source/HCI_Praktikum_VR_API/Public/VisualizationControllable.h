
// --- START OF FILE VisualizationControllable.h ---
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StudyManager.h" // Required for EConditionType enum
#include "VisualizationControllable.generated.h"

// Forward declare enum if StudyManager.h causes issues, but including is usually fine
// enum class EConditionType : uint8;

UINTERFACE(MinimalAPI, Blueprintable)
class UVisualizationControllable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that manage or are part of the visualization
 * and need to be started/stopped by the StudyGameController.
 */
class HCI_PRAKTIKUM_VR_API_API IVisualizationControllable
{
	GENERATED_BODY() // Line 22 according to previous error messages

public:
	/**
	 * Function to call when the main visualization phase should begin.
	 * @param CurrentCondition The study condition context for this activation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Visualization Control")
	void StartActiveVisualization(EConditionType CurrentCondition); // Declaration MUST end with a semicolon ONLY.

	/**
	 * Function to call when the visualization phase should end completely.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Visualization Control")
	void StopActiveVisualization(); // Declaration MUST end with a semicolon ONLY.

	/**
	 * Function to start ONLY the core particle simulation (e.g., for tutorial).
	 * No audio, no interaction logic should be started here by default.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Visualization Control")
	void StartVisualization_ControlOnly(); // Declaration MUST end with a semicolon ONLY.

	// NO CODE (ESPECIALLY NO '{...}') SHOULD BE PRESENT AFTER THIS POINT WITHIN THE CLASS DEFINITION
};

// --- END OF FILE VisualizationControllable.h ---

