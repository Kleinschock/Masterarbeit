// Fill out your copyright notice in the Description page of Project Settings.

#include "Movement_Superman.h"
#include "GameFramework/Pawn.h"
#include "MotionControllerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"

// Sets default values for this component's properties
UMovement_Superman::UMovement_Superman()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.
    PrimaryComponentTick.bCanEverTick = true;
    FlyingSpeed = 500.0f;
    bIsFlying = false;
}

// Called when the game starts
void UMovement_Superman::BeginPlay()
{
    Super::BeginPlay();

    // Get the owner as a Character
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        // Get the Character Movement Component
        CharacterMovement = OwnerCharacter->GetCharacterMovement();

        // Find and store reference to the right motion controller component
        TArray<UMotionControllerComponent*> MotionControllers;
        OwnerCharacter->GetComponents<UMotionControllerComponent>(MotionControllers);

        // Find the right controller specifically
        for (auto* Controller : MotionControllers)
        {
            if (Controller && Controller->MotionSource == FName("Right"))
            {

                RightControllerReference = Controller;
                break;
            }
        }
    }
}

// Called every frame
void UMovement_Superman::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsFlying)
    {
        UpdateMovement(DeltaTime);
    }
}

void UMovement_Superman::StartFlying()
{
    bIsFlying = true;

    if (CharacterMovement)
    {
        // Modify character movement settings for flying
        CharacterMovement->SetMovementMode(MOVE_Flying);
        CharacterMovement->GravityScale = 0.0f;
    }
}

void UMovement_Superman::StopFlying()
{
    bIsFlying = false;

    if (CharacterMovement)
    {
        // Reset character movement settings
        CharacterMovement->SetMovementMode(MOVE_Walking);
        CharacterMovement->GravityScale = 1.0f;
    }
}

void UMovement_Superman::UpdateMovement(float DeltaTime)
{
    if (!RightControllerReference || !GetOwner())
    {
        return;
    }

    // Get the controller's forward vector
    FVector ControllerForward = RightControllerReference->GetForwardVector();

    // Calculate new velocity based on controller direction and flying speed
    FVector NewVelocity = ControllerForward * FlyingSpeed;

    if (CharacterMovement)
    {
        // Apply the new velocity
        CharacterMovement->Velocity = NewVelocity;
    }
    else
    {
        // Fallback if no character movement component: directly update location
        FVector NewLocation = GetOwner()->GetActorLocation() + (NewVelocity * DeltaTime);
        GetOwner()->SetActorLocation(NewLocation, true);
    }
}