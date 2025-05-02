// --- START OF FILE VisualizationManager.cpp ---

#include "VisualizationManager.h"
#include "StudyManager.h" // For EConditionType and EnumToString
#include "UObject/EnumProperty.h" // For StaticEnum (if using UE5.1+) or manual GetNameStringByValue
#include "Logging/LogMacros.h" // For logging
// #include "AudioAnalysis.h"
// #include "Firefly.h"
// #include "Kismet/GameplayStatics.h"

AVisualizationManager::AVisualizationManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AVisualizationManager::BeginPlay()
{
    Super::BeginPlay();
    InitializeSystems();
    DeactivateVisualizationSystems(); // Ensure everything is off initially
}

void AVisualizationManager::InitializeSystems()
{
    // Find your AudioAnalysis actor, find/spawn Fireflies, etc.
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Initialized systems (Implement specific logic here)."));
}

// --- Interface Implementation ---

void AVisualizationManager::StartActiveVisualization_Implementation(EConditionType CurrentCondition)
{
    // Use StaticEnum for safer enum to string conversion
    const UEnum* EnumPtr = StaticEnum<EConditionType>();
    FString ConditionStr = EnumPtr ? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(CurrentCondition)).ToString() : TEXT("Unknown");
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Received StartActiveVisualization signal for condition %s."), *ConditionStr);

    // --- YOUR FULL ACTIVATION LOGIC HERE ---
    ActivateVisualizationSystems(CurrentCondition);
}

void AVisualizationManager::StopActiveVisualization_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Received StopActiveVisualization signal."));
    // --- YOUR FULL DEACTIVATION LOGIC HERE ---
    DeactivateVisualizationSystems();
}

// *** Implementation for the added interface function (Ensure this DEFINITION exists) ***
void AVisualizationManager::StartVisualization_ControlOnly_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Received StartVisualization_ControlOnly signal (e.g., for Tutorial)."));

    // --- YOUR 'CONTROL ONLY' ACTIVATION LOGIC ---
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Activating ONLY core visuals... (Implement specific logic)"));
    // Example:
    // TArray<UNiagaraComponent*> NiagaraComponents;
    // GetComponents<UNiagaraComponent>(NiagaraComponents);
    // for(UNiagaraComponent* Comp : NiagaraComponents)
    // {
    //     if(Comp) Comp->Activate(true);
    // }
    // Ensure audio/interaction systems remain OFF
}


// --- Specific Activation/Deactivation Logic ---

void AVisualizationManager::ActivateVisualizationSystems(EConditionType Condition)
{
    // Placeholder: Implement your full activation logic based on condition
    const UEnum* EnumPtr = StaticEnum<EConditionType>();
    FString ConditionStr = EnumPtr ? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Condition)).ToString() : TEXT("Unknown");
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Activating FULL visualization systems for condition %s... (Implement specific logic)"), *ConditionStr);

    // Example:
    // StartVisuals();
    // StartAudio();
    // EnableInteraction(Condition == EConditionType::WithInteraction);
}

void AVisualizationManager::DeactivateVisualizationSystems()
{
    // Placeholder: Implement your full deactivation logic
    UE_LOG(LogTemp, Log, TEXT("VisualizationManager: Deactivating ALL visualization systems... (Implement specific logic)"));

    // Example:
    // StopVisuals();
    // StopAudio();
    // EnableInteraction(false);
    // ResetParameters();
}

// --- END OF FILE VisualizationManager.cpp ---