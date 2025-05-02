// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Movement_Superman.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API UMovement_Superman : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UMovement_Superman();

    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Function to start flying mode
    UFUNCTION(BlueprintCallable, Category = "VR Movement")
    void StartFlying();

    // Function to stop flying mode
    UFUNCTION(BlueprintCallable, Category = "VR Movement")
    void StopFlying();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

private:
    // Speed multiplier for flying movement
    UPROPERTY(EditAnywhere, Category = "VR Movement", meta = (AllowPrivateAccess = "true"))
    float FlyingSpeed = 500.0f;

    // Reference to the motion controller component
    UPROPERTY()
    class UMotionControllerComponent* RightControllerReference;

    // Boolean to track if we're currently flying
    bool bIsFlying = false;

    // Function to handle movement updates
    void UpdateMovement(float DeltaTime);

    // Reference to the character's movement component
    UPROPERTY()
    class UCharacterMovementComponent* CharacterMovement;
};