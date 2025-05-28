// MovementLoggerComponent.cpp
#include "MovementLoggerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "MotionControllerComponent.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/Engine.h" // For GEngine

#include "XRTrackingSystemBase.h" // Dieser Header sollte FXRMotionControllerBase in UE 5.3 definieren

// ---> HIER EINFÜGEN <---
#include "XRMotionControllerBase.h"

#include "IMotionController.h" // Enthält FXRMotionControllerBase und die Source IDs


DEFINE_LOG_CATEGORY_STATIC(LogMovementLogger, Log, All);

UMovementLoggerComponent::UMovementLoggerComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // We'll use a timer
}

void UMovementLoggerComponent::BeginPlay()
{
    Super::BeginPlay();
    FindAndCacheMotionControllers();
}

void UMovementLoggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLoggingPeriod(); // Ensure timer is stopped
    // Consider if logs should be auto-saved if EndPlay is called unexpectedly
    // For now, we rely on explicit FinalizeAndSaveLogs call from StudyGameController.
    Super::EndPlay(EndPlayReason);
}

void UMovementLoggerComponent::FindAndCacheMotionControllers()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogMovementLogger, Warning, TEXT("FindAndCacheMotionControllers: Owner is null. Cannot find motion controllers."));
        return;
    }

    TArray<UMotionControllerComponent*> MotionControllers;
    Owner->GetComponents<UMotionControllerComponent>(MotionControllers);

    for (UMotionControllerComponent* MC : MotionControllers)
    {
        if (MC)
        {
            if (MC->MotionSource == FXRMotionControllerBase::LeftHandSourceId)
            {
                LeftMotionController = MC;
                UE_LOG(LogMovementLogger, Log, TEXT("Found Left Motion Controller: %s"), *MC->GetName());
            }
            else if (MC->MotionSource == FXRMotionControllerBase::RightHandSourceId)
            {
                RightMotionController = MC;
                UE_LOG(LogMovementLogger, Log, TEXT("Found Right Motion Controller: %s"), *MC->GetName());
            }
        }
    }

    if (!LeftMotionController.IsValid())
    {
        UE_LOG(LogMovementLogger, Warning, TEXT("Left Motion Controller not found on owner %s."), *Owner->GetName());
    }
    if (!RightMotionController.IsValid())
    {
        UE_LOG(LogMovementLogger, Warning, TEXT("Right Motion Controller not found on owner %s."), *Owner->GetName());
    }
}


void UMovementLoggerComponent::InitializeLoggingSession(int32 InParticipantID, const FString& InBetweenCondition, const FString& InGroupType)
{
    CurrentParticipantID = InParticipantID;
    CurrentBetweenCondition = InBetweenCondition;
    CurrentGroupType = InGroupType;
    DataBuffer.Empty(); // Clear any previous data
    bIsLoggingActive = false; // Ensure logging is off initially
    GetWorld()->GetTimerManager().ClearTimer(LoggingTimerHandle); // Clear any existing timer

    UE_LOG(LogMovementLogger, Log, TEXT("Movement Logger Initialized for PID: %d, Env: %s, Group: %s"), CurrentParticipantID, *CurrentBetweenCondition, *CurrentGroupType);
}

void UMovementLoggerComponent::StartLoggingPeriod(int32 InConditionOrder, const FString& InConditionName, const FString& InStudyPhase)
{
    if (bIsLoggingActive)
    {
        UE_LOG(LogMovementLogger, Warning, TEXT("StartLoggingPeriod called while already logging. Current Phase: %s. New Phase: %s. Will continue with new phase info."), *CurrentStudyPhaseName, *InStudyPhase);
        // Optionally stop and restart, but for now, just update info and continue
    }

    CurrentConditionOrder = InConditionOrder;
    CurrentConditionName = InConditionName;
    CurrentStudyPhaseName = InStudyPhase;

    if (LoggingIntervalSeconds <= 0.0f)
    {
        UE_LOG(LogMovementLogger, Error, TEXT("LoggingIntervalSeconds is zero or negative (%.2f). Cannot start logging timer."), LoggingIntervalSeconds);
        bIsLoggingActive = false;
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(LoggingTimerHandle, this, &UMovementLoggerComponent::RecordMovementDataTick, LoggingIntervalSeconds, true, 0.0f); // Start immediately
    bIsLoggingActive = true;
    UE_LOG(LogMovementLogger, Log, TEXT("Started movement logging period. Interval: %.2fs. ConditionOrder: %d, ConditionName: %s, StudyPhase: %s"), LoggingIntervalSeconds, CurrentConditionOrder, *CurrentConditionName, *CurrentStudyPhaseName);
}

void UMovementLoggerComponent::StopLoggingPeriod()
{
    if (bIsLoggingActive)
    {
        GetWorld()->GetTimerManager().ClearTimer(LoggingTimerHandle);
        bIsLoggingActive = false;
        UE_LOG(LogMovementLogger, Log, TEXT("Stopped movement logging period for Phase: %s."), *CurrentStudyPhaseName);
    }
}

void UMovementLoggerComponent::RecordMovementDataTick()
{
    if (!bIsLoggingActive) return; // Should not happen if timer is managed correctly

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogMovementLogger, Error, TEXT("RecordMovementDataTick: Owner is not a Pawn. Cannot get HMD/Controller data."));
        StopLoggingPeriod(); // Stop logging if we can't get data
        return;
    }

    APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
    if (!PC)
    {
        UE_LOG(LogMovementLogger, Error, TEXT("RecordMovementDataTick: PlayerController is null. Cannot get HMD data."));
        StopLoggingPeriod();
        return;
    }

    FMovementDataEntry NewEntry;
    NewEntry.Timestamp = GetWorld()->GetTimeSeconds();
    NewEntry.ParticipantID = CurrentParticipantID;
    NewEntry.BetweenCondition = CurrentBetweenCondition;
    NewEntry.GroupType = CurrentGroupType;
    NewEntry.ConditionOrder = CurrentConditionOrder;
    NewEntry.ConditionName = CurrentConditionName;
    NewEntry.StudyPhase = CurrentStudyPhaseName;

    // HMD Data
    if (GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
    {
        FQuat HMDOrientationQuat;
        FVector HMDPositionVec;
        if (GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDOrientationQuat, HMDPositionVec))
        {
            NewEntry.HMDPosition = HMDPositionVec;
            NewEntry.HMDRotation = HMDOrientationQuat.Rotator();
        }
        else
        {
            UE_LOG(LogMovementLogger, Warning, TEXT("RecordMovementDataTick: Failed to get HMD pose."));
        }
    }
    else // Fallback for non-XR or if system unavailable - might get camera location
    {
        if (PC->PlayerCameraManager)
        {
            NewEntry.HMDPosition = PC->PlayerCameraManager->GetCameraLocation();
            NewEntry.HMDRotation = PC->PlayerCameraManager->GetCameraRotation();
            // This might not be true HMD data in VR, but a fallback
            UE_LOG(LogMovementLogger, Verbose, TEXT("RecordMovementDataTick: Using PlayerCameraManager for HMD fallback."));
        }
    }


    // Left Controller Data
    if (LeftMotionController.IsValid() && LeftMotionController->IsTracked())
    {
        NewEntry.bLeftControllerTracked = true;
        NewEntry.LeftControllerPosition = LeftMotionController->GetComponentLocation();
        NewEntry.LeftControllerRotation = LeftMotionController->GetComponentRotation();
    }
    else
    {
        NewEntry.bLeftControllerTracked = false;
        // Positions/Rotations remain ZeroVector/ZeroRotator
    }

    // Right Controller Data
    if (RightMotionController.IsValid() && RightMotionController->IsTracked())
    {
        NewEntry.bRightControllerTracked = true;
        NewEntry.RightControllerPosition = RightMotionController->GetComponentLocation();
        NewEntry.RightControllerRotation = RightMotionController->GetComponentRotation();
    }
    else
    {
        NewEntry.bRightControllerTracked = false;
    }

    DataBuffer.Add(NewEntry);
    // UE_LOG(LogMovementLogger, Verbose, TEXT("Recorded movement data point. Buffer size: %d"), DataBuffer.Num());
}

void UMovementLoggerComponent::FinalizeAndSaveLogs()
{
    if (bIsLoggingActive)
    {
        StopLoggingPeriod(); // Ensure logging is stopped before saving
    }

    if (DataBuffer.Num() == 0)
    {
        UE_LOG(LogMovementLogger, Log, TEXT("FinalizeAndSaveLogs: No movement data buffered. Nothing to save."));
        return;
    }

    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("StudyResults/MovementData/");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        if (!PlatformFile.CreateDirectoryTree(*SaveDirectory)) // CreateDirectoryTree for nested paths
        {
            UE_LOG(LogMovementLogger, Error, TEXT("Failed to create directory: %s"), *SaveDirectory);
            return;
        }
    }

    // Sanitize BetweenCondition and GroupType for filename (replace spaces, etc.)
    FString SanitizedBetweenCond = CurrentBetweenCondition.Replace(TEXT(" "), TEXT("_"));
    FString SanitizedGroupType = CurrentGroupType.Replace(TEXT(" "), TEXT("_"));

    FString Filename = FString::Printf(TEXT("MovementData_VP%d_%s_Group%s.csv"), CurrentParticipantID, *SanitizedBetweenCond, *SanitizedGroupType);
    FString FilePath = SaveDirectory + Filename;

    FString CSVContent = FMovementDataEntry::GetCSVHeader();
    for (const FMovementDataEntry& Entry : DataBuffer)
    {
        CSVContent += Entry.ToString() + TEXT("\n");
    }

    if (FFileHelper::SaveStringToFile(CSVContent, *FilePath))
    {
        UE_LOG(LogMovementLogger, Log, TEXT("Movement logs saved successfully to: %s (%d entries)"), *FilePath, DataBuffer.Num());
    }
    else
    {
        UE_LOG(LogMovementLogger, Error, TEXT("Failed to save movement logs to: %s"), *FilePath);
    }
    // ClearBufferedData(); // Optionally clear after saving, or let Initialize handle it for next session
}

void UMovementLoggerComponent::ClearBufferedData()
{
    DataBuffer.Empty();
    UE_LOG(LogMovementLogger, Log, TEXT("Movement data buffer cleared."));
}

FName UMovementLoggerComponent::GetXRSystemName() const
{
    if (GEngine && GEngine->XRSystem.IsValid())
    {
        return GEngine->XRSystem->GetSystemName();
    }
    return NAME_None;
}