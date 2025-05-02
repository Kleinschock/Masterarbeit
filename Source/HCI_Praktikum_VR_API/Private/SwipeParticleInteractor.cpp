#include "SwipeParticleInteractor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USwipeParticleInteractor::USwipeParticleInteractor()
{
    PrimaryComponentTick.bCanEverTick = false;
    MaxSwipeDistance = 100.0f;
    MaxSwipeStrength = 1.0f;

    // Debug settings
    bShowDebug = true;           // Toggle for debug visualization
    DebugDuration = 2.0f;        // How long debug shapes stay visible
    DebugThickness = 2.0f;       // Line thickness for debug drawing
}

void USwipeParticleInteractor::BeginPlay()
{
    Super::BeginPlay();
    if (!NiagaraSystem)
    {
        ShowDebugMessage(FString("NiagaraSystem not set!"), FColor::Red);
        UE_LOG(LogTemp, Warning, TEXT("SwipeParticleInteractor: NiagaraSystem is not set. Please set it in the Details panel or via SetNiagaraSystem."));
    }
    else
    {
        ShowDebugMessage(FString("NiagaraSystem initialized"), FColor::Green);

    }
}

void USwipeParticleInteractor::SetNiagaraSystem(UNiagaraComponent* Niagara)
{
    NiagaraSystem = Niagara;
    if (NiagaraSystem)
    {
        ShowDebugMessage(FString("Niagara System Set Successfully"), FColor::Green);
        UE_LOG(LogTemp, Log, TEXT("SwipeParticleInteractor: NiagaraSystem set successfully."));
    }
    else
    {
        ShowDebugMessage(FString("Failed to Set Niagara System"), FColor::Red);
        UE_LOG(LogTemp, Warning, TEXT("SwipeParticleInteractor: Failed to set NiagaraSystem."));
    }
}

void USwipeParticleInteractor::InteractWithParticles(FVector SwipeOrigin, FVector SwipeDirection, float SwipeStrength)
{
    if (!NiagaraSystem)
    {
        ShowDebugMessage(FString("Niagara System is null!"), FColor::Red);
        UE_LOG(LogTemp, Warning, TEXT("SwipeParticleInteractor: NiagaraSystem is null. Cannot interact with particles."));
        return;
    }

    // Clamp the swipe strength
    SwipeStrength = FMath::Clamp(SwipeStrength, 0.0f, MaxSwipeStrength);

    // Debug visualization
    if (bShowDebug)
    {
        // Draw debug sphere at swipe origin
        DrawDebugSphere(
            GetWorld(),
            SwipeOrigin,
            10.0f,  // Radius
            12,     // Segments
            FColor::Yellow,
            false,  // Persistent
            DebugDuration,
            0,      // DepthPriority
            DebugThickness
        );

        // Draw debug line showing swipe direction
        DrawDebugLine(
            GetWorld(),
            SwipeOrigin,
            SwipeOrigin + (SwipeDirection * 100.0f * SwipeStrength),
            FColor::Blue,
            false,
            DebugDuration,
            0,
            DebugThickness
        );

        // Show debug values
        FString DebugText = FString::Printf(TEXT("Swipe Strength: %.2f"), SwipeStrength);
        ShowDebugMessage(DebugText, FColor::White);
    }

    // Log the interaction for debugging
    UE_LOG(LogTemp, Log, TEXT("Interacting with particles: Origin: %s, Direction: %s, Strength: %f"),
        *SwipeOrigin.ToString(), *SwipeDirection.ToString(), SwipeStrength);

    // Set parameters in the Niagara system
    NiagaraSystem->SetVectorParameter(FName("User.SwipeDirection"), SwipeDirection);
    NiagaraSystem->SetFloatParameter(FName("User.SwipeStrength"), SwipeStrength);
    NiagaraSystem->SetVectorParameter(FName("User.SwipeOrigin"), SwipeOrigin);

    // Show success message
    ShowDebugMessage(FString("Particle Interaction Applied"), FColor::Green);
}

void USwipeParticleInteractor::ShowDebugMessage(const FString& Message, FColor Color)
{
    if (!bShowDebug)
        return;

    // Get the world
    UWorld* World = GetWorld();
    if (!World)
        return;

    // Show on-screen debug message
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,                    // Key (-1 = unique key)
            DebugDuration,        // Duration
            Color,                // Color
            Message              // Message
        );
    }

    // Optional: Draw debug string in 3D space
    // Get component location for 3D text placement
    FVector Location = GetOwner()->GetActorLocation();
    Location.Z += 100.0f; // Offset above the actor

    DrawDebugString(
        World,
        Location,
        Message,
        nullptr,  // Actor
        Color,    // Color
        DebugDuration,
        true,     // bDrawShadow
        1.0f      // Text scale
    );
}

// Add these to your header file (SwipeParticleInteractor.h):
/*
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
*/