// StudyManager.cpp
#include "StudyManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "UObject/EnumProperty.h" // Required for StaticEnum

AStudyManager::AStudyManager()
{
    PrimaryActorTick.bCanEverTick = true;
    CurrentConditionIndex = -1; // Initialize to -1 to indicate study not started/no condition active
}

void AStudyManager::BeginPlay()
{
    Super::BeginPlay();
}

void AStudyManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AStudyManager::InitializeStudy(int32 InParticipantID, EBetweenCondition InBetweenCondition, EGroupType InGroupType)
{
    ParticipantID = InParticipantID;
    BetweenCondition = InBetweenCondition;
    GroupType = InGroupType;

    CurrentConditionIndex = -1; // Reset index
    ConditionResults.Empty();

    // Generate the sequence of conditions based on group type
    GenerateConditionSequence();

    // After generating, set index to 0 for the first condition
    if (ConditionSequence.Num() > 0)
    {
        CurrentConditionIndex = 0;
    }
}

void AStudyManager::GenerateConditionSequence()
{
    ConditionSequence.Empty();

    // First condition is always Control
    ConditionSequence.Add(EConditionType::Control);

    // Remaining conditions depend on group type
    if (GroupType == EGroupType::A)
    {
        ConditionSequence.Add(EConditionType::NoInteraction);
        ConditionSequence.Add(EConditionType::WithInteraction);
    }
    else // Group B
    {
        ConditionSequence.Add(EConditionType::WithInteraction);
        ConditionSequence.Add(EConditionType::NoInteraction);
    }
}

EConditionType AStudyManager::GetNextCondition()
{
    // Returns the condition at the current index without advancing
    if (CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num())
    {
        return ConditionSequence[CurrentConditionIndex];
    }

    // If index is invalid (e.g., before init or after end), return Control as default/fallback
    UE_LOG(LogTemp, Warning, TEXT("GetNextCondition called with invalid index %d. Returning Control."), CurrentConditionIndex);
    return EConditionType::Control;
}

void AStudyManager::RecordSAMData(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence)
{
    // Records data for the CURRENT index and advances the index
    if (CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num())
    {
        FConditionData Data;
        Data.ConditionType = ConditionSequence[CurrentConditionIndex];

        FSAMData SAM;
        SAM.Valence = Valence;
        SAM.Arousal = Arousal;
        SAM.Dominance = Dominance;
        SAM.Presence = Presence;

        Data.QuestionnaireData = SAM;

        // Ensure array is large enough if needed, or just add
        // If results might be added out of order (they shouldn't here), you'd need different logic
        ConditionResults.Add(Data);

        // Move to next condition index for the *next* call to GetNextCondition/RecordSAMData
        CurrentConditionIndex++;
        UE_LOG(LogTemp, Log, TEXT("Recorded data for condition %d. Advanced index to %d."), CurrentConditionIndex - 1, CurrentConditionIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RecordSAMData called with invalid index %d. Cannot record."), CurrentConditionIndex);
    }
}
bool AStudyManager::HasMoreConditions() const
{
    // Check if the current index is valid and within the bounds of the sequence
    return CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num();
}

// *** ADD THIS GETTER FUNCTION IMPLEMENTATION ***
int32 AStudyManager::GetTotalNumberOfConditions() const
{
    return ConditionSequence.Num();
}
// *** END OF ADDED FUNCTION ***

void AStudyManager::SaveResultsToCSV()
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("StudyResults/");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // Create directory if it doesn't exist
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectory(*SaveDirectory);
        if (!PlatformFile.DirectoryExists(*SaveDirectory))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *SaveDirectory);
            return; // Cannot save if directory creation fails
        }
    }

    // Find the UEnum objects - ensure you have #include "UObject/EnumProperty.h"
    const UEnum* BetweenEnum = StaticEnum<EBetweenCondition>();
    const UEnum* GroupEnum = StaticEnum<EGroupType>();
    const UEnum* ConditionEnum = StaticEnum<EConditionType>();

    // Generate a unique filename based on participant ID and condition
    FString BetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)BetweenCondition);
    FString GroupStr = GroupEnum ? GroupEnum->GetDisplayNameTextByValue((int64)GroupType).ToString() : FString::Printf(TEXT("UnknownGroup_%d"), (int32)GroupType);

    // Simplify GroupStr if needed for filename:
    if (GroupType == EGroupType::A) GroupStr = TEXT("A");
    else if (GroupType == EGroupType::B) GroupStr = TEXT("B");

    FString Filename = FString::Printf(TEXT("Participant_%d_%s_Group%s.csv"), ParticipantID, *BetweenStr, *GroupStr);
    FString FilePath = SaveDirectory + Filename;

    // Create CSV content
    FString CSVContent = TEXT("Participant_ID,Between_Condition,Group_Type,Condition_Order,Condition_Name,Valence,Arousal,Dominance,Presence\n");

    int32 ConditionOrder = 1;
    for (const FConditionData& Data : ConditionResults)
    {
        FString ConditionTypeStr = ConditionEnum ? ConditionEnum->GetDisplayNameTextByValue((int64)Data.ConditionType).ToString() : FString::Printf(TEXT("UnknownCondition_%d"), (int32)Data.ConditionType);

        // Get the Between and Group strings again for the row data (as they are constant for the file)
        FString RowBetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)BetweenCondition);
        FString RowGroupStr = TEXT("UnknownGroup"); // Default
        if (GroupType == EGroupType::A) RowGroupStr = TEXT("A");
        else if (GroupType == EGroupType::B) RowGroupStr = TEXT("B");


        CSVContent += FString::Printf(TEXT("%d,%s,%s,%d,%s,%d,%d,%d,%d\n"),
            ParticipantID,
            *RowBetweenStr, // Use the row-specific string
            *RowGroupStr,   // Use the row-specific string
            ConditionOrder++, // Add the order in which it was run
            *ConditionTypeStr,
            Data.QuestionnaireData.Valence,
            Data.QuestionnaireData.Arousal,
            Data.QuestionnaireData.Dominance,
            Data.QuestionnaireData.Presence);
    }

    // Save the file
    if (FFileHelper::SaveStringToFile(CSVContent, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Results saved successfully to: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save results to: %s"), *FilePath);
    }
}