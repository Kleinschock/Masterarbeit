//=============================================================================
// AudioSmoothingBPLibrary.cpp
//=============================================================================

#include "AudioSmoothingBPLibrary.h"
#include "Math/UnrealMathUtility.h"

void UAudioSmoothingBPLibrary::SMA_Smooth(
    const TArray<float>& InBuffer,
    float NewSample,
    int32 WindowSize,
    TArray<float>& OutBuffer,
    float& OutMean)
{
    OutBuffer = InBuffer;
    OutBuffer.Add(NewSample);
    if (OutBuffer.Num() > WindowSize)
    {
        OutBuffer.RemoveAt(0);
    }
    float Sum = 0.f;
    for (float Val : OutBuffer) Sum += Val;
    OutMean = (OutBuffer.Num() > 0) ? (Sum / OutBuffer.Num()) : 0.f;
}

void UAudioSmoothingBPLibrary::EMA_Smooth(
    float NewSample,
    float Alpha,
    float& InOutPrevSmoothed,
    float& OutSmoothed)
{
    OutSmoothed = Alpha * NewSample + (1.f - Alpha) * InOutPrevSmoothed;
    InOutPrevSmoothed = OutSmoothed;
}

void UAudioSmoothingBPLibrary::Median3_Smooth(
    const TArray<float>& InBuffer,
    float NewSample,
    TArray<float>& OutBuffer,
    float& OutMedian)
{
    OutBuffer.Empty();
    int32 Count = InBuffer.Num();
    if (Count >= 2)
    {
        OutBuffer.Add(InBuffer[Count - 2]);
        OutBuffer.Add(InBuffer[Count - 1]);
    }
    else
    {
        for (int i = 0; i < FMath::Max(0, 2 - Count); ++i)
            OutBuffer.Add(0.f);
        for (float V : InBuffer)
            OutBuffer.Add(V);
    }
    OutBuffer.Add(NewSample);
    OutBuffer.Sort();
    OutMedian = OutBuffer[1];
}

void UAudioSmoothingBPLibrary::PulseEnhance_Smooth(
    float NewSample,
    float BaselineAlpha,
    float MinRatio,
    float MaxRatio,
    float Gamma,
    float& InOutPrevBaseline,
    float& OutEnhanced)
{
    // 1) EMA baseline
    InOutPrevBaseline = BaselineAlpha * NewSample + (1.f - BaselineAlpha) * InOutPrevBaseline;
    float Baseline = FMath::Max(InOutPrevBaseline, KINDA_SMALL_NUMBER);

    // 2) ratio computation
    float Ratio = NewSample / Baseline;

    // 3) clamp and threshold
    Ratio = FMath::Clamp(Ratio, MinRatio, MaxRatio);
    Ratio = (Ratio > MinRatio) ? (Ratio - MinRatio) / (MaxRatio - MinRatio) : 0.f;

    // 4) power-law warp
    OutEnhanced = FMath::Pow(Ratio, Gamma);
}
