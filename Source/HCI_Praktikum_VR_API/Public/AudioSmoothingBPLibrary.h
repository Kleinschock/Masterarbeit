#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioSmoothingBPLibrary.generated.h"

/**
 * Blueprint‐callable smoothing utilities for audio analysis features.
 */
UCLASS()
class HCI_PRAKTIKUM_VR_API_API UAudioSmoothingBPLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Simple Moving Average over a sliding window. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Smoothing")
    static void SMA_Smooth(
        const TArray<float>& InBuffer,
        float NewSample,
        int32 WindowSize,
        TArray<float>& OutBuffer,
        float& OutMean);

    /** Exponential Moving Average (IIR low‑pass): yₙ = α·xₙ + (1–α)·yₙ₋₁ */
    UFUNCTION(BlueprintCallable, Category = "Audio|Smoothing")
    static void EMA_Smooth(
        float NewSample,
        float Alpha,
        float& InOutPrevSmoothed,
        float& OutSmoothed);

    /** 3‑point median filter: median of [ prev[-1], prev[0], NewSample ] */
    UFUNCTION(BlueprintCallable, Category = "Audio|Smoothing")
    static void Median3_Smooth(
        const TArray<float>& InBuffer,
        float NewSample,
        TArray<float>& OutBuffer,
        float& OutMedian);

    /**
     * Enhanced pulse mapping:
     * Computes a baseline EMA of the raw feature, then extracts a ratio-based spike value,
     * applies thresholding and a gamma power-law warp to emphasize peaks.
     * @param NewSample         The incoming raw feature value (e.g. ComplexDiff).
     * @param BaselineAlpha     EMA α for baseline calculation (0<α≤1).
     * @param MinRatio          Minimum ratio threshold to start output (>1).
     * @param MaxRatio          Maximum ratio to clamp before warping.
     * @param Gamma             Exponent to apply (γ>1 emphasizes peaks).
     * @param InOutPrevBaseline Previous baseline value (maintained across calls).
     * @param OutEnhanced       The resulting enhanced impulse value [0…1].
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Smoothing")
    static void PulseEnhance_Smooth(
        float NewSample,
        float BaselineAlpha,
        float MinRatio,
        float MaxRatio,
        float Gamma,
        float& InOutPrevBaseline,
        float& OutEnhanced);
};
