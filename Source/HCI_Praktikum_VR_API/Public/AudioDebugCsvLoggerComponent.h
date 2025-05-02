// AudioDebugCsvLoggerComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioDebugCsvLoggerComponent.generated.h"

/**
 * Logs arbitrary named audio features per frame into a uniquely-named CSV file under Saved/.
 */
UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent))
class HCI_PRAKTIKUM_VR_API_API UAudioDebugCsvLoggerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAudioDebugCsvLoggerComponent();

    /**
     * Begin logging to the given filename (relative to Saved/).
     * If a file with that name already exists, appends a datetime suffix to create a unique file.
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Debug")
    void StartLogging(const FString& FileName);

    /**
     * Log one frame’s worth of dynamic features. Names and Values must be same length.
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Debug")
    void LogFrameDynamic(const TArray<FString>& FeatureNames, const TArray<float>& FeatureValues);

    /**
     * Stop logging and flush any buffered lines.
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Debug")
    void StopLogging();

protected:
    virtual void BeginPlay() override;

private:
    /** Full path (including timestamp suffix) for the CSV under Saved/. */
    FString OutputPath;

    /** True once StartLogging has created a new unique file. */
    bool bIsLogging = false;

    /** Tracks whether we've written the header row. */
    bool bHeaderWritten = false;

    /** Current frame index. */
    int32 FrameCounter = 0;

    /** Internal buffer of pending CSV lines. */
    FString CsvBuffer;

    /** Flush buffered lines to the CSV file (append mode). */
    void FlushBuffer();

    /** Generate a unique file path under Saved/ based on FileName. */
    FString GetUniqueSavePath(const FString& FileName) const;
};
