// Fill out your copyright notice in the Description page of Project Settings.
#include "ParticleSystemController.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

// Sets default values
AParticleSystemController::AParticleSystemController()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    // Create and setup the motion controller component
    MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
    RootComponent = MotionController;

    // Initialize variables
    bIsHoldingParticleSystem = false;
    GrabOffset = FVector::ZeroVector;
}

// Called when the game starts or when spawned
void AParticleSystemController::BeginPlay()
{
    Super::BeginPlay();
}

// Called every frame
void AParticleSystemController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsHoldingParticleSystem)
    {
        UpdateParticleSystemPosition();
    }
}

void AParticleSystemController::GrabParticleSystem()
{
    if (!TargetNiagaraSystem || bIsHoldingParticleSystem)
        return;

    // Get the distance between controller and particle system
    float Distance = FVector::Distance(
        MotionController->GetComponentLocation(),
        TargetNiagaraSystem->GetComponentLocation()
    );

    // Check if particle system is within grab radius
    if (Distance <= GrabRadius)
    {
        bIsHoldingParticleSystem = true;

        // Calculate and store the offset
        GrabOffset = TargetNiagaraSystem->GetComponentLocation() -
            MotionController->GetComponentLocation();

        // Optional: Visualize the grab
        DrawDebugSphere(
            GetWorld(),
            TargetNiagaraSystem->GetComponentLocation(),
            10.0f,
            12,
            FColor::Green,
            false,
            2.0f
        );
    }
}

void AParticleSystemController::ReleaseParticleSystem()
{
    bIsHoldingParticleSystem = false;
}

void AParticleSystemController::UpdateParticleSystemPosition()
{
    if (!TargetNiagaraSystem || !bIsHoldingParticleSystem)
        return;

    // Update particle system position based on controller movement
    FVector NewLocation = MotionController->GetComponentLocation() + GrabOffset;
    TargetNiagaraSystem->SetWorldLocation(NewLocation);
}

void AParticleSystemController::ScaleParticleSystem(float ScaleFactor)
{
    if (!TargetNiagaraSystem || !bIsHoldingParticleSystem)
        return;

    FVector CurrentScale = TargetNiagaraSystem->GetComponentScale();
    TargetNiagaraSystem->SetWorldScale3D(CurrentScale * ScaleFactor);
}

void AParticleSystemController::SetEmissionRate(float Rate)
{
    if (!TargetNiagaraSystem)
        return;

    // Set the spawn rate parameter in the Niagara system
    TargetNiagaraSystem->SetVariableFloat(FName("SpawnRate"), Rate);
}