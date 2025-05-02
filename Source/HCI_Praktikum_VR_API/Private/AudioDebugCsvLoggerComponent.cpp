// AudioDebugCsvLoggerComponent.cpp

#include "AudioDebugCsvLoggerComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

UAudioDebugCsvLoggerComponent::UAudioDebugCsvLoggerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UAudioDebugCsvLoggerComponent::BeginPlay()
{
    Super::BeginPlay();
}

FString UAudioDebugCsvLoggerComponent::GetUniqueSavePath(const FString& FileName) const
{
    // Base saved directory
    FString BaseDir = FPaths::ProjectSavedDir();
    FString BaseName = FPaths::GetBaseFilename(FileName, /* bRemovePath = */ false);
    FString Ext = FPaths::GetExtension(FileName, /* bIncludeDot = */ true);

    FString Candidate = BaseDir / FileName;
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.FileExists(*Candidate))
    {
        return Candidate;
    }

    // Append timestamp
    FDateTime Now = FDateTime::UtcNow();
    FString Timestamp = Now.ToString(TEXT("yyyyMMdd_HHmmss"));
    FString NewName = FString::Printf(TEXT("%s_%s%s"), *BaseName, *Timestamp, *Ext);
    return BaseDir / NewName;
}

void UAudioDebugCsvLoggerComponent::StartLogging(const FString& FileName)
{
    if (bIsLogging)
    {
        // Already logging
        return;
    }
    // Create unique path
    OutputPath = GetUniqueSavePath(FileName);
    // Ensure no leftover file
    IFileManager::Get().Delete(*OutputPath);

    // Reset state
    bIsLogging = true;
    bHeaderWritten = false;
    FrameCounter = 0;
    CsvBuffer.Empty();
}

void UAudioDebugCsvLoggerComponent::LogFrameDynamic(
    const TArray<FString>& FeatureNames,
    const TArray<float>& FeatureValues)
{
    if (!bIsLogging || FeatureNames.Num() != FeatureValues.Num())
    {
        return;
    }

    // Write header once
    if (!bHeaderWritten)
    {
        CsvBuffer += TEXT("Frame");
        for (const FString& Name : FeatureNames)
        {
            CsvBuffer += TEXT(",") + Name;
        }
        CsvBuffer += LINE_TERMINATOR;
        bHeaderWritten = true;
    }

    // Build line: frame index + all values
    CsvBuffer += FString::FromInt(FrameCounter);
    for (float Val : FeatureValues)
    {
        CsvBuffer += FString::Printf(TEXT(",%.6f"), Val);
    }
    CsvBuffer += LINE_TERMINATOR;

    FrameCounter++;

    // Flush periodically
    if (FrameCounter % 64 == 0)
    {
        FlushBuffer();
    }
}

void UAudioDebugCsvLoggerComponent::StopLogging()
{
    if (!bIsLogging)
    {
        return;
    }
    FlushBuffer();
    CsvBuffer.Empty();
    bIsLogging = false;
}

void UAudioDebugCsvLoggerComponent::FlushBuffer()
{
    if (CsvBuffer.IsEmpty() || OutputPath.IsEmpty())
    {
        return;
    }
    FFileHelper::SaveStringToFile(
        CsvBuffer,
        *OutputPath,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(),
        EFileWrite::FILEWRITE_Append);
    CsvBuffer.Empty();
}
