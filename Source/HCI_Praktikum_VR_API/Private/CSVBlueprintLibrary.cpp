#include "CSVBlueprintLibrary.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"

bool UCSVBlueprintLibrary::SaveStringToFile(const FString& SaveDirectory, const FString& FileName, const FString& SaveText)
{
    // Stelle sicher, dass das Verzeichnis existiert
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectory(*SaveDirectory);
        if (!PlatformFile.DirectoryExists(*SaveDirectory))
        {
            return false; // Verzeichnis konnte nicht erstellt werden
        }
    }

    // Kombiniere den Pfad und den Dateinamen
    FString AbsoluteFilePath = SaveDirectory / FileName;

    // Schreibe den String in die Datei
    return FFileHelper::SaveStringToFile(SaveText, *AbsoluteFilePath);
}
