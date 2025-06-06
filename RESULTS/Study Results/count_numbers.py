import os
import pandas as pd
import re
from collections import defaultdict

# Verzeichnis auf das aktuelle Skript setzen
data_dir = os.path.dirname(os.path.abspath(__file__))

# Regex zum Parsen von Dateinamen im Format Participant_<ID>_<Between>_<Group>.csv
pattern = re.compile(r'^Participant_(\d+)_(\w+)_(Group[A-Z])\.csv$')

# Listen bzw. Dictionarys für:
# 1. Inkonsistenzen wie zuvor
# 2. Zählen der Gruppen-Kombinationen (AR/VR × GroupA/GroupB)
mismatches = []
count_combinations = defaultdict(int)

# Über alle Dateien im Verzeichnis iterieren
for fname in os.listdir(data_dir):
    match = pattern.match(fname)
    if not match:
        # Wenn der Dateiname nicht reinpasst, überspringen
        print(f"Dateiname '{fname}' entspricht nicht dem erwarteten Muster und wird übersprungen.")
        continue
    
    pid_fname, between_fname, group_fname = match.groups()
    file_path = os.path.join(data_dir, fname)
    
    # CSV-Datei einlesen
    try:
        df = pd.read_csv(file_path)
    except Exception as e:
        print(f"Fehler beim Einlesen von '{fname}': {e}")
        continue
    
    # Werte aus der ersten Zeile für den Vergleich extrahieren
    pid_content = str(df['Participant_ID'].iloc[0])
    between_content = df['Between_Condition'].iloc[0]
    group_content = "Group" + df['Group_Type'].iloc[0]
    
    # Auf Abweichungen prüfen
    errors = []
    if pid_fname != pid_content:
        errors.append(f"Participant_ID mismatch: filename={pid_fname} vs content={pid_content}")
    if between_fname != between_content:
        errors.append(f"Between_Condition mismatch: filename={between_fname} vs content={between_content}")
    if group_fname != group_content:
        errors.append(f"Group_Type mismatch: filename={group_fname} vs content={group_content}")
    
    if errors:
        mismatches.append((fname, errors))
    
    # Nur wenn keine Inkonsistenzen vorliegen, zur Zählung hinzufügen
    if not errors:
        # Key = (Between_Condition, Group_Type), z.B. ("AR", "GroupA")
        key = (between_content, group_content)
        count_combinations[key] += 1

# Ergebnisse der Konsistenzprüfung ausgeben
if mismatches:
    print("Folgende Dateien weisen Inkonsistenzen auf:")
    for fname, errs in mismatches:
        print(f"\nDatei: {fname}")
        for err in errs:
            print(f"  - {err}")
else:
    print("Alle Dateien haben die korrekten Werte im Inhalt entsprechend dem Dateinamen.")

# Ausgabe der Häufigkeiten pro Kombinationsgruppe
print("\nAnzahl der Dateien pro (Between_Condition, Group_Type):")
# Mögliche Kombinationen ausdrücklich angeben (für AR/VR × GroupA/GroupB)
kombinationen = [
    ("AR", "GroupA"),
    ("AR", "GroupB"),
    ("VR", "GroupA"),
    ("VR", "GroupB")
]
for combo in kombinationen:
    anzahl = count_combinations.get(combo, 0)
    print(f"  {combo[0]} & {combo[1]}: {anzahl}")
