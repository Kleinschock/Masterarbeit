#include "StudyGameController.h"
#include "StudyManager.h"
#include "InteractionLoggerComponent.h"
#include "TutorialWidgetInterface.h" // Interface for updating tutorial widget progress
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/EnumProperty.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Sound/SoundBase.h"
#include "Logging/LogMacros.h"
#include "Components/AudioComponent.h" // If needed for sounds
#include "IXRTrackingSystem.h" // If needed for HMD checks etc.
#include "Engine/Engine.h" // If needed for GEngine
#include "Kismet/KismetMathLibrary.h" // For math functions like Clamp

DEFINE_LOG_CATEGORY_STATIC(LogStudyController, Log, All);

// Constructor
AStudyGameController::AStudyGameController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentPhase = EStudyPhase::MainMenu;

	// Setup Widget Component
	CurrentDisplayWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DisplayWidgetComponent"));
	if (CurrentDisplayWidgetComponent)
	{
		// Ensure RootComponent exists before attaching
		if (!GetRootComponent()) {
			USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
			SetRootComponent(SceneRoot);
		}
		CurrentDisplayWidgetComponent->SetupAttachment(GetRootComponent());
		CurrentDisplayWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
		CurrentDisplayWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f)); // Center pivot
		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetVisibility(false); // Start hidden
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Enable collision for interaction
		CurrentDisplayWidgetComponent->SetGenerateOverlapEvents(false); // Overlaps not typically needed for UI
	}
	StudyManager = nullptr;

	// Initialize GEMS score array with default values (e.g., 0) for 9 items
	GemsScores.Init(0, 9);
}

// BeginPlay
void AStudyGameController::BeginPlay()
{
	Super::BeginPlay();

	// Find or Spawn StudyManager
	if (!StudyManager) {
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStudyManager::StaticClass(), FoundActors);
		if (FoundActors.Num() > 0) {
			StudyManager = Cast<AStudyManager>(FoundActors[0]);
			UE_LOG(LogStudyController, Log, TEXT("Found existing StudyManager."));
		}
		else {
			UE_LOG(LogStudyController, Warning, TEXT("StudyManager not found. Spawning new one."));
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			StudyManager = GetWorld()->SpawnActor<AStudyManager>(AStudyManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		}
	}
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("Failed to find or spawn StudyManager! Study cannot proceed correctly."));
		// Consider adding fallback logic, preventing start, or showing an error message.
	}

	FindVisualizationTargets(); // Find actors implementing the interface
	ShowMainMenu(); // Start the experience with the main menu
}

// EndPlay
void AStudyGameController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear all timers associated with this object to prevent crashes
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ClearWorldUI(); // Hide any active UI
	Super::EndPlay(EndPlayReason);
}

// BeginDestroy
void AStudyGameController::BeginDestroy()
{
	// Attempt to clear timers again as a safety measure, checking for valid World
	if (UWorld* World = GetWorld()) {
		GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
		GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
		GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
	}
	Super::BeginDestroy();
}

// Tick
void AStudyGameController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update fade out timer remaining display value
	if (CurrentPhase == EStudyPhase::FadingOut && GetWorldTimerManager().IsTimerActive(FadeOutTimerHandle)) {
		CurrentFadeOutTimeRemaining = GetWorldTimerManager().GetTimerRemaining(FadeOutTimerHandle);
	}
	else if (CurrentPhase != EStudyPhase::FadingOut) {
		CurrentFadeOutTimeRemaining = 0.0f; // Reset if not fading
	}

	// Update interaction tutorial logic (hold timers, etc.)
	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		UpdateTutorialLogic(DeltaTime);
	}

	// Update conduct vibration intensity based on hand speed if active
	if (bRightTriggerVibrationActive && ShouldVibrateForCondition(CurrentCondition)) {
		UpdateConductVibration(LastKnownRightHandSpeed);
	}
}

// FindVisualizationTargets
void AStudyGameController::FindVisualizationTargets()
{
	VisualizationTargets.Empty();
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UVisualizationControllable::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		if (Actor) // Basic validity check
		{
			VisualizationTargets.Add(Actor);
		}
	}
	UE_LOG(LogStudyController, Log, TEXT("Found %d visualization target(s)."), VisualizationTargets.Num());
}

// InitializeStudy
void AStudyGameController::InitializeStudy(int32 ParticipantID, EBetweenCondition BetweenID, EGroupType GroupID)
{
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("InitializeStudy failed: StudyManager is null."));
		ShowMainMenu(); // Go back to main menu or show error
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Initializing Study: ID=%d, Env=%s, Group=%s"), ParticipantID, *UEnum::GetDisplayValueAsText(BetweenID).ToString(), *UEnum::GetDisplayValueAsText(GroupID).ToString());
	StudyManager->InitializeStudy(ParticipantID, BetweenID, GroupID);
	FindVisualizationTargets(); // Ensure targets are found after potential level changes/setup

	// Set Passthrough mode based on the assigned between-subject condition
	UE_LOG(LogStudyController, Log, TEXT("Setting initial Passthrough mode based on BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(BetweenID).ToString());
	SetPassthroughMode(BetweenID == EBetweenCondition::AR);

	// Initialize Interaction Logger
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (IsValid(PlayerPawn)) {
		LoggerComponentRef = PlayerPawn->FindComponentByClass<UInteractionLoggerComponent>();
		if (LoggerComponentRef.IsValid()) {
			FString ParticipantStr = FString::FromInt(ParticipantID);
			// Pass this controller as context if logger needs access to phase/condition info
			LoggerComponentRef->InitializeLogging(ParticipantStr, this);
			UE_LOG(LogStudyController, Log, TEXT("Interaction Logger Component initialized for Participant %s."), *ParticipantStr);
		}
		else {
			UE_LOG(LogStudyController, Error, TEXT("InitializeStudy FAILED to find UInteractionLoggerComponent on Player Pawn '%s'. Interaction logging will NOT work."), *PlayerPawn->GetName());
		}
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("InitializeStudy FAILED to get Player Pawn. Cannot initialize Interaction Logger."));
	}

	StartGeneralTutorial(); // Proceed to the general tutorial sequence
}

// --- General Tutorial Flow ---
void AStudyGameController::StartGeneralTutorial()
{
	UE_LOG(LogStudyController, Log, TEXT("Starting General Tutorial Sequence"));
	CurrentPhase = EStudyPhase::GeneralTutorial;
	ClearWorldUI();
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	SetTutorialStep(ETutorialStep::None); // Reset interaction tutorial state
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None; // Reset general tutorial state

	// Begin with the intro step
	SetGeneralTutorialStep(EGeneralTutorialStep::Intro);
}

void AStudyGameController::SetGeneralTutorialStep(EGeneralTutorialStep NewStep)
{
	if (CurrentPhase != EStudyPhase::GeneralTutorial) {
		UE_LOG(LogStudyController, Warning, TEXT("SetGeneralTutorialStep called outside of GeneralTutorial Phase. Current Phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Optionally force phase or return, for now just log and proceed
	}
	CurrentGeneralTutorialStep = NewStep;
	UE_LOG(LogStudyController, Log, TEXT("Setting General Tutorial Step to: %s"), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());
	DisplayWidgetInWorld(GetWidgetClassForGeneralTutorialStep(NewStep));
	// If reusing SAM/Presence widgets, you might need logic here to tell them they are in "tutorial mode"
}

void AStudyGameController::CompleteGeneralTutorialIntro()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::Intro) {
		SetGeneralTutorialStep(EGeneralTutorialStep::SAMExplanation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialIntro called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialSAMExplanation()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::SAMExplanation) {
		SetGeneralTutorialStep(EGeneralTutorialStep::SAMEvaluation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialSAMExplanation called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::SubmitTutorialSAM(int32 Valence, int32 Arousal, int32 Dominance)
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::SAMEvaluation) {
		UE_LOG(LogStudyController, Log, TEXT("Tutorial SAM Submitted: V=%d, A=%d, D=%d (Data NOT recorded)"), Valence, Arousal, Dominance);
		// DO NOT call StudyManager->RecordConditionResults here!
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceExplanation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialSAM called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialPresenceExplanation()
{
	// Check the correct step before accessing it (Typo Fix)
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceExplanation) {
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceEvaluation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialPresenceExplanation called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::SubmitTutorialPresence(int32 Presence)
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceEvaluation) {
		UE_LOG(LogStudyController, Log, TEXT("Tutorial Presence Submitted: P=%d (Data NOT recorded)"), Presence);
		// DO NOT call StudyManager->RecordConditionResults here!
		SetGeneralTutorialStep(EGeneralTutorialStep::Ending);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialPresence called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialEnding()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::Ending) {
		UE_LOG(LogStudyController, Log, TEXT("General Tutorial Sequence Complete. Proceeding to first condition briefing."));
		CurrentGeneralTutorialStep = EGeneralTutorialStep::None; // Reset state
		BeginCondition(); // Start the actual study flow (which begins with briefing)
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialEnding called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}
// --- End General Tutorial Flow ---


// BeginCondition
void AStudyGameController::BeginCondition()
{
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("BeginCondition failed: StudyManager is null. Cannot proceed."));
		ShowMainMenu(); // Fallback to main menu
		return;
	}

	// Ensure Passthrough mode is correct for the study's BetweenCondition
	UE_LOG(LogStudyController, Log, TEXT("BeginCondition: Ensuring Passthrough mode for BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(StudyManager->BetweenCondition).ToString());
	SetPassthroughMode(StudyManager->BetweenCondition == EBetweenCondition::AR);

	if (StudyManager->HasMoreConditions())
	{
		// Get the next condition type from the manager
		CurrentCondition = StudyManager->GetNextCondition();
		UE_LOG(LogStudyController, Log, TEXT("Beginning Briefing Phase for: %s (Condition Index: %d)"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString(), StudyManager->CurrentConditionIndex);

		CurrentPhase = EStudyPhase::Briefing;
		TSubclassOf<UUserWidget> BriefingWidget = GetWidgetClassForBriefing(CurrentCondition);
		DisplayWidgetInWorld(BriefingWidget);
		// The Briefing widget's button should call StartPreparationPhase()
	}
	else
	{
		// No more conditions left in the sequence
		UE_LOG(LogStudyController, Log, TEXT("BeginCondition: No more conditions left. Ending study."));
		EndStudy();
	}
}

// StartPreparationPhase
void AStudyGameController::StartPreparationPhase()
{
	// Allow calling from Briefing OR InteractionTutorial completion
	if (CurrentPhase != EStudyPhase::Briefing && CurrentPhase != EStudyPhase::InteractionTutorial) {
		UE_LOG(LogStudyController, Warning, TEXT("StartPreparationPhase called from unexpected phase: %s. Expected Briefing or InteractionTutorial."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// You might want to add recovery logic or simply return
		return;
	}

	// Check if the current condition requires the interaction tutorial AND we didn't just finish it
	// Assuming Baseline=Control, Passive=NoInteraction, Active=WithInteraction
	bool bRequiresInteractionTutorial = (CurrentCondition == EConditionType::WithInteraction); // 'Active' condition

	if (bRequiresInteractionTutorial && CurrentPhase != EStudyPhase::InteractionTutorial)
	{
		// Start the interaction tutorial first
		UE_LOG(LogStudyController, Log, TEXT("Condition requires Interaction Tutorial. Starting it now."));
		StartInteractionTutorial();
		// The tutorial completion (AdvanceTutorialStep) will eventually call StartPreparationPhase again
	}
	else // For Control, NoInteraction, or after completing Interaction Tutorial
	{
		// Proceed to the 'Ready to Start' screen
		UE_LOG(LogStudyController, Log, TEXT("Starting Preparation Phase UI for: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
		CurrentPhase = EStudyPhase::Preparation;
		SetupVisualizationForCondition(CurrentCondition); // Prepare particles etc.
		DisplayWidgetInWorld(PreparationWidgetClass);
		// The button on PreparationWidgetClass calls StartVisualization()
	}
}

// StartVisualization
void AStudyGameController::StartVisualization()
{
	// Should only be called from Preparation phase (button press)
	if (CurrentPhase != EStudyPhase::Preparation) {
		UE_LOG(LogStudyController, Warning, TEXT("StartVisualization called from unexpected phase: %s. Expected Preparation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	CurrentPhase = EStudyPhase::Running;
	FString ConditionNameStr = StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString();
	UE_LOG(LogStudyController, Log, TEXT("Starting Visualization RUNNING Phase for condition: %s. Found %d targets."), *ConditionNameStr, VisualizationTargets.Num());

	ClearWorldUI(); // Hide preparation widget

	// Tell visualization targets to start based on the current condition
	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass())) {
			IVisualizationControllable::Execute_StartActiveVisualization(TargetObj, CurrentCondition);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("StartVisualization: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}

	// Start the timer for the visualization duration
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle); // Clear fade timer just in case
	GetWorldTimerManager().SetTimer(VisualizationTimerHandle, this, &AStudyGameController::EndVisualizationPhase, VisualizationDuration, false);
}

// StartVisualizationControlOnly (for Interaction Tutorial)
void AStudyGameController::StartVisualizationControlOnly()
{
	UE_LOG(LogStudyController, Log, TEXT("Starting Visualization Control Only (for Tutorial)."));
	ClearWorldUI();

	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets)
	{
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass()))
		{
			// Call the specific function for control-only visualization on the interface
			IVisualizationControllable::Execute_StartVisualization_ControlOnly(TargetObj);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("StartVisualizationControlOnly: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}
}

// EndVisualizationPhase
void AStudyGameController::EndVisualizationPhase()
{
	if (CurrentPhase != EStudyPhase::Running) {
		UE_LOG(LogStudyController, Warning, TEXT("EndVisualizationPhase called from incorrect phase: %s. Expected Running."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Visualization Duration Elapsed. Starting Fade Out (%f seconds)."), FadeOutDuration);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle); // Stop the main viz timer

	CurrentPhase = EStudyPhase::FadingOut;
	CurrentFadeOutTimeRemaining = FadeOutDuration;
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle); // Ensure previous fade timer is clear

	// Start fade out timer if duration > 0, otherwise transition immediately
	if (FadeOutDuration > KINDA_SMALL_NUMBER) { // Use KINDA_SMALL_NUMBER for float comparison
		GetWorldTimerManager().SetTimer(FadeOutTimerHandle, this, &AStudyGameController::CompleteFadeOutAndTransition, FadeOutDuration, false);
	}
	else {
		CompleteFadeOutAndTransition(); // Call directly if no fade duration
	}
}

// CompleteFadeOutAndTransition
void AStudyGameController::CompleteFadeOutAndTransition()
{
	// Check if we are in FadingOut, or if we were in Running with zero FadeOutDuration
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= KINDA_SMALL_NUMBER)) {
		UE_LOG(LogStudyController, Warning, TEXT("CompleteFadeOutAndTransition called from incorrect phase: %s. Expected FadingOut or Running (with 0 fade)."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Fade Out Complete (or skipped). Stopping Visualization and Starting Questionnaire Sequence."));
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	CurrentFadeOutTimeRemaining = 0.0f;

	// Stop visualization on all targets
	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass())) {
			IVisualizationControllable::Execute_StopActiveVisualization(TargetObj);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("CompleteFadeOutAndTransition: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}

	// Stop any active haptics
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	bLeftGrabVibrationActive = bRightGrabVibrationActive = bLeftTriggerVibrationActive = bRightTriggerVibrationActive = false; // Reset flags

	StartSAMEvaluation(); // <--- Transition to the first questionnaire (SAM)
}

// --- Questionnaire Flow ---

// StartSAMEvaluation
void AStudyGameController::StartSAMEvaluation()
{
	// Should be called from CompleteFadeOutAndTransition
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= KINDA_SMALL_NUMBER)) {
		UE_LOG(LogStudyController, Warning, TEXT("StartSAMEvaluation called from unexpected phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Decide recovery: return? proceed? For now, proceed with warning.
	}

	CurrentPhase = EStudyPhase::SamEvaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting SAM Evaluation Phase."));

	// Reset temporary scores before starting questionnaires for the *current* condition
	ValenceScore = ArousalScore = DominanceScore = PresenceScore = 0;
	GemsScores.Init(0, 9); // Reset GEMS scores array to defaults

	DisplayWidgetInWorld(SAMWidgetClass);
	// The WBP_SAM widget's submit button should call StartGemsPart1Evaluation()
}

// StartGemsPart1Evaluation (Called by SAM Widget Submit)
void AStudyGameController::StartGemsPart1Evaluation()
{
	if (CurrentPhase != EStudyPhase::SamEvaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("StartGemsPart1Evaluation called from incorrect phase: %s. Expected SamEvaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return; // Don't proceed if flow is wrong
	}
	// NOTE: SAM scores (Valence, Arousal, Dominance) are passed later in SubmitAllQuestionnairesAndAdvance
	// We just transition the phase and show the next widget here.

	CurrentPhase = EStudyPhase::GemsPart1Evaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting GEMS Part 1 Evaluation Phase."));
	DisplayWidgetInWorld(GemsPart1WidgetClass);
	// The WBP_GEMS_Part1 widget's submit button should call SubmitGemsPart1AndAdvance()
}

// SubmitGemsPart1AndAdvance (Called by GEMS Part 1 Widget Submit)
void AStudyGameController::SubmitGemsPart1AndAdvance(const TArray<int32>& Part1Scores)
{
	if (CurrentPhase != EStudyPhase::GemsPart1Evaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitGemsPart1AndAdvance called from incorrect phase: %s. Expected GemsPart1Evaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	// Validate received data (expecting 5 scores)
	if (Part1Scores.Num() != 5) {
		UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart1AndAdvance received incorrect number of scores: %d. Expected 5. Storing defaults."), Part1Scores.Num());
		// Handle error: Don't store partial data, rely on initialized defaults in GemsScores array.
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Received GEMS Part 1 scores (Count: %d). Storing."), Part1Scores.Num());
		// Store the first 5 scores into our temporary member array
		for (int32 i = 0; i < Part1Scores.Num(); ++i) {
			// Double check array bounds for safety, though GemsScores should be size 9
			if (GemsScores.IsValidIndex(i)) {
				GemsScores[i] = Part1Scores[i];
			}
			else {
				UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart1AndAdvance: Invalid index %d while storing GEMS scores."), i);
			}
		}
	}

	// Proceed to the next part of the GEMS questionnaire
	StartGemsPart2Evaluation();
}

// StartGemsPart2Evaluation (Called internally by SubmitGemsPart1AndAdvance)
void AStudyGameController::StartGemsPart2Evaluation()
{
	// This function is called immediately after SubmitGemsPart1AndAdvance successfully stores data.
	// The phase is still technically GemsPart1Evaluation when this is called,
	// so we set the new phase here.
	CurrentPhase = EStudyPhase::GemsPart2Evaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting GEMS Part 2 Evaluation Phase."));
	DisplayWidgetInWorld(GemsPart2WidgetClass);
	// The WBP_GEMS_Part2 widget's submit button should call SubmitGemsPart2AndAdvance()
}

// SubmitGemsPart2AndAdvance (Called by GEMS Part 2 Widget Submit)
void AStudyGameController::SubmitGemsPart2AndAdvance(const TArray<int32>& Part2Scores)
{
	if (CurrentPhase != EStudyPhase::GemsPart2Evaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitGemsPart2AndAdvance called from incorrect phase: %s. Expected GemsPart2Evaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	// Validate received data (expecting 4 scores)
	if (Part2Scores.Num() != 4) {
		UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart2AndAdvance received incorrect number of scores: %d. Expected 4. Storing defaults."), Part2Scores.Num());
		// Handle error: Don't store partial data.
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Received GEMS Part 2 scores (Count: %d). Storing."), Part2Scores.Num());
		// Store the remaining 4 scores (indices 5, 6, 7, 8)
		for (int32 i = 0; i < Part2Scores.Num(); ++i) {
			int32 TargetIndex = i + 5; // Start storing at index 5
			if (GemsScores.IsValidIndex(TargetIndex)) {
				GemsScores[TargetIndex] = Part2Scores[i];
			}
			else {
				UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart2AndAdvance: Invalid index %d while storing GEMS scores."), TargetIndex);
			}
		}
	}

	// All GEMS scores collected (hopefully), proceed to Presence
	StartPresenceEvaluation();
}

// StartPresenceEvaluation (Called internally by SubmitGemsPart2AndAdvance)
void AStudyGameController::StartPresenceEvaluation()
{
	// Called after GEMS Part 2 is submitted. Phase is GemsPart2Evaluation before this.
	CurrentPhase = EStudyPhase::PresenceEvaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting Presence Evaluation Phase."));
	DisplayWidgetInWorld(PresenceWidgetClass);
	// The WBP_Presence widget's submit button should call SubmitAllQuestionnairesAndAdvance()
}

// SubmitAllQuestionnairesAndAdvance (Called by Presence Widget Submit)
void AStudyGameController::SubmitAllQuestionnairesAndAdvance(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence)
{
	if (CurrentPhase != EStudyPhase::PresenceEvaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitAllQuestionnairesAndAdvance called from incorrect phase: %s. Expected PresenceEvaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Received final Presence score (%d). Submitting all collected questionnaire data for condition."), Presence);

	// Store the final scores from SAM/Presence inputs provided by the Presence widget
	ValenceScore = Valence;
	ArousalScore = Arousal;
	DominanceScore = Dominance;
	PresenceScore = Presence;
	// The GEMS scores should already be stored in the GemsScores member array from previous steps.

	if (StudyManager)
	{
		// Pass all collected data (SAM, Presence, GEMS) to StudyManager for recording
		// Make sure the GemsScores array is correctly populated at this point
		if (GemsScores.Num() != 9) {
			UE_LOG(LogStudyController, Error, TEXT("SubmitAllQuestionnairesAndAdvance: GEMS scores array size is %d, expected 9! Recording potentially incorrect data."), GemsScores.Num());
			// Optionally re-initialize to defaults if size is wrong: GemsScores.Init(0, 9);
		}

		StudyManager->RecordConditionResults(ValenceScore, ArousalScore, DominanceScore, PresenceScore, GemsScores);

		// Move to the next condition or end the study
		CheckForMoreConditions();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("SubmitAllQuestionnairesAndAdvance failed: StudyManager is null. Cannot record results."));
		EndStudy(); // Attempt to save any previously recorded data and end
	}
}
// --- End Questionnaire Flow ---

// CheckForMoreConditions
void AStudyGameController::CheckForMoreConditions()
{
	if (StudyManager && StudyManager->HasMoreConditions()) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: More conditions remaining. Starting next briefing."));
		BeginCondition(); // Start the briefing for the next condition in the sequence
	}
	else if (StudyManager) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: All conditions complete. Ending study."));
		EndStudy(); // All conditions finished, proceed to end
	}
	else {
		// Should not happen if checks in SubmitAllQuestionnairesAndAdvance are correct
		UE_LOG(LogStudyController, Error, TEXT("CheckForMoreConditions failed: StudyManager is null. Cannot determine next step. Ending study as fallback."));
		EndStudy(); // Fallback
	}
}

// HasMoreConditions
bool AStudyGameController::HasMoreConditions() const
{
	return StudyManager ? StudyManager->HasMoreConditions() : false;
}

// EndStudy
void AStudyGameController::EndStudy()
{
	UE_LOG(LogStudyController, Log, TEXT("Ending Study. Saving results if possible."));
	CurrentPhase = EStudyPhase::Complete;

	// Clear all timers associated with this controller
	GetWorldTimerManager().ClearAllTimersForObject(this);

	CurrentFadeOutTimeRemaining = 0.0f; // Reset timer display
	SetTutorialStep(ETutorialStep::None); // Reset tutorial states
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None;

	// Stop any active haptics
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);

	// Attempt to save results via StudyManager
	if (StudyManager) {
		StudyManager->SaveResultsToCSV();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("EndStudy failed: StudyManager is null. Cannot save results."));
	}

	// Show the final completion screen
	DisplayWidgetInWorld(CompletionWidgetClass);
	// TODO: Add logic for final questionnaires (GoldMSI, etc.) here or triggered from CompletionWidget
}

// ShowMainMenu
void AStudyGameController::ShowMainMenu()
{
	UE_LOG(LogStudyController, Log, TEXT("Showing Main Menu"));
	CurrentPhase = EStudyPhase::MainMenu;
	SetTutorialStep(ETutorialStep::None); // Reset tutorial states
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None;
	ClearWorldUI(); // Hide any existing UI
	StopHaptic(EControllerHand::Left); // Ensure haptics are off
	StopHaptic(EControllerHand::Right);

	// Stop any active visualization
	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		if (Target.GetObject()) IVisualizationControllable::Execute_StopActiveVisualization(Target.GetObject());
	}

	DisplayWidgetInWorld(MainMenuWidgetClass);
	// The Main Menu widget should allow the user to trigger InitializeStudy()
}

// GetConditionName
FString AStudyGameController::GetConditionName()
{
	// Return appropriate name based on current phase and condition index
	EConditionType CondToGetName = EConditionType::Control; // Default

	if (CurrentPhase >= EStudyPhase::Briefing && CurrentPhase < EStudyPhase::Complete &&
		CurrentPhase != EStudyPhase::GeneralTutorial && CurrentPhase != EStudyPhase::InteractionTutorial)
	{
		// If we are in any phase related to an active condition (Briefing to PresenceEval)
		if (StudyManager && StudyManager->CurrentConditionIndex >= 0 && StudyManager->CurrentConditionIndex < StudyManager->ConditionSequence.Num()) {
			CondToGetName = StudyManager->ConditionSequence[StudyManager->CurrentConditionIndex];
		}
		else {
			// Fallback if index is invalid during these phases (shouldn't happen)
			return TEXT("Condition Error");
		}
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial) return TEXT("Interaction Tutorial");
	else if (CurrentPhase == EStudyPhase::GeneralTutorial) return TEXT("General Tutorial");
	else if (CurrentPhase == EStudyPhase::MainMenu) return TEXT("Main Menu");
	else if (CurrentPhase == EStudyPhase::Complete) return TEXT("Finished");
	else {
		// Other states (should ideally not request condition name)
		return TEXT("N/A");
	}

	const UEnum* EnumPtr = StaticEnum<EConditionType>();
	return EnumPtr ? EnumPtr->GetDisplayNameTextByValue((int64)CondToGetName).ToString() : TEXT("Unknown Condition");
}

// SetupVisualizationForCondition
void AStudyGameController::SetupVisualizationForCondition(EConditionType Condition)
{
	UE_LOG(LogStudyController, Log, TEXT("Setting up visualization for condition: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)Condition).ToString());
	// This is a good place to call any preparation functions on your visualization actors if needed
	// e.g., loading specific assets, resetting particle parameters before StartVisualization
	// for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
	//     if (Target.GetObject()) {
	//         IVisualizationControllable::Execute_PrepareForCondition(Target.GetObject(), Condition); // Example interface function
	//     }
	// }
}

// GetStudyStatus
FString AStudyGameController::GetStudyStatus()
{
	if (!IsValid(StudyManager)) return TEXT("Error: Study Manager not found");

	// Basic Info
	FString StatusText = FString::Printf(TEXT("PID: %d | Env: %s | Group: %s\n"),
		StudyManager->ParticipantID,
		*StaticEnum<EBetweenCondition>()->GetDisplayNameTextByValue((int64)StudyManager->BetweenCondition).ToString(),
		StudyManager->GroupType == EGroupType::A ? TEXT("A") : TEXT("B")); // Add handling for other groups/randomization later

	// Condition Info
	int32 CurrentIdx = StudyManager->CurrentConditionIndex;
	int32 TotalConditions = StudyManager->GetTotalNumberOfConditions(); // Ensure StudyManager calculates this correctly
	if (TotalConditions <= 0 && StudyManager->ConditionSequence.Num() > 0) TotalConditions = StudyManager->ConditionSequence.Num(); // Fallback if GetTotal is 0 but sequence exists
	if (TotalConditions <= 0) TotalConditions = 3; // Absolute fallback assuming 3 conditions

	FString ConditionNameStr = GetConditionName(); // Use helper function
	FString ConditionProgressStr = TEXT("");

	// Determine Progress String based on current index and total
	if (CurrentPhase == EStudyPhase::Complete) {
		ConditionProgressStr = FString::Printf(TEXT("(%d / %d)"), TotalConditions, TotalConditions);
	}
	else if (CurrentPhase == EStudyPhase::MainMenu || CurrentIdx < 0) {
		ConditionProgressStr = FString::Printf(TEXT("(0 / %d)"), TotalConditions);
	}
	else if (CurrentIdx >= 0 && CurrentIdx < TotalConditions) {
		ConditionProgressStr = FString::Printf(TEXT("(%d / %d)"), CurrentIdx + 1, TotalConditions);
	}
	else {
		ConditionProgressStr = TEXT("(? / ?) - Idx Error"); // Index out of bounds
	}

	StatusText += FString::Printf(TEXT("Condition: %s %s\n"), *ConditionNameStr, *ConditionProgressStr);

	// Phase Info
	FString PhaseStr = StaticEnum<EStudyPhase>() ? StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString() : FString::Printf(TEXT("Unknown Phase (%d)"), (int32)CurrentPhase);
	StatusText += FString::Printf(TEXT("Phase: %s"), *PhaseStr);

	// Add specific details for certain phases
	if (CurrentPhase == EStudyPhase::FadingOut) {
		StatusText += FString::Printf(TEXT(" (%.1fs left)"), CurrentFadeOutTimeRemaining);
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		FString StepName = StaticEnum<ETutorialStep>() ? StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString() : TEXT("Unknown Step");
		StatusText += FString::Printf(TEXT(" (%s)"), *StepName);
		if (bIsHoldingRequiredInput) {
			StatusText += FString::Printf(TEXT(" | Hold: %.1f/%.1f s"), CurrentTutorialHoldTime, TutorialRequiredHoldTime);
		}
		StatusText += FString::Printf(TEXT(" | OK: %d/%d"), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount);
	}
	else if (CurrentPhase == EStudyPhase::GeneralTutorial) {
		FString StepName = StaticEnum<EGeneralTutorialStep>() ? StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString() : TEXT("Unknown Step");
		StatusText += FString::Printf(TEXT(" (%s)"), *StepName);
	}
	StatusText += TEXT("\n");

	return StatusText;
}

// --- UI Helper Functions ---
void AStudyGameController::ClearWorldUI()
{
	if (CurrentDisplayWidgetComponent)
	{
		CurrentDisplayWidgetComponent->SetVisibility(false);
		CurrentDisplayWidgetComponent->SetWidget(nullptr); // Clear the widget instance
	}
}

void AStudyGameController::DisplayWidgetInWorld(TSubclassOf<UUserWidget> WidgetClassToShow)
{
	if (!CurrentDisplayWidgetComponent) {
		UE_LOG(LogStudyController, Error, TEXT("DisplayWidgetInWorld - FAILED: CurrentDisplayWidgetComponent is null."));
		return;
	}

	// Clear previous widget first
	ClearWorldUI();

	if (!WidgetClassToShow) {
		UE_LOG(LogStudyController, Warning, TEXT("DisplayWidgetInWorld - WidgetClassToShow is null. UI remains hidden."));
		return; // Nothing to show
	}

	// Set the new widget class
	CurrentDisplayWidgetComponent->SetWidgetClass(WidgetClassToShow);
	UUserWidget* CreatedWidget = CurrentDisplayWidgetComponent->GetUserWidgetObject();

	if (CreatedWidget)
	{
		// Apply world transform and other settings
		CurrentDisplayWidgetComponent->SetWorldTransform(WorldUITransform);
		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Make it interactable
		CurrentDisplayWidgetComponent->SetGenerateOverlapEvents(false);

		// --- Draw Size Handling (Optional Refinement) ---
		// You might want to force a prepass and get desired size, but it can be unreliable.
		// Sticking to the component's default or a specified fallback might be safer.
		FVector2D CurrentDrawSize = CurrentDisplayWidgetComponent->GetDrawSize();
		if (CurrentDrawSize.X < 10.f || CurrentDrawSize.Y < 10.f) { // Check if default size is tiny
			FVector2D FallbackDrawSize(1000.f, 800.f); // Define a reasonable fallback
			UE_LOG(LogStudyController, Warning, TEXT("Widget Component DrawSize is very small (%s). Using fallback: %s"), *CurrentDrawSize.ToString(), *FallbackDrawSize.ToString());
			CurrentDisplayWidgetComponent->SetDrawSize(FallbackDrawSize);
		}
		// Else, assume the DrawSize set on the component is intentional.

		// Make the widget visible
		CurrentDisplayWidgetComponent->SetVisibility(true);
		UE_LOG(LogStudyController, Log, TEXT("Displayed widget: %s"), *WidgetClassToShow->GetName());

		// Special handling for tutorial UI updates immediately after display
		if (CurrentPhase == EStudyPhase::InteractionTutorial) {
			UpdateTutorialUI(); // Ensure progress bars etc. are correct
		}
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("DisplayWidgetInWorld: FAILED to get/create widget instance for %s."), *WidgetClassToShow->GetName());
	}
}

TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForBriefing(EConditionType Condition)
{
	switch (Condition) {
	case EConditionType::Control: return BriefingControlWidgetClass;
	case EConditionType::NoInteraction: return BriefingNoInteractionWidgetClass; // Passive
	case EConditionType::WithInteraction: return BriefingWithInteractionWidgetClass; // Active
	default:
		UE_LOG(LogStudyController, Error, TEXT("GetWidgetClassForBriefing: Unknown condition type %d"), (int32)Condition);
		return nullptr; // Return null or a default error widget class
	}
}

TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForGeneralTutorialStep(EGeneralTutorialStep Step)
{
	switch (Step) {
	case EGeneralTutorialStep::Intro:               return GeneralTutorialIntroWidgetClass;
	case EGeneralTutorialStep::SAMExplanation:      return GeneralTutorialSAMExplanationWidgetClass;
	case EGeneralTutorialStep::SAMEvaluation:       return SAMWidgetClass; // Reusing study widget
	case EGeneralTutorialStep::PresenceExplanation: return GeneralTutorialPresenceExplanationWidgetClass;
	case EGeneralTutorialStep::PresenceEvaluation:  return PresenceWidgetClass; // Reusing study widget
	case EGeneralTutorialStep::Ending:              return GeneralTutorialEndingWidgetClass;
	case EGeneralTutorialStep::None: default:       return nullptr;
	}
}

TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForTutorialStep(ETutorialStep Step)
{
	// Interaction Tutorial Widgets
	switch (Step) {
	case ETutorialStep::Intro:        return TutorialIntroWidgetClass;
	case ETutorialStep::LeftSpawn:    return TutorialLeftSpawnWidgetClass;
	case ETutorialStep::RightSpawn:   return TutorialRightSpawnWidgetClass;
	case ETutorialStep::RightConduct: return TutorialRightConductWidgetClass;
	case ETutorialStep::LeftPull:     return TutorialLeftPullWidgetClass;
	case ETutorialStep::Complete:     return TutorialCompleteWidgetClass;
	case ETutorialStep::None: default:return nullptr;
	}
}
// --- End UI Helper Functions ---


// --- Interaction Tutorial Logic ---
void AStudyGameController::StartInteractionTutorial()
{
	if (CurrentPhase != EStudyPhase::Briefing) {
		UE_LOG(LogStudyController, Warning, TEXT("StartInteractionTutorial called from unexpected phase: %s. Expected Briefing."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Proceeding anyway, but log indicates potential flow issue
	}

	UE_LOG(LogStudyController, Log, TEXT("Starting Interaction Tutorial"));
	CurrentPhase = EStudyPhase::InteractionTutorial;
	ClearWorldUI(); // Hide briefing widget
	StopHaptic(EControllerHand::Left); // Ensure previous vibrations stopped
	StopHaptic(EControllerHand::Right);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle); // Stop study timers if running
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);

	// Start the particle visualization without music sync/interaction logic
	StartVisualizationControlOnly();

	// Begin with the tutorial intro step
	SetTutorialStep(ETutorialStep::Intro); // This calls UpdateTutorialUI -> DisplayWidgetInWorld
}

void AStudyGameController::AdvanceTutorialStep()
{
	UE_LOG(LogStudyController, Log, TEXT("Advancing Interaction Tutorial Step from %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());

	// Stop any haptics/timers related to the completed step
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);

	ETutorialStep NextStep = ETutorialStep::None;
	switch (CurrentTutorialStep) {
	case ETutorialStep::Intro:        NextStep = ETutorialStep::LeftSpawn; break;
	case ETutorialStep::LeftSpawn:    NextStep = ETutorialStep::RightSpawn; break;
	case ETutorialStep::RightSpawn:   NextStep = ETutorialStep::RightConduct; break;
	case ETutorialStep::RightConduct: NextStep = ETutorialStep::LeftPull; break;
	case ETutorialStep::LeftPull:     NextStep = ETutorialStep::Complete; break;
	case ETutorialStep::Complete:
		UE_LOG(LogStudyController, Log, TEXT("Interaction Tutorial Completed. Moving to Preparation Phase."));
		// Stop the control-only visualization
		for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
			if (Target.GetObject()) IVisualizationControllable::Execute_StopActiveVisualization(Target.GetObject());
		}
		SetTutorialStep(ETutorialStep::None); // Reset tutorial state internally
		// Call StartPreparationPhase to show the "Ready to Start" UI for the actual condition
		StartPreparationPhase();
		return; // Exit function after starting preparation
	case ETutorialStep::None: default:
		UE_LOG(LogStudyController, Warning, TEXT("Cannot advance from None or invalid tutorial step (%d)."), (int32)CurrentTutorialStep);
		return; // Cannot advance
	}

	if (NextStep != ETutorialStep::None) {
		SetTutorialStep(NextStep); // Set the next step (resets state, updates UI)
	}
}

void AStudyGameController::SetTutorialStep(ETutorialStep NewStep)
{
	// Reset state variables for the new step
	CurrentTutorialStep = NewStep;
	CurrentTutorialHoldTime = 0.0f;
	CurrentTutorialSuccessCount = 0;
	bIsHoldingRequiredInput = false;
	GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle); // Ensure timer is cleared
	StopHaptic(EControllerHand::Left); // Stop vibration when step changes
	StopHaptic(EControllerHand::Right);

	UE_LOG(LogStudyController, Log, TEXT("Set Interaction Tutorial Step to: %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());

	UpdateTutorialUI(); // Display the widget and update progress for the new step
}

void AStudyGameController::UpdateTutorialUI()
{
	TSubclassOf<UUserWidget> ExpectedWidgetClass = GetWidgetClassForTutorialStep(CurrentTutorialStep);

	// If the step is None or doesn't have a widget, clear the UI.
	if (!ExpectedWidgetClass) {
		UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: No widget class for step %s. Clearing UI."), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
		ClearWorldUI();
		return;
	}

	UUserWidget* CurrentWidget = nullptr;
	if (CurrentDisplayWidgetComponent) {
		CurrentWidget = CurrentDisplayWidgetComponent->GetUserWidgetObject();
	}

	bool bWidgetNeedsDisplayOrUpdate = true;

	// Check if the correct widget is *already* displayed and visible
	if (CurrentWidget && CurrentWidget->GetClass() == ExpectedWidgetClass && CurrentDisplayWidgetComponent->IsVisible()) {
		// Correct widget is showing, just update its progress display
		UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: Correct widget (%s) visible. Updating progress."), *ExpectedWidgetClass->GetName());
		bWidgetNeedsDisplayOrUpdate = true; // Ensure update happens below
	}
	else {
		// Widget is wrong, null, or hidden. Display the correct one.
		if (!CurrentWidget)
		{
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: No current widget. Displaying %s."), *ExpectedWidgetClass->GetName());
		}
		else if (CurrentWidget->GetClass() != ExpectedWidgetClass)
		{
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: Widget mismatch (Current: %s, Expected: %s). Displaying %s."), *CurrentWidget->GetClass()->GetName(), *ExpectedWidgetClass->GetName(), *ExpectedWidgetClass->GetName());
		}
		else if (!CurrentDisplayWidgetComponent->IsVisible())
		{
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: Widget component hidden. Displaying %s."), *ExpectedWidgetClass->GetName());
		}
		DisplayWidgetInWorld(ExpectedWidgetClass); // This handles clearing old/showing new

		// Get the new widget instance *after* displaying it
		CurrentWidget = CurrentDisplayWidgetComponent ? CurrentDisplayWidgetComponent->GetUserWidgetObject() : nullptr;
		bWidgetNeedsDisplayOrUpdate = (CurrentWidget != nullptr); // Update if widget was successfull y createdx
	}

	// --- UPDATE WIDGET PROGRESS (using Interface) ---
	if (bWidgetNeedsDisplayOrUpdate && CurrentWidget) {
		// Check if the widget implements our progress update interface
		if (CurrentWidget->GetClass()->ImplementsInterface(UTutorialWidgetInterface::StaticClass())) {
			// Calculate hold progress safely (avoid division by zero)
			float HoldProgress = (TutorialRequiredHoldTime > KINDA_SMALL_NUMBER)
				? FMath::Clamp(CurrentTutorialHoldTime / TutorialRequiredHoldTime, 0.0f, 1.0f)
				: (bIsHoldingRequiredInput ? 1.0f : 0.0f); // Show full if holding and required time is ~0

			// Call the interface function (Blueprint Event) to update the visual display
			ITutorialWidgetInterface::Execute_UpdateProgressDisplay(CurrentWidget, CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
			// UE_LOG(LogStudyController, VeryVerbose, TEXT("UpdateTutorialUI: Called UpdateProgressDisplay (Success: %d/%d, Hold: %.2f)"), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
		}
		else {
			// Log a warning if the displayed widget is supposed to show progress but lacks the interface
			UE_LOG(LogStudyController, Warning, TEXT("UpdateTutorialUI: Widget %s does NOT implement UTutorialWidgetInterface! Cannot update progress."), *CurrentWidget->GetName());
		}
	}
	else if (bWidgetNeedsDisplayOrUpdate && !CurrentWidget) {
		UE_LOG(LogStudyController, Error, TEXT("UpdateTutorialUI: Failed to get widget instance after display/check. Cannot update progress."));
	}
	// --- End Update Widget Progress ---
}


void AStudyGameController::UpdateTutorialLogic(float DeltaTime)
{
	// No active input tracking needed for Intro, Complete, or None steps
	if (CurrentTutorialStep == ETutorialStep::Intro || CurrentTutorialStep == ETutorialStep::Complete || CurrentTutorialStep == ETutorialStep::None) {
		bIsHoldingRequiredInput = false; // Ensure flag is correct
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle); // Clear timer if running
			StopHaptic(EControllerHand::Left); // Stop vibration if timer cleared
			StopHaptic(EControllerHand::Right);
		}
		return;
	}

	// If the correct input is currently being held
	if (bIsHoldingRequiredInput) {
		CurrentTutorialHoldTime += DeltaTime;

		// Determine which hand should vibrate based on the current step
		EControllerHand HandToVibrate = EControllerHand::Left; // Default
		if (CurrentTutorialStep == ETutorialStep::RightSpawn || CurrentTutorialStep == ETutorialStep::RightConduct) {
			HandToVibrate = EControllerHand::Right;
		}

		// Start or continue hold timer and vibration if not already running/complete
		if (!GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			float RemainingTime = FMath::Max(0.0f, TutorialRequiredHoldTime - CurrentTutorialHoldTime);
			if (RemainingTime > KINDA_SMALL_NUMBER) {
				// Start timer for the remaining duration
				GetWorldTimerManager().SetTimer(TutorialHoldTimerHandle, this, &AStudyGameController::OnTutorialHoldTimeComplete, RemainingTime, false);
				// Start looping hold vibration on the correct hand
				PlayHaptic(HandToVibrate, TutorialHoldHaptic, 1.0f, true);
			}
			else if (CurrentTutorialHoldTime >= TutorialRequiredHoldTime) {
				// Time already met or exceeded on this frame, trigger completion immediately
				OnTutorialHoldTimeComplete();
			}
		}
		// Update progress bar visualization via UpdateTutorialUI (called elsewhere or could be called here too)
		// Calling it here ensures smoother progress bar updates:
		UpdateTutorialUI();

	}
	else { // Input is NOT being held (or wrong input is held)
		bool bWasHolding = CurrentTutorialHoldTime > 0.0f; // Check if we *were* holding just before this frame

		// Reset hold time and clear timer if input released
		CurrentTutorialHoldTime = 0.0f;
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
			// Stop vibration ONLY if the timer was cleared (meaning we were vibrating)
			StopHaptic(EControllerHand::Left);
			StopHaptic(EControllerHand::Right);
		}

		// Update UI to show 0 progress if we were previously holding and just released
		if (bWasHolding) {
			UpdateTutorialUI();
		}
	}
}

void AStudyGameController::OnTutorialHoldTimeComplete()
{
	UE_LOG(LogStudyController, Log, TEXT("Tutorial Hold Time Complete for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());

	// Determine the hand *before* potentially advancing the step
	EControllerHand HandThatHeld = EControllerHand::Left;
	if (CurrentTutorialStep == ETutorialStep::RightSpawn || CurrentTutorialStep == ETutorialStep::RightConduct) {
		HandThatHeld = EControllerHand::Right;
	}

	// Crucial Check: Ensure the input is STILL being held when the timer completes!
	if (!bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Warning, TEXT("Hold timer completed, but input was released early!"));
		CurrentTutorialHoldTime = 0.0f; // Reset time
		StopHaptic(HandThatHeld); // Stop vibration if it was playing
		UpdateTutorialUI(); // Update UI to show 0 progress
		return; // Bail out, don't count as success
	}

	// --- Success Case ---
	CurrentTutorialSuccessCount++;
	CurrentTutorialHoldTime = 0.0f; // Reset hold timer for this success
	bIsHoldingRequiredInput = false; // **Crucial:** Reset flag so next PRESS is detected by CheckTutorialInput

	// Play "success" feedback
	StopHaptic(HandThatHeld); // Stop the looping hold haptic first
	PlayHaptic(HandThatHeld, TutorialSuccessHaptic, 1.0f, false); // Play one-shot success haptic
	if (TutorialSuccessSound) {
		UGameplayStatics::PlaySound2D(GetWorld(), TutorialSuccessSound); // Play success sound
	}

	// Update the on-screen progress display (shows new success count and resets hold bar)
	UpdateTutorialUI();

	// Check if enough successful holds have been completed for this step
	if (CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount) {
		UE_LOG(LogStudyController, Log, TEXT("Required success count (%d) reached for step %s. Advancing."), TutorialRequiredSuccessCount, *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
		// Advance to the next tutorial step
		// Optional: Add a small delay before advancing?
		// FTimerHandle TempHandle;
		// GetWorldTimerManager().SetTimer(TempHandle, this, &AStudyGameController::AdvanceTutorialStep, 0.5f, false);
		AdvanceTutorialStep(); // Advance immediately
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Success %d/%d for step %s. Waiting for next input hold."), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
		// Ready for the next hold attempt in the same step
	}
}

void AStudyGameController::CheckTutorialInput(ETutorialStep StepToCheck, bool bLeftGrab, bool bRightGrab, bool bLeftTrigger, bool bRightTrigger)
{
	// Ignore input if tutorial step doesn't require it or is already complete
	if (StepToCheck == ETutorialStep::Intro || StepToCheck == ETutorialStep::Complete || StepToCheck == ETutorialStep::None || CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount) {
		bIsHoldingRequiredInput = false; // Ensure flag is false if step doesn't need input
		return;
	}

	bool bCorrectInputPressed = false;
	EControllerHand HandToCheck = EControllerHand::Left; // Used for stopping vibration on release

	// Determine if the *correct* input for the *current* step is pressed
	switch (StepToCheck) {
	case ETutorialStep::LeftSpawn:    bCorrectInputPressed = bLeftGrab;    HandToCheck = EControllerHand::Left; break;
	case ETutorialStep::RightSpawn:   bCorrectInputPressed = bRightGrab;   HandToCheck = EControllerHand::Right; break;
	case ETutorialStep::RightConduct: bCorrectInputPressed = bRightTrigger; HandToCheck = EControllerHand::Right; break;
	case ETutorialStep::LeftPull:     bCorrectInputPressed = bLeftTrigger; HandToCheck = EControllerHand::Left; break;
	default: // Should not happen based on initial check
		return;
	}

	// --- State Change Logic ---
	// Check for PRESSING the correct input (Rising Edge Detection)
	if (bCorrectInputPressed && !bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input PRESSED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = true;
		CurrentTutorialHoldTime = 0.0f; // Reset hold time on new press
		// UpdateTutorialLogic in Tick will start the timer and haptics.
		UpdateTutorialUI(); // Update UI immediately to show press/reset progress bar
	}
	// Check for RELEASING the correct input (Falling Edge Detection)
	else if (!bCorrectInputPressed && bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input RELEASED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = false;
		// UpdateTutorialLogic in Tick will clear timer and haptics.
		// Explicitly stop haptic/timer here for immediate feedback if needed, though Tick should handle it.
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
			StopHaptic(HandToCheck); // Stop vibration if timer was active
		}
		CurrentTutorialHoldTime = 0.0f; // Ensure time resets on release
		UpdateTutorialUI(); // Update UI to show 0 hold progress
	}

	// Note: This logic currently ignores incorrect inputs. You could add feedback for pressing the wrong button here if desired.
}
// --- End Interaction Tutorial Logic ---


// --- Interaction Logging ---
void AStudyGameController::NotifyInteractionStart(FName ActionName, EControllerHand Hand)
{
	if (LoggerComponentRef.IsValid()) {
		LoggerComponentRef->LogInteractionEvent(ActionName, Hand, true); // True for Press/Start
	}
	else {
		// Log internally if logger component not found
		UE_LOG(LogStudyController, Warning, TEXT("NotifyInteractionStart: LoggerComponentRef is invalid. Cannot log action: %s"), *ActionName.ToString());
	}
}

void AStudyGameController::NotifyInteractionEnd(FName ActionName, EControllerHand Hand)
{
	if (LoggerComponentRef.IsValid()) {
		LoggerComponentRef->LogInteractionEvent(ActionName, Hand, false); // False for Release/End
	}
	else {
		UE_LOG(LogStudyController, Warning, TEXT("NotifyInteractionEnd: LoggerComponentRef is invalid. Cannot log action: %s"), *ActionName.ToString());
	}
}
// --- End Interaction Logging ---


// --- Input Handling ---
bool AStudyGameController::ShouldVibrateForCondition(EConditionType ConditionToCheck) const
{
	// Allow vibration ONLY during the Running phase AND for the 'WithInteraction' (Active) condition.
	// Note: Tutorial haptics are handled separately inside PlayHaptic/StopHaptic checks based on phase
	return (CurrentPhase == EStudyPhase::Running && ConditionToCheck == EConditionType::WithInteraction);
}

void AStudyGameController::HandleLeftGrab(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleLeftGrab: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	if (bPressed) {
		NotifyInteractionStart(FName("LeftGrab"), EControllerHand::Left); // Log interaction start
	}
	else {
		NotifyInteractionEnd(FName("LeftGrab"), EControllerHand::Left); // Log interaction end
	}

	// Tutorial Input Check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, bPressed, false, false, false);
	}

	// Study Interaction Vibration Logic (only if correct phase/condition)
	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bLeftGrabVibrationActive) {
				PlayHaptic(EControllerHand::Left, SpawnHaptic, SpawnHapticScale, true); // Start looping spawn haptic
				bLeftGrabVibrationActive = true;
			}
		}
		else {
			if (bLeftGrabVibrationActive) {
				StopHaptic(EControllerHand::Left); // Stop haptic on release
				bLeftGrabVibrationActive = false;
			}
		}
	} // NO SEMICOLON HERE BEFORE ELSE
	else {
		// Ensure vibration is stopped if condition/phase is wrong but flag was somehow true
		if (bLeftGrabVibrationActive) {
			StopHaptic(EControllerHand::Left);
			bLeftGrabVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleRightGrab(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleRightGrab: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	if (bPressed) {
		NotifyInteractionStart(FName("RightGrab"), EControllerHand::Right);
	}
	else {
		NotifyInteractionEnd(FName("RightGrab"), EControllerHand::Right);
	}

	// Tutorial Input Check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, bPressed, false, false);
	}

	// Study Interaction Vibration Logic
	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bRightGrabVibrationActive) {
				PlayHaptic(EControllerHand::Right, SpawnHaptic, SpawnHapticScale, true);
				bRightGrabVibrationActive = true;
			}
		}
		else {
			if (bRightGrabVibrationActive) {
				StopHaptic(EControllerHand::Right);
				bRightGrabVibrationActive = false;
			}
		}
	} // NO SEMICOLON HERE BEFORE ELSE
	else {
		if (bRightGrabVibrationActive) {
			StopHaptic(EControllerHand::Right);
			bRightGrabVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleLeftTrigger(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleLeftTrigger: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	if (bPressed) {
		NotifyInteractionStart(FName("LeftTrigger"), EControllerHand::Left);
	}
	else {
		NotifyInteractionEnd(FName("LeftTrigger"), EControllerHand::Left);
	}

	// Tutorial Input Check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, false, bPressed, false);
	}

	// Study Interaction Vibration Logic
	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bLeftTriggerVibrationActive) {
				PlayHaptic(EControllerHand::Left, PullHaptic, PullHapticScale, true); // Start looping pull haptic
				bLeftTriggerVibrationActive = true;
			}
		}
		else {
			if (bLeftTriggerVibrationActive) {
				StopHaptic(EControllerHand::Left); // Stop on release
				bLeftTriggerVibrationActive = false;
			}
		}
	} // NO SEMICOLON HERE BEFORE ELSE
	else {
		if (bLeftTriggerVibrationActive) {
			StopHaptic(EControllerHand::Left);
			bLeftTriggerVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleRightTrigger(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleRightTrigger: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	if (bPressed) {
		NotifyInteractionStart(FName("RightTrigger"), EControllerHand::Right);
	}
	else {
		NotifyInteractionEnd(FName("RightTrigger"), EControllerHand::Right);
	}

	// Tutorial Input Check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, false, false, bPressed);
	}

	// Study Interaction Vibration Logic (Conduct)
	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bRightTriggerVibrationActive) {
				// Start conduct haptic loop at low intensity, Tick will update based on speed
				PlayHaptic(EControllerHand::Right, ConductHaptic, 0.1f, true);
				bRightTriggerVibrationActive = true;
				LastKnownRightHandSpeed = 0.0f; // Reset speed on press
			}
		}
		else {
			if (bRightTriggerVibrationActive) {
				StopHaptic(EControllerHand::Right); // Stop on release
				bRightTriggerVibrationActive = false;
				LastKnownRightHandSpeed = 0.0f; // Reset speed
			}
		}
	} // NO SEMICOLON HERE BEFORE ELSE
	else {
		// Ensure stopped if phase/condition is wrong
		if (bRightTriggerVibrationActive) {
			StopHaptic(EControllerHand::Right);
			bRightTriggerVibrationActive = false;
			LastKnownRightHandSpeed = 0.0f;
		}
	}
}

void AStudyGameController::UpdateRightHandSpeed(float Speed)
{
	// Store the speed, Tick uses this to update conduct vibration intensity
	LastKnownRightHandSpeed = FMath::Abs(Speed); // Use absolute speed

	// Log interaction data (velocity) if logger is set up for it
	if (LoggerComponentRef.IsValid() && CurrentPhase == EStudyPhase::Running && CurrentCondition == EConditionType::WithInteraction) {
		// LoggerComponentRef->LogMovementData(EControllerHand::Right, Speed); // *** COMMENTED OUT - FUNCTION MISSING IN LOGGER ***
		// TODO: Uncomment this line after adding LogMovementData(EControllerHand Hand, float SpeedValue) to UInteractionLoggerComponent
	}
}
// --- End Input Handling ---


// --- Vibration Helpers ---
APlayerController* AStudyGameController::GetPlayerControllerRef() const
{
	// Cache this if called frequently? For now, just get it.
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

void AStudyGameController::PlayHaptic(EControllerHand Hand, UHapticFeedbackEffect_Base* HapticEffect, float Scale, bool bLoop)
{
	if (!HapticEffect) {
		// UE_LOG(LogStudyController, Warning, TEXT("PlayHaptic: No HapticEffect provided for hand %s."), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"));
		return;
	}

	APlayerController* PC = GetPlayerControllerRef();
	if (!PC) {
		UE_LOG(LogStudyController, Warning, TEXT("PlayHaptic: PlayerController is null."));
		return;
	}

	// Determine if haptic should play based on context (Tutorial vs Study)
	bool bAllowPlay = false;
	bool bIsTutorialHaptic = (HapticEffect == TutorialHoldHaptic || HapticEffect == TutorialSuccessHaptic);

	if (CurrentPhase == EStudyPhase::InteractionTutorial && bIsTutorialHaptic) {
		// Allow tutorial-specific haptics during the tutorial phase
		bAllowPlay = true;
	}
	else if (ShouldVibrateForCondition(CurrentCondition) && !bIsTutorialHaptic) {
		// Allow study interaction haptics only during the correct phase/condition
		bAllowPlay = true;
	}


	if (bAllowPlay) {
		// UE_LOG(LogStudyController, Verbose, TEXT("Playing Haptic %s on %s hand. Scale: %.2f, Loop: %s"), *HapticEffect->GetName(), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"), Scale, bLoop ? TEXT("True") : TEXT("False"));
		PC->PlayHapticEffect(HapticEffect, Hand, Scale, bLoop);
	}
	else {
		// Log why it was skipped if needed for debugging
		// UE_LOG(LogStudyController, Verbose, TEXT("PlayHaptic SKIPPED for %s on %s hand. Phase: %s, Condition: %s, IsTutorialHaptic: %s"),
		// 	*HapticEffect->GetName(),
		// 	Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"),
		// 	*StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(),
		// 	*StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString(),
		// 	bIsTutorialHaptic ? TEXT("True") : TEXT("False"));
	}
}

void AStudyGameController::StopHaptic(EControllerHand Hand)
{
	APlayerController* PC = GetPlayerControllerRef();
	if (PC) {
		// UE_LOG(LogStudyController, Verbose, TEXT("Stopping Haptic on %s hand."), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"));
		PC->StopHapticEffect(Hand);

		// Reset internal vibration state flags for the stopped hand
		if (Hand == EControllerHand::Left) {
			bLeftGrabVibrationActive = false;
			bLeftTriggerVibrationActive = false;
		}
		else { // Right Hand
			bRightGrabVibrationActive = false;
			bRightTriggerVibrationActive = false;
			LastKnownRightHandSpeed = 0.0f; // Reset speed tracking if stopping right hand
		}
	} // else { UE_LOG(LogStudyController, Warning, TEXT("StopHaptic: PlayerController is null.")); }
}

void AStudyGameController::UpdateConductVibration(float Speed)
{
	// This is called from Tick ONLY if bRightTriggerVibrationActive is true AND ShouldVibrateForCondition is true
	if (!ConductHaptic) return; // Check if effect asset is assigned

	APlayerController* PC = GetPlayerControllerRef();
	if (PC) {
		// Calculate intensity scale based on speed
		float BaseIntensity = 0.1f; // Minimum intensity when trigger is held but not moving
		float SpeedBasedIntensity = FMath::Abs(Speed) * ConductSpeedIntensityFactor; // Use absolute speed
		// Clamp total intensity to prevent excessive vibration
		float TotalIntensityScale = FMath::Clamp(BaseIntensity + SpeedBasedIntensity, 0.0f, MaxConductIntensityScale);

		// UE_LOG(LogStudyController, VeryVerbose, TEXT("Updating Conduct Haptic. Speed: %.2f -> Scale: %.2f"), Speed, TotalIntensityScale);

		// Update the looping haptic effect with the new scale.
		// Calling PlayHapticEffect repeatedly on a looping effect updates its parameters.
		PC->PlayHapticEffect(ConductHaptic, EControllerHand::Right, TotalIntensityScale, true); // Ensure loop is true
	}
}
// --- End Vibration Helpers ---