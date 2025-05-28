#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StudyManager.h" // Include StudyManager for Enums/Structs
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "VisualizationControllable.h" // Include interface
#include "GameFramework/PlayerController.h" // Include PlayerController

#include "MovementLoggerComponent.h" 
#include "StudyGameController.generated.h"



// Forward Declarations
class UUserWidget;
class AStudyManager;
class UHapticFeedbackEffect_Base;
class UInteractionLoggerComponent;
class USoundBase; // Added for TutorialSuccessSound

UENUM(BlueprintType)
enum class EStudyPhase : uint8
{
	MainMenu UMETA(DisplayName = "Main Menu"),
	GeneralTutorial UMETA(DisplayName = "General Tutorial"),
	InteractionTutorial UMETA(DisplayName = "Interaction Tutorial"),
	Briefing UMETA(DisplayName = "Briefing Condition"),
	Preparation UMETA(DisplayName = "Preparation Phase"),
	Running UMETA(DisplayName = "Running Visualization"),
	FadingOut UMETA(DisplayName = "Fading Out Visualization"),
	SamEvaluation UMETA(DisplayName = "SAM Evaluation"),
	GemsPart1Evaluation UMETA(DisplayName = "GEMS Evaluation (Part 1)"), // Added
	GemsPart2Evaluation UMETA(DisplayName = "GEMS Evaluation (Part 2)"), // Added
	PresenceEvaluation UMETA(DisplayName = "Presence Evaluation"),
	// TODO: Add SSQ Phase here if needed
	Complete UMETA(DisplayName = "Study Complete")
};

UENUM(BlueprintType)
enum class EGeneralTutorialStep : uint8
{
	None UMETA(DisplayName = "None"),
	Intro UMETA(DisplayName = "Introduction"),
	SAMExplanation UMETA(DisplayName = "SAM Explanation"),
	SAMEvaluation UMETA(DisplayName = "SAM Evaluation (Tutorial)"),
	PresenceExplanation UMETA(DisplayName = "Presence Explanation"),
	PresenceEvaluation UMETA(DisplayName = "Presence Evaluation (Tutorial)"),
	Ending UMETA(DisplayName = "Ending")
};

UENUM(BlueprintType)
enum class ETutorialStep : uint8 // Interaction Tutorial Steps
{
	None UMETA(DisplayName = "None"),
	Intro UMETA(DisplayName = "Introduction"),
	LeftSpawn UMETA(DisplayName = "Left Spawn (Grab)"),
	RightSpawn UMETA(DisplayName = "Right Spawn (Grab)"),
	RightConduct UMETA(DisplayName = "Right Conduct (Trigger)"),
	LeftPull UMETA(DisplayName = "Left Pull (Trigger)"),
	Complete UMETA(DisplayName = "Tutorial Complete")
};


UCLASS()
class HCI_PRAKTIKUM_VR_API_API AStudyGameController : public AActor
{
	GENERATED_BODY()

public:
	AStudyGameController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

public:
	virtual void Tick(float DeltaTime) override;

	// --- Study manager reference ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
	TObjectPtr<AStudyManager> StudyManager;

	// --- Current State ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
	EStudyPhase CurrentPhase = EStudyPhase::MainMenu;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial")
	EGeneralTutorialStep CurrentGeneralTutorialStep = EGeneralTutorialStep::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
	EConditionType CurrentCondition; // Set during BeginCondition

	// --- UI References ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> MainMenuWidgetClass;
	// General Tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialIntroWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialSAMExplanationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialPresenceExplanationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialEndingWidgetClass;
	// Briefing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingControlWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingNoInteractionWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingWithInteractionWidgetClass;
	// Study Phase Widgets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> PreparationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Questionnaires") TSubclassOf<UUserWidget> SAMWidgetClass; // Used for actual study & potentially tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Questionnaires") TSubclassOf<UUserWidget> GemsPart1WidgetClass; // Added
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Questionnaires") TSubclassOf<UUserWidget> GemsPart2WidgetClass; // Added
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Questionnaires") TSubclassOf<UUserWidget> PresenceWidgetClass; // Used for actual study & potentially tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> CompletionWidgetClass;
	// Interaction Tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialIntroWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialLeftSpawnWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialRightSpawnWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialRightConductWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialLeftPullWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialCompleteWidgetClass;


	// --- Interaction Logging Control ---
	/** Notifies the logger that an interaction input action has started (pressed). */
	UFUNCTION(BlueprintCallable, Category = "Interaction Logging")
	void NotifyInteractionStart(FName ActionName, EControllerHand Hand);
	/** Notifies the logger that an interaction input action has ended (released or canceled). */
	UFUNCTION(BlueprintCallable, Category = "Interaction Logging")
	void NotifyInteractionEnd(FName ActionName, EControllerHand Hand);

	// --- 3D UI Settings ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|3D Settings", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> CurrentDisplayWidgetComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|3D Settings")
	FTransform WorldUITransform = FTransform(FRotator(0, 180, 0), FVector(200.0f, 0.0f, 150.0f), FVector(1.0f));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|3D Settings")
	bool bWidgetIsTwoSided = false;

	// --- Study Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
	float VisualizationDuration = 140.0f; // Match description
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
	float FadeOutDuration = 3.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
	float CurrentFadeOutTimeRemaining = 0.0f;

	// --- Temporary Questionnaire Data Storage ---
	UPROPERTY(BlueprintReadWrite, Category = "Study|Questionnaire") int32 ValenceScore = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Study|Questionnaire") int32 ArousalScore = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Study|Questionnaire") int32 DominanceScore = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Study|Questionnaire") int32 PresenceScore = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Study|Questionnaire") TArray<int32> GemsScores; // Holds all 9


	// --- Interaction Tutorial Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Tutorial") float TutorialRequiredHoldTime = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Tutorial") int32 TutorialRequiredSuccessCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Tutorial") TObjectPtr<USoundBase> TutorialSuccessSound;

	// --- Vibration Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Tutorial") TObjectPtr<UHapticFeedbackEffect_Base> TutorialHoldHaptic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Tutorial") TObjectPtr<UHapticFeedbackEffect_Base> TutorialSuccessHaptic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") TObjectPtr<UHapticFeedbackEffect_Base> SpawnHaptic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") float SpawnHapticScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") TObjectPtr<UHapticFeedbackEffect_Base> ConductHaptic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") float ConductSpeedIntensityFactor = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") float MaxConductIntensityScale = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") TObjectPtr<UHapticFeedbackEffect_Base> PullHaptic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study|Vibration|Interaction") float PullHapticScale = 1.0f;


	// --- Blueprint callable functions ---
	// Study Control
	UFUNCTION(BlueprintCallable, Category = "Study") void InitializeStudy(int32 ParticipantID, EBetweenCondition BetweenID, EGroupType GroupID);
	UFUNCTION(BlueprintCallable, Category = "Study") void BeginCondition(); // Starts Briefing
	UFUNCTION(BlueprintCallable, Category = "Study") void StartPreparationPhase(); // Called by Briefing/Tutorial Widget
	UFUNCTION(BlueprintCallable, Category = "Study") void StartVisualization(); // Called by Preparation Widget
	UFUNCTION(BlueprintCallable, Category = "Study") void StartSAMEvaluation(); // Starts SAM Questionnaire (Called by CompleteFadeOutAndTransition)
	// GEMS Flow Functions
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void StartGemsPart1Evaluation(); // Called by SAM Widget Submit
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void SubmitGemsPart1AndAdvance(const TArray<int32>& Part1Scores); // Called by GEMS Part 1 Widget Submit
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void StartGemsPart2Evaluation(); // Called internally
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void SubmitGemsPart2AndAdvance(const TArray<int32>& Part2Scores); // Called by GEMS Part 2 Widget Submit
	// Presence and Final Submit
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void StartPresenceEvaluation(); // Called internally
	UFUNCTION(BlueprintCallable, Category = "Study|Questionnaire") void SubmitAllQuestionnairesAndAdvance(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence); // Called by Presence Widget Submit
	UFUNCTION(BlueprintCallable, Category = "Study") void EndStudy();
	// UI/Status
	UFUNCTION(BlueprintCallable, Category = "UI") void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category = "UI") FString GetConditionName();
	UFUNCTION(BlueprintCallable, Category = "Study") void SetupVisualizationForCondition(EConditionType Condition);
	UFUNCTION(BlueprintCallable, Category = "Status") FString GetStudyStatus();
	UFUNCTION(BlueprintCallable, Category = "Study") bool HasMoreConditions() const;

	// --- General Tutorial Control ---
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialIntro();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialSAMExplanation();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void SubmitTutorialSAM(int32 Valence, int32 Arousal, int32 Dominance);
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialPresenceExplanation();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void SubmitTutorialPresence(int32 Presence);
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialEnding();

	// --- Interaction Tutorial Control ---
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void StartInteractionTutorial();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void AdvanceTutorialStep();


	// --- Interaction Tutorial State ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial|State") ETutorialStep CurrentTutorialStep = ETutorialStep::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial|State") float CurrentTutorialHoldTime = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial|State") int32 CurrentTutorialSuccessCount = 0;
	UPROPERTY(VisibleAnywhere, Category = "Study|Tutorial|State") bool bIsHoldingRequiredInput = false;

	// --- Input Handling ---
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleLeftGrab(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleRightGrab(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleLeftTrigger(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleRightTrigger(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void UpdateRightHandSpeed(float Speed);

	// --- Passthrough Control ---
	UFUNCTION(BlueprintImplementableEvent, Category = "Passthrough")
	void SetPassthroughMode(bool bEnable);

private:
	// Timers
	FTimerHandle VisualizationTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle TutorialHoldTimerHandle;
	
	// References
	UPROPERTY() TArray<TScriptInterface<IVisualizationControllable>> VisualizationTargets;
	TWeakObjectPtr<UInteractionLoggerComponent> LoggerComponentRef; // Bereits vorhanden

	// *** ADDED FOR MOVEMENT LOGGER ***
	TWeakObjectPtr<UMovementLoggerComponent> MovementLoggerComponentRef;
	void InitializeMovementLogger();
	void StartMovementLoggingForCurrentPhase();
	void StopMovementLoggingForCurrentPhase();

	// Study Flow Helpers
	void EndVisualizationPhase();
	void CompleteFadeOutAndTransition(); // Leads to StartSAMEvaluation
	void CheckForMoreConditions(); // Leads to BeginCondition or EndStudy
	// UI Helpers
	void ClearWorldUI();
	void DisplayWidgetInWorld(TSubclassOf<UUserWidget> WidgetClassToShow);
	TSubclassOf<UUserWidget> GetWidgetClassForBriefing(EConditionType Condition);
	// Visualization Helpers
	void FindVisualizationTargets();
	void StartVisualizationControlOnly(); // For Interaction Tutorial

	// --- General Tutorial ---
	void StartGeneralTutorial();
	void SetGeneralTutorialStep(EGeneralTutorialStep NewStep);
	TSubclassOf<UUserWidget> GetWidgetClassForGeneralTutorialStep(EGeneralTutorialStep Step);

	// --- Interaction Tutorial ---
	void UpdateTutorialLogic(float DeltaTime);
	void OnTutorialHoldTimeComplete();
	void SetTutorialStep(ETutorialStep NewStep);
	void UpdateTutorialUI(); // Updates progress display on current tutorial widget
	void CheckTutorialInput(ETutorialStep StepToCheck, bool bLeftGrab, bool bRightGrab, bool bLeftTrigger, bool bRightTrigger);
	TSubclassOf<UUserWidget> GetWidgetClassForTutorialStep(ETutorialStep Step);

	// --- Vibration Control ---
	APlayerController* GetPlayerControllerRef() const;
	void PlayHaptic(EControllerHand Hand, UHapticFeedbackEffect_Base* HapticEffect, float Scale = 1.0f, bool bLoop = false);
	void StopHaptic(EControllerHand Hand);
	void UpdateConductVibration(float Speed); // Called from Tick for Right Trigger
	bool ShouldVibrateForCondition(EConditionType ConditionToCheck) const; // Checks phase and condition

	// Vibration State Flags
	bool bLeftGrabVibrationActive = false;
	bool bRightGrabVibrationActive = false;
	bool bLeftTriggerVibrationActive = false;
	bool bRightTriggerVibrationActive = false;
	float LastKnownRightHandSpeed = 0.0f; // For conduct vibration intensity
};