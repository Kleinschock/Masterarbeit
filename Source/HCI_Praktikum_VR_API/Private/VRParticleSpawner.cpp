
// VRParticleSpawner.cpp
#include "VRParticleSpawner.h"

AVRParticleSpawner::AVRParticleSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    // Initialize component
    ParticleSystemReference = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ParticleSystem"));
    RootComponent = ParticleSystemReference;
}

void AVRParticleSpawner::BeginPlay()
{
    Super::BeginPlay();
    TimeSinceLastSpawn = 0.0f;
    ControllerLocation = FVector::ZeroVector;
}

void AVRParticleSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TimeSinceLastSpawn += DeltaTime;
    float SpawnInterval = 1.0f / SpawnRate;

    if (TimeSinceLastSpawn >= SpawnInterval)
    {
        // Calculate random point in sphere
        float Theta = FMath::RandRange(0.0f, 2.0f * PI);
        float Phi = FMath::RandRange(0.0f, PI);
        float R = FMath::RandRange(0.0f, SphereRadius);

        FVector SpawnOffset(
            R * FMath::Sin(Phi) * FMath::Cos(Theta),
            R * FMath::Sin(Phi) * FMath::Sin(Theta),
            R * FMath::Cos(Phi)
        );

        FVector SpawnLocation = ControllerLocation + SpawnOffset;

        if (ParticleSystemReference)
        {
            ParticleSystemReference->SetVectorParameter(FName("SpawnLocation"), SpawnLocation);
            ParticleSystemReference->SetFloatParameter(FName("SpawnRadius"), SphereRadius);
        }

        TimeSinceLastSpawn = 0.0f;
    }
}

void AVRParticleSpawner::SetControllerLocation(const FVector& NewLocation)
{
    ControllerLocation = NewLocation;
}