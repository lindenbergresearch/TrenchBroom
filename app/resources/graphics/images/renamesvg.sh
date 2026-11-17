#!/usr/bin/env bash

# Der String, ab dem (und inklusive dessen) abgeschnitten werden soll
SEARCH="_20dp"

for file in *.svg; do
  # Prüfen, ob die Datei den Suchstring enthält
  if [[ "$file" == *"$SEARCH"* ]]; then
    # Alles ab dem Suchstring abschneiden
    prefix="${file%%"$SEARCH"*}"
    
    # Optional: Ein eventuell übrig bleibendes "_" am Ende entfernen
    prefix="${prefix%_}"
    
    new_name="${prefix}.svg"
    
    # Datei umbenennen
    mv -- "$file" "$new_name"
    echo "Umbesannt: '$file' -> '$new_name'"
  fi
done