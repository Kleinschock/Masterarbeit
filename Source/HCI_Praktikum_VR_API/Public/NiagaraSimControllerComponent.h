#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraSimControllerComponent.generated.h"

class UNiagaraComponent;
class AActor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API UNiagaraSimControllerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNiagaraSimControllerComponent();

protected:
    virtual void BeginPlay() override;

public:
    // --- Simulation Parameters (for Niagara systems) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Intensity")
    float Sim_Colorintensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Conduction")
    float Sim_Conduction_Strength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Conduction")
    float Sim_Conduction_visMultiplier = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Hand Force")
    float Sim_LeftHand_ForceFallOffDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Hand Force")
    float Sim_LeftHand_ForceStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Pawn Force")
    float Sim_Pawn_ForceFallOffDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Pawn Force")
    float Sim_Pawn_ForceStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Hand Force")
    float Sim_RightHand_ForceFallOffDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Hand Force")
    float Sim_RightHand_ForceStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Vortex")
    float Sim_Vortex_AttractionStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Vortex")
    float Sim_Vortex_InnerRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Vortex")
    float Sim_Vortex_OuterRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Vortex")
    float Sim_Vortex_RotationStrength = 0.0f;

    // --- Statistics Parameters (for Firefly actors) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_RootSquare = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_Crest = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_Zero = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_Complex = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_CSD = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_Centroid = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_Flatness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Simulation|Statistics")
    float default_EnergyDifference = 0.0f;

    // --- Manual targets & search strings ---

    /** Drag‑and‑drop any actor whose NiagaraComponents you want to update **/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Targets")
    TArray<TObjectPtr<AActor>> TargetNiagaraActors;

    /** Partial actor‑name matches to pick up additional NiagaraComponents **/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Targets")
    TArray<FString> TargetNiagaraActorNameSubstrings;

    /** Partial component‑name matches to pick up NiagaraComponents by their component name **/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Targets")
    TArray<FString> TargetNiagaraComponentNameSubstrings;

    /** Drag‑and‑drop any Firefly actor you want to apply stats to **/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firefly Targets")
    TArray<TObjectPtr<AActor>> TargetFireflyActors;

    /** Partial actor‑name matches to pick up additional Firefly actors **/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Firefly Targets")
    TArray<FString> TargetFireflyNameSubstrings;

    // --- Functions to run in Editor or at runtime ---

    /** Updates Sim_… on all matched NiagaraComponents */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Niagara Simulation")
    void ApplyNiagaraParameters();

    /** Updates default_… on all matched Firefly actors via reflection */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Niagara Simulation")
    void ApplyFireflyStatistics();

    /** Calls both of the above in one go */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Niagara Simulation")
    void ApplyAllParameters();
};
