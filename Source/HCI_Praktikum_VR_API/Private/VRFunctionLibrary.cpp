#include "VRFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#include "MotionControllerComponent.h"

void UVRFunctionLibrary::HandleVRMovement(
    APawn* VRPawn,
    UMotionControllerComponent* RightController,
    float JoystickX,
    float JoystickY,
    float DeltaTime,
    float FlySpeed,
    bool IsSpeedBoostActive,
    float SpeedBoostMultiplier)
{
    if (!VRPawn || !RightController) return;

    // Only move if there's significant joystick input
    if (FMath::Abs(JoystickX) > 0.1f || FMath::Abs(JoystickY) > 0.1f)
    {
        // Get controller's transform
        FRotator ControllerRotation = RightController->GetComponentRotation();

        // Create forward direction from controller's rotation
        FVector Direction = ControllerRotation.Vector();

        // If you want strafing, add the right vector component
        if (FMath::Abs(JoystickX) > 0.1f)
        {
            FVector RightDirection = FRotationMatrix(ControllerRotation).GetScaledAxis(EAxis::Y);
            Direction = Direction * JoystickY + RightDirection * JoystickX;
            Direction.Normalize();
        }
        else
        {
            // Just use forward direction with joystick Y intensity
            Direction = Direction * JoystickY;
        }

        // Apply speed boost if active
        float CurrentSpeed = FlySpeed;
        if (IsSpeedBoostActive)
        {
            CurrentSpeed *= SpeedBoostMultiplier;
        }

        // Move the pawn
        FVector NewLocation = VRPawn->GetActorLocation() + Direction * CurrentSpeed * DeltaTime;
        VRPawn->SetActorLocation(NewLocation, true);
    }
}

void UVRFunctionLibrary::ScaleVRPawn(
    APawn* VRPawn,
    float DeltaTime,
    float ScaleSpeed,
    bool ScaleUp,
    float MinScale,
    float MaxScale,
    FVector& TargetScale)
{
    if (!VRPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("ScaleVRPawn: VRPawn is null."));
        return;
    }

    // Get the current scale
    FVector CurrentScale = VRPawn->GetActorScale3D();

    // Determine the desired scale increment
    FVector ScaleIncrement = FVector(ScaleSpeed * DeltaTime);

    if (ScaleUp)
    {
        // Increase target scale uniformly
        TargetScale = CurrentScale + ScaleIncrement;

        // Clamp each axis to MaxScale
        TargetScale.X = FMath::Min(TargetScale.X, MaxScale);
        TargetScale.Y = FMath::Min(TargetScale.Y, MaxScale);
        TargetScale.Z = FMath::Min(TargetScale.Z, MaxScale);
    }
    else
    {
        // Decrease target scale uniformly
        TargetScale = CurrentScale - ScaleIncrement;

        // Clamp each axis to MinScale
        TargetScale.X = FMath::Max(TargetScale.X, MinScale);
        TargetScale.Y = FMath::Max(TargetScale.Y, MinScale);
        TargetScale.Z = FMath::Max(TargetScale.Z, MinScale);
    }

    // Smoothly interpolate to the target scale
    FVector NewScale = FMath::VInterpTo(CurrentScale, TargetScale, DeltaTime, ScaleSpeed);

    // Apply the new scale
    VRPawn->SetActorScale3D(NewScale);

    // Optional: Add visual feedback (e.g., particle effects) here
    // Example:
    /*
    UParticleSystemComponent* ParticleSystem = VRPawn->FindComponentByClass<UParticleSystemComponent>();
    if (ParticleSystem)
    {
        ParticleSystem->ActivateSystem();
    }
    */
}