#include "StudyGameController.h"
#include "StudyManager.h"
#include "InteractionLoggerComponent.h"
#include "TutorialWidgetInterface.h" 
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
#include "Components/AudioComponent.h" 
#include "IXRTrackingSystem.h" 
#include "Engine/Engine.h" 
#include "Kismet/KismetMathLibrary.h"

// *** ADDED FOR MOVEMENT LOGGER ***
#include "MovementLoggerComponent.h"
// *** END ADDED ***


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
		if (!GetRootComponent()) {
			USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
			SetRootComponent(SceneRoot);
		}
		CurrentDisplayWidgetComponent->SetupAttachment(GetRootComponent());
		CurrentDisplayWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
		CurrentDisplayWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetVisibility(false);
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CurrentDisplayWidgetComponent->SetGenerateOverlapEvents(false);
	}
	StudyManager = nullptr;
	GemsScores.Init(0, 9);
}

// BeginPlay
void AStudyGameController::BeginPlay()
{
	Super::BeginPlay();

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
	}

	FindVisualizationTargets();
	ShowMainMenu();

	// *** ADDED FOR MOVEMENT LOGGER ***
	// InitializeMovementLogger will be called from InitializeStudy after PID etc. are known
	// *** END ADDED ***
}

// EndPlay
void AStudyGameController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ClearWorldUI();

	// *** ADDED FOR MOVEMENT LOGGER ***
	// Ensure logs are saved if not already, in case of unexpected end
	if (MovementLoggerComponentRef.IsValid())
	{
		MovementLoggerComponentRef->FinalizeAndSaveLogs();
	}
	// *** END ADDED ***

	Super::EndPlay(EndPlayReason);
}

// BeginDestroy
void AStudyGameController::BeginDestroy()
{
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

	if (CurrentPhase == EStudyPhase::FadingOut && GetWorldTimerManager().IsTimerActive(FadeOutTimerHandle)) {
		CurrentFadeOutTimeRemaining = GetWorldTimerManager().GetTimerRemaining(FadeOutTimerHandle);
	}
	else if (CurrentPhase != EStudyPhase::FadingOut) {
		CurrentFadeOutTimeRemaining = 0.0f;
	}

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		UpdateTutorialLogic(DeltaTime);
	}

	if (bRightTriggerVibrationActive && ShouldVibrateForCondition(CurrentCondition)) {
		UpdateConductVibration(LastKnownRightHandSpeed);
	}
}

// *** ADDED FOR MOVEMENT LOGGER ***
void AStudyGameController::InitializeMovementLogger()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (IsValid(PlayerPawn))
	{
		MovementLoggerComponentRef = PlayerPawn->FindComponentByClass<UMovementLoggerComponent>();
		if (MovementLoggerComponentRef.IsValid())
		{
			if (StudyManager)
			{
				const UEnum* BetweenEnum = StaticEnum<EBetweenCondition>();
				FString BetweenStr = BetweenEnum ? BetweenEnum->GetDisplayNameTextByValue((int64)StudyManager->BetweenCondition).ToString() : FString::Printf(TEXT("UnknownBetween_%d"), (int32)StudyManager->BetweenCondition);

				FString GroupStr = TEXT("UnknownGroup");
				const UEnum* GroupEnum = StaticEnum<EGroupType>();
				if (GroupEnum)
				{
					GroupStr = GroupEnum->GetNameStringByValue((int64)StudyManager->GroupType);
					GroupStr.RemoveFromStart(TEXT("EGroupType::"));
				}
				else if (StudyManager->GroupType == EGroupType::A) GroupStr = TEXT("A");
				else if (StudyManager->GroupType == EGroupType::B) GroupStr = TEXT("B");


				MovementLoggerComponentRef->InitializeLoggingSession(StudyManager->ParticipantID, BetweenStr, GroupStr);
				UE_LOG(LogStudyController, Log, TEXT("Movement Logger Component initialized and session configured."));
			}
			else
			{
				UE_LOG(LogStudyController, Error, TEXT("InitializeMovementLogger: StudyManager is null. Cannot configure MovementLogger session."));
			}
		}
		else
		{
			UE_LOG(LogStudyController, Error, TEXT("InitializeMovementLogger: FAILED to find UMovementLoggerComponent on Player Pawn '%s'. Movement logging will NOT work."), *PlayerPawn->GetName());
		}
	}
	else
	{
		UE_LOG(LogStudyController, Error, TEXT("InitializeMovementLogger: FAILED to get Player Pawn. Cannot initialize Movement Logger."));
	}
}

void AStudyGameController::StartMovementLoggingForCurrentPhase()
{
	if (MovementLoggerComponentRef.IsValid() && StudyManager)
	{
		// Check if the current phase is one where we want to log movement
		if (CurrentPhase == EStudyPhase::Running || CurrentPhase == EStudyPhase::FadingOut)
		{
			FString PhaseName = StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString();
			FString CondName = StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString();
			int32 CondOrder = StudyManager->CurrentConditionIndex + 1; // 1-based for logging

			MovementLoggerComponentRef->StartLoggingPeriod(CondOrder, CondName, PhaseName);
		}
	}
}

void AStudyGameController::StopMovementLoggingForCurrentPhase()
{
	if (MovementLoggerComponentRef.IsValid())
	{
		// Stop logging regardless of phase, if it was active
		MovementLoggerComponentRef->StopLoggingPeriod();
	}
}
// *** END ADDED ***


// FindVisualizationTargets
void AStudyGameController::FindVisualizationTargets()
{
	VisualizationTargets.Empty();
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UVisualizationControllable::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		if (Actor)
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
		ShowMainMenu();
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Initializing Study: ID=%d, Env=%s, Group=%s"), ParticipantID, *UEnum::GetDisplayValueAsText(BetweenID).ToString(), *UEnum::GetDisplayValueAsText(GroupID).ToString());
	StudyManager->InitializeStudy(ParticipantID, BetweenID, GroupID);
	FindVisualizationTargets();

	UE_LOG(LogStudyController, Log, TEXT("Setting initial Passthrough mode based on BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(BetweenID).ToString());
	SetPassthroughMode(BetweenID == EBetweenCondition::AR);

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (IsValid(PlayerPawn)) {
		LoggerComponentRef = PlayerPawn->FindComponentByClass<UInteractionLoggerComponent>();
		if (LoggerComponentRef.IsValid()) {
			FString ParticipantStr = FString::FromInt(ParticipantID);
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

	// *** ADDED FOR MOVEMENT LOGGER ***
	InitializeMovementLogger(); // Call after StudyManager is initialized
	// *** END ADDED ***

	StartGeneralTutorial();
}

// --- General Tutorial Flow ---
void AStudyGameController::StartGeneralTutorial()
{
	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Ensure no movement logging during tutorial
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("Starting General Tutorial Sequence"));
	CurrentPhase = EStudyPhase::GeneralTutorial;
	ClearWorldUI();
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	SetTutorialStep(ETutorialStep::None);
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None;
	SetGeneralTutorialStep(EGeneralTutorialStep::Intro);
}

void AStudyGameController::SetGeneralTutorialStep(EGeneralTutorialStep NewStep)
{
	if (CurrentPhase != EStudyPhase::GeneralTutorial) {
		UE_LOG(LogStudyController, Warning, TEXT("SetGeneralTutorialStep called outside of GeneralTutorial Phase. Current Phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
	}
	CurrentGeneralTutorialStep = NewStep;
	UE_LOG(LogStudyController, Log, TEXT("Setting General Tutorial Step to: %s"), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());
	DisplayWidgetInWorld(GetWidgetClassForGeneralTutorialStep(NewStep));
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
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceExplanation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialSAM called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialPresenceExplanation()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceExplanation) {
		SetGeneralTutorialStep(EGeneralTutorialStep::PresenceEvaluation);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialPresenceExplanation called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::SubmitTutorialPresence(int32 Presence)
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::PresenceEvaluation) {
		UE_LOG(LogStudyController, Log, TEXT("Tutorial Presence Submitted: P=%d (Data NOT recorded)"), Presence);
		SetGeneralTutorialStep(EGeneralTutorialStep::Ending);
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("SubmitTutorialPresence called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}

void AStudyGameController::CompleteGeneralTutorialEnding()
{
	if (CurrentPhase == EStudyPhase::GeneralTutorial && CurrentGeneralTutorialStep == EGeneralTutorialStep::Ending) {
		UE_LOG(LogStudyController, Log, TEXT("General Tutorial Sequence Complete. Proceeding to first condition briefing."));
		CurrentGeneralTutorialStep = EGeneralTutorialStep::None;
		BeginCondition();
	}
	else { UE_LOG(LogStudyController, Warning, TEXT("CompleteGeneralTutorialEnding called at wrong time. Phase: %s, Step: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString(), *StaticEnum<EGeneralTutorialStep>()->GetDisplayNameTextByValue((int64)CurrentGeneralTutorialStep).ToString()); }
}
// --- End General Tutorial Flow ---


// BeginCondition
void AStudyGameController::BeginCondition()
{
	if (!StudyManager) {
		UE_LOG(LogStudyController, Error, TEXT("BeginCondition failed: StudyManager is null. Cannot proceed."));
		ShowMainMenu();
		return;
	}

	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Ensure logging stops from previous phase/condition
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("BeginCondition: Ensuring Passthrough mode for BetweenCondition: %s"), *UEnum::GetDisplayValueAsText(StudyManager->BetweenCondition).ToString());
	SetPassthroughMode(StudyManager->BetweenCondition == EBetweenCondition::AR);

	if (StudyManager->HasMoreConditions())
	{
		CurrentCondition = StudyManager->GetNextCondition();
		UE_LOG(LogStudyController, Log, TEXT("Beginning Briefing Phase for: %s (Condition Index: %d)"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString(), StudyManager->CurrentConditionIndex);

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

// StartPreparationPhase
void AStudyGameController::StartPreparationPhase()
{
	if (CurrentPhase != EStudyPhase::Briefing && CurrentPhase != EStudyPhase::InteractionTutorial) {
		UE_LOG(LogStudyController, Warning, TEXT("StartPreparationPhase called from unexpected phase: %s. Expected Briefing or InteractionTutorial."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Make sure logging is off before preparation
	// *** END ADDED ***

	bool bRequiresInteractionTutorial = (CurrentCondition == EConditionType::WithInteraction);

	if (bRequiresInteractionTutorial && CurrentPhase != EStudyPhase::InteractionTutorial)
	{
		UE_LOG(LogStudyController, Log, TEXT("Condition requires Interaction Tutorial. Starting it now."));
		StartInteractionTutorial();
	}
	else
	{
		UE_LOG(LogStudyController, Log, TEXT("Starting Preparation Phase UI for: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)CurrentCondition).ToString());
		CurrentPhase = EStudyPhase::Preparation;
		SetupVisualizationForCondition(CurrentCondition);
		DisplayWidgetInWorld(PreparationWidgetClass);
	}
}

// StartVisualization
void AStudyGameController::StartVisualization()
{
	if (CurrentPhase != EStudyPhase::Preparation) {
		UE_LOG(LogStudyController, Warning, TEXT("StartVisualization called from unexpected phase: %s. Expected Preparation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

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

	// *** ADDED FOR MOVEMENT LOGGER ***
	StartMovementLoggingForCurrentPhase();
	// *** END ADDED ***
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
			IVisualizationControllable::Execute_StartVisualization_ControlOnly(TargetObj);
		}
		else { UE_LOG(LogStudyController, Warning, TEXT("StartVisualizationControlOnly: Invalid target or target does not implement interface: %s"), TargetObj ? *TargetObj->GetName() : TEXT("NULL")); }
	}
	// *** ADDED FOR MOVEMENT LOGGER ***
	// Do NOT start movement logging here as it's tutorial specific and not part of main data
	StopMovementLoggingForCurrentPhase();
	// *** END ADDED ***
}

// EndVisualizationPhase
void AStudyGameController::EndVisualizationPhase()
{
	if (CurrentPhase != EStudyPhase::Running) {
		UE_LOG(LogStudyController, Warning, TEXT("EndVisualizationPhase called from incorrect phase: %s. Expected Running."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Visualization Duration Elapsed. Starting Fade Out (%f seconds)."), FadeOutDuration);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);

	CurrentPhase = EStudyPhase::FadingOut;

	// *** ADDED FOR MOVEMENT LOGGER ***
	// Logging should continue or restart for FadingOut phase
	StopMovementLoggingForCurrentPhase(); // Stop logging for "Running"
	StartMovementLoggingForCurrentPhase(); // Start logging for "FadingOut"
	// *** END ADDED ***

	CurrentFadeOutTimeRemaining = FadeOutDuration;
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);

	if (FadeOutDuration > KINDA_SMALL_NUMBER) {
		GetWorldTimerManager().SetTimer(FadeOutTimerHandle, this, &AStudyGameController::CompleteFadeOutAndTransition, FadeOutDuration, false);
	}
	else {
		CompleteFadeOutAndTransition();
	}
}

// CompleteFadeOutAndTransition
void AStudyGameController::CompleteFadeOutAndTransition()
{
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= KINDA_SMALL_NUMBER)) {
		UE_LOG(LogStudyController, Warning, TEXT("CompleteFadeOutAndTransition called from incorrect phase: %s. Expected FadingOut or Running (with 0 fade)."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Stop logging as FadingOut (or Running if 0 fade) ends
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("Fade Out Complete (or skipped). Stopping Visualization and Starting Questionnaire Sequence."));
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

	StartSAMEvaluation();
}

// --- Questionnaire Flow ---

// StartSAMEvaluation
void AStudyGameController::StartSAMEvaluation()
{
	if (CurrentPhase != EStudyPhase::FadingOut && !(CurrentPhase == EStudyPhase::Running && FadeOutDuration <= KINDA_SMALL_NUMBER)) {
		UE_LOG(LogStudyController, Warning, TEXT("StartSAMEvaluation called from unexpected phase: %s"), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
	}
	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Ensure no movement logging during questionnaires
	// *** END ADDED ***

	CurrentPhase = EStudyPhase::SamEvaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting SAM Evaluation Phase."));

	ValenceScore = ArousalScore = DominanceScore = PresenceScore = 0;
	GemsScores.Init(0, 9);

	DisplayWidgetInWorld(SAMWidgetClass);
}

// StartGemsPart1Evaluation 
void AStudyGameController::StartGemsPart1Evaluation()
{
	if (CurrentPhase != EStudyPhase::SamEvaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("StartGemsPart1Evaluation called from incorrect phase: %s. Expected SamEvaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}
	CurrentPhase = EStudyPhase::GemsPart1Evaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting GEMS Part 1 Evaluation Phase."));
	DisplayWidgetInWorld(GemsPart1WidgetClass);
}

// SubmitGemsPart1AndAdvance 
void AStudyGameController::SubmitGemsPart1AndAdvance(const TArray<int32>& Part1Scores)
{
	if (CurrentPhase != EStudyPhase::GemsPart1Evaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitGemsPart1AndAdvance called from incorrect phase: %s. Expected GemsPart1Evaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	if (Part1Scores.Num() != 5) {
		UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart1AndAdvance received incorrect number of scores: %d. Expected 5. Storing defaults."), Part1Scores.Num());
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Received GEMS Part 1 scores (Count: %d). Storing."), Part1Scores.Num());
		for (int32 i = 0; i < Part1Scores.Num(); ++i) {
			if (GemsScores.IsValidIndex(i)) {
				GemsScores[i] = Part1Scores[i];
			}
			else {
				UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart1AndAdvance: Invalid index %d while storing GEMS scores."), i);
			}
		}
	}
	StartGemsPart2Evaluation();
}

// StartGemsPart2Evaluation 
void AStudyGameController::StartGemsPart2Evaluation()
{
	CurrentPhase = EStudyPhase::GemsPart2Evaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting GEMS Part 2 Evaluation Phase."));
	DisplayWidgetInWorld(GemsPart2WidgetClass);
}

// SubmitGemsPart2AndAdvance 
void AStudyGameController::SubmitGemsPart2AndAdvance(const TArray<int32>& Part2Scores)
{
	if (CurrentPhase != EStudyPhase::GemsPart2Evaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitGemsPart2AndAdvance called from incorrect phase: %s. Expected GemsPart2Evaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	if (Part2Scores.Num() != 4) {
		UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart2AndAdvance received incorrect number of scores: %d. Expected 4. Storing defaults."), Part2Scores.Num());
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Received GEMS Part 2 scores (Count: %d). Storing."), Part2Scores.Num());
		for (int32 i = 0; i < Part2Scores.Num(); ++i) {
			int32 TargetIndex = i + 5;
			if (GemsScores.IsValidIndex(TargetIndex)) {
				GemsScores[TargetIndex] = Part2Scores[i];
			}
			else {
				UE_LOG(LogStudyController, Error, TEXT("SubmitGemsPart2AndAdvance: Invalid index %d while storing GEMS scores."), TargetIndex);
			}
		}
	}
	StartPresenceEvaluation();
}

// StartPresenceEvaluation 
void AStudyGameController::StartPresenceEvaluation()
{
	CurrentPhase = EStudyPhase::PresenceEvaluation;
	UE_LOG(LogStudyController, Log, TEXT("Starting Presence Evaluation Phase."));
	DisplayWidgetInWorld(PresenceWidgetClass);
}

// SubmitAllQuestionnairesAndAdvance 
void AStudyGameController::SubmitAllQuestionnairesAndAdvance(int32 Valence, int32 Arousal, int32 Dominance, int32 Presence)
{
	if (CurrentPhase != EStudyPhase::PresenceEvaluation) {
		UE_LOG(LogStudyController, Warning, TEXT("SubmitAllQuestionnairesAndAdvance called from incorrect phase: %s. Expected PresenceEvaluation."), *StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString());
		return;
	}

	UE_LOG(LogStudyController, Log, TEXT("Received final Presence score (%d). Submitting all collected questionnaire data for condition."), Presence);

	ValenceScore = Valence;
	ArousalScore = Arousal;
	DominanceScore = Dominance;
	PresenceScore = Presence;

	if (StudyManager)
	{
		if (GemsScores.Num() != 9) {
			UE_LOG(LogStudyController, Error, TEXT("SubmitAllQuestionnairesAndAdvance: GEMS scores array size is %d, expected 9! Recording potentially incorrect data."), GemsScores.Num());
		}
		StudyManager->RecordConditionResults(ValenceScore, ArousalScore, DominanceScore, PresenceScore, GemsScores);
		CheckForMoreConditions();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("SubmitAllQuestionnairesAndAdvance failed: StudyManager is null. Cannot record results."));
		EndStudy();
	}
}
// --- End Questionnaire Flow ---

// CheckForMoreConditions
void AStudyGameController::CheckForMoreConditions()
{
	if (StudyManager && StudyManager->HasMoreConditions()) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: More conditions remaining. Starting next briefing."));
		BeginCondition();
	}
	else if (StudyManager) {
		UE_LOG(LogStudyController, Log, TEXT("CheckForMoreConditions: All conditions complete. Ending study."));
		EndStudy();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("CheckForMoreConditions failed: StudyManager is null. Cannot determine next step. Ending study as fallback."));
		EndStudy();
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
	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Final stop
	if (MovementLoggerComponentRef.IsValid())
	{
		MovementLoggerComponentRef->FinalizeAndSaveLogs(); // Save all buffered movement data
	}
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("Ending Study. Saving results if possible."));
	CurrentPhase = EStudyPhase::Complete;

	GetWorldTimerManager().ClearAllTimersForObject(this);

	CurrentFadeOutTimeRemaining = 0.0f;
	SetTutorialStep(ETutorialStep::None);
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None;

	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);

	if (StudyManager) {
		StudyManager->SaveResultsToCSV();
	}
	else {
		UE_LOG(LogStudyController, Error, TEXT("EndStudy failed: StudyManager is null. Cannot save results."));
	}

	DisplayWidgetInWorld(CompletionWidgetClass);
}

// ShowMainMenu
void AStudyGameController::ShowMainMenu()
{
	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Ensure no logging in main menu
	if (MovementLoggerComponentRef.IsValid())
	{
		MovementLoggerComponentRef->ClearBufferedData(); // Clear buffer if returning to main menu (e.g. new study)
	}
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("Showing Main Menu"));
	CurrentPhase = EStudyPhase::MainMenu;
	SetTutorialStep(ETutorialStep::None);
	CurrentGeneralTutorialStep = EGeneralTutorialStep::None;
	ClearWorldUI();
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);

	for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
		if (Target.GetObject()) IVisualizationControllable::Execute_StopActiveVisualization(Target.GetObject());
	}

	DisplayWidgetInWorld(MainMenuWidgetClass);
}

// GetConditionName
FString AStudyGameController::GetConditionName()
{
	EConditionType CondToGetName = EConditionType::Control;

	if (CurrentPhase >= EStudyPhase::Briefing && CurrentPhase < EStudyPhase::Complete &&
		CurrentPhase != EStudyPhase::GeneralTutorial && CurrentPhase != EStudyPhase::InteractionTutorial)
	{
		if (StudyManager && StudyManager->CurrentConditionIndex >= 0 && StudyManager->CurrentConditionIndex < StudyManager->ConditionSequence.Num()) {
			CondToGetName = StudyManager->ConditionSequence[StudyManager->CurrentConditionIndex];
		}
		else {
			return TEXT("Condition Error");
		}
	}
	else if (CurrentPhase == EStudyPhase::InteractionTutorial) return TEXT("Interaction Tutorial");
	else if (CurrentPhase == EStudyPhase::GeneralTutorial) return TEXT("General Tutorial");
	else if (CurrentPhase == EStudyPhase::MainMenu) return TEXT("Main Menu");
	else if (CurrentPhase == EStudyPhase::Complete) return TEXT("Finished");
	else {
		return TEXT("N/A");
	}

	const UEnum* EnumPtr = StaticEnum<EConditionType>();
	return EnumPtr ? EnumPtr->GetDisplayNameTextByValue((int64)CondToGetName).ToString() : TEXT("Unknown Condition");
}

// SetupVisualizationForCondition
void AStudyGameController::SetupVisualizationForCondition(EConditionType Condition)
{
	UE_LOG(LogStudyController, Log, TEXT("Setting up visualization for condition: %s"), *StaticEnum<EConditionType>()->GetDisplayNameTextByValue((int64)Condition).ToString());
}

// GetStudyStatus
FString AStudyGameController::GetStudyStatus()
{
	if (!IsValid(StudyManager)) return TEXT("Error: Study Manager not found");

	FString StatusText = FString::Printf(TEXT("PID: %d | Env: %s | Group: %s\n"),
		StudyManager->ParticipantID,
		*StaticEnum<EBetweenCondition>()->GetDisplayNameTextByValue((int64)StudyManager->BetweenCondition).ToString(),
		StudyManager->GroupType == EGroupType::A ? TEXT("A") : TEXT("B"));

	int32 CurrentIdx = StudyManager->CurrentConditionIndex;
	int32 TotalConditions = StudyManager->GetTotalNumberOfConditions();
	if (TotalConditions <= 0 && StudyManager->ConditionSequence.Num() > 0) TotalConditions = StudyManager->ConditionSequence.Num();
	if (TotalConditions <= 0) TotalConditions = 3;

	FString ConditionNameStr = GetConditionName();
	FString ConditionProgressStr = TEXT("");

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
		ConditionProgressStr = TEXT("(? / ?) - Idx Error");
	}

	StatusText += FString::Printf(TEXT("Condition: %s %s\n"), *ConditionNameStr, *ConditionProgressStr);

	FString PhaseStr = StaticEnum<EStudyPhase>() ? StaticEnum<EStudyPhase>()->GetDisplayNameTextByValue((int64)CurrentPhase).ToString() : FString::Printf(TEXT("Unknown Phase (%d)"), (int32)CurrentPhase);
	StatusText += FString::Printf(TEXT("Phase: %s"), *PhaseStr);

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
		CurrentDisplayWidgetComponent->SetWidget(nullptr);
	}
}

void AStudyGameController::DisplayWidgetInWorld(TSubclassOf<UUserWidget> WidgetClassToShow)
{
	if (!CurrentDisplayWidgetComponent) {
		UE_LOG(LogStudyController, Error, TEXT("DisplayWidgetInWorld - FAILED: CurrentDisplayWidgetComponent is null."));
		return;
	}

	ClearWorldUI();

	if (!WidgetClassToShow) {
		UE_LOG(LogStudyController, Warning, TEXT("DisplayWidgetInWorld - WidgetClassToShow is null. UI remains hidden."));
		return;
	}

	CurrentDisplayWidgetComponent->SetWidgetClass(WidgetClassToShow);
	UUserWidget* CreatedWidget = CurrentDisplayWidgetComponent->GetUserWidgetObject();

	if (CreatedWidget)
	{
		CurrentDisplayWidgetComponent->SetWorldTransform(WorldUITransform);
		CurrentDisplayWidgetComponent->SetTwoSided(bWidgetIsTwoSided);
		CurrentDisplayWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CurrentDisplayWidgetComponent->SetGenerateOverlapEvents(false);

		FVector2D CurrentDrawSize = CurrentDisplayWidgetComponent->GetDrawSize();
		if (CurrentDrawSize.X < 10.f || CurrentDrawSize.Y < 10.f) {
			FVector2D FallbackDrawSize(1000.f, 800.f);
			UE_LOG(LogStudyController, Warning, TEXT("Widget Component DrawSize is very small (%s). Using fallback: %s"), *CurrentDrawSize.ToString(), *FallbackDrawSize.ToString());
			CurrentDisplayWidgetComponent->SetDrawSize(FallbackDrawSize);
		}
		CurrentDisplayWidgetComponent->SetVisibility(true);
		UE_LOG(LogStudyController, Log, TEXT("Displayed widget: %s"), *WidgetClassToShow->GetName());

		if (CurrentPhase == EStudyPhase::InteractionTutorial) {
			UpdateTutorialUI();
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
	case EConditionType::NoInteraction: return BriefingNoInteractionWidgetClass;
	case EConditionType::WithInteraction: return BriefingWithInteractionWidgetClass;
	default:
		UE_LOG(LogStudyController, Error, TEXT("GetWidgetClassForBriefing: Unknown condition type %d"), (int32)Condition);
		return nullptr;
	}
}

TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForGeneralTutorialStep(EGeneralTutorialStep Step)
{
	switch (Step) {
	case EGeneralTutorialStep::Intro:               return GeneralTutorialIntroWidgetClass;
	case EGeneralTutorialStep::SAMExplanation:      return GeneralTutorialSAMExplanationWidgetClass;
	case EGeneralTutorialStep::SAMEvaluation:       return SAMWidgetClass;
	case EGeneralTutorialStep::PresenceExplanation: return GeneralTutorialPresenceExplanationWidgetClass;
	case EGeneralTutorialStep::PresenceEvaluation:  return PresenceWidgetClass;
	case EGeneralTutorialStep::Ending:              return GeneralTutorialEndingWidgetClass;
	case EGeneralTutorialStep::None: default:       return nullptr;
	}
}

TSubclassOf<UUserWidget> AStudyGameController::GetWidgetClassForTutorialStep(ETutorialStep Step)
{
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
	}

	// *** ADDED FOR MOVEMENT LOGGER ***
	StopMovementLoggingForCurrentPhase(); // Ensure no movement logging during interaction tutorial
	// *** END ADDED ***

	UE_LOG(LogStudyController, Log, TEXT("Starting Interaction Tutorial"));
	CurrentPhase = EStudyPhase::InteractionTutorial;
	ClearWorldUI();
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);
	GetWorldTimerManager().ClearTimer(VisualizationTimerHandle);
	GetWorldTimerManager().ClearTimer(FadeOutTimerHandle);

	StartVisualizationControlOnly();
	SetTutorialStep(ETutorialStep::Intro);
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
		for (const TScriptInterface<IVisualizationControllable>& Target : VisualizationTargets) {
			if (Target.GetObject()) IVisualizationControllable::Execute_StopActiveVisualization(Target.GetObject());
		}
		SetTutorialStep(ETutorialStep::None);
		StartPreparationPhase();
		return;
	case ETutorialStep::None: default:
		UE_LOG(LogStudyController, Warning, TEXT("Cannot advance from None or invalid tutorial step (%d)."), (int32)CurrentTutorialStep);
		return;
	}

	if (NextStep != ETutorialStep::None) {
		SetTutorialStep(NextStep);
	}
}

void AStudyGameController::SetTutorialStep(ETutorialStep NewStep)
{
	CurrentTutorialStep = NewStep;
	CurrentTutorialHoldTime = 0.0f;
	CurrentTutorialSuccessCount = 0;
	bIsHoldingRequiredInput = false;
	GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
	StopHaptic(EControllerHand::Left);
	StopHaptic(EControllerHand::Right);

	UE_LOG(LogStudyController, Log, TEXT("Set Interaction Tutorial Step to: %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)NewStep).ToString());
	UpdateTutorialUI();
}

void AStudyGameController::UpdateTutorialUI()
{
	TSubclassOf<UUserWidget> ExpectedWidgetClass = GetWidgetClassForTutorialStep(CurrentTutorialStep);

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

	if (CurrentWidget && CurrentWidget->GetClass() == ExpectedWidgetClass && CurrentDisplayWidgetComponent->IsVisible()) {
		UE_LOG(LogStudyController, Verbose, TEXT("UpdateTutorialUI: Correct widget (%s) visible. Updating progress."), *ExpectedWidgetClass->GetName());
		bWidgetNeedsDisplayOrUpdate = true;
	}
	else {
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
		DisplayWidgetInWorld(ExpectedWidgetClass);
		CurrentWidget = CurrentDisplayWidgetComponent ? CurrentDisplayWidgetComponent->GetUserWidgetObject() : nullptr;
		bWidgetNeedsDisplayOrUpdate = (CurrentWidget != nullptr);
	}

	if (bWidgetNeedsDisplayOrUpdate && CurrentWidget) {
		if (CurrentWidget->GetClass()->ImplementsInterface(UTutorialWidgetInterface::StaticClass())) {
			float HoldProgress = (TutorialRequiredHoldTime > KINDA_SMALL_NUMBER)
				? FMath::Clamp(CurrentTutorialHoldTime / TutorialRequiredHoldTime, 0.0f, 1.0f)
				: (bIsHoldingRequiredInput ? 1.0f : 0.0f);
			ITutorialWidgetInterface::Execute_UpdateProgressDisplay(CurrentWidget, CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, HoldProgress);
		}
		else {
			UE_LOG(LogStudyController, Warning, TEXT("UpdateTutorialUI: Widget %s does NOT implement UTutorialWidgetInterface! Cannot update progress."), *CurrentWidget->GetName());
		}
	}
	else if (bWidgetNeedsDisplayOrUpdate && !CurrentWidget) {
		UE_LOG(LogStudyController, Error, TEXT("UpdateTutorialUI: Failed to get widget instance after display/check. Cannot update progress."));
	}
}


void AStudyGameController::UpdateTutorialLogic(float DeltaTime)
{
	if (CurrentTutorialStep == ETutorialStep::Intro || CurrentTutorialStep == ETutorialStep::Complete || CurrentTutorialStep == ETutorialStep::None) {
		bIsHoldingRequiredInput = false;
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
			StopHaptic(EControllerHand::Left);
			StopHaptic(EControllerHand::Right);
		}
		return;
	}

	if (bIsHoldingRequiredInput) {
		CurrentTutorialHoldTime += DeltaTime;

		EControllerHand HandToVibrate = EControllerHand::Left;
		if (CurrentTutorialStep == ETutorialStep::RightSpawn || CurrentTutorialStep == ETutorialStep::RightConduct) {
			HandToVibrate = EControllerHand::Right;
		}

		if (!GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			float RemainingTime = FMath::Max(0.0f, TutorialRequiredHoldTime - CurrentTutorialHoldTime);
			if (RemainingTime > KINDA_SMALL_NUMBER) {
				GetWorldTimerManager().SetTimer(TutorialHoldTimerHandle, this, &AStudyGameController::OnTutorialHoldTimeComplete, RemainingTime, false);
				PlayHaptic(HandToVibrate, TutorialHoldHaptic, 1.0f, true);
			}
			else if (CurrentTutorialHoldTime >= TutorialRequiredHoldTime) {
				OnTutorialHoldTimeComplete();
			}
		}
		UpdateTutorialUI();

	}
	else {
		bool bWasHolding = CurrentTutorialHoldTime > 0.0f;

		CurrentTutorialHoldTime = 0.0f;
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
			StopHaptic(EControllerHand::Left);
			StopHaptic(EControllerHand::Right);
		}
		if (bWasHolding) {
			UpdateTutorialUI();
		}
	}
}

void AStudyGameController::OnTutorialHoldTimeComplete()
{
	UE_LOG(LogStudyController, Log, TEXT("Tutorial Hold Time Complete for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());

	EControllerHand HandThatHeld = EControllerHand::Left;
	if (CurrentTutorialStep == ETutorialStep::RightSpawn || CurrentTutorialStep == ETutorialStep::RightConduct) {
		HandThatHeld = EControllerHand::Right;
	}

	if (!bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Warning, TEXT("Hold timer completed, but input was released early!"));
		CurrentTutorialHoldTime = 0.0f;
		StopHaptic(HandThatHeld);
		UpdateTutorialUI();
		return;
	}

	CurrentTutorialSuccessCount++;
	CurrentTutorialHoldTime = 0.0f;
	bIsHoldingRequiredInput = false;

	StopHaptic(HandThatHeld);
	PlayHaptic(HandThatHeld, TutorialSuccessHaptic, 1.0f, false);
	if (TutorialSuccessSound) {
		UGameplayStatics::PlaySound2D(GetWorld(), TutorialSuccessSound);
	}

	UpdateTutorialUI();

	if (CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount) {
		UE_LOG(LogStudyController, Log, TEXT("Required success count (%d) reached for step %s. Advancing."), TutorialRequiredSuccessCount, *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
		AdvanceTutorialStep();
	}
	else {
		UE_LOG(LogStudyController, Log, TEXT("Success %d/%d for step %s. Waiting for next input hold."), CurrentTutorialSuccessCount, TutorialRequiredSuccessCount, *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)CurrentTutorialStep).ToString());
	}
}

void AStudyGameController::CheckTutorialInput(ETutorialStep StepToCheck, bool bLeftGrab, bool bRightGrab, bool bLeftTrigger, bool bRightTrigger)
{
	if (StepToCheck == ETutorialStep::Intro || StepToCheck == ETutorialStep::Complete || StepToCheck == ETutorialStep::None || CurrentTutorialSuccessCount >= TutorialRequiredSuccessCount) {
		bIsHoldingRequiredInput = false;
		return;
	}

	bool bCorrectInputPressed = false;
	EControllerHand HandToCheck = EControllerHand::Left;

	switch (StepToCheck) {
	case ETutorialStep::LeftSpawn:    bCorrectInputPressed = bLeftGrab;    HandToCheck = EControllerHand::Left; break;
	case ETutorialStep::RightSpawn:   bCorrectInputPressed = bRightGrab;   HandToCheck = EControllerHand::Right; break;
	case ETutorialStep::RightConduct: bCorrectInputPressed = bRightTrigger; HandToCheck = EControllerHand::Right; break;
	case ETutorialStep::LeftPull:     bCorrectInputPressed = bLeftTrigger; HandToCheck = EControllerHand::Left; break;
	default:
		return;
	}

	if (bCorrectInputPressed && !bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input PRESSED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = true;
		CurrentTutorialHoldTime = 0.0f;
		UpdateTutorialUI();
	}
	else if (!bCorrectInputPressed && bIsHoldingRequiredInput) {
		UE_LOG(LogStudyController, Verbose, TEXT("CheckTutorialInput: Correct input RELEASED for step %s"), *StaticEnum<ETutorialStep>()->GetDisplayNameTextByValue((int64)StepToCheck).ToString());
		bIsHoldingRequiredInput = false;
		if (GetWorldTimerManager().IsTimerActive(TutorialHoldTimerHandle)) {
			GetWorldTimerManager().ClearTimer(TutorialHoldTimerHandle);
			StopHaptic(HandToCheck);
		}
		CurrentTutorialHoldTime = 0.0f;
		UpdateTutorialUI();
	}
}
// --- End Interaction Tutorial Logic ---


// --- Interaction Logging ---
void AStudyGameController::NotifyInteractionStart(FName ActionName, EControllerHand Hand)
{
	if (LoggerComponentRef.IsValid()) {
		LoggerComponentRef->LogInteractionEvent(ActionName, Hand, true);
	}
	else {
		UE_LOG(LogStudyController, Warning, TEXT("NotifyInteractionStart: LoggerComponentRef is invalid. Cannot log action: %s"), *ActionName.ToString());
	}
}

void AStudyGameController::NotifyInteractionEnd(FName ActionName, EControllerHand Hand)
{
	if (LoggerComponentRef.IsValid()) {
		LoggerComponentRef->LogInteractionEvent(ActionName, Hand, false);
	}
	else {
		UE_LOG(LogStudyController, Warning, TEXT("NotifyInteractionEnd: LoggerComponentRef is invalid. Cannot log action: %s"), *ActionName.ToString());
	}
}
// --- End Interaction Logging ---


// --- Input Handling ---
bool AStudyGameController::ShouldVibrateForCondition(EConditionType ConditionToCheck) const
{
	return (CurrentPhase == EStudyPhase::Running && ConditionToCheck == EConditionType::WithInteraction);
}

void AStudyGameController::HandleLeftGrab(bool bPressed)
{
	UE_LOG(LogStudyController, Verbose, TEXT("HandleLeftGrab: %s"), bPressed ? TEXT("Pressed") : TEXT("Released"));
	if (bPressed) {
		NotifyInteractionStart(FName("LeftGrab"), EControllerHand::Left);
	}
	else {
		NotifyInteractionEnd(FName("LeftGrab"), EControllerHand::Left);
	}

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, bPressed, false, false, false);
	}

	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bLeftGrabVibrationActive) {
				PlayHaptic(EControllerHand::Left, SpawnHaptic, SpawnHapticScale, true);
				bLeftGrabVibrationActive = true;
			}
		}
		else {
			if (bLeftGrabVibrationActive) {
				StopHaptic(EControllerHand::Left);
				bLeftGrabVibrationActive = false;
			}
		}
	}
	else {
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

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, bPressed, false, false);
	}

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
	}
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

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, false, bPressed, false);
	}

	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bLeftTriggerVibrationActive) {
				PlayHaptic(EControllerHand::Left, PullHaptic, PullHapticScale, true);
				bLeftTriggerVibrationActive = true;
			}
		}
		else {
			if (bLeftTriggerVibrationActive) {
				StopHaptic(EControllerHand::Left);
				bLeftTriggerVibrationActive = false;
			}
		}
	}
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

	if (CurrentPhase == EStudyPhase::InteractionTutorial) {
		CheckTutorialInput(CurrentTutorialStep, false, false, false, bPressed);
	}

	if (ShouldVibrateForCondition(CurrentCondition)) {
		if (bPressed) {
			if (!bRightTriggerVibrationActive) {
				PlayHaptic(EControllerHand::Right, ConductHaptic, 0.1f, true);
				bRightTriggerVibrationActive = true;
				LastKnownRightHandSpeed = 0.0f;
			}
		}
		else {
			if (bRightTriggerVibrationActive) {
				StopHaptic(EControllerHand::Right);
				bRightTriggerVibrationActive = false;
				LastKnownRightHandSpeed = 0.0f;
			}
		}
	}
	else {
		if (bRightTriggerVibrationActive) {
			StopHaptic(EControllerHand::Right);
			bRightTriggerVibrationActive = false;
			LastKnownRightHandSpeed = 0.0f;
		}
	}
}

void AStudyGameController::UpdateRightHandSpeed(float Speed)
{
	LastKnownRightHandSpeed = FMath::Abs(Speed);

	if (LoggerComponentRef.IsValid() && CurrentPhase == EStudyPhase::Running && CurrentCondition == EConditionType::WithInteraction) {
		// LoggerComponentRef->LogMovementData(EControllerHand::Right, Speed); 
	}
}
// --- End Input Handling ---


// --- Vibration Helpers ---
APlayerController* AStudyGameController::GetPlayerControllerRef() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

void AStudyGameController::PlayHaptic(EControllerHand Hand, UHapticFeedbackEffect_Base* HapticEffect, float Scale, bool bLoop)
{
	if (!HapticEffect) {
		return;
	}

	APlayerController* PC = GetPlayerControllerRef();
	if (!PC) {
		UE_LOG(LogStudyController, Warning, TEXT("PlayHaptic: PlayerController is null."));
		return;
	}

	bool bAllowPlay = false;
	bool bIsTutorialHaptic = (HapticEffect == TutorialHoldHaptic || HapticEffect == TutorialSuccessHaptic);

	if (CurrentPhase == EStudyPhase::InteractionTutorial && bIsTutorialHaptic) {
		bAllowPlay = true;
	}
	else if (ShouldVibrateForCondition(CurrentCondition) && !bIsTutorialHaptic) {
		bAllowPlay = true;
	}

	if (bAllowPlay) {
		PC->PlayHapticEffect(HapticEffect, Hand, Scale, bLoop);
	}
}

void AStudyGameController::StopHaptic(EControllerHand Hand)
{
	APlayerController* PC = GetPlayerControllerRef();
	if (PC) {
		PC->StopHapticEffect(Hand);
		if (Hand == EControllerHand::Left) {
			bLeftGrabVibrationActive = false;
			bLeftTriggerVibrationActive = false;
		}
		else {
			bRightGrabVibrationActive = false;
			bRightTriggerVibrationActive = false;
			LastKnownRightHandSpeed = 0.0f;
		}
	}
}

void AStudyGameController::UpdateConductVibration(float Speed)
{
	if (!ConductHaptic) return;

	APlayerController* PC = GetPlayerControllerRef();
	if (PC) {
		float BaseIntensity = 0.1f;
		float SpeedBasedIntensity = FMath::Abs(Speed) * ConductSpeedIntensityFactor;
		float TotalIntensityScale = FMath::Clamp(BaseIntensity + SpeedBasedIntensity, 0.0f, MaxConductIntensityScale);
		PC->PlayHapticEffect(ConductHaptic, EControllerHand::Right, TotalIntensityScale, true);
	}
}
// --- End Vibration Helpers ---