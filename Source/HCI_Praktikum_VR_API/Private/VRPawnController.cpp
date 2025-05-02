#include "VRPawnController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "MotionControllerComponent.h"
#include "Kismet/GameplayStatics.h"

AVRPawnController::AVRPawnController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AVRPawnController::BeginPlay()
{
    Super::BeginPlay();

    // Get reference to the existing VR Pawn
    VRPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!VRPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("VRPawnController: VR Pawn not found!"));
        return;
    }

    // Find Right Motion Controller
    TArray<UMotionControllerComponent*> MotionControllers;
    VRPawn->GetComponents<UMotionControllerComponent>(MotionControllers);

    for (UMotionControllerComponent* Controller : MotionControllers)
    {
        if (Controller && Controller->MotionSource == FName("Right"))
        {
            RightController = Controller;
            break;
        }
    }

    if (!RightController)
    {
        UE_LOG(LogTemp, Warning, TEXT("VRPawnController: Right Motion Controller not found!"));
        return;
    }

    // Setup input bindings
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC && PC->InputComponent)
    {
        // Bind joystick input
        PC->InputComponent->BindAxis("MoveForward", this, &AVRPawnController::MoveForward);
        PC->InputComponent->BindAxis("MoveRight", this, &AVRPawnController::MoveRight);

        // Bind scaling buttons
        PC->InputComponent->BindAction("ScaleUp", IE_Pressed, this, &AVRPawnController::ScaleUp);
        PC->InputComponent->BindAction("ScaleUp", IE_Released, this, &AVRPawnController::ScaleUpReleased);
        PC->InputComponent->BindAction("ScaleDown", IE_Pressed, this, &AVRPawnController::ScaleDown);
        PC->InputComponent->BindAction("ScaleDown", IE_Released, this, &AVRPawnController::ScaleDownReleased);

        // Bind speed boost trigger
        PC->InputComponent->BindAction("SpeedBoost", IE_Pressed, this, &AVRPawnController::SpeedBoostPressed);
        PC->InputComponent->BindAction("SpeedBoost", IE_Released, this, &AVRPawnController::SpeedBoostReleased);
    }
}

void AVRPawnController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!VRPawn || !RightController) return;

    // Handle movement
    if (FMath::Abs(ForwardInput) > 0.1f || FMath::Abs(RightInput) > 0.1f)
    {
        FVector Direction = RightController->GetForwardVector() * ForwardInput +
            RightController->GetRightVector() * RightInput;
        Direction.Normalize();

        float CurrentFlySpeed = FlySpeed * CurrentSpeedModifier;
        FVector NewLocation = VRPawn->GetActorLocation() + Direction * CurrentFlySpeed * DeltaTime;
        VRPawn->SetActorLocation(NewLocation, true);
    }

    // Handle scaling
    if (bIsScalingUp)
    {
        FVector CurrentScale = VRPawn->GetActorScale3D();
        FVector NewScale = CurrentScale + FVector(ScaleSpeed * DeltaTime);
        NewScale = NewScale.GetClampedToMaxSize(MaxScale);
        VRPawn->SetActorScale3D(NewScale);
    }
    else if (bIsScalingDown)
    {
        FVector CurrentScale = VRPawn->GetActorScale3D();
        FVector NewScale = CurrentScale - FVector(ScaleSpeed * DeltaTime);
        NewScale.X = FMath::Max(NewScale.X, MinScale);
        NewScale.Y = FMath::Max(NewScale.Y, MinScale);
        NewScale.Z = FMath::Max(NewScale.Z, MinScale);
        VRPawn->SetActorScale3D(NewScale);
    }
}

void AVRPawnController::MoveForward(float Value)
{
    ForwardInput = Value;
}

void AVRPawnController::MoveRight(float Value)
{
    RightInput = Value;
}

void AVRPawnController::ScaleUp()
{
    bIsScalingUp = true;
}

void AVRPawnController::ScaleDown()
{
    bIsScalingDown = true;
}

void AVRPawnController::ScaleUpReleased()
{
    bIsScalingUp = false;
}

void AVRPawnController::ScaleDownReleased()
{
    bIsScalingDown = false;
}

void AVRPawnController::SpeedBoostPressed()
{
    if (!bIsSpeedBoostActive)
    {
        CurrentSpeedModifier = SpeedModifier;
        bIsSpeedBoostActive = true;
    }
}

void AVRPawnController::SpeedBoostReleased()
{
    if (bIsSpeedBoostActive)
    {
        CurrentSpeedModifier = 1.0f;
        bIsSpeedBoostActive = false;
    }
}