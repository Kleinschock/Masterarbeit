// --- START OF FILE VisualizationManager.h ---

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualizationControllable.h" // *** INCLUDE THE INTERFACE HEADER ***
#include "StudyManager.h" // Include for EConditionType if needed here
// #include "AudioAnalysis.h" // Include your specific classes
// #include "Firefly.h"

#include "VisualizationManager.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AVisualizationManager : public AActor, public IVisualizationControllable // *** IMPLEMENT THE INTERFACE ***
{
    GENERATED_BODY()

public:
    AVisualizationManager();

    // --- References to your specific systems ---
    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    //class audioAnaly_Plugin* AudioAnalyzer;

    // UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    // TArray<class AFirefly*> Fireflies;

protected:
    virtual void BeginPlay() override;

    // --- Implement the Interface Functions ---
    // Note the _Implementation suffix for BlueprintNativeEvent
    virtual void StartActiveVisualization_Implementation(EConditionType CurrentCondition) override;
    virtual void StopActiveVisualization_Implementation() override;
    // *** FIX: Ensure this DECLARATION exists ***
    virtual void StartVisualization_ControlOnly_Implementation() override;


public:
    // Add your specific functions to control audio, particles etc.
    void ActivateVisualizationSystems(EConditionType Condition);
    void DeactivateVisualizationSystems();

private:
    // Helper to find/manage fireflies, etc.
    void InitializeSystems();
};
// --- END OF FILE VisualizationManager.h ---