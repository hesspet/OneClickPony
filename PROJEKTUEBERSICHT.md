# Projektübersicht: OneKlickPony

## Zweck

OneKlickPony 0.5.0 verbindet einen ESP32-C3 dauerhaft mit einem ausdrücklich angelernten BLE-Kameraauslöser und schaltet dessen Board-LED beziehungsweise einen Ausgang an GPIO8 in einem erweiterbaren Funktionsmodus (LedToggle, LedPulse mit einstellbarer Haltezeit, Servo-Platzhalter). Das Projekt arbeitet ohne WLAN, Cloud oder Smartphone.

## Bestandteile

- `product-c3` (Standard): Produktivfirmware 0.5.0 mit selbstlernendem Pairing, persistent gespeicherter gebondeter Identität einschließlich Adresstyp, gezielter Reconnect-Diagnose inklusive Endpunkt-Dump, symmetrischer Reconnect-Endpunktlogik, SSD1306-Anzeige, BOOT-Gesten und erweiterbarem Funktionsmodus (persistent, mit Modus-Anzeige als Boot-Flash-Screen). Jeder Mode ist eine Device-Klasse in eigener Datei (`include/devices/*.h`, `src/common/devices/*.cpp`) und wird über eine Mode-Registry in `src/common/ModeDevice.cpp` angeschlossen; neue Funktionen werden so ohne Umbau ergänzt.
- Fehlernummern-Katalog in `include/ErrorCodes.h` (Fehler erscheinen auf dem OLED als `ERROR:####` und in der Dokumentation tabellarisch).
- OLED zeigt bei verbundener Verbindung invertiert und bei aktivem Ausgang ein großes `ON`.
- `diagnose-c3`: eigenständige Diagnosefirmware mit seriellen BLE-, GATT-, HID- und Bond-Werkzeugen; ihr Firmware-Banner steht weiterhin auf 0.1.0.
- `native`: Unity-Tests für Ausgangs-, Polaritäts-, Report-, Lern-, Modus- und Parameterlogik.
- `Tools\product` und `Tools\diagnose`: getrennte Batchdateien zum Kompilieren, Flashen und Überwachen; COM9 wird zentral in `Tools\env.bat` konfiguriert.

## Hardware

ESP32-C3 (`esp32-c3-devkitm-1`), SSD1306 72 × 40 an SDA GPIO5/SCL GPIO6, BOOT an GPIO9 und Active-Low-Ausgang/Board-LED an GPIO8. GPIO8 und GPIO9 sind Strapping-Pins; externe Lasten benötigen einen geeigneten Lasttreiber, störanfällige Tasterbeschaltungen gegebenenfalls einen externen Pull-up.

## Prüfstand

- `product-c3` 0.5.0 und `diagnose-c3` wurden erfolgreich kompiliert; alle 17 nativen Unity-Tests sind erfolgreich.
- Unter 0.2.7 wurde ein reales Koppeln erfolgreich durchgeführt und gespeichert. Die anschließende direkte Wiederverbindung scheiterte jedoch wiederholt mit `Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden`, obwohl Connect, Security, Verschlüsselung, Bond und Peer-Identität bestanden.
- Version 0.2.8 stellte die Reconnect-Endpunktprüfung symmetrisch zur Lernseite (CCCD-0x2902-Filter, HID-Reportzählung, tolerante 2908-Behandlung) und gibt bei einem fehlgeschlagenen Endpunkt-Match automatisch den gespeicherten Endpunkt sowie den neu aufgelösten GATT-Dienst mit allen Kandidaten, CCCD-Status, Report-Referenzen und Match-Ergebnis aus. Ein realer Kontrolllauf mit 0.5.0 steht aus.
- `diagnose-c3` wurde nicht geflasht.
- Noch NICHT mit 0.5.0 hardwareseitig getestet sind die erfolgreiche Wiederverbindung nach Koppeln und Neustart, die neue Endpunkt-Diagnose, die invertierte OLED-Anzeige und das große `ON`, die Fehleranzeige `ERROR:####`, sichtbare OLED-Ausgabe, GPIO-Pegel beziehungsweise Board-LED, der Boot-Flash-Screen mit aktuellem Modus, die serial-basierte Modusumschaltung samt Haltezeit-Persistenz, BOOT-Gesten und RPA-Verhalten.

Details stehen in `README.md`; die schrittweise Anleitung für Laien steht in `ANWENDERDOKUMENTATION.md`.
