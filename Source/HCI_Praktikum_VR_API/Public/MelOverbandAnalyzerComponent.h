// MelOverbandAnalyzerComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioAnalysisToolsLibrary.h"
#include "MelOverbandAnalyzerComponent.generated.h"

/**
 *  Consumes an existing UAudioAnalysisToolsLibrary FFT,
 *  groups into Mel‑spaced over‑bands, applies envelope/peak tracking,
 *  log‑warp, adaptive thresholding—and finally smoothes via
 *  exponential smoothing before output.
 */
UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API UMelOverbandAnalyzerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMelOverbandAnalyzerComponent();

    /** Bind this component to your Blueprint’s AudioAnalysisToolsLibrary instance. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Analyzer")
    void SetAnalyzer(
        UAudioAnalysisToolsLibrary* InAnalyzer,
        int32 InFrameSize,
        float InSampleRate,
        int32 InOverBandCount,
        float InDecayEnv,
        float InDecayPeak,
        float InLogScaleG,
        float InThreshAlpha);

    /** After AATools->ProcessAudioFrames(...), call each tick to fill OutVis. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Analyzer")
    void Process(TArray<float>& OutVis);

    /** Enable per‑frame CSV dumping to Saved/ folder. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugToCSV = false;

    /** Name of the CSV file under Saved/. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FString DebugCSVFileName = TEXT("MelAnalyzerDebug.csv");

protected:
    // Your plugin instance
    UPROPERTY()
    UAudioAnalysisToolsLibrary* AATools = nullptr;

    // Derived from FrameSize/SampleRate
    int32 SubBandCount = 0;
    float SampleRate = 0.f;

    // Configured in SetAnalyzer()
    int32 OverBandCount = 0;
    float DecayEnv = 0.85f;       // release
    float DecayPeak = 0.90f;
    float LogScaleG = 1000.f;
    float ThreshAlpha = 0.99f;

    // Attack coefficient (0–1, larger → slower attack)
    float AttackCoef = 0.8f;

    // Exponential smoothing α for final OutVis (0–1, larger → smoother)
    float VisSmoothAlpha = 0.9f;

    // State arrays
    TArray<int32>   BandEdges;
    TArray<float>   EnvBuf, PeakBuf, ThrBuf;
    TArray<float>   LastVis;

    // Debug CSV
    FString DebugCSVBuffer;
    int32   DebugFrameCounter = 0;
};
