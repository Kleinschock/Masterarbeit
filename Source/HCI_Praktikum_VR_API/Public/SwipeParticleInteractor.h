#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraComponent.h"
#include "SwipeParticleInteractor.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API USwipeParticleInteractor : public UActorComponent
{
    GENERATED_BODY()

private:
    // Debug settings
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowDebug;

    UPROPERTY(EditAnywhere, Category = "Debug")
    float DebugDuration;

    UPROPERTY(EditAnywhere, Category = "Debug")
    float DebugThickness;

    // Debug helper function
    void ShowDebugMessage(const FString& Message, FColor Color);

public:
    // Sets default values for this component's properties
    USwipeParticleInteractor();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    // Niagara System reference
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
    UNiagaraComponent* NiagaraSystem;

    // Swipe logic parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swipe Settings")
    float MaxSwipeDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swipe Settings")
    float MaxSwipeStrength;

public:
    // Blueprint-callable function to interact with the Niagara system
    UFUNCTION(BlueprintCallable, Category = "Swipe Interaction")
    void InteractWithParticles(FVector SwipeOrigin, FVector SwipeDirection, float SwipeStrength);

    // Helper function to set the Niagara system reference
    UFUNCTION(BlueprintCallable, Category = "Swipe Interaction")
    void SetNiagaraSystem(UNiagaraComponent* Niagara);
};
