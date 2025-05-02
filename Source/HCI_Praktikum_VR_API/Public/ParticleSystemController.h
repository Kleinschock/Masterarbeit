// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "MotionControllerComponent.h"
#include "ParticleSystemController.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AParticleSystemController : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AParticleSystemController();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Function to select/grab the particle system
    UFUNCTION(BlueprintCallable, Category = "VR Interaction")
    void GrabParticleSystem();

    // Function to release the particle system
    UFUNCTION(BlueprintCallable, Category = "VR Interaction")
    void ReleaseParticleSystem();

    // Function to update particle system position
    UFUNCTION(BlueprintCallable, Category = "VR Interaction")
    void UpdateParticleSystemPosition();

    // Function to scale the particle system
    UFUNCTION(BlueprintCallable, Category = "VR Interaction")
    void ScaleParticleSystem(float ScaleFactor);

    // Function to modify emission rate
    UFUNCTION(BlueprintCallable, Category = "VR Interaction")
    void SetEmissionRate(float Rate);

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Reference to the Niagara particle system we want to manipulate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Interaction")
    UNiagaraComponent* TargetNiagaraSystem;

    // Reference to the motion controller
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Interaction")
    UMotionControllerComponent* MotionController;

    // Grab radius for particle system selection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Interaction")
    float GrabRadius = 100.0f;

    // Name of the spawn rate parameter in the Niagara system
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Interaction")
    FString SpawnRateParameterName = "SpawnRate";

private:
    // Flag to track if we're currently holding the particle system
    bool bIsHoldingParticleSystem;

    // Initial offset between controller and particle system when grabbed
    FVector GrabOffset;
};