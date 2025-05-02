#include "NiagaraSimControllerComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"    // For FProperty, FFloatProperty
#include "Logging/LogMacros.h"

UNiagaraSimControllerComponent::UNiagaraSimControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNiagaraSimControllerComponent::BeginPlay()
{
    Super::BeginPlay();
    // You can choose to call one or both at startup:
    ApplyNiagaraParameters();
    ApplyFireflyStatistics();
}

void UNiagaraSimControllerComponent::ApplyNiagaraParameters()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("No World in NiagaraSimControllerComponent"));
        return;
    }

    // Gather all NiagaraComponents in a set
    TSet<UNiagaraComponent*> CompsToUpdate;

    // 1) Manual Actor list
    for (AActor* A : TargetNiagaraActors)
    {
        if (!IsValid(A)) continue;
        TArray<UNiagaraComponent*> Comps;
        A->GetComponents<UNiagaraComponent>(Comps);
        for (UNiagaraComponent* C : Comps)
            if (C->IsRegistered()) CompsToUpdate.Add(C);
    }

    // 2) Actor‑name substring search
    for (const FString& Sub : TargetNiagaraActorNameSubstrings)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A = *It;
            if (!IsValid(A) || !A->GetName().Contains(Sub)) continue;
            TArray<UNiagaraComponent*> Comps;
            A->GetComponents<UNiagaraComponent>(Comps);
            for (UNiagaraComponent* C : Comps)
                if (C->IsRegistered()) CompsToUpdate.Add(C);
        }
    }

    // 3) Component‑name substring search
    for (const FString& Sub : TargetNiagaraComponentNameSubstrings)
    {
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* C = *It;
            if (!IsValid(C) || !C->IsRegistered()) continue;
            if (C->GetName().Contains(Sub))
                CompsToUpdate.Add(C);
        }
    }

    // Apply your Sim_… parameters
    int32 Count = 0;
    for (UNiagaraComponent* C : CompsToUpdate)
    {
        C->SetVariableFloat(TEXT("User.Sim_Colorintensity"), Sim_Colorintensity);
        C->SetVariableFloat(TEXT("User.Sim_Conduction_Strength"), Sim_Conduction_Strength);
        C->SetVariableFloat(TEXT("User.Sim_Conduction_visMultiplier"), Sim_Conduction_visMultiplier);
        C->SetVariableFloat(TEXT("User.Sim_LeftHand_ForceFallOffDistance"), Sim_LeftHand_ForceFallOffDistance);
        C->SetVariableFloat(TEXT("User.Sim_LeftHand_ForceStrength"), Sim_LeftHand_ForceStrength);
        C->SetVariableFloat(TEXT("User.Sim_Pawn_ForceFallOffDistance"), Sim_Pawn_ForceFallOffDistance);
        C->SetVariableFloat(TEXT("User.Sim_Pawn_ForceStrength"), Sim_Pawn_ForceStrength);
        C->SetVariableFloat(TEXT("User.Sim_RightHand_ForceFallOffDistance"), Sim_RightHand_ForceFallOffDistance);
        C->SetVariableFloat(TEXT("User.Sim_RightHand_ForceStrength"), Sim_RightHand_ForceStrength);
        C->SetVariableFloat(TEXT("User.Sim_Vortex_AttractionStrength"), Sim_Vortex_AttractionStrength);
        C->SetVariableFloat(TEXT("User.Sim_Vortex_InnerRadius"), Sim_Vortex_InnerRadius);
        C->SetVariableFloat(TEXT("User.Sim_Vortex_OuterRadius"), Sim_Vortex_OuterRadius);
        C->SetVariableFloat(TEXT("User.Sim_Vortex_RotationStrength"), Sim_Vortex_RotationStrength);
        ++Count;
    }

    UE_LOG(LogTemp, Log, TEXT("ApplyNiagaraParameters: updated %d NiagaraComponents"), Count);
}

void UNiagaraSimControllerComponent::ApplyFireflyStatistics()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("No World in NiagaraSimControllerComponent"));
        return;
    }

    // Gather all Actors to update
    TSet<AActor*> ActorsToUpdate;

    // 1) Manual list
    for (AActor* A : TargetFireflyActors)
        if (IsValid(A))
            ActorsToUpdate.Add(A);

    // 2) Actor‑name substring
    for (const FString& Sub : TargetFireflyNameSubstrings)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A = *It;
            if (IsValid(A) && A->GetName().Contains(Sub))
                ActorsToUpdate.Add(A);
        }
    }

    // For each actor, reflectively set float properties:
    int32 Count = 0;
    for (AActor* A : ActorsToUpdate)
    {
        UClass* Cls = A->GetClass();
        auto SetFloatProp = [&](FName PropName, float Value)
            {
                if (FProperty* P = Cls->FindPropertyByName(PropName))
                {
                    if (FFloatProperty* FP = CastField<FFloatProperty>(P))
                    {
                        void* Addr = FP->ContainerPtrToValuePtr<void>(A);
                        FP->SetFloatingPointPropertyValue(Addr, Value);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Prop %s on %s not float"),
                            *PropName.ToString(), *A->GetName());
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Prop %s not found on %s"),
                        *PropName.ToString(), *A->GetName());
                }
            };

        SetFloatProp(TEXT("default_RootSquare"), default_RootSquare);
        SetFloatProp(TEXT("default_Crest"), default_Crest);
        SetFloatProp(TEXT("default_Zero"), default_Zero);
        SetFloatProp(TEXT("default_Complex"), default_Complex);
        SetFloatProp(TEXT("default_CSD"), default_CSD);
        SetFloatProp(TEXT("default_Centroid"), default_Centroid);
        SetFloatProp(TEXT("default_Flatness"), default_Flatness);
        SetFloatProp(TEXT("default_EnergyDifference"), default_EnergyDifference);

        ++Count;
    }

    UE_LOG(LogTemp, Log, TEXT("ApplyFireflyStatistics: updated %d actors"), Count);
}

void UNiagaraSimControllerComponent::ApplyAllParameters()
{
    ApplyNiagaraParameters();
    ApplyFireflyStatistics();
}
