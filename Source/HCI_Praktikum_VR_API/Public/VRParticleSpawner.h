// VRParticleSpawner.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "VRParticleSpawner.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AVRParticleSpawner : public AActor
{
	GENERATED_BODY()

public:
	AVRParticleSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	float SphereRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Settings")
	float SpawnRate = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Particle Spawning")
	void SetControllerLocation(const FVector& NewLocation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle System")
	UNiagaraComponent* ParticleSystemReference;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	FVector ControllerLocation;
	float TimeSinceLastSpawn;
};
