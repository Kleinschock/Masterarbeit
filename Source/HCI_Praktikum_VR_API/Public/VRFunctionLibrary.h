#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRFunctionLibrary.generated.h"

UCLASS()
class HCI_PRAKTIKUM_VR_API_API UVRFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Movement function - call this from Tick in your VR Pawn blueprint
    UFUNCTION(BlueprintCallable, Category = "VR Functions")
    static void HandleVRMovement(
        APawn* VRPawn,
        UMotionControllerComponent* RightController,
        float JoystickX,
        float JoystickY,
        float DeltaTime,
        float FlySpeed,
        bool IsSpeedBoostActive,
        float SpeedBoostMultiplier = 2.0f
    );
    UFUNCTION(BlueprintCallable, Category = "VR Functions")
    static void ScaleVRPawn(
        APawn* VRPawn,
        float DeltaTime,
        float ScaleSpeed,
        bool ScaleUp,
        float MinScale,
        float MaxScale,
        FVector& TargetScale);
};