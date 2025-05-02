// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSVBlueprintLibrary.generated.h"

/**
 * 
 */
UCLASS()
class HCI_PRAKTIKUM_VR_API_API UCSVBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Speichert einen String als Textdatei (z.B. CSV) in das angegebene Verzeichnis.
     * @param SaveDirectory - Das Verzeichnis, in dem die Datei gespeichert werden soll (z.B. FPaths::ProjectDir() / "SavedCSV").
     * @param FileName - Der Name der Datei, z.B. "Results.csv".
     * @param SaveText - Der zu speichernde Text (CSV-Inhalt).
     * @return true, wenn das Schreiben erfolgreich war, andernfalls false.
     */
    UFUNCTION(BlueprintCallable, Category = "File IO")
    static bool SaveStringToFile(const FString& SaveDirectory, const FString& FileName, const FString& SaveText);
};
