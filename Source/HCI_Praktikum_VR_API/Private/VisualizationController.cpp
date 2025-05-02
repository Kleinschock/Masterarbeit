// VisualizationController.cpp
#include "VisualizationController.h"
#include "Particles/ParticleSystemComponent.h"

AVisualizationController::AVisualizationController()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create and set up the particle system component
    ParticleSystem = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleSystem"));
    RootComponent = ParticleSystem;

    bInteractionEnabled = false;
    CurrentCondition = EConditionType::Control;
}

void AVisualizationController::BeginPlay()
{
    Super::BeginPlay();

    // Initially stop the particle system
    ParticleSystem->DeactivateSystem();
}

void AVisualizationController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AVisualizationController::SetupForCondition(EConditionType Condition)
{
    CurrentCondition = Condition;

    // Configure the visualization based on the condition
    switch (Condition)
    {
    case EConditionType::Control:
        // Basic visualization setup for control condition
        // No interaction in control condition
        EnableInteraction(false);
        break;

    case EConditionType::NoInteraction:
        // Enhanced visualization setup for no interaction condition
        // No interaction in this condition
        EnableInteraction(false);
        break;

    case EConditionType::WithInteraction:
        // Enhanced visualization setup for interaction condition
        // Enable interaction
        EnableInteraction(true);
        break;
    }
}

void AVisualizationController::StartVisualization()
{
    // Activate the particle system
    ParticleSystem->ActivateSystem();

    // Call the blueprint event
    OnVisualizationStarted();
}

void AVisualizationController::StopVisualization()
{
    // Deactivate the particle system
    ParticleSystem->DeactivateSystem();

    // Call the blueprint event
    OnVisualizationStopped();
}

void AVisualizationController::EnableInteraction(bool bEnable)
{
    bInteractionEnabled = bEnable;

    // Call the blueprint event
    OnInteractionChanged(bEnable);
}