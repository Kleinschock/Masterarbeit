// StudyManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StudyManager.generated.h"

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

USTRUCT(BlueprintType)
struct FSAMData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Valence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Arousal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Dominance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    int32 Presence;
};

USTRUCT(BlueprintType)
struct FConditionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EConditionType ConditionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Questionnaire")
    FSAMData QuestionnaireData;
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

    // Study Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    int32 ParticipantID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EBetweenCondition BetweenCondition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
    EGroupType GroupType;

    // Current state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
    int32 CurrentConditionIndex;

    // Results for each condition
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Study")
    TArray<FConditionData> ConditionResults;

    // Blueprint callable functions
    UFUNCTION(BlueprintCallable, Category = "Study")
    void InitializeStudy(int32 InParticipantID, EBetweenCondition InBetweenCondition, EGroupType InGroupType);

    UFUNCTION(BlueprintCallable, Category = "Study")
    EConditionType GetNextCondition(); // Note: This returns the condition but doesn't advance index

    UFUNCTION(BlueprintCallable, Category = "Study")
    void RecordSAMData(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence); // Advances index

    UFUNCTION(BlueprintCallable, Category = "Study")
    void SaveResultsToCSV();

    UFUNCTION(BlueprintCallable, Category = "Study")
    bool HasMoreConditions() const;

    // *** ADD THIS GETTER FUNCTION ***
    UFUNCTION(BlueprintCallable, Category = "Study")
    int32 GetTotalNumberOfConditions() const;


public:
    TArray<EConditionType> ConditionSequence;
    void GenerateConditionSequence();
};