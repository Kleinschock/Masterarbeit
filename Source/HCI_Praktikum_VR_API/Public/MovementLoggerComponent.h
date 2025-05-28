// MovementLoggerComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h" // Required for GetController()
#include "GameFramework/PlayerController.h" // Required for HMD data
#include "IXRTrackingSystem.h" // Required for HMD data and controller tracking status
#include "MotionControllerComponent.h" // Required for controller data
#include "TimerManager.h"
#include "StudyManager.h" // For EConditionType, EStudyPhase (if used directly, otherwise pass as string)
#include "MovementLoggerComponent.generated.h"

// Forward declaration for StudyGameController if needed for specific phase/condition info
class AStudyGameController;

UENUM(BlueprintType)
enum class ETrackingDeviceType : uint8
{
    HMD UMETA(DisplayName = "HMD"),
    LeftController UMETA(DisplayName = "Left Controller"),
    RightController UMETA(DisplayName = "Right Controller")
};

USTRUCT(BlueprintType)
struct FMovementDataEntry
{
    GENERATED_BODY()

    UPROPERTY() float Timestamp = 0.0f;
    UPROPERTY() int32 ParticipantID = 0;
    UPROPERTY() FString BetweenCondition;
    UPROPERTY() FString GroupType;
    UPROPERTY() int32 ConditionOrder = 0;
    UPROPERTY() FString ConditionName;
    UPROPERTY() FString StudyPhase;

    UPROPERTY() FVector HMDPosition = FVector::ZeroVector;
    UPROPERTY() FRotator HMDRotation = FRotator::ZeroRotator;

    UPROPERTY() bool bLeftControllerTracked = false;
    UPROPERTY() FVector LeftControllerPosition = FVector::ZeroVector;
    UPROPERTY() FRotator LeftControllerRotation = FRotator::ZeroRotator;

    UPROPERTY() bool bRightControllerTracked = false;
    UPROPERTY() FVector RightControllerPosition = FVector::ZeroVector;
    UPROPERTY() FRotator RightControllerRotation = FRotator::ZeroRotator;

    FString ToString() const
    {
        return FString::Printf(TEXT("%.3f,%d,%s,%s,%d,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f"),
            Timestamp,
            ParticipantID,
            *BetweenCondition,
            *GroupType,
            ConditionOrder,
            *ConditionName,
            *StudyPhase,
            HMDPosition.X, HMDPosition.Y, HMDPosition.Z,
            HMDRotation.Roll, HMDRotation.Pitch, HMDRotation.Yaw,
            bLeftControllerTracked ? 1 : 0,
            LeftControllerPosition.X, LeftControllerPosition.Y, LeftControllerPosition.Z,
            LeftControllerRotation.Roll, LeftControllerRotation.Pitch, LeftControllerRotation.Yaw,
            bRightControllerTracked ? 1 : 0,
            RightControllerPosition.X, RightControllerPosition.Y, RightControllerPosition.Z,
            RightControllerRotation.Roll, RightControllerRotation.Pitch, RightControllerRotation.Yaw
        );
    }

    static FString GetCSVHeader()
    {
        return TEXT("Timestamp,ParticipantID,BetweenCondition,GroupType,ConditionOrder,ConditionName,StudyPhase,HMD_X,HMD_Y,HMD_Z,HMD_Roll,HMD_Pitch,HMD_Yaw,LeftController_IsTracked,LeftController_X,LeftController_Y,LeftController_Z,LeftController_Roll,LeftController_Pitch,LeftController_Yaw,RightController_IsTracked,RightController_X,RightController_Y,RightController_Z,RightController_Roll,RightController_Pitch,RightController_Yaw\n");
    }
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent)) // <--- DIESE ZEILE PRÜFEN
class HCI_PRAKTIKUM_VR_API_API UMovementLoggerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMovementLoggerComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // --- Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Logging")
    float LoggingIntervalSeconds = 0.5f; // How often to log data

    // --- Public Control Functions ---
    UFUNCTION(BlueprintCallable, Category = "Movement Logging")
    void InitializeLoggingSession(int32 InParticipantID, const FString& InBetweenCondition, const FString& InGroupType);

    UFUNCTION(BlueprintCallable, Category = "Movement Logging")
    void StartLoggingPeriod(int32 InConditionOrder, const FString& InConditionName, const FString& InStudyPhase);

    UFUNCTION(BlueprintCallable, Category = "Movement Logging")
    void StopLoggingPeriod();

    UFUNCTION(BlueprintCallable, Category = "Movement Logging")
    void FinalizeAndSaveLogs(); // Saves all buffered data

    UFUNCTION(BlueprintCallable, Category = "Movement Logging")
    void ClearBufferedData();

private:
    // --- Internal State ---
    bool bIsLoggingActive = false;
    FTimerHandle LoggingTimerHandle;

    // --- Study Context (set during Initialize) ---
    int32 CurrentParticipantID = 0;
    FString CurrentBetweenCondition;
    FString CurrentGroupType;

    // --- Current Period Context (set during StartLoggingPeriod) ---
    int32 CurrentConditionOrder = 0;
    FString CurrentConditionName;
    FString CurrentStudyPhaseName;


    UPROPERTY() TArray<FMovementDataEntry> DataBuffer;

    // --- Helper Functions ---
    void RecordMovementDataTick();
    void FindAndCacheMotionControllers(); // Call in BeginPlay or on demand

    UPROPERTY() TWeakObjectPtr<UMotionControllerComponent> LeftMotionController;
    UPROPERTY() TWeakObjectPtr<UMotionControllerComponent> RightMotionController;

    // For HMD data if not using deprecated methods
    FName GetXRSystemName() const;
};