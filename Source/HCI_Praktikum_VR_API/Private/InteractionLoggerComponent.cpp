// --- START OF FILE InteractionLoggerComponent.cpp ---
#include "InteractionLoggerComponent.h"
#include "StudyGameController.h"
#include "StudyManager.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h" // <------------- ADDED THIS CORRECT INCLUDE based on documentation
#include "UObject/EnumProperty.h"


DEFINE_LOG_CATEGORY_STATIC(LogInteractionLogger, Log, All);

UInteractionLoggerComponent::UInteractionLoggerComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // No tick needed currently
}

void UInteractionLoggerComponent::BeginPlay()
{
    Super::BeginPlay();
    // Initialization is deferred to InitializeLogging, called by StudyGameController
}

void UInteractionLoggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear any lingering press timestamps if the component is destroyed mid-press
    ActivePressTimestamps.Empty();
    bIsInitialized = false;
    LogFilePath = ""; // Clear file path
    StudyControllerRef.Reset(); // Clear reference

    Super::EndPlay(EndPlayReason);
}

void UInteractionLoggerComponent::InitializeLogging(const FString& InParticipantID, AStudyGameController* Controller)
{
    if (bIsInitialized)
    {
        UE_LOG(LogInteractionLogger, Warning, TEXT("InteractionLoggerComponent::InitializeLogging called again for Participant %s. Ignoring."), *InParticipantID);
        return;
    }

    if (InParticipantID.IsEmpty() || !IsValid(Controller))
    {
        UE_LOG(LogInteractionLogger, Error, TEXT("InteractionLoggerComponent::InitializeLogging FAILED - Invalid ParticipantID ('%s') or StudyGameController."), *InParticipantID);
        return;
    }

    ParticipantID = InParticipantID;
    SessionID = GenerateSessionID(ParticipantID); // Generate a session ID
    StudyControllerRef = Controller;

    // Construct file path
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("InteractionLogs/");
    FString FileName = FString::Printf(TEXT("InteractionLog_P%s_S%s.log"), *ParticipantID, *SessionID); // Use .log or .jsonl extension
    LogFilePath = FPaths::ConvertRelativePathToFull(SaveDirectory + FileName);

    // Ensure directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        if (!PlatformFile.CreateDirectory(*SaveDirectory))
        {
            UE_LOG(LogInteractionLogger, Error, TEXT("Failed to create interaction log directory: %s"), *SaveDirectory);
            return; // Cannot log without directory
        }
    }

    // Optional: Write a header line to indicate a new session start (though JSON Lines format typically avoids headers)
    // FString Header = FString::Printf(TEXT("{\"EventType\":\"SessionStart\", \"TimestampISO8601\":\"%s\", \"ParticipantID\":\"%s\", \"SessionID\":\"%s\"}\n"),
    //     *FDateTime::UtcNow().ToIso8601(), *ParticipantID, *SessionID);
    // FFileHelper::SaveStringToFile(Header, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

    bIsInitialized = true;
    UE_LOG(LogInteractionLogger, Log, TEXT("Interaction Logger Initialized for Participant %s. Logging to: %s"), *ParticipantID, *LogFilePath);
}

FString UInteractionLoggerComponent::GenerateSessionID(const FString& InParticipantID) const
{
    // Simple session ID based on participant and start time
    // FDateTime is available now with the correct include
    return FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
}

FString UInteractionLoggerComponent::GetTimestampMapKey(FName ActionName, EControllerHand Hand) const
{
    return FString::Printf(TEXT("%s_%s"), *ActionName.ToString(), (Hand == EControllerHand::Left ? TEXT("L") : TEXT("R")));
}


void UInteractionLoggerComponent::LogInteractionEvent(FName ActionName, EControllerHand Hand, bool bIsPress)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogInteractionLogger, Warning, TEXT("LogInteractionEvent called before logger was initialized. Action: %s"), *ActionName.ToString());
        return;
    }
    if (!StudyControllerRef.IsValid())
    {
        UE_LOG(LogInteractionLogger, Warning, TEXT("LogInteractionEvent called but StudyControllerRef is invalid. Action: %s"), *ActionName.ToString());
        return;
    }

    double CurrentTime = FPlatformTime::Seconds();
    FDateTime CurrentTimestampUTC = FDateTime::UtcNow();
    FString TimestampMapKey = GetTimestampMapKey(ActionName, Hand);

    FInteractionLogEntry LogEntry;
    LogEntry.TimestampISO8601 = CurrentTimestampUTC.ToIso8601(); // High precision ISO 8601 UTC timestamp
    LogEntry.ButtonID = ActionName;
    LogEntry.ControllerID = GetControllerIDString(Hand);
    LogEntry.ParticipantID = ParticipantID;
    LogEntry.SessionID = SessionID;
    LogEntry.ConditionID = GetCurrentConditionID();
    LogEntry.AppState = GetCurrentAppState();
    LogEntry.DurationSeconds = 0.0; // Default duration

    if (bIsPress)
    {
        LogEntry.EventType = TEXT("Press");
        // Store the press time
        ActivePressTimestamps.Add(TimestampMapKey, CurrentTime);
        UE_LOG(LogInteractionLogger, Verbose, TEXT("Logged PRESS: %s (%s) at %f"), *ActionName.ToString(), *LogEntry.ControllerID, CurrentTime);
    }
    else // Release or Canceled
    {
        LogEntry.EventType = TEXT("Release");
        // Find the corresponding press time
        double PressTime = 0.0;
        if (ActivePressTimestamps.RemoveAndCopyValue(TimestampMapKey, PressTime))
        {
            LogEntry.DurationSeconds = CurrentTime - PressTime;
            UE_LOG(LogInteractionLogger, Verbose, TEXT("Logged RELEASE: %s (%s) at %f, Duration: %.4f"),
                *ActionName.ToString(), *LogEntry.ControllerID, CurrentTime, LogEntry.DurationSeconds);
        }
        else
        {
            // Release event without a corresponding press found (could happen if logging started mid-press)
            UE_LOG(LogInteractionLogger, Warning, TEXT("Release event for '%s' (%s) logged without corresponding press timestamp."), *ActionName.ToString(), *LogEntry.ControllerID);
            LogEntry.DurationSeconds = -1.0; // Indicate invalid duration
        }
    }

    // Write the entry to the file
    WriteLogEntryToFile(LogEntry);
}

void UInteractionLoggerComponent::WriteLogEntryToFile(const FInteractionLogEntry& LogEntry)
{
    if (!bIsInitialized || LogFilePath.IsEmpty())
    {
        UE_LOG(LogInteractionLogger, Error, TEXT("Attempted to write log entry but logger is not initialized or file path is empty."));
        return;
    }

    // Convert UStruct to Json Object
    TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(LogEntry);

    if (!JsonObject.IsValid())
    {
        UE_LOG(LogInteractionLogger, Error, TEXT("Failed to convert FInteractionLogEntry to JSON object."));
        return;
    }

    // Serialize Json Object to String
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogInteractionLogger, Error, TEXT("Failed to serialize JSON object to string."));
        return;
    }

    // Append the JSON string (as a single line) to the file
    JsonString += TEXT("\n"); // Ensure each entry is on a new line (JSON Lines format)

    // Append to file
    // Note: FFileHelper::SaveStringToFile with Append is simple but reads the whole file on some platforms.
    // For very high frequency logging, a dedicated FArchive might be better, but this is usually sufficient.
    if (!FFileHelper::SaveStringToFile(JsonString, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
    {
        UE_LOG(LogInteractionLogger, Error, TEXT("Failed to write log entry to file: %s"), *LogFilePath);
        // Consider adding a flag to stop trying if writing fails repeatedly
    }
}

FString UInteractionLoggerComponent::GetControllerIDString(EControllerHand Hand) const
{
    return (Hand == EControllerHand::Left) ? TEXT("Left") : TEXT("Right");
}

// --- Context Fetching Helpers ---

FString UInteractionLoggerComponent::GetCurrentConditionID() const
{
    if (StudyControllerRef.IsValid())
    {
        AStudyGameController* SC = StudyControllerRef.Get();
        // Use GetConditionName() which handles different phases/tutorials correctly
        return SC->GetConditionName();
        /* // Alternative Direct Access:
        EConditionType CurrentCond = SC->CurrentCondition; // Get the condition type enum
        const UEnum* EnumPtr = StaticEnum<EConditionType>();
        if (EnumPtr) {
            return EnumPtr->GetDisplayNameTextByValue((int64)CurrentCond).ToString();
        } */
    }
    return TEXT("Unknown_NoControllerRef");
}

FString UInteractionLoggerComponent::GetCurrentAppState() const
{
    if (StudyControllerRef.IsValid())
    {
        AStudyGameController* SC = StudyControllerRef.Get();
        EStudyPhase CurrentPhase = SC->CurrentPhase;
        const UEnum* PhaseEnum = StaticEnum<EStudyPhase>();
        FString PhaseStr = PhaseEnum ? PhaseEnum->GetDisplayNameTextByValue((int64)CurrentPhase).ToString() : FString::Printf(TEXT("UnknownPhase_%d"), (int32)CurrentPhase);

        // Add more detail if in a tutorial phase
        if (CurrentPhase == EStudyPhase::GeneralTutorial)
        {
            const UEnum* TutorialEnum = StaticEnum<EGeneralTutorialStep>();
            EGeneralTutorialStep CurrentStep = SC->CurrentGeneralTutorialStep;
            FString StepStr = TutorialEnum ? TutorialEnum->GetDisplayNameTextByValue((int64)CurrentStep).ToString() : FString::Printf(TEXT("UnknownStep_%d"), (int32)CurrentStep);
            return FString::Printf(TEXT("%s (%s)"), *PhaseStr, *StepStr);
        }
        else if (CurrentPhase == EStudyPhase::InteractionTutorial)
        {
            const UEnum* TutorialEnum = StaticEnum<ETutorialStep>();
            ETutorialStep CurrentStep = SC->CurrentTutorialStep;
            FString StepStr = TutorialEnum ? TutorialEnum->GetDisplayNameTextByValue((int64)CurrentStep).ToString() : FString::Printf(TEXT("UnknownStep_%d"), (int32)CurrentStep);
            return FString::Printf(TEXT("%s (%s)"), *PhaseStr, *StepStr);
        }
        // Add other phase specific details if needed (e.g., remaining time in FadingOut)
        // else if (CurrentPhase == EStudyPhase::FadingOut) {
        //     return FString::Printf(TEXT("%s (%.1fs left)"), *PhaseStr, SC->CurrentFadeOutTimeRemaining);
        // }

        return PhaseStr; // Return the basic phase name otherwise
    }
    return TEXT("Unknown_NoControllerRef");
}


// --- END OF FILE InteractionLoggerComponent.cpp ---