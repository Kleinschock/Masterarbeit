// MelOverbandAnalyzerComponent.cpp

#include "MelOverbandAnalyzerComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Math/UnrealMathUtility.h"

// Mel ↔ Hz conversions (O’Shaughnessy)
static inline float HzToMel(float f)
{
    return 2595.0f * FMath::LogX(10.0f, 1.0f + f / 700.0f);
}
static inline float MelToHz(float m)
{
    return 700.0f * (FMath::Pow(10.0f, m / 2595.0f) - 1.0f);
}

UMelOverbandAnalyzerComponent::UMelOverbandAnalyzerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMelOverbandAnalyzerComponent::SetAnalyzer(
    UAudioAnalysisToolsLibrary* InAnalyzer,
    int32 InFrameSize,
    float InSampleRate,
    int32 InOverBandCount,
    float InDecayEnvVal,
    float InDecayPeakVal,
    float InLogScaleGVal,
    float InThreshAlphaVal)
{
    check(InAnalyzer);
    AATools = InAnalyzer;
    SampleRate = InSampleRate;
    SubBandCount = InFrameSize / 2;  // adjust if FFT returns N/2+1
    OverBandCount = InOverBandCount;

    DecayEnv = InDecayEnvVal;
    DecayPeak = InDecayPeakVal;
    LogScaleG = InLogScaleGVal;
    ThreshAlpha = InThreshAlphaVal;

    // Allocate & reset state
    EnvBuf.Init(0.f, OverBandCount);
    PeakBuf.Init(0.f, OverBandCount);
    ThrBuf.Init(0.f, OverBandCount);
    LastVis.Init(0.f, OverBandCount);
    BandEdges.SetNumUninitialized(OverBandCount + 1);

    // Compute Mel‑spaced band edges
    const float mel0 = HzToMel(0.f);
    const float melN = HzToMel(SampleRate * 0.5f);
    for (int32 b = 0; b <= OverBandCount; ++b)
    {
        float frac = float(b) / OverBandCount;
        float m = FMath::Lerp(mel0, melN, frac);
        float fHz = MelToHz(m);
        int32 idx = FMath::Clamp(
            int32((fHz / (SampleRate * 0.5f)) * SubBandCount),
            0, SubBandCount);
        BandEdges[b] = idx;
    }

    DebugFrameCounter = 0;
    DebugCSVBuffer.Empty();
}

void UMelOverbandAnalyzerComponent::Process(TArray<float>& OutVis)
{
    check(AATools);
    const TArray<float>& Mag = AATools->GetMagnitudeSpectrum();

    OutVis.SetNumUninitialized(OverBandCount);
    const float invLogDen = 1.f / FMath::Loge(1.f + LogScaleG);

    // CSV header on first frame
    if (bDebugToCSV && DebugFrameCounter == 0)
    {
        DebugCSVBuffer = TEXT("Frame,Band,RawAvg,Env,Peak,Norm,Warped,Thr,AdjRaw,Smoothed\n");
    }

    for (int32 b = 0; b < OverBandCount; ++b)
    {
        // 1) raw average
        int32 s = BandEdges[b], e = BandEdges[b + 1];
        int32 cnt = FMath::Max(1, e - s);
        float sum = 0.f;
        for (int32 i = s; i < e; ++i) sum += Mag[i];
        float rawAvg = sum / cnt;

        // 2) envelope (attack/release)
        float prevE = EnvBuf[b];
        float riseE = prevE * AttackCoef + rawAvg * (1 - AttackCoef);
        float fallE = prevE * DecayEnv;
        float env = FMath::Max(riseE, fallE);
        EnvBuf[b] = env;

        // 3) peak tracker
        float peak = FMath::Max(env, PeakBuf[b] * DecayPeak);
        PeakBuf[b] = peak;

        // 4) normalize
        float norm = (peak > KINDA_SMALL_NUMBER) ? (env / peak) : 0.f;

        // 5) log‑warp
        float warped = FMath::Loge(1 + LogScaleG * norm) * invLogDen;

        // 6) adaptive threshold
        float thr = ThrBuf[b] = ThrBuf[b] * ThreshAlpha + warped * (1 - ThreshAlpha);

        // 7) raw adjusted
        float adjRaw = (warped <= thr) ? 0.f : (warped - thr) / (1 - thr);
        adjRaw = FMath::Clamp(adjRaw, 0.f, 1.f);

        // 8) exponential smoothing on final output
        float sm = LastVis[b] * VisSmoothAlpha + adjRaw * (1 - VisSmoothAlpha);
        LastVis[b] = sm;
        OutVis[b] = sm;

        // 9) append debug
        if (bDebugToCSV)
        {
            DebugCSVBuffer += FString::Printf(
                TEXT("%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
                DebugFrameCounter, b,
                rawAvg, env, peak, norm, warped, thr, adjRaw, sm
            );
        }
    }

    // flush CSV (append only newest lines)
    if (bDebugToCSV)
    {
        const FString Path = FPaths::ProjectSavedDir() / DebugCSVFileName;
        FFileHelper::SaveStringToFile(
            DebugCSVBuffer, *Path,
            FFileHelper::EEncodingOptions::AutoDetect,
            &IFileManager::Get(),
            EFileWrite::FILEWRITE_Append
        );
        DebugFrameCounter++;
        DebugCSVBuffer.Empty();
    }
}
