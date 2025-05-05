// --- START OF FILE InteractionLoggerComponent.h ---
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h" // Required for EControllerHand
#include "StudyGameController.h" // Include StudyGameController for context enums
#include "InteractionLoggerComponent.generated.h"

// Forward declaration
class AStudyGameController;

// Structure to hold all data for a single log entry, matching research recommendations
// Marked as BlueprintType for potential future reflection/use, though primarily C++ driven
USTRUCT(BlueprintType)
struct FInteractionLogEntry
{
    GENERATED_BODY()

    // Using ISO 8601 format for timestamps (YYYY-MM-DDTHH:MM:SS.ffffffZ) - Robust and standard
    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString TimestampISO8601;

    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString EventType; // "Press" or "Release"

    // Use FName for Action Mapping name - efficient string comparison/storage
    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FName ButtonID;

    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString ControllerID; // "Left" or "Right"

    // Duration in seconds, only relevant for "Release" events
    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    double DurationSeconds = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString ParticipantID;

    // Could be derived from ParticipantID + Start Time, or just Participant ID if only one session per run
    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString SessionID;

    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString ConditionID; // e.g., "Control", "NoInteraction", "WithInteraction", "GeneralTutorial", "InteractionTutorial"

    UPROPERTY(BlueprintReadWrite, Category = "Logging")
    FString AppState; // e.g., "MainMenu", "Running", "SAM_Evaluation", "Tutorial_LeftSpawn" - More specific context

    // Constructor for convenience
    FInteractionLogEntry() = default;
};


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API UInteractionLoggerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionLoggerComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    /**
     * Initializes the logger component. Should be called once after the Study Manager is initialized.
     * Creates the log file path based on Participant ID.
     * @param InParticipantID The unique ID for the current participant.
     * @param Controller The StudyGameController instance for context fetching.
     */
    UFUNCTION(BlueprintCallable, Category = "Interaction Logging")
    void InitializeLogging(const FString& InParticipantID, AStudyGameController* Controller);

    /**
     * Logs a button interaction event (press or release).
     * Calculates duration for release events.
     * @param ActionName The FName of the Input Action being triggered (e.g., "IA_LeftGrab").
     * @param Hand Which controller hand triggered the event.
     * @param bIsPress True if the button was pressed, false if released/canceled.
     */
    UFUNCTION(BlueprintCallable, Category = "Interaction Logging") // Although primarily called via StudyController, keep BP callable for flexibility
        void LogInteractionEvent(FName ActionName, EControllerHand Hand, bool bIsPress);


private:
    /** Stores the FPlatformTime::Seconds() timestamp when a button is pressed, keyed by ActionName + Hand */
    TMap<FString, double> ActivePressTimestamps;

    /** Full path to the log file for the current session. */
    FString LogFilePath;

    /** Flag to ensure InitializeLogging has been called. */
    bool bIsInitialized = false;

    /** Cached Participant ID. */
    FString ParticipantID;

    /** Cached Session ID. */
    FString SessionID;

    /** Weak pointer to the Study Game Controller to fetch context safely. */
    TWeakObjectPtr<AStudyGameController> StudyControllerRef;

    /** Handles JSON serialization and writing the log entry to the file. */
    void WriteLogEntryToFile(const FInteractionLogEntry& LogEntry);

    /** Helper to generate a unique session ID. */
    FString GenerateSessionID(const FString& InParticipantID) const;

    /** Helper to get the current study condition string. */
    FString GetCurrentConditionID() const;

    /** Helper to get the detailed current application state string. */
    FString GetCurrentAppState() const;

    /** Helper to convert EControllerHand to string. */
    FString GetControllerIDString(EControllerHand Hand) const;

    /** Helper to create composite key for timestamp map */
    FString GetTimestampMapKey(FName ActionName, EControllerHand Hand) const;
};
// --- END OF FILE InteractionLoggerComponent.h ---