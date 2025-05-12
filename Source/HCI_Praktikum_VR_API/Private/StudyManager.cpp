// StudyManager.cpp
#include "StudyManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "UObject/EnumProperty.h" // Required for StaticEnum

AStudyManager::AStudyManager()
{
    PrimaryActorTick.bCanEverTick = true;
    CurrentConditionIndex = -1;
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
    GroupType = InGroupType; // GroupType is assigned here

    // --- DEBUG LOGGING ADDED ---
    const UEnum* GroupEnumForLog = StaticEnum<EGroupType>();
    FString GroupTypeString = GroupEnumForLog ? GroupEnumForLog->GetDisplayNameTextByValue((int64)GroupType).ToString() : FString::Printf(TEXT("UnknownGroupEnum_%d"), (int32)GroupType);
    UE_LOG(LogTemp, Warning, TEXT("AStudyManager::InitializeStudy - ParticipantID: %d, BetweenCondition: %s, Received GroupType: %s (Value: %d)"),
        InParticipantID,
        *StaticEnum<EBetweenCondition>()->GetDisplayNameTextByValue((int64)InBetweenCondition).ToString(),
        *GroupTypeString,
        (int32)InGroupType);
    // --- END DEBUG LOGGING ---

    CurrentConditionIndex = -1;
    ConditionResults.Empty();
    GenerateConditionSequence();
    if (ConditionSequence.Num() > 0)
    {
        CurrentConditionIndex = 0;
    }
}

void AStudyManager::GenerateConditionSequence()
{
    // --- DEBUG LOGGING ADDED ---
    const UEnum* GroupEnumForLog = StaticEnum<EGroupType>();
    FString CurrentGroupTypeString = GroupEnumForLog ? GroupEnumForLog->GetDisplayNameTextByValue((int64)GroupType).ToString() : FString::Printf(TEXT("UnknownGroupEnum_%d"), (int32)GroupType);
    UE_LOG(LogTemp, Warning, TEXT("AStudyManager::GenerateConditionSequence - Using GroupType: %s (Value: %d) to generate sequence."), *CurrentGroupTypeString, (int32)GroupType);
    // --- END DEBUG LOGGING ---

    ConditionSequence.Empty();
    ConditionSequence.Add(EConditionType::Control); // Baseline/Control first

    // Assuming: Baseline=Control, Passive=NoInteraction, Active=WithInteraction
    if (GroupType == EGroupType::A) // Example: A = Control -> NoInteraction -> WithInteraction
    {
        UE_LOG(LogTemp, Log, TEXT("AStudyManager::GenerateConditionSequence - Group A path chosen. Adding NoInteraction then WithInteraction.")); // DEBUG LOG
        ConditionSequence.Add(EConditionType::NoInteraction);
        ConditionSequence.Add(EConditionType::WithInteraction);
    }
    else // B = Control -> WithInteraction -> NoInteraction (covers EGroupType::B and any other unexpected values)
    {
        UE_LOG(LogTemp, Log, TEXT("AStudyManager::GenerateConditionSequence - Group B (or other) path chosen. Adding WithInteraction then NoInteraction.")); // DEBUG LOG
        ConditionSequence.Add(EConditionType::WithInteraction);
        ConditionSequence.Add(EConditionType::NoInteraction);
    }

    // --- DEBUG LOGGING ADDED ---
    FString SequenceString = TEXT("Generated Condition Sequence: ");
    const UEnum* ConditionEnumForLog = StaticEnum<EConditionType>();
    if (ConditionEnumForLog)
    {
        for (int32 i = 0; i < ConditionSequence.Num(); ++i)
        {
            SequenceString += ConditionEnumForLog->GetDisplayNameTextByValue((int64)ConditionSequence[i]).ToString();
            if (i < ConditionSequence.Num() - 1)
            {
                SequenceString += TEXT(" -> ");
            }
        }
    }
    else
    {
        SequenceString += TEXT("Could not get EConditionType enum for logging names.");
    }
    UE_LOG(LogTemp, Warning, TEXT("%s"), *SequenceString);
    // --- END DEBUG LOGGING ---

    // *** TODO: Implement proper randomization of the non-baseline conditions here!
    // The current A/B group only counterbalances the order of the two non-baseline ones.
    // For full randomization, you'd shuffle {NoInteraction, WithInteraction} after adding Control.
    // Example using Fisher-Yates shuffle (needs #include "Algo/RandomShuffle.h"):
    // TArray<EConditionType> ConditionsToShuffle;
    // ConditionsToShuffle.Add(EConditionType::NoInteraction);
    // ConditionsToShuffle.Add(ECondiçãoType::WithInteraction);
    // Algo::RandomShuffle(ConditionsToShuffle);
    // ConditionSequence.Append(ConditionsToShuffle);
    // *** END TODO ***
}


EConditionType AStudyManager::GetNextCondition()
{
    if (CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num())
    {
        return ConditionSequence[CurrentConditionIndex];
    }
    UE_LOG(LogTemp, Warning, TEXT("GetNextCondition called with invalid index %d. Returning Control."), CurrentConditionIndex);
    return EConditionType::Control;
}

void AStudyManager::RecordConditionResults(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence, const TArray<int32>& InGemsScores)
{
    if (CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num())
    {
        FConditionData Data;
        Data.ConditionType = ConditionSequence[CurrentConditionIndex];

        FSAMData SAM;
        SAM.Valence = Valence;
        SAM.Arousal = Arousal;
        SAM.Dominance = Dominance;
        SAM.Presence = Presence;
        Data.SAMQuestionnaireData = SAM;

        FGemsData GEMS;
        if (InGemsScores.Num() == 9)
        {
            GEMS.GemsScores = InGemsScores;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RecordConditionResults: Incorrect number of GEMS scores provided (%d). Expected 9. Recording defaults."), InGemsScores.Num());
        }
        Data.GemsQuestionnaireData = GEMS;

        ConditionResults.Add(Data);

        CurrentConditionIndex++;
        UE_LOG(LogTemp, Log, TEXT("Recorded results for condition %d (Type: %s). Advanced index to %d."),
            CurrentConditionIndex - 1,
            *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)Data.ConditionType).ToString(),
            CurrentConditionIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RecordConditionResults called with invalid index %d. Cannot record."), CurrentConditionIndex);
    }
}

bool AStudyManager::HasMoreConditions() const
{
    return CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num();
}

int32 AStudyManager::GetTotalNumberOfConditions() const
{
    return ConditionSequence.Num(); // Should be 3 based on current logic
}


void AStudyManager::SaveResultsToCSV()
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("StudyResults/");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        if (!PlatformFile.CreateDirectory(*SaveDirectory))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *SaveDirectory);
            return;
        }
    }

    const UEnum* BetweenEnum = StaticEnum<EBetweenCondition>();
    const UEnum* GroupEnum = StaticEnum<EGroupType>();
    const UEnum* ConditionEnum = StaticEnum<EConditionType>();

    FString BetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)BetweenCondition);
    BetweenStr = BetweenStr.Replace(TEXT(" "), TEXT("_"));
    FString GroupStr = TEXT("UnknownGroup");
    if (GroupEnum) // Use the enum for consistent naming
    {
        GroupStr = GroupEnum->GetNameStringByValue((int64)GroupType); // Gets "A" or "B" from EGroupType::A or EGroupType::B
        // Remove the "EGroupType::" prefix if it exists, though GetNameStringByValue usually gives the short name.
        GroupStr.RemoveFromStart(TEXT("EGroupType::"));
    }
    else if (GroupType == EGroupType::A) GroupStr = TEXT("A"); // Fallback
    else if (GroupType == EGroupType::B) GroupStr = TEXT("B"); // Fallback


    FString Filename = FString::Printf(TEXT("Participant_%d_%s_Group%s.csv"), ParticipantID, *BetweenStr, *GroupStr);
    FString FilePath = SaveDirectory + Filename;

    FString CSVContent = TEXT("Participant_ID,Between_Condition,Group_Type,Condition_Order,Condition_Name,Valence,Arousal,Dominance,Presence,GEMS_1,GEMS_2,GEMS_3,GEMS_4,GEMS_5,GEMS_6,GEMS_7,GEMS_8,GEMS_9\n");

    int32 ConditionOrder = 1;
    for (const FConditionData& Data : ConditionResults)
    {
        FString ConditionTypeStr = ConditionEnum ? ConditionEnum->GetDisplayNameTextByValue((int64)Data.ConditionType).ToString() : FString::Printf(TEXT("UnknownCondition_%d"), (int32)Data.ConditionType);
        ConditionTypeStr = ConditionTypeStr.Replace(TEXT(" "), TEXT("_"));

        FString RowBetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)BetweenCondition);

        FString RowGroupStr = TEXT("UnknownGroup");
        if (GroupEnum) // Use enum for consistency
        {
            RowGroupStr = GroupEnum->GetNameStringByValue((int64)GroupType);
            RowGroupStr.RemoveFromStart(TEXT("EGroupType::"));
        }
        else if (GroupType == EGroupType::A) RowGroupStr = TEXT("A"); // Fallback
        else if (GroupType == EGroupType::B) RowGroupStr = TEXT("B"); // Fallback


        FString GemsScoresString = "";
        if (Data.GemsQuestionnaireData.GemsScores.Num() == 9)
        {
            for (int i = 0; i < 9; ++i)
            {
                GemsScoresString += FString::Printf(TEXT(",%d"), Data.GemsQuestionnaireData.GemsScores[i]);
            }
        }
        else
        {
            GemsScoresString = TEXT(",,,,,,,,");
            UE_LOG(LogTemp, Warning, TEXT("SaveResultsToCSV: Missing or invalid GEMS data for Participant %d, Condition %s (Order %d). Saving defaults."), ParticipantID, *ConditionTypeStr, ConditionOrder);
        }

        CSVContent += FString::Printf(TEXT("%d,%s,%s,%d,%s,%d,%d,%d,%d%s\n"),
            ParticipantID,
            *RowBetweenStr,
            *RowGroupStr,
            ConditionOrder++,
            *ConditionTypeStr,
            Data.SAMQuestionnaireData.Valence,
            Data.SAMQuestionnaireData.Arousal,
            Data.SAMQuestionnaireData.Dominance,
            Data.SAMQuestionnaireData.Presence,
            *GemsScoresString);
    }

    if (FFileHelper::SaveStringToFile(CSVContent, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Results saved successfully to: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save results to: %s"), *FilePath);
    }
}