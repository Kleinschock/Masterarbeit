// VisualizationController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StudyManager.h"
#include "VisualizationController.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AVisualizationController : public AActor
{
    GENERATED_BODY()

public:
    AVisualizationController();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // Reference to the particle system
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
    class UParticleSystemComponent* ParticleSystem;

    // Whether interaction is enabled
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visualization")
    bool bInteractionEnabled;

    // Current condition
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visualization")
    EConditionType CurrentCondition;

    // Blueprint callable functions
    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void SetupForCondition(EConditionType Condition);

    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void StartVisualization();

    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void StopVisualization();

    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void EnableInteraction(bool bEnable);

    UFUNCTION(BlueprintImplementableEvent, Category = "Visualization")
    void OnVisualizationStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Visualization")
    void OnVisualizationStopped();

    UFUNCTION(BlueprintImplementableEvent, Category = "Visualization")
    void OnInteractionChanged(bool bEnabled);
};