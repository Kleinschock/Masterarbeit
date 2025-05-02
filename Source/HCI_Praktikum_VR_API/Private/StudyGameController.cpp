// --- START OF FILE StudyGameController.cpp ---

#include "StudyGameController.h"
#include "StudyManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "UObject/EnumProperty.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Logging/LogMacros.h"
#include "TutorialWidgetInterface.h" // Make sure this is included

DEFINE_LOG_CATEGORY_STATIC(LogStudyController, Log, All);

// Constructor remains the same
AStudyGameController::AStudyGameController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentPhase = EStudyPhase::MainMenu;

	CurrentDisplayWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DisplayWidgetComponent"));
	if (CurrentDisplayWidgetComponent)
	{
		if (!GetRootComponent()) {
			USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
			SetRootComponent(SceneRoot);
		}
		CurrentDisplayWidgetComponent->SetupAttachment(GetRootComponent());
		CurrentDisplayWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
		CurrentDisplayWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetVisibility(false);
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	StudyManager = nullptr;
}

// BeginPlay remains the same
void AStudyGameController::BeginPlay()
{
	Super::BeginPlay();
	// Find or Spawn StudyManager (Keep existing logic)
	if (!StudyManager) {
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStudyManager::StaticClass(), FoundActors);
		if (FoundActors.Num() > 0) {
			StudyManager = Cast<AStudyManager>(FoundActors[0]);
			UE_LOG(LogStudyController, Log, TEXT("StudyGameController: Found existing StudyManager."));
		}
		else {
			UE_LOG(LogStudyController, Warning, TEXT("StudyGameController: StudyManager not found. Spawning new one."));
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			StudyManager = GetWorld()->SpawnActor<AStudyManager>(AStudyManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		}
	}
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("StudyGameController: Failed to find or spawn StudyManager!"));
	}

	FindVisualizationTargets();
	ShowMainMenu();
}


// EndPlay remains the same
void AStudyGameController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Kill every timer bound to this controller so no callbacks fire on a dead object
	GetWorldTimerManager().ClearAllTimersForObject(this);

	ClearWorldUI();
	Super::EndPlay(EndPlayReason);
}


// BeginDestroy remains the same
void AStudyGameController::BeginDestroy()
{
	UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::BeginDestroy - Starting cleanup."));
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
		GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
		GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
		UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::BeginDestroy - Attempted to clear timers."));
	}
	else
	{
		UE_LOG(LogStudyController, Warning, TEXT("AStudyGameController::BeginDestroy - Called with invalid World, cannot clear timers."));
	}
	Super::BeginDestroy();
	UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::BeginDestroy - Finished cleanup."));
}


// Tick remains the same
void AStudyGameController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentPhase == EStudyPhase::FadingOut && GetWorldTimerManager().IsTimerActive(FadeOutTimerHandle)) {
		CurrentFadeOutTimeRemaining = GetWorldTimerManager().GetTimerRemaining(FadeOutTimerHandle);
	}
	else if (CurrentPhase != EStudyPhase::FadingOut) {
		CurrentFadeOutTimeRemaining = 0.0f;
	}

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		UpdateTutorialLogic(DeltaTime);
	}

	if (bRightTriggerVibrationActive && CurrentPhase == EStudyPhase::Running && ShouldVibrateForCondition(CurrentCondition)) {
		UpdateConductVibration(LastKnownRightHandSpeed);
	}
}


// FindVisualizationTargets remains the same
void AStudyGameController::FindVisualizationTargets()
{
	VisualizationTargets.Empty();
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UVisualizationControllable::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		VisualizationTargets.Add(Actor);
	}
	UE_LOG(LogStudyController, Log, TEXT("StudyGameController: Found %d visualization target(s)."), VisualizationTargets.Num());
}


// InitializeStudy now starts General Tutorial
void AStudyGameController::InitializeStudy(int32 ParticipantID, EBetweenCondition BetweenID, EGroupType GroupID)
{
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("InitializeStudy failed: StudyManager is null."));
		return;
	}
	UE_LOG(LogStudyController, Log, TEXT("Initializing Study: ID=%d, Env=%s, Group=%s"), ParticipantID, *UEnum::GetDisplayValueAsText(BetweenID).ToString(), *UEnum::GetDisplayValueAsText(GroupID).ToString());
	StudyManager->InitializeStudy(ParticipantID, BetweenID, GroupID);
	FindVisualizationTargets();

	// *** ADDED: Set Passthrough Mode based on initial BetweenCondition ***
	UE_LOG(LogStudyController, Log, TEXT("Setting initial Passthrough mode based on BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(BetweenID).ToString());
	SetPassthroughMode(BetweenID == EBetweenCondition::AR);
	// *** END ADDED ***

	StartGeneralTutorial(); // Start the multi-step general tutorial
}


// StartGeneralTutorial initializes the new flow
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

// *** ADDED: Function to manage setting the general tutorial step and UI ***
void AStudyGameController::SetGeneralTutorialStep(EGeneralTutorialStep NewStep)
{
	if (CurrentPhase != EStudyPhase::GeneralTutorial)
	{
		UE_LOG(LogStudyController, Warning, TEXT("SetGeneralTutorialStep called outside of GeneralTutorial Phase. Current Phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Decide how to handle this - maybe force phase? For now, just log and set step.
		// CurrentPhase = EStudyPhase::GeneralTutorial; // Optional: Force phase
	}

	CurrentGeneralTutorialStep = NewStep;
	UE_LOG(LogStudyController, Log, TEXT("Setting General Tutorial Step to: %s"), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());

	TSubclassOf<UUserWidget> WidgetToShow = GetWidgetClassForGeneralTutorialStep(NewStep);
	DisplayWidgetInWorld(WidgetToShow);

	// If stepping into SAM or Presence Evaluation within the tutorial,
	// you might need to tell the reused widget that it's in tutorial mode.
	// This requires modifying the SAMWidgetClass/PresenceWidgetClass or using dedicated tutorial versions.
	// Example (if widgets have a SetTutorialMode function):
	// UUserWidget* CurrentWidget = CurrentDisplayWidgetComponent ? CurrentDisplayWidgetComponent->GetUserWidgetObject() : nullptr;
	// if (NewStep == EGeneralTutorialStep::SAMEvaluation || NewStep == EGeneralTutorialStep::PresenceEvaluation) {
	//     if (CurrentWidget && CurrentWidget->Implements<UTutorialModeWidgetInterface>()) { // Assuming an interface
	//          ITutorialModeWidgetInterface::Execute_SetTutorialMode(CurrentWidget, true);
	//     }
	// }
}


void AStudyGameController::CompleteGeneralTutorialIntro()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::Intro)
	{
		SetGeneralTutorialStep(EGeneralTutorialStep::SAMExplanation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialIntro called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialSAMExplanation()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::SAMExplanation)
	{
		SetGeneralTutorialStep(EGeneralTutorialStep::SAMEvaluation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialSAMExplanation called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::SubmitTutorialSAM(int32 Valence, int32 Arousal, int32 Dominance)
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::SAMEvaluation)
	{
		UE_LOG(LogStudyController, Log, TEXT("Tutorial SAM Submitted: V=%d, A=%d, D=%d (Data NOT recorded)"), Valence, Arousal, Dominance);
		// DO NOT call StudyManager->RecordSAMData here!
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceExplanation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialSAM called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialPresenceExplanation()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceExplanation)
	{
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceEvaluation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialPresenceExplanation called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::SubmitTutorialPresence(int32 Presence)
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceEvaluation)
	{
		UE_LOG(LogStudyController, Log, TEXT("Tutorial Presence Submitted: P=%d (Data NOT recorded)"), Presence);
		// DO NOT call StudyManager->RecordSAMData here!
		SetGeneralTutorialStep(EGeneralTutorialStep::Ending);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialPresence called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialEnding()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::Ending)
	{
		UE_LOG(LogStudyController, Log, TEXT("General Tutorial Sequence Complete. Proceeding to first condition."));
		CurrentGeneralTutorialStep = EGeneralTutorialStep::None; // Reset state
		// CurrentPhase will be set by BeginCondition
		BeginCondition(); // Start the actual study flow (which now includes briefing)
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialEnding called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

// --- End of General Tutorial Advancement ---



// *** MODIFIED: BeginCondition now transitions to Briefing phase first ***
void AStudyGameController::BeginCondition()
{
	if (!StudyManager) { UE_LOG(LogStudyController, Error, TEXT("BeginCondition failed: StudyManager is null.")); return; }

	// *** ADDED: Ensure Passthrough Mode is correct for the study environment ***
	// Redundant if InitializeStudy is the only place BetweenCondition is set,
	// but safe to include if the environment context could theoretically change.
	if (StudyManager) // Check StudyManager validity again
	{
		UE_LOG(LogStudyController, Log, TEXT("BeginCondition: Ensuring Passthrough mode for BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(StudyManager->BetweenCondition).ToString());
		SetPassthroughMode(StudyManager->BetweenCondition == EBetweenCondition::AR);
	}
	// *** END ADDED ***

	if (StudyManager->HasMoreConditions())
	{
		// ... (rest of BeginCondition remains the same) ...
		CurrentCondition = StudyManager->GetNextCondition();
		UE_LOG(LogStudyController, Log, TEXT("Beginning Briefing Phase for: %s (Index: %d)"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString(), StudyManager->CurrentConditionIndex);

		// --- Start Briefing ---
		CurrentPhase = EStudyPhase::Briefing;
		TSubclassOf<UUserWidget> BriefingWidget = GetWidgetClassForBriefing(CurrentCondition);
		DisplayWidgetInWorld(BriefingWidget);
	}
	else
	{
		UE_LOG(LogStudyController, Log, TEXT("BeginCondition: No more conditions left. Ending study."));
		EndStudy();
	}
}

// *** ADDED: Called by Briefing widget buttons to proceed ***
void AStudyGameController::StartPreparationPhase()
{
	if (CurrentPhase != EStudyPhase::Briefing && CurrentPhase != EStudyPhase::InteractionTutorial) // Allow calling from Interaction Tutorial completion
	{
		UE_LOG(LogStudyController, Warning, TEXT("StartPreparationPhase called from unexpected phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Optionally: Add recovery logic or just return
		// If we are already in Prep or beyond, maybe just do nothing?
		// if (CurrentPhase >= EStudyPhase::Preparation) return; // Example guard
		return;
	}

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		UE_LOG(LogStudyController, Log, TEXT("StartPreparationPhase called after Interaction Tutorial completion."));
		// Reset tutorial state, stop haptics etc. handled by AdvanceTutorialStep calling this
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Moving from Briefing to next step for condition: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
	}


	// If the condition requires the interaction tutorial AND it hasn't just finished
	if (CurrentCondition == EConditionType::WithInteraction && CurrentPhase != EStudyPhase::InteractionTutorial)
	{
		UE_LOG(LogStudyController, Log, TEXT("Condition requires Interaction Tutorial. Starting it now."));
		StartInteractionTutorial(); // Tutorial completion (AdvanceTutorialStep) will now call StartPreparationPhase again
	}
	else // For Control, NoInteraction, or after completing Interaction Tutorial
	{
		UE_LOG(LogStudyController, Log, TEXT("Starting Preparation Phase UI for: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
		CurrentPhase = EStudyPhase::Preparation;
		SetupVisualizationForCondition(CurrentCondition); // Setup particles etc.
		DisplayWidgetInWorld(PreparationWidgetClass); // Show the "Press Button to Start" screen
		// Button on PreparationWidgetClass calls StartVisualization()
	}
}


// StartVisualization is called from Preparation Phase UI
void AStudyGameController::StartVisualization()
{
	// Should now ONLY be called from Preparation (button press)
	if (CurrentPhase != EStudyPhase::Preparation) {
		UE_LOG(LogStudyController, Warning, TEXT("StartVisualization called from unexpected phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Starting Visualization from Preparation Phase."));

	CurrentPhase = EStudyPhase::Running;
	FString ConditionNameStr = StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString();
	UE_LOG(LogStudyController, Log, TEXT("Starting Visualization RUNNING Phase for condition: %s. Found %d targets."), *ConditionNameStr, VisualizationTargets.Num());

	ClearWorldUI();

	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass())) {
			IVisualizationControllable::Execute_StartActiveVisualization(TargetObj, CurrentCondition);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("StartVisualization: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}

	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	GetWorldTimerManager().SetTimer(VisualizationTimerHandle, this, &AStudyGameController::EndVisualizationPhase, VisualizationDuration, false);
}


// StartVisualizationControlOnly remains the same (for Interaction Tutorial)
void AStudyGameController::StartVisualizationControlOnly()
{
	UE_LOG(LogStudyController, Log, TEXT("Starting Visualization Control Only (for Tutorial)."));
	ClearWorldUI();

	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets)
	{
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass()))
		{
			IVisualizationControllable::Execute_StartVisualization_ControlOnly(TargetObj);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("StartVisualizationControlOnly: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}
}


// EndVisualizationPhase remains the same
void AStudyGameController::EndVisualizationPhase()
{
	if (CurrentPhase != EStudyPhase::Running) { UE_LOG(LogStudyController, Warning, TEXT("EndVisualizationPhase called from incorrect phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString()); return; }
	UE_LOG(LogStudyController, Log, TEXT("Visualization Duration Elapsed. Starting Fade Out (%f seconds)."), FadeOutDuration);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	CurrentPhase = EStudyPhase::FadingOut;
	CurrentFadeOutTimeRemaining = FadeOutDuration;
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	if (FadeOutDuration > 0.0f) {
		GetWorldTimerManager().SetTimer(FadeOutTimerHandle, this, &AStudyGameController::CompleteFadeOutAndTransition, FadeOutDuration, false);
	}
	else {
		CompleteFadeOutAndTransition();
	}
}

// CompleteFadeOutAndTransition remains the same
void AStudyGameController::CompleteFadeOutAndTransition()
{
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= 0.0f)) { UE_LOG(LogStudyController, Warning, TEXT("CompleteFadeOutAndTransition called from incorrect phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString()); return; }
	UE_LOG(LogStudyController, Log, TEXT("Fade Out Complete. Stopping Visualization and Starting SAM Evaluation."));
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	CurrentFadeOutTimeRemaining = 0.0f;
	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass())) {
			IVisualizationControllable::Execute_StopActiveVisualization(TargetObj);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("CompleteFadeOutAndTransition: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	bLeftGrabVibrationActive = bRightGrabVibrationActive = bLeftTriggerVibrationActive = bRightTriggerVibrationActive = false;
	StartSAMEvaluation(); // Start SAM for the actual study condition
}


// StartSAMEvaluation is for the *actual study* evaluation
void AStudyGameController::StartSAMEvaluation()
{
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= 0.0f)) { UE_LOG(LogStudyController, Warning, TEXT("StartSAMEvaluation called from incorrect phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString()); }

	CurrentPhase = EStudyPhase::SamEvaluation; // Set phase for actual study SAM
	UE_LOG(LogStudyController, Log, TEXT("Starting ACTUAL STUDY SAM Evaluation"));

	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	CurrentFadeOutTimeRemaining = 0.0f;

	DisplayWidgetInWorld(SAMWidgetClass);
	// If reusing SAMWidgetClass, ensure it knows it's *not* in tutorial mode now.
	// Example:
	// UUserWidget* CurrentWidget = CurrentDisplayWidgetComponent ? CurrentDisplayWidgetComponent->GetUserWidgetObject() : nullptr;
	// if (CurrentWidget && CurrentWidget->Implements<UTutorialModeWidgetInterface>()) {
	//      ITutorialModeWidgetInterface::Execute_SetTutorialMode(CurrentWidget, false);
	// }
	// The Submit button on this widget should call SubmitEvaluationAndAdvance.
}

// StartPresenceEvaluation is for the *actual study* evaluation
void AStudyGameController::StartPresenceEvaluation()
{
	if (CurrentPhase != EStudyPhase::SamEvaluation) { UE_LOG(LogStudyController, Warning, TEXT("StartPresenceEvaluation called from incorrect phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString()); }
	CurrentPhase = EStudyPhase::PresenceEvaluation; // Set phase for actual study Presence
	UE_LOG(LogStudyController, Log, TEXT("Starting ACTUAL STUDY Presence Evaluation"));
	DisplayWidgetInWorld(PresenceWidgetClass);
	// If reusing PresenceWidgetClass, ensure it knows it's *not* in tutorial mode now.
	// The Submit button on this widget should call SubmitEvaluationAndAdvance.
}

// SubmitEvaluationAndAdvance is for the *actual study* evaluation
// *** MAKE SURE ValenceScore etc. are declared in .h ***
void AStudyGameController::SubmitEvaluationAndAdvance(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence)
{
	if (CurrentPhase != EStudyPhase::PresenceEvaluation) { UE_LOG(LogStudyController, Warning, TEXT("SubmitEvaluationAndAdvance called from incorrect phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString()); return; } // Added return

	UE_LOG(LogStudyController, Log, TEXT("Submitting ACTUAL STUDY Evaluation: V=%d, A=%d, D=%d, P=%d"), Valence, Arousal, Dominance, Presence);

	// Store scores in member variables
	ValenceScore = Valence;
	ArousalScore = Arousal;
	DominanceScore = Dominance;
	PresenceScore = Presence;

	if (StudyManager)
	{
		// Record data using the stored member variables
		StudyManager->RecordSAMData(ValenceScore, ArousalScore, DominanceScore, PresenceScore); // Record to CSV
		CheckForMoreConditions(); // Advance study
	}
	else { UE_LOG(LogStudyController, Error, TEXT("SubmitEvaluationAndAdvance failed: StudyManager is null.")); }
}

// CheckForMoreConditions remains the same
void AStudyGameController::CheckForMoreConditions()
{
	if (StudyManager && StudyManager->HasMoreConditions()) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: More conditions remaining. Starting next one (briefing)."));
		BeginCondition(); // This will start the briefing for the next condition
	}
	else if (StudyManager) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: All conditions complete. Ending study."));
		EndStudy();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("CheckForMoreConditions failed: StudyManager is null."));
		CurrentPhase = EStudyPhase::Complete;
		DisplayWidgetInWorld(CompletionWidgetClass);
	}
}


// HasMoreConditions remains the same
bool AStudyGameController::HasMoreConditions() const
{
	return StudyManager ? StudyManager->HasMoreConditions() : false;
}

// EndStudy remains the same
void AStudyGameController::EndStudy()
{
	UE_LOG(LogStudyController, Log, TEXT("Ending Study. Saving results."));
	CurrentPhase = EStudyPhase::Complete;
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);
	GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
	CurrentFadeOutTimeRemaining = 0.0f;
	SetTutorialStep(ETutorialStep::None);
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None; // Reset general tutorial state too
	if (StudyManager) {
		StudyManager->SaveResultsToCSV();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("EndStudy failed: StudyManager is null. Cannot save results."));
	}
	DisplayWidgetInWorld(CompletionWidgetClass);
}

// ShowMainMenu remains the same
void AStudyGameController::ShowMainMenu()
{
	UE_LOG(LogStudyController, Log, TEXT("Showing Main Menu"));
	CurrentPhase = EStudyPhase::MainMenu;
	SetTutorialStep(ETutorialStep::None);
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None; // Reset general tutorial state
	ClearWorldUI();
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		UObject* TargetObj = Target.GetObject();
		if (IsValid(TargetObj) && TargetObj->GetClass()->ImplementsInterface(UVisualizationControllable::StaticClass())) {
			IVisualizationControllable::Execute_StopActiveVisualization(TargetObj);
		}
	}
	DisplayWidgetInWorld(MainMenuWidgetClass);
	// Flow Note: User action in Main Menu should call InitializeStudy()
}



// GetConditionName updated for Briefing phase
FString AStudyGameController::GetConditionName()
{
	EConditionType CondToGetName = EConditionType::Control; // Default fallback
	if (CurrentPhase == EStudyPhase::Preparation || CurrentPhase == EStudyPhase::Running || CurrentPhase == EStudyPhase::FadingOut || CurrentPhase == EStudyPhase::SamEvaluation || CurrentPhase == EStudyPhase::PresenceEvaluation || CurrentPhase == EStudyPhase::Briefing) {
		CondToGetName = CurrentCondition;
	}
	else if (StudyManager && StudyManager->HasMoreConditions() && CurrentPhase != EStudyPhase::InteractionTutorial && CurrentPhase != EStudyPhase::GeneralTutorial) {
		// This case might be less relevant now with Briefing handling the transition
		CondToGetName = StudyManager->GetNextCondition();
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		// While IN the tutorial, the underlying condition is 'WithInteraction'
		CondToGetName = EConditionType::WithInteraction;
		// return TEXT("Interaction Tutorial"); // Option: Return "Interaction Tutorial" explicitly
	}
	else if (CurrentPhase == EStudyPhase::GeneralTutorial) {
		return TEXT("General Tutorial");
	}
	const UEnum* EnumPtr = StaticEnum<EConditionType>();
	return EnumPtr ? EnumPtr->GetDisplayNameTextByValue((int64)CondToGetName).ToString() : TEXT("Unknown");
}



// SetupVisualizationForCondition remains the same
void AStudyGameController::SetupVisualizationForCondition(EConditionType Condition)
{
	UE_LOG(LogStudyController, Log, TEXT("Setting up visualization for condition: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)Condition).ToString());
	// Potentially tell visualization targets about the upcoming condition NOW if needed for setup
	// for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
	//     if (Target.GetObject()) {
	//         IVisualizationControllable::Execute_PrepareForCondition(Target.GetObject(), Condition); // Hypothetical interface function
	//     }
	// }
}



// GetStudyStatus updated for Briefing Phase
FString AStudyGameController::GetStudyStatus()
{
	FString StatusText;
	if (!IsValid(StudyManager)) return TEXT("Study Manager not found");
	StatusText += FString::Printf(TEXT("Participant ID: %d\n"), StudyManager->ParticipantID);
	StatusText += FString::Printf(TEXT("Environment: %s\n"), *StaticEnum<EBetweenCondition>()->GetDisplayNameTextByValue((int64)StudyManager->BetweenCondition).ToString());
	StatusText += FString::Printf(TEXT("Group: %s\n"), StudyManager->GroupType == EGroupType::A ? TEXT("A") : TEXT("B"));

	int32 CurrentIdx = StudyManager->CurrentConditionIndex;
	int32 TotalConditions = StudyManager->GetTotalNumberOfConditions();
	FString ConditionNameStr = TEXT("N/A");
	FString ConditionProgressStr = TEXT("");

	if (CurrentPhase == EStudyPhase::Complete && (!StudyManager->HasMoreConditions() || TotalConditions == 0)) {
		ConditionNameStr = TEXT("Finished");
		ConditionProgressStr = FString::Printf(TEXT("(%d / %d)"), TotalConditions, TotalConditions);
	}
	else if (CurrentPhase == EStudyPhase::GeneralTutorial) { // Check General Tutorial Phase first
		ConditionNameStr = TEXT("General Tutorial");
		// Display the current step within the general tutorial
		FString StepName = StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString();
		ConditionProgressStr = FString::Printf(TEXT("(Step: %s)"), *StepName);
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial) { // Check Interaction Tutorial Phase
		ConditionNameStr = TEXT("Interaction Tutorial");
		FString StepName = StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString();
		ConditionProgressStr = FString::Printf(TEXT("(Step: %s, %d/%d holds)"), *StepName, CurrentTutorialSuccessCount, TutorialRequiredSuccessCount);
	}
	else if (CurrentPhase >= EStudyPhase::Briefing && CurrentPhase < EStudyPhase::Complete) { // Check Briefing through Presence Evaluation
		const UEnum* ConditionEnum = StaticEnum<EConditionType>();
		ConditionNameStr = ConditionEnum ? ConditionEnum->GetDisplayNameTextByValue((int64)CurrentCondition).ToString() : TEXT("Unknown");
		// Show progress based on the *current* condition index
		ConditionProgressStr = FString::Printf(TEXT("(%d / %d)"), CurrentIdx + 1, TotalConditions);
	}
	else if (CurrentPhase == EStudyPhase::MainMenu || CurrentIdx == -1) {
		ConditionNameStr = TEXT("Not Started");
		ConditionProgressStr = FString::Printf(TEXT("(0 / %d)"), TotalConditions);
	}
	else {
		ConditionNameStr = TEXT("State Error");
		ConditionProgressStr = FString::Printf(TEXT("(Index: %d / %d)"), CurrentIdx, TotalConditions);
	}

	StatusText += FString::Printf(TEXT("Condition: %s %s\n"), *ConditionNameStr, *ConditionProgressStr);

	const UEnum* PhaseEnum = StaticEnum<EStudyPhase>();
	FString PhaseStr = PhaseEnum ? PhaseEnum->GetDisplayNameTextByValue((int64)CurrentPhase).ToString() : FString::Printf(TEXT("Unknown Phase (%d)"), (int32)CurrentPhase);
	StatusText += FString::Printf(TEXT("Phase: %s"), *PhaseStr);

	if (CurrentPhase == EStudyPhase::FadingOut) {
		StatusText += FString::Printf(TEXT(" (%.1fs left)"), CurrentFadeOutTimeRemaining);
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial && bIsHoldingRequiredInput) {
		StatusText += FString::Printf(TEXT(" (Holding: %.1fs / %.1fs)"), CurrentTutorialHoldTime, TutorialRequiredHoldTime);
	}
	StatusText += TEXT("\n");

	return StatusText;
}

// --- UI Helper ---

void AStudyGameController::ClearWorldUI()
{
	UE_LOG(LogStudyController, Verbose, TEXT("Clearing World UI"));
	if (CurrentDisplayWidgetComponent)
	{
		CurrentDisplayWidgetComponent->SetVisibility(false);
		CurrentDisplayWidgetComponent->SetWidget(nullptr); // More robust than SetWidgetClass(nullptr)
	}
}

void AStudyGameController::DisplayWidgetInWorld(TSubclassOf<UUserWidget> WidgetClassToShow)
{
	if (!CurrentDisplayWidgetComponent)
	{
		UE_LOG(LogStudyController, Error, TEXT("DisplayWidgetInWorld - FAILED: CurrentDisplayWidgetComponent is null."));
		return;
	}

	// Always clear/hide previous widget *before* checking the new class
	// This ensures UI is hidden if the new class is null or invalid.
	ClearWorldUI();

	if (!WidgetClassToShow)
	{
		UE_LOG(LogStudyController, Warning, TEXT("DisplayWidgetInWorld - WidgetClassToShow is null. UI remains hidden."));
		return; // UI is already hidden by ClearWorldUI()
	}

	UE_LOG(LogStudyController, Log, TEXT("DisplayWidgetInWorld START (Class: %s)"), *WidgetClassToShow->GetName());

	CurrentDisplayWidgetComponent->SetWidgetClass(WidgetClassToShow);
	UUserWidget* CreatedWidget = CurrentDisplayWidgetComponent->GetUserWidgetObject();

	if (CreatedWidget)
	{
		CreatedWidget->SynchronizeProperties();
		CreatedWidget->ForceLayoutPrepass();
		FVector2D DesiredSize = CreatedWidget->GetDesiredSize();

		if (DesiredSize.X > 1.0f && DesiredSize.Y > 1.0f)
		{
			// Use the dynamically calculated size if it seems valid
			CurrentDisplayWidgetComponent->SetDrawSize(DesiredSize);
			UE_LOG(LogStudyController, Log, TEXT("Using Dynamic Draw Size: %s"), *DesiredSize.ToString());
		}
		else
		{
			// --- EXPLICIT FALLBACK ---
			// Get the default draw size set on the component in the editor/defaults
			FVector2D CurrentComponentDrawSize = CurrentDisplayWidgetComponent->GetDrawSize();
			FVector2D FallbackDrawSize = FVector2D(1000.0f, 800.0f); // Define a generous fallback

			// Decide whether to use the component's existing size or the hardcoded fallback
			// If the component's current size seems invalidly small, force the fallback.
			if (CurrentComponentDrawSize.X < 100.f || CurrentComponentDrawSize.Y < 100.f)
			{
				UE_LOG(LogStudyController, Warning, TEXT("GetDesiredSize() invalid (%s) AND Component's current DrawSize (%s) is small. Using hardcoded fallback: %s"),
					*DesiredSize.ToString(), *CurrentComponentDrawSize.ToString(), *FallbackDrawSize.ToString());
				CurrentDisplayWidgetComponent->SetDrawSize(FallbackDrawSize);
			}
			else
			{
				// If the component's size seems okay, maybe keep it? Or still use fallback?
				// Using the hardcoded fallback might be more consistent. Let's stick with that for now.
				UE_LOG(LogStudyController, Warning, TEXT("GetDesiredSize() invalid (%s). Component's current DrawSize is %s. Applying hardcoded fallback: %s"),
					*DesiredSize.ToString(), *CurrentComponentDrawSize.ToString(), *FallbackDrawSize.ToString());
				CurrentDisplayWidgetComponent->SetDrawSize(FallbackDrawSize);
				// Alternatively, to keep component's existing size if it's reasonable:
				// CurrentDisplayWidgetComponent->SetDrawSize(CurrentComponentDrawSize);
			}
		}

		// --- Apply the FIXED World Transform ---
		UE_LOG(LogStudyController, Log, TEXT("Applying World UI Transform: Loc: %s, Rot: %s, Scale: %s"), *WorldUITransform.GetLocation().ToString(), *WorldUITransform.GetRotation().Rotator().ToString(), *WorldUITransform.GetScale3D().ToString());
		CurrentDisplayWidgetComponent->SetWorldTransform(WorldUITransform);

		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CurrentDisplayWidgetComponent->SetGenerateOverlapEvents(false);

		CurrentDisplayWidgetComponent->SetVisibility(true);
		UE_LOG(LogStudyController, Log, TEXT("--- DisplayWidgetInWorld END --- Widget (%s) should be visible."), *CreatedWidget->GetName());

		// Optional: Update progress immediately after display
		// ... (your interface call here) ...
		if (CurrentPhase == EStudyPhase::InteractionTutorial) UpdateTutorialUI(); // Ensure tutorial UI updates correctly
	}
	else
	{
		UE_LOG(LogStudyController, Error, TEXT("DisplayWidgetInWorld: FAILED to get widget instance for %s AFTER SetWidgetClass. UI will remain hidden."), *WidgetClassToShow->GetName());
	}
}


// *** ADDED: Get widget class for Briefing phase ***
TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForBriefing(EConditionType Condition)
{
	switch (Condition)
	{
	case EConditionType::Control: return BriefingControlWidgetClass;
	case EConditionType::NoInteraction: return BriefingNoInteractionWidgetClass;
	case EConditionType::WithInteraction: return BriefingWithInteractionWidgetClass;
	default:
		UE_LOG(LogStudyController, Error, TEXT("GetWidgetClassForBriefing: Unknown condition type %d"), (int32)Condition);
		return nullptr; // Return null or a default error widget
	}
}

// --- Interaction Tutorial Logic ---

void AStudyGameController::StartInteractionTutorial()
{
	// Called from StartPreparationPhase when CurrentCondition == WithInteraction
	if (CurrentPhase != EStudyPhase::Briefing) // It should be called right after Briefing phase set UI
	{
		UE_LOG(LogStudyController, Warning, TEXT("StartInteractionTutorial called from unexpected phase: %s. Expected Briefing."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		// Optional: Force phase? For now, just proceed.
	}

	UE_LOG(LogStudyController, Log, TEXT("Starting Interaction Tutorial (during Briefing phase)"));
	CurrentPhase = EStudyPhase::InteractionTutorial; // Now switch to Interaction Tutorial Phase
	ClearWorldUI(); // Briefing widget is cleared
	StopHaptic(EControllerHand::Left); // Ensure previous vibrations stopped
	StopHaptic(EControllerHand::Right);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle); // Stop study timers
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);

	// Start the particle visualization without music/interaction logic
	StartVisualizationControlOnly();

	// Begin with the intro step
	SetTutorialStep(ETutorialStep::Intro); // This will call DisplayWidgetInWorld for TutorialIntroWidgetClass
}

void AStudyGameController::AdvanceTutorialStep()
{
	UE_LOG(LogStudyController, Log, TEXT("Advancing Interaction Tutorial Step from %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());

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
		// *** CHANGED: Call StartPreparationPhase to show the "Ready to Start" UI ***
		StartPreparationPhase(); // This will now show PreparationWidgetClass because tutorial is complete
		return; // Exit function
	case ETutorialStep::None: default:
		UE_LOG(LogStudyController, Warning, TEXT("Cannot advance from None or invalid tutorial step.")); return;
	}

	if (NextStep != ETutorialStep::None) { SetTutorialStep(NextStep); }
}


void AStudyGameController::SetTutorialStep(ETutorialStep NewStep)
{
	CurrentTutorialStep = NewStep;
	CurrentTutorialHoldTime = 0.0f;
	CurrentTutorialSuccessCount = 0;
	bIsHoldingRequiredInput = false;
	GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle); // Ensure timer is cleared
	StopHaptic(EControllerHand::Left); // Stop vibration when step changes
	StopHaptic(EControllerHand::Right);

	UE_LOG(LogStudyController, Log, TEXT("Set Interaction Tutorial Step to: %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());

	UpdateTutorialUI(); // Update/Display the widget for the new step
}

// --- In StudyGameController.cpp ---

// Function Definition remains the same
void AStudyGameController::UpdateTutorialUI()
{
	TSubclassOf<UUserWidget> ExpectedWidgetClass = GetWidgetClassForTutorialStep(CurrentTutorialStep);

	// If the step is None or doesn't have a widget, clear the UI.
	if (!ExpectedWidgetClass) {
		UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: No expected widget class for step %s. Clearing UI."), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
		ClearWorldUI();
		return;
	}

	UUserWidget* CurrentWidget = nullptr;
	if (CurrentDisplayWidgetComponent) {
		CurrentWidget = CurrentDisplayWidgetComponent->GetUserWidgetObject();
	}

	bool bWidgetNeedsDisplay = true; // Assume we need to display a new widget by default

	// Check if the correct widget is *already* displayed and valid
	if (CurrentWidget && CurrentWidget->GetClass() == ExpectedWidgetClass && CurrentDisplayWidgetComponent->IsVisible())
	{
		UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: Correct widget (%s) already visible. Attempting to update properties."), *ExpectedWidgetClass->GetName());
		bWidgetNeedsDisplay = false; // Correct widget is already showing, just needs update

		// --- UPDATE EXISTING WIDGET ---
		// Check if the widget implements our interface
		if (CurrentWidget->GetClass()->ImplementsInterface(UTutorialWidgetInterface::StaticClass()))
		{
			// Calculate hold progress safely
			float HoldProgress = (TutorialRequiredHoldTime > KINDA_SMALL_NUMBER) ? FMath::Clamp(CurrentTutorialHoldTime / TutorialRequiredHoldTime, 0.0f, 1.0f) : 0.0f;

			// Call the interface function (which triggers the Blueprint event)
			ITutorialWidgetInterface::Execute_UpdateProgressDisplay(CurrentWidget, CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
			UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: Called UpdateProgressDisplay on existing widget (Success: %d/%d, Hold: %.2f)"), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
		}
		else
		{
			// Log a warning if the widget is supposed to be a tutorial widget but doesn't have the interface
			UE_LOG(LogStudyController, Warning, TEXT("UpdateTutorialUI: Widget %s is visible but does NOT implement UTutorialWidgetInterface! Cannot update progress."), *CurrentWidget->GetName());
		}
		// --- End Update Existing ---
	}

	// If we determined that we need to display a new/different widget
	if (bWidgetNeedsDisplay)
	{
		// Log the reason for displaying
		if (!CurrentWidget) {
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: No current widget found. Displaying %s."), *ExpectedWidgetClass->GetName());
		}
		else if (CurrentWidget->GetClass() != ExpectedWidgetClass) {
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: Widget class mismatch (Current: %s, Expected: %s). Displaying %s."), *CurrentWidget->GetClass()->GetName(), *ExpectedWidgetClass->GetName(), *ExpectedWidgetClass->GetName());
		}
		else if (!CurrentDisplayWidgetComponent->IsVisible()) {
			UE_LOG(LogStudyController, Log, TEXT("UpdateTutorialUI: Widget component was hidden. Displaying %s."), *ExpectedWidgetClass->GetName());
		}

		// Display the correct widget class
		DisplayWidgetInWorld(ExpectedWidgetClass); // DisplayWidgetInWorld handles clearing old widget

		// --- OPTIONAL BUT RECOMMENDED: Update the NEWLY created widget immediately ---
		UUserWidget* NewWidget = CurrentDisplayWidgetComponent ? CurrentDisplayWidgetComponent->GetUserWidgetObject() : nullptr;
		if (NewWidget && NewWidget->GetClass()->ImplementsInterface(UTutorialWidgetInterface::StaticClass()))
		{
			float HoldProgress = (TutorialRequiredHoldTime > KINDA_SMALL_NUMBER) ? FMath::Clamp(CurrentTutorialHoldTime / TutorialRequiredHoldTime, 0.0f, 1.0f) : 0.0f;
			ITutorialWidgetInterface::Execute_UpdateProgressDisplay(NewWidget, CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
			UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: Called UpdateProgressDisplay on NEWLY displayed widget (Success: %d/%d, Hold: %.2f)"), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
		}
		else if (NewWidget)
		{
			UE_LOG(LogStudyController, Warning, TEXT("UpdateTutorialUI: Newly displayed widget %s does NOT implement UTutorialWidgetInterface!"), *NewWidget->GetName());
		}
		// --- End Update Newly Created ---
	}
}

// --- End of changes in StudyGameController.cpp ---
TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForGeneralTutorialStep(EGeneralTutorialStep Step)
{
	switch (Step)
	{
	case EGeneralTutorialStep::Intro:               return GeneralTutorialIntroWidgetClass;
	case EGeneralTutorialStep::SAMExplanation:      return GeneralTutorialSAMExplanationWidgetClass;
	case EGeneralTutorialStep::SAMEvaluation:       return SAMWidgetClass; // Reusing study widget (needs adaptation)
	case EGeneralTutorialStep::PresenceExplanation: return GeneralTutorialPresenceExplanationWidgetClass;
	case EGeneralTutorialStep::PresenceEvaluation:  return PresenceWidgetClass; // Reusing study widget (needs adaptation)
	case EGeneralTutorialStep::Ending:              return GeneralTutorialEndingWidgetClass;
	case EGeneralTutorialStep::None:
	default:                                        return nullptr;
	}
}



TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForTutorialStep(ETutorialStep Step)
{
	switch (Step)
	{
	case ETutorialStep::Intro: return TutorialIntroWidgetClass;
	case ETutorialStep::LeftSpawn: return TutorialLeftSpawnWidgetClass;
	case ETutorialStep::RightSpawn: return TutorialRightSpawnWidgetClass;
	case ETutorialStep::RightConduct: return TutorialRightConductWidgetClass;
	case ETutorialStep::LeftPull: return TutorialLeftPullWidgetClass;
	case ETutorialStep::Complete: return TutorialCompleteWidgetClass;
	case ETutorialStep::None:
	default: return nullptr; // Or maybe Main Menu?
	}
}

void AStudyGameController::UpdateTutorialLogic(float DeltaTime)
{
	if (CurrentTutorialStep == ETutorialStep::Intro || CurrentTutorialStep == ETutorialStep::Complete || CurrentTutorialStep == ETutorialStep::None)
	{
		// No active input tracking needed for these steps
		bIsHoldingRequiredInput = false;
		GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
		return;
	}

	// If the correct input is being held, increment the timer
	if (bIsHoldingRequiredInput)
	{
		CurrentTutorialHoldTime += DeltaTime;

		// Start or continue hold vibration only on the correct hand
		EControllerHand HandToVibrate = EControllerHand::Left; // Default guess
		if (CurrentTutorialStep == ETutorialStep::RightSpawn || CurrentTutorialStep == ETutorialStep::RightConduct) HandToVibrate = EControllerHand::Right;

		// Check if timer needs to be started
		if (!GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle))
		{
			float RemainingTime = FMath::Max(0.0f, TutorialRequiredHoldTime - CurrentTutorialHoldTime);
			if (RemainingTime > 0)
			{
				GetWorldTimerManager().SetTimer(TutorialHoldTimerHandle, this, &AStudyGameController::OnTutorialHoldTimeComplete, RemainingTime, false);
				PlayHaptic(HandToVibrate, TutorialHoldHaptic, 1.0f, true); // Start looping hold vibration
			}
			else // Should have already triggered if time was >= required, but handle edge case
			{
				OnTutorialHoldTimeComplete();
			}
		}
		// Update progress bar on the displayed widget (via UpdateTutorialUI)
		UpdateTutorialUI();
	}
	else
	{
		// Input released or wrong input held
		bool bWasHolding = CurrentTutorialHoldTime > 0.0f; // Check if we *were* holding
		CurrentTutorialHoldTime = 0.0f; // Reset timer if released
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
		}
		StopHaptic(EControllerHand::Left); // Stop vibration if input released
		StopHaptic(EControllerHand::Right);

		// Update progress bar to 0 if we were previously holding
		if (bWasHolding) {
			UpdateTutorialUI();
		}
	}
}

void AStudyGameController::OnTutorialHoldTimeComplete()
{
	UE_LOG(LogStudyController, Log, TEXT("Tutorial Hold Time Complete for step %s"),
		*StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());

	// Get the correct hand *before* potentially changing step
	EControllerHand HandToVibrate =
		(CurrentTutorialStep == ETutorialStep::RightSpawn ||
			CurrentTutorialStep == ETutorialStep::RightConduct)
		? EControllerHand::Right
		: EControllerHand::Left;

	// If the player let go early, just reset and bail
	if (!bIsHoldingRequiredInput)
	{
		UE_LOG(LogStudyController, Warning, TEXT("Hold timer completed, but input was released!"));
		CurrentTutorialHoldTime = 0.0f;
		StopHaptic(HandToVibrate); // Stop haptic on the relevant hand
		UpdateTutorialUI(); // Update UI to show 0 progress
		return;
	}

	// Record one successful hold
	CurrentTutorialSuccessCount++;
	CurrentTutorialHoldTime = 0.0f;

	// Play “success” haptic & sound
	StopHaptic(HandToVibrate); // Stop hold haptic first
	PlayHaptic(HandToVibrate, TutorialSuccessHaptic, 1.0f, false);
	if (TutorialSuccessSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), TutorialSuccessSound);
	}

	// Reset for the next hold/check (timer is already cleared)
	bIsHoldingRequiredInput = false; // Important: Reset this flag so CheckTutorialInput works correctly on next press

	// Update the on‐screen progress
	UpdateTutorialUI(); // Show the updated success count and reset progress bar

	// Check if we've completed enough holds for this step
	if (CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount)
	{
		// Delay slightly before advancing to let the success feedback play out? Optional.
		// FTimerHandle TempHandle;
		// GetWorldTimerManager().SetTimer(TempHandle, this, &AStudyGameController::AdvanceTutorialStep, 0.5f, false);
		// OR advance immediately:
		AdvanceTutorialStep();
	}
}



void AStudyGameController::CheckTutorialInput(ETutorialStep StepToCheck, bool bLeftGrab, bool bRightGrab, bool bLeftTrigger, bool bRightTrigger)
{
	// Only process if we are in a step that requires input and haven't completed it
	if (StepToCheck == ETutorialStep::Intro || StepToCheck == ETutorialStep::Complete || StepToCheck == ETutorialStep::None || CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount)
	{
		bIsHoldingRequiredInput = false; // Ensure flag is false
		return;
	}

	bool bCorrectInputPressed = false;
	EControllerHand HandToCheck = EControllerHand::Left; // Assume left by default

	switch (StepToCheck)
	{
	case ETutorialStep::LeftSpawn:
		bCorrectInputPressed = bLeftGrab;
		HandToCheck = EControllerHand::Left;
		break;
	case ETutorialStep::RightSpawn:
		bCorrectInputPressed = bRightGrab;
		HandToCheck = EControllerHand::Right;
		break;
	case ETutorialStep::RightConduct:
		bCorrectInputPressed = bRightTrigger;
		HandToCheck = EControllerHand::Right;
		break;
	case ETutorialStep::LeftPull:
		bCorrectInputPressed = bLeftTrigger;
		HandToCheck = EControllerHand::Left;
		break;
	default: break; // Should not happen based on initial check
	}

	// --- Logic for PRESSING the correct input ---
	if (bCorrectInputPressed && !bIsHoldingRequiredInput) // Only trigger on the rising edge (first press)
	{
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input PRESSED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = true;
		CurrentTutorialHoldTime = 0.0f; // Reset hold time on new press
		// UpdateTutorialLogic in Tick will start the timer and haptics
		UpdateTutorialUI(); // Update UI immediately if needed (e.g., show target pressed)
	}
	// --- Logic for RELEASING the correct input ---
	else if (!bCorrectInputPressed && bIsHoldingRequiredInput) // Only trigger on the falling edge (release)
	{
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input RELEASED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = false;
		// UpdateTutorialLogic in Tick will clear timer and haptics
		// Explicitly stop haptic here just in case Tick doesn't run immediately
		StopHaptic(HandToCheck);
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
		}
		CurrentTutorialHoldTime = 0.0f; // Reset progress visually on release
		UpdateTutorialUI(); // Update UI to show 0 hold progress
	}

	// If the wrong input is pressed during a step, we currently ignore it.
	// You could add feedback here if desired (e.g., play a failure sound).
}

// --- Input Handling (Modify Condition Check) ---

// *** ADDED Helper function ***
bool AStudyGameController::ShouldVibrateForCondition(EConditionType ConditionToCheck) const
{
	// Only allow vibration when actually in the "WithInteraction" condition RUNNING phase:
	return (CurrentPhase == EStudyPhase::Running && ConditionToCheck == EConditionType::WithInteraction);
}



void AStudyGameController::HandleLeftGrab(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleLeftGrab: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	// Tutorial check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) { CheckTutorialInput(CurrentTutorialStep, bPressed, false, false, false); }

	// Study Interaction Logic
	if (ShouldVibrateForCondition(CurrentCondition))
	{
		UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::HandleLeftGrab - Phase/Condition Check PASSED (Pressed: %s)"), bPressed ? TEXT("True") : TEXT("False"));
		if (bPressed) {
			if (!bLeftGrabVibrationActive) {
				PlayHaptic(EControllerHand::Left, SpawnHaptic, SpawnHapticScale, true);
				bLeftGrabVibrationActive = true;
				UE_LOG(LogStudyController, Verbose, TEXT("Started Left Spawn Vibration"));
			}
		}
		else {
			if (bLeftGrabVibrationActive) {
				StopHaptic(EControllerHand::Left);
				bLeftGrabVibrationActive = false;
				UE_LOG(LogStudyController, Verbose, TEXT("Stopped Left Spawn Vibration"));
			}
		}
	}
	else {
		// Ensure vibration is stopped if condition/phase is wrong
		if (bLeftGrabVibrationActive) {
			UE_LOG(LogStudyController, Warning, TEXT("HandleLeftGrab: Stopping vibration because ShouldVibrateForCondition is false (Phase: %s, Condition: %s)"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
			StopHaptic(EControllerHand::Left);
			bLeftGrabVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleRightGrab(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleRightGrab: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	// Tutorial check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) { CheckTutorialInput(CurrentTutorialStep, false, bPressed, false, false); }

	// Study Interaction Logic
	if (ShouldVibrateForCondition(CurrentCondition))
	{
		UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::HandleRightGrab - Phase/Condition Check PASSED (Pressed: %s)"), bPressed ? TEXT("True") : TEXT("False"));
		if (bPressed) {
			if (!bRightGrabVibrationActive) {
				PlayHaptic(EControllerHand::Right, SpawnHaptic, SpawnHapticScale, true);
				bRightGrabVibrationActive = true;
				UE_LOG(LogStudyController, Verbose, TEXT("Started Right Spawn Vibration"));
			}
		}
		else {
			if (bRightGrabVibrationActive) {
				StopHaptic(EControllerHand::Right);
				bRightGrabVibrationActive = false;
				UE_LOG(LogStudyController, Verbose, TEXT("Stopped Right Spawn Vibration"));
			}
		}
	}
	else {
		// Ensure vibration is stopped if condition/phase is wrong
		if (bRightGrabVibrationActive) {
			UE_LOG(LogStudyController, Warning, TEXT("HandleRightGrab: Stopping vibration because ShouldVibrateForCondition is false (Phase: %s, Condition: %s)"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
			StopHaptic(EControllerHand::Right);
			bRightGrabVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleLeftTrigger(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleLeftTrigger: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	// Tutorial check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) { CheckTutorialInput(CurrentTutorialStep, false, false, bPressed, false); }

	// Study Interaction Logic
	if (ShouldVibrateForCondition(CurrentCondition))
	{
		UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::HandleLeftTrigger - Phase/Condition Check PASSED (Pressed: %s)"), bPressed ? TEXT("True") : TEXT("False"));
		if (bPressed) {
			if (!bLeftTriggerVibrationActive) {
				PlayHaptic(EControllerHand::Left, PullHaptic, PullHapticScale, true);
				bLeftTriggerVibrationActive = true;
				UE_LOG(LogStudyController, Verbose, TEXT("Started Left Pull Vibration"));
			}
		}
		else {
			if (bLeftTriggerVibrationActive) {
				StopHaptic(EControllerHand::Left);
				bLeftTriggerVibrationActive = false;
				UE_LOG(LogStudyController, Verbose, TEXT("Stopped Left Pull Vibration"));
			}
		}
	}
	else {
		// Ensure vibration is stopped if condition/phase is wrong
		if (bLeftTriggerVibrationActive) {
			UE_LOG(LogStudyController, Warning, TEXT("HandleLeftTrigger: Stopping vibration because ShouldVibrateForCondition is false (Phase: %s, Condition: %s)"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
			StopHaptic(EControllerHand::Left);
			bLeftTriggerVibrationActive = false;
		}
	}
}

void AStudyGameController::HandleRightTrigger(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleRightTrigger: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	// Tutorial check
	if (CurrentPhase == EStudyPhase::InteractionTutorial) { CheckTutorialInput(CurrentTutorialStep, false, false, false, bPressed); }

	// Study Interaction Logic
	if (ShouldVibrateForCondition(CurrentCondition))
	{
		UE_LOG(LogStudyController, Log, TEXT("AStudyGameController::HandleRightTrigger - Phase/Condition Check PASSED (Pressed: %s)"), bPressed ? TEXT("True") : TEXT("False"));
		if (bPressed) {
			if (!bRightTriggerVibrationActive) {
				PlayHaptic(EControllerHand::Right, ConductHaptic, 0.1f, true); // Start low
				bRightTriggerVibrationActive = true;
				LastKnownRightHandSpeed = 0.0f; // Reset speed on press
				UE_LOG(LogStudyController, Verbose, TEXT("Started Right Conduct Vibration"));
			}
		}
		else {
			if (bRightTriggerVibrationActive) {
				StopHaptic(EControllerHand::Right);
				bRightTriggerVibrationActive = false;
				LastKnownRightHandSpeed = 0.0f;
				UE_LOG(LogStudyController, Verbose, TEXT("Stopped Right Conduct Vibration"));
			}
		}
	}
	else {
		// Ensure vibration is stopped if condition/phase is wrong
		if (bRightTriggerVibrationActive) {
			UE_LOG(LogStudyController, Warning, TEXT("HandleRightTrigger: Stopping vibration because ShouldVibrateForCondition is false (Phase: %s, Condition: %s)"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
			StopHaptic(EControllerHand::Right);
			bRightTriggerVibrationActive = false;
		}
	}
}

void AStudyGameController::UpdateRightHandSpeed(float Speed)
{
	// Store the speed - used in Tick to update vibration intensity
	LastKnownRightHandSpeed = Speed;
	// The actual vibration update happens in Tick to avoid potential performance issues
	// if this is called very rapidly. Called ONLY if bRightTriggerVibrationActive is true.
}


// --- Vibration Helpers ---

APlayerController* AStudyGameController::GetPlayerControllerRef() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

void AStudyGameController::PlayHaptic(EControllerHand Hand, UHapticFeedbackEffect_Base* HapticEffect, float Scale, bool bLoop)
{
	if (!HapticEffect)
	{
		UE_LOG(LogStudyController, Warning, TEXT("PlayHaptic: No HapticEffect provided for hand %s."), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"));
		return;
	}

	APlayerController* PC = GetPlayerControllerRef();
	if (PC)
	{
		// Add check to prevent playing during inappropriate phases (e.g., menus, evaluation) unless it's tutorial haptics
		bool bIsTutorialHaptic = (HapticEffect == TutorialHoldHaptic || HapticEffect == TutorialSuccessHaptic);
		bool bCanPlayStudyHaptic = ShouldVibrateForCondition(CurrentCondition); // Checks phase is Running & condition is WithInteraction

		if ((CurrentPhase == EStudyPhase::InteractionTutorial && bIsTutorialHaptic) || bCanPlayStudyHaptic)
		{
			UE_LOG(LogStudyController, Log, TEXT("Playing Haptic %s on %s hand. Scale: %.2f, Loop: %s"), *HapticEffect->GetName(), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"), Scale, bLoop ? TEXT("True") : TEXT("False"));
			PC->PlayHapticEffect(HapticEffect, Hand, Scale, bLoop);
		}
		else
		{
			UE_LOG(LogStudyController, Verbose, TEXT("PlayHaptic SKIPPED for %s on %s hand. Phase: %s, Condition: %s, IsTutorialHaptic: %s"),
				*HapticEffect->GetName(),
				Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"),
				*StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(),
				*StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString(),
				bIsTutorialHaptic ? TEXT("True") : TEXT("False"));
		}
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("PlayHaptic: PlayerController is null.")); }
}

void AStudyGameController::StopHaptic(EControllerHand Hand)
{
	APlayerController* PC = GetPlayerControllerRef();
	if (PC)
	{
		UE_LOG(LogStudyController, Verbose, TEXT("Stopping Haptic on %s hand."), Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right"));
		PC->StopHapticEffect(Hand);

		// Also update internal flags to prevent immediate restart attempts
		if (Hand == EControllerHand::Left)
		{
			bLeftGrabVibrationActive = false;
			bLeftTriggerVibrationActive = false;
		}
		else // Right Hand
		{
			bRightGrabVibrationActive = false;
			bRightTriggerVibrationActive = false;
			LastKnownRightHandSpeed = 0.0f; // Reset speed tracking if stopping right hand
		}
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("StopHaptic: PlayerController is null.")); }
}

void AStudyGameController::UpdateConductVibration(float Speed)
{
	// This is called from Tick ONLY if bRightTriggerVibrationActive is true AND ShouldVibrateForCondition is true
	if (!ConductHaptic) return;

	APlayerController* PC = GetPlayerControllerRef();
	if (PC)
	{
		// Calculate intensity scale based on speed
		float BaseIntensity = 0.1f; // Minimum intensity when trigger is held
		float SpeedBasedIntensity = Speed * ConductSpeedIntensityFactor;
		float TotalIntensityScale = FMath::Clamp(BaseIntensity + SpeedBasedIntensity, 0.0f, MaxConductIntensityScale); // Clamp to max

		// UE_LOG(LogStudyController, VeryVerbose, TEXT("Updating Conduct Haptic. Speed: %.2f -> Scale: %.2f"), Speed, TotalIntensityScale); // Use VeryVerbose for frequent logs

		// Re-trigger the PlayHapticEffect with the new scale.
		// Important: Calling PlayHapticEffect repeatedly while looping is generally how intensity/scale updates work.
		PC->PlayHapticEffect(ConductHaptic, EControllerHand::Right, TotalIntensityScale, true);
	}
}


// --- END OF FILE StudyGameController.cpp ---