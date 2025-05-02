// --- START OF FILE StudyGameController.h ---
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StudyManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "VisualizationControllable.h"
#include "GameFramework/PlayerController.h"
#include "StudyGameController.generated.h"

class UUserWidget;
class AStudyManager;
class UHapticFeedbackEffect_Base;

UENUM(BlueprintType)
enum class EStudyPhase : uint8
{
	MainMenu UMETA(DisplayName = "Main Menu"),
	GeneralTutorial UMETA(DisplayName = "General Tutorial"),
	InteractionTutorial UMETA(DisplayName = "Interaction Tutorial"),
	Briefing UMETA(DisplayName = "Briefing Condition"), // *** ADDED Briefing Phase ***
	Preparation UMETA(DisplayName = "Preparation Phase"),
	Running UMETA(DisplayName = "Running Visualization"),
	FadingOut UMETA(DisplayName = "Fading Out Visualization"),
	SamEvaluation UMETA(DisplayName = "SAM Evaluation"),
	PresenceEvaluation UMETA(DisplayName = "Presence Evaluation"),
	Complete UMETA(DisplayName = "Study Complete")
};

// *** ADDED Enum for General Tutorial Steps ***
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
enum class ETutorialStep : uint8 // Renamed from EInteractionTutorialStep if needed for clarity
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

	// Study manager reference
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study")
	TObjectPtr<AStudyManager> StudyManager;

	// Current study phase
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
	EStudyPhase CurrentPhase = EStudyPhase::MainMenu;

	// *** ADDED State for General Tutorial Step ***
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial")
	EGeneralTutorialStep CurrentGeneralTutorialStep = EGeneralTutorialStep::None;

	// Current condition being tested (during actual study)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study")
	EConditionType CurrentCondition;

	// --- UI References ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> MainMenuWidgetClass;
	// --- General Tutorial Widgets ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialIntroWidgetClass; // Was GeneralTutorialWidgetClass
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialSAMExplanationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialPresenceExplanationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|General Tutorial") TSubclassOf<UUserWidget> GeneralTutorialEndingWidgetClass;
	// --- Briefing Widgets ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingControlWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingNoInteractionWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Briefing") TSubclassOf<UUserWidget> BriefingWithInteractionWidgetClass;
	// --- Study Phase Widgets ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> PreparationWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> SAMWidgetClass; // Used for actual study & potentially tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> PresenceWidgetClass; // Used for actual study & potentially tutorial
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets") TSubclassOf<UUserWidget> CompletionWidgetClass;
	// --- Interaction Tutorial Widgets ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialIntroWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialLeftSpawnWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialRightSpawnWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialRightConductWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialLeftPullWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Widgets|Interaction Tutorial") TSubclassOf<UUserWidget> TutorialCompleteWidgetClass;

	// --- 3D UI Settings ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|3D Settings", meta = (AllowPrivateAccess = "true")) TObjectPtr<UWidgetComponent> CurrentDisplayWidgetComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|3D Settings") FTransform WorldUITransform = FTransform(FRotator(0, 180, 0), FVector(200.0f, 0.0f, 150.0f), FVector(1.0f));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|3D Settings") bool bWidgetIsTwoSided = false;

	// --- Study Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study") float VisualizationDuration = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Study") float FadeOutDuration = 3.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study") float CurrentFadeOutTimeRemaining = 0.0f;

	// *** ADDED BACK: SAM/Presence data storage ***
	// Needed for SubmitEvaluationAndAdvance
	UPROPERTY(BlueprintReadWrite, Category = "Study")
	int32 ValenceScore;
	UPROPERTY(BlueprintReadWrite, Category = "Study")
	int32 ArousalScore;
	UPROPERTY(BlueprintReadWrite, Category = "Study")
	int32 DominanceScore;
	UPROPERTY(BlueprintReadWrite, Category = "Study")
	int32 PresenceScore;
	// *** --- ***

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
	UFUNCTION(BlueprintCallable, Category = "Study") void BeginCondition();
	UFUNCTION(BlueprintCallable, Category = "Study") void StartPreparationPhase(); // *** ADDED: Called by Briefing widgets ***
	UFUNCTION(BlueprintCallable, Category = "Study") void StartVisualization();
	UFUNCTION(BlueprintCallable, Category = "Study") void StartSAMEvaluation(); // For actual study
	UFUNCTION(BlueprintCallable, Category = "Study") void StartPresenceEvaluation(); // For actual study
	UFUNCTION(BlueprintCallable, Category = "Study") void SubmitEvaluationAndAdvance(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence); // For actual study
	UFUNCTION(BlueprintCallable, Category = "Study") void EndStudy();
	// UI/Status
	UFUNCTION(BlueprintCallable, Category = "UI") void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category = "UI") FString GetConditionName();
	UFUNCTION(BlueprintCallable, Category = "Study") void SetupVisualizationForCondition(EConditionType Condition);
	UFUNCTION(BlueprintCallable, Category = "Status") FString GetStudyStatus();
	UFUNCTION(BlueprintCallable, Category = "Study") bool HasMoreConditions() const;

	// --- General Tutorial Control (Called from Widgets) ---
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialIntro();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialSAMExplanation();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void SubmitTutorialSAM(int32 Valence, int32 Arousal, int32 Dominance); // No Presence needed for SAM
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialPresenceExplanation();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void SubmitTutorialPresence(int32 Presence); // Only Presence needed
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void CompleteGeneralTutorialEnding();

	// --- Interaction Tutorial Control ---
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void StartInteractionTutorial();
	UFUNCTION(BlueprintCallable, Category = "Study|Tutorial") void AdvanceTutorialStep();


	// --- Interaction Tutorial State ---
	UPROPERTY(VisibleAnywhere, Category = "Study|Tutorial|State") ETutorialStep CurrentTutorialStep = ETutorialStep::None; // Interaction tutorial step

	// Ensure BlueprintReadOnly is present for variables accessed by Widgets
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial|State")
	float CurrentTutorialHoldTime = 0.0f; // Needs BlueprintReadOnly

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Study|Tutorial|State")
	int32 CurrentTutorialSuccessCount = 0; // Needs BlueprintReadOnly

	UPROPERTY(VisibleAnywhere, Category = "Study|Tutorial|State") bool bIsHoldingRequiredInput = false; // This one likely doesn't need BP access if only C++ uses it

	// --- Input Handling ---
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleLeftGrab(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleRightGrab(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleLeftTrigger(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void HandleRightTrigger(bool bPressed);
	UFUNCTION(BlueprintCallable, Category = "Input") void UpdateRightHandSpeed(float Speed);

	// --- Passthrough Control ---
	/**
	 * Tells the Blueprint implementation to enable or disable the AR Passthrough mode.
	 * @param bEnable Pass true to enable Passthrough (AR mode), false to disable it (VR mode).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Passthrough")
	void SetPassthroughMode(bool bEnable);

private:
	FTimerHandle VisualizationTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle TutorialHoldTimerHandle;

	UPROPERTY() TArray<TScriptInterface<IVisualizationControllable>> VisualizationTargets;

	// Study Flow Helpers
	void EndVisualizationPhase();
	void CompleteFadeOutAndTransition();
	void CheckForMoreConditions();
	// UI Helpers
	void ClearWorldUI();
	void DisplayWidgetInWorld(TSubclassOf<UUserWidget> WidgetClassToShow);
	TSubclassOf<UUserWidget> GetWidgetClassForBriefing(EConditionType Condition); // *** ADDED ***
	// Visualization Helpers
	void FindVisualizationTargets();
	void StartVisualizationControlOnly();

	// --- General Tutorial State ---
	void StartGeneralTutorial();
	void SetGeneralTutorialStep(EGeneralTutorialStep NewStep);
	TSubclassOf<UUserWidget> GetWidgetClassForGeneralTutorialStep(EGeneralTutorialStep Step);


	void UpdateTutorialLogic(float DeltaTime); // Handles Interaction Tutorial timing
	void OnTutorialHoldTimeComplete();
	void SetTutorialStep(ETutorialStep NewStep); // Sets Interaction Tutorial step
	void UpdateTutorialUI(); // Updates Interaction Tutorial UI
	void CheckTutorialInput(ETutorialStep StepToCheck, bool bLeftGrab, bool bRightGrab, bool bLeftTrigger, bool bRightTrigger);
	TSubclassOf<UUserWidget> GetWidgetClassForTutorialStep(ETutorialStep Step); // Gets Interaction Tutorial Widget


	// --- Vibration Control ---
	APlayerController* GetPlayerControllerRef() const;
	void PlayHaptic(EControllerHand Hand, UHapticFeedbackEffect_Base* HapticEffect, float Scale = 1.0f, bool bLoop = false);
	void StopHaptic(EControllerHand Hand);
	void UpdateConductVibration(float Speed);
	bool ShouldVibrateForCondition(EConditionType ConditionToCheck) const;

	bool bLeftGrabVibrationActive = false;
	bool bRightGrabVibrationActive = false;
	bool bLeftTriggerVibrationActive = false;
	bool bRightTriggerVibrationActive = false;

	float LastKnownRightHandSpeed = 0.0f;
};
// --- END OF FILE StudyGameController.h ---