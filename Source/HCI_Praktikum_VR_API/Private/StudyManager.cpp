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
    GroupType = InGroupType;
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
    ConditionSequence.Empty();
    ConditionSequence.Add(EConditionType::Control); // Baseline/Control first
    // *** NOTE: Your description mentions Active vs Passive vs Baseline.
    //     The EConditionType enum has Control, NoInteraction, WithInteraction.
    //     Assuming: Baseline=Control, Passive=NoInteraction, Active=WithInteraction
    //     Adjust this logic if your mapping is different! ***
    if (GroupType == EGroupType::A) // Example: A = Baseline -> Passive -> Active
    {
        ConditionSequence.Add(EConditionType::NoInteraction);
        ConditionSequence.Add(EConditionType::WithInteraction);
    }
    else // B = Baseline -> Active -> Passive
    {
        ConditionSequence.Add(EConditionType::WithInteraction);
        ConditionSequence.Add(EConditionType::NoInteraction);
    }
    // *** TODO: Implement proper randomization of the non-baseline conditions here!
    // The current A/B group only counterbalances the order of the two non-baseline ones.
    // For full randomization, you'd shuffle {NoInteraction, WithInteraction} after adding Control.
    // Example using Fisher-Yates shuffle (needs #include "Algo/RandomShuffle.h"):
    // TArray<EConditionType> ConditionsToShuffle;
    // ConditionsToShuffle.Add(EConditionType::NoInteraction);
    // ConditionsToShuffle.Add(EConditionType::WithInteraction);
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

// *** MODIFIED: Implementation for recording all questionnaire data ***
void AStudyManager::RecordConditionResults(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence, const TArray<int32>& InGemsScores)
{
    if (CurrentConditionIndex >= 0 && CurrentConditionIndex < ConditionSequence.Num())
    {
        FConditionData Data;
        Data.ConditionType = ConditionSequence[CurrentConditionIndex];

        // Record SAM + Presence Data
        FSAMData SAM;
        SAM.Valence = Valence;
        SAM.Arousal = Arousal;
        SAM.Dominance = Dominance;
        SAM.Presence = Presence;
        Data.SAMQuestionnaireData = SAM; // Assign SAM struct

        // Record GEMS Data
        FGemsData GEMS;
        if (InGemsScores.Num() == 9)
        {
            GEMS.GemsScores = InGemsScores; // Copy GEMS scores
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RecordConditionResults: Incorrect number of GEMS scores provided (%d). Expected 9. Recording defaults."), InGemsScores.Num());
            // GEMS struct already initializes to defaults, so we just don't overwrite
        }
        Data.GemsQuestionnaireData = GEMS; // Assign GEMS struct

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
    // Make sure sequence is generated if called early, though InitializeStudy should handle it.
    // if (ConditionSequence.IsEmpty() && (GroupType == EGroupType::A || GroupType == EGroupType::B)) {
    //     const_cast<AStudyManager*>(this)->GenerateConditionSequence(); // Be careful with const_cast
    // }
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
    BetweenStr = BetweenStr.Replace(TEXT(" "), TEXT("_")); // Make filename friendly
    FString GroupStr = TEXT("UnknownGroup");
    if (GroupType == EGroupType::A) GroupStr = TEXT("A");
    else if (GroupType == EGroupType::B) GroupStr = TEXT("B");
    // Add handling if you implement full randomization later

    FString Filename = FString::Printf(TEXT("Participant_%d_%s_Group%s.csv"), ParticipantID, *BetweenStr, *GroupStr);
    FString FilePath = SaveDirectory + Filename;

    // *** MODIFIED: Add GEMS columns to header ***
    FString CSVContent = TEXT("Participant_ID,Between_Condition,Group_Type,Condition_Order,Condition_Name,Valence,Arousal,Dominance,Presence,GEMS_1,GEMS_2,GEMS_3,GEMS_4,GEMS_5,GEMS_6,GEMS_7,GEMS_8,GEMS_9\n");

    int32 ConditionOrder = 1;
    for (const FConditionData& Data : ConditionResults)
    {
        FString ConditionTypeStr = ConditionEnum ? ConditionEnum->GetDisplayNameTextByValue((int64)Data.ConditionType).ToString() : FString::Printf(TEXT("UnknownCondition_%d"), (int32)Data.ConditionType);
        ConditionTypeStr = ConditionTypeStr.Replace(TEXT(" "), TEXT("_")); // Make CSV friendly

        FString RowBetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)BetweenCondition);
        FString RowGroupStr = TEXT("UnknownGroup");
        if (GroupType == EGroupType::A) RowGroupStr = TEXT("A");
        else if (GroupType == EGroupType::B) RowGroupStr = TEXT("B");


        // *** MODIFIED: Add GEMS scores to data row ***
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
            // Append 9 commas with empty/default values if data is missing/invalid
            GemsScoresString = TEXT(",,,,,,,,"); // 9 commas for 9 missing values
            UE_LOG(LogTemp, Warning, TEXT("SaveResultsToCSV: Missing or invalid GEMS data for Participant %d, Condition %s (Order %d). Saving defaults."), ParticipantID, *ConditionTypeStr, ConditionOrder);
        }

        CSVContent += FString::Printf(TEXT("%d,%s,%s,%d,%s,%d,%d,%d,%d%s\n"), // Added %s for GEMS string
            ParticipantID,
            *RowBetweenStr,
            *RowGroupStr,
            ConditionOrder++,
            *ConditionTypeStr,
            Data.SAMQuestionnaireData.Valence,
            Data.SAMQuestionnaireData.Arousal,
            Data.SAMQuestionnaireData.Dominance,
            Data.SAMQuestionnaireData.Presence,
            *GemsScoresString); // Append the generated GEMS string
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