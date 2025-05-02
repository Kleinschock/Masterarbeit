#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRPawnController.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AVRPawnController : public AActor
{
    GENERATED_BODY()

public:
    AVRPawnController();

protected:
    virtual void BeginPlay() override;

    UPROPERTY()
    APawn* VRPawn;

    UPROPERTY()
    class UMotionControllerComponent* RightController;

    // Configuration parameters
    UPROPERTY(EditAnywhere, Category = "Movement")
    float FlySpeed = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float SpeedModifier = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Scaling")
    float ScaleSpeed = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Scaling")
    float MinScale = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Scaling")
    float MaxScale = 5.0f;

public:
    virtual void Tick(float DeltaTime) override;

private:
    // Input handlers
    void MoveForward(float Value);
    void MoveRight(float Value);
    void ScaleUp();
    void ScaleDown();
    void ScaleUpReleased();
    void ScaleDownReleased();
    void SpeedBoostPressed();
    void SpeedBoostReleased();

    float CurrentSpeedModifier = 1.0f;
    bool bIsSpeedBoostActive = false;
    bool bIsScalingUp = false;
    bool bIsScalingDown = false;
    float ForwardInput = 0.0f;
    float RightInput = 0.0f;
};