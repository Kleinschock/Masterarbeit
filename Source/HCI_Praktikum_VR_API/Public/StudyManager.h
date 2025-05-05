// StudyManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StudyManager.generated.h"

// --- ENUMS remain the same ---
UENUM(BlueprintType)
enum class EBetweenCondition : uint8
{
    VR UMETA(DisplayName = "VR"),
    AR UMETA(DisplayName = "AR")
};

UENUM(BlueprintType)
enum class EGroupType : uint8
{
    A UMETA(DisplayName = "Group A: Control -> No Interaction -> With Interaction"),
    B UMETA(DisplayName = "Group B: Control -> With Interaction -> No Interaction")
};

UENUM(BlueprintType)
enum class EConditionType : uint8
{
    Control UMETA(DisplayName = "Control Condition"),
    NoInteraction UMETA(DisplayName = "No Interaction"),
    WithInteraction UMETA(DisplayName = "With Interaction")
};
// --- END ENUMS ---

USTRUCT(BlueprintType)
struct FSAMData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Valence = 0; // Default values

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Arousal = 0; // Default values

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Dominance = 0; // Default values

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Presence = 0; // Default values (Single Item Presence)
};

// *** ADDED GEMS Data Structure ***
USTRUCT(BlueprintType)
struct FGemsData
{
    GENERATED_BODY()

    // Assuming 9 GEMS items, store their scores
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire|GEMS")
    TArray<int32> GemsScores;

    FGemsData()
    {
        // Initialize with 9 default values (e.g., 0 or a mid-point if scale is known)
        GemsScores.Init(0, 9);
    }
};


USTRUCT(BlueprintType)
struct FConditionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EConditionType ConditionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    FSAMData SAMQuestionnaireData; // Renamed for clarity

    // *** ADDED GEMS Data ***
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    FGemsData GemsQuestionnaireData;
};

UCLASS()
class HCI_PRAKTIKUM_VR_API_API AStudyManager : public AActor
{
    GENERATED_BODY()

public:
    AStudyManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // --- Study Configuration remains the same ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    int32 ParticipantID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EBetweenCondition BetweenCondition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EGroupType GroupType;
    // --- END Configuration ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
    int32 CurrentConditionIndex;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Study")
    TArray<FConditionData> ConditionResults;

    // --- Functions ---
    UFUNCTION(BlueprintCallable, Category = "Study")
    void InitializeStudy(int32 InParticipantID, EBetweenCondition InBetweenCondition, EGroupType InGroupType);

    UFUNCTION(BlueprintCallable, Category = "Study")
    EConditionType GetNextCondition();

    // *** MODIFIED: Changed signature to include GEMS data ***
    // Renamed slightly for better representation of what it does now
    UFUNCTION(BlueprintCallable, Category = "Study")
    void RecordConditionResults(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence, const TArray<int32>& InGemsScores); // Advances index

    UFUNCTION(BlueprintCallable, Category = "Study")
    void SaveResultsToCSV();

    UFUNCTION(BlueprintCallable, Category = "Study")
    bool HasMoreConditions() const;

    UFUNCTION(BlueprintCallable, Category = "Study")
    int32 GetTotalNumberOfConditions() const;


public:
    TArray<EConditionType> ConditionSequence;
    void GenerateConditionSequence();
};