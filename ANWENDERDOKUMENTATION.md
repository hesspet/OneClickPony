# Anwenderdokumentation OneKlickPony

Diese Anleitung beschreibt OneKlickPony 0.5.0 ohne Programmierkenntnisse. Unter 0.2.7 war das Koppeln eines realen Auslösers erfolgreich, die anschließende Wiederverbindung scheiterte jedoch mit `Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden`. Version 0.2.8 behebt die Endpunktprüfung und protokolliert bei Bedarf den kompletten GATT-Aufbau zur Ursachenanalyse. Seit 0.5.0 bilden die Arbeitsmodi einzelne Funktionen ab (Toggle, Impuls und ein Servo-Platzhalter); sie lassen sich per seriellem Befehl umschalten, werden dauerhaft gespeichert und beim Start kurz auf dem Display angezeigt.

## 1. Was das Gerät macht

Ein ESP32-C3 verbindet sich per Bluetooth Low Energy (BLE) mit genau einem zuvor angelernten Kameraauslöser. Ein Druck auf den gelernten Knopf schaltet standardmäßig die Board-LED und GPIO8 ein beziehungsweise aus. Das kleine OLED zeigt den aktuellen Zustand.

Es gibt zwei getrennte Firmwares:

- **Produktivfirmware:** für den normalen Betrieb, PlatformIO-Umgebung `product-c3`, Version 0.5.0.
- **Diagnosefirmware:** zum Untersuchen problematischer oder unbekannter Auslöser, Umgebung `diagnose-c3`; ihr serielles Firmware-Banner steht noch auf 0.1.0.

## 2. Benötigte Teile

- ESP32-C3-OLED-Board mit SSD1306-Display, 72 × 40 Pixel
- Datenfähiges USB-Kabel
- BLE-Kameraauslöser oder Selfie-Shutter
- Windows-PC mit PlatformIO
- Für externe Lasten: geeigneter Lasttreiber und Schutzbeschaltung

Das Projekt ist derzeit auf COM9 eingestellt. Falls Windows einen anderen Port anzeigt, muss dieser in `Tools\env.bat` bei `PRODUCT_COM_PORT` und `DIAGNOSE_COM_PORT` eingetragen werden.

## 3. Sicherheit bei der Verdrahtung

- GPIO8 ist Ausgang und zugleich die vorgesehene Active-Low-Board-LED: HIGH bedeutet inaktiv, LOW bedeutet aktiv.
- GPIO9 ist der BOOT-Taster und wird nach dem Start als Bedientaster verwendet.
- GPIO8 und GPIO9 beeinflussen als Strapping-Pins den Start des ESP32-C3. Externe Schaltungen dürfen ihre Pegel beim Einschalten und Reset nicht unzulässig verändern.
- Bei langer oder störanfälliger Leitung am BOOT-Taster einen passenden externen Pull-up vorsehen.
- Relais, Lampen, Motoren und andere größere Lasten niemals direkt an GPIO8 betreiben. Einen geeigneten Transistor-, MOSFET- oder Relais-Treiber verwenden und die Masse verbinden.
- Vor dem Anschließen immer die Schaltung des tatsächlich verwendeten Boards prüfen.

## 4. Produktivfirmware installieren

1. Das Board per USB anschließen.
2. Prüfen, ob es unter Windows als COM9 erscheint.
3. Einen eventuell laufenden seriellen Monitor schließen.
4. Im Explorer `C:\dev\OneKlickPony\Tools\product\Flashen.bat` doppelt anklicken.
5. Warten, bis PlatformIO den Vorgang ohne Fehler beendet.
6. Für Meldungen anschließend `C:\dev\OneKlickPony\Tools\product\Monitor.bat` starten.

`product-c3` 0.2.5 wurde erfolgreich auf COM9 geflasht; der serielle Kontrolllauf zeigte das Banner `OneKlickPony 0.2.5`, und das Pairing mit einem realen Auslöser gelang. Unter 0.2.6 wurde auf das direkte Wiederverbinden umgestellt; Version 0.2.7 wurde real geflasht und gekoppelt, die direkte Wiederverbindung scheiterte dort wiederholt mit `Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden`. Version 0.2.8 macht die Endpunktprüfung symmetrisch zur Lernseite und protokolliert bei einem Fehlschlag den gespeicherten und den neu aufgelösten Endpunkt samt Dienst, CCCD und Report-Referenz.

Falls das Board nicht automatisch in den Flashmodus wechselt:

1. BOOT gedrückt halten.
2. RESET kurz drücken und loslassen.
3. BOOT loslassen.
4. `Flashen.bat` erneut starten.

Nur kompilieren, ohne zu flashen: `C:\dev\OneKlickPony\Tools\product\Kompilieren.bat`.

## 5. Einen Auslöser erstmals anlernen

Beim ersten Start ist noch keine Zuordnung gespeichert. Das Pairing startet automatisch.

1. Alle anderen BLE-Kameraauslöser in der Nähe ausschalten. Nur den gewünschten Auslöser einschalten.
2. Auf dem Display `Suche` und danach `Verbinde` abwarten.
3. Nach der internen Beruhigungszeit von 400 ms erscheint `Knopf druecken`.
4. Den gewünschten Knopf am Kameraauslöser einmal normal drücken und loslassen.
5. Bei `Noch einmal` denselben Knopf ein zweites Mal drücken und loslassen.
6. Erscheint `BOOT`, den BOOT-Taster am ESP32-C3 kürzer als 3 Sekunden drücken und loslassen.
7. Die Zuordnung wird erst nach dieser lokalen Bestätigung dauerhaft gespeichert. Dazu gehören die vom BLE-Stack aufgelöste Geräteidentität einschließlich Adresstyp sowie das gelernte Reportprofil. Danach sucht das Gerät wieder nach genau diesem Auslöser.
8. Bei `Bereit` kann der Kameraauslöser verwendet werden.

Warum zwei Tastendrücke? Beim ersten Zyklus lernt das Gerät das Press- und Release-Muster. Beim zweiten identischen Zyklus prüft es, dass Knopf und Datenquelle wirklich übereinstimmen. Identische Wiederholungsreports während eines gehaltenen Knopfs werden nicht als zusätzliche Tastendrücke gewertet.

Wenn mehrere mögliche Geräte sichtbar sind, ein Report abweicht, die Verbindung abbricht oder die Zeit abläuft, wird das Pairing sicher abgebrochen. Es wird nicht einfach das stärkste, erste oder gleichnamige Gerät übernommen.

## 6. Bedienung im Alltag

Der Ausgang hat frei wählbare Arbeitsmodi, die mit dem seriellen Monitor ohne Neukompilieren umgeschaltet werden können (siehe Abschnitt 10). Jeder Modus bildet eine Funktion ab und ist in der Firmware als eigene Device-Klasse hinterlegt; so lassen sich später weitere Funktionen ergänzen. Die zuletzt gewählte Einstellung wird dauerhaft gespeichert und beim Einschalten für etwa zwei Sekunden als Flash-Screen auf dem Display angezeigt (`Modus` mit `Toggle` beziehungsweise `Impuls` inklusive Haltezeit, z. B. `Impuls 750`).

Im Modus **Toggle** (`LedToggle`) gilt:

1. Erster vollständiger Tastendruck: GPIO8/Board-LED wird aktiv.
2. Zweiter vollständiger Tastendruck: GPIO8/Board-LED wird inaktiv.
3. Wiederholungsreports eines gehaltenen Knopfs lösen kein mehrfaches Umschalten aus.

Im Modus **Impuls** (`LedPulse`) aktiviert ein vollständiger Tastendruck den Ausgang für eine einstellbare Haltezeit (Standardwert 1000 ms, per Parameter 1–60000 ms änderbar, siehe Abschnitt 10); danach wird er automatisch inaktiv. Weitere Tastendrücke während eines laufenden Impulses werden ignoriert. Beim Umschalten des Modus wird ein eventuell aktiver Ausgang sofort inaktiv.

Zusätzlich ist der Modus **Servo** als Platzhalter für eine spätere Servo-Anbindung vorgesehen. Wird er gewählt, zeigt das Display `not implemented`; der aktive Modus bleibt unverändert, und es wird nichts gespeichert.

Nach dem Start und nach jeder Wiederverbindung bleibt der Ausgang zunächst inaktiv. Die Firmware wartet zuerst auf ein Loslassen des gelernten Knopfs. Dadurch löst ein beim Verbindungsaufbau bereits gedrückter Knopf keine Aktion aus.

Bei Verbindungsverlust wird der Ausgang sofort inaktiv. Das Gerät sucht anschließend ausschließlich die persistent gespeicherte, gebondete Identität einschließlich Adresstyp. Es wählt keinen Auslöser anhand von Gerätename oder Signalstärke aus.

### Zustandsanzeige auf dem OLED

Ohne Verbindung zeigt das Display Zustandstexte wie `Koppeln`, `Suche`, `Verbinde`, `Knopf druecken`, `Noch einmal`, `BOOT` oder `Abbruch`. Beim Einschalten erscheint zunächst als Flash-Screen für etwa zwei Sekunden `Modus` mit `Toggle` beziehungsweise `Impuls` (bei Impuls inkl. Haltezeit); danach der reguläre Zustand. Bei aufgebauter Verbindung wird die Anzeige invertiert dargestellt (heller Hintergrund, dunkler Text) und zeigt `Bereit`. Ist der Ausgang aktiv, füllt ein großes `ON` den gesamten Bildschirm. Bei einem Fehler erscheint `ERROR:####` mit einer festen Fehlernummer; den zugehörigen Fehlertext und eine Empfehlung liefert Abschnitt 13.

## 7. BOOT-Taster verwenden

Die Aktion wird beim Loslassen ausgelöst:

| Haltezeit | Wirkung |
| --- | --- |
| Kürzer als 3 Sekunden | Bei `BOOT` das Pairing bestätigen; während einer anderen Pairing-Phase abbrechen; im Normalbetrieb ohne Wirkung |
| 3 bis einschließlich 9 Sekunden | Neues Pairing starten |
| Mehr als 9, aber weniger als 10 Sekunden | Keine Aktion |
| Mindestens 10 Sekunden | Gespeicherte Zuordnung und Bluetooth-Bonds löschen |

Weil BOOT zugleich ein Bootloader-Taster und GPIO9 ein Strapping-Pin ist, den Taster beim normalen Einschalten nicht gedrückt halten.

## 8. Anderen Auslöser anlernen

1. BOOT 3 bis 9 Sekunden gedrückt halten und loslassen.
2. Nur den neuen gewünschten Auslöser einschalten.
3. Die beiden Lernzyklen aus Abschnitt 5 durchführen.
4. Die neue Zuordnung mit einem BOOT-Druck unter 3 Sekunden bestätigen.

Die bestehende bestätigte Zuordnung bleibt bis zur erfolgreichen Bestätigung erhalten. Unbestätigte Bluetooth-Bonds werden beim Abbruch bereinigt.

## 9. Zuordnung löschen

1. BOOT mindestens 10 Sekunden gedrückt halten.
2. BOOT loslassen.
3. Die gespeicherte Zuordnung und die dazugehörigen Bonds werden gelöscht.
4. Das Display zeigt `Koppeln`; anschließend kann neu angelernt werden.

Alternativ kann im seriellen Monitor `clear` eingegeben werden.

## 10. Optional: Modus, Impulsdauer und Polarität einstellen

Der Arbeitsmodus lässt sich seit Version 0.5.0 ohne Neukompilieren im seriellen Monitor umschalten und mit festen Parameterpositionen versehen:

- `mode` zeigt den aktuellen Modus an (bei Impuls inkl. Haltezeit, z. B. `Impuls 750`).
- `mode ledtoggle` (Kurzform `mode toggle`) stellt auf Toggle um und speichert die Einstellung dauerhaft.
- `mode ledpulse[,Haltezeit]` (Kurzform `mode pulse[,Haltezeit]`) stellt auf Impuls um. Die Haltezeit ist die feste erste Parameterposition in Millisekunden (1–60000), zum Beispiel `mode ledpulse,750`. Ohne Parameter gilt die zuletzt eingestellte beziehungsweise die Standard-Haltezeit. Die Einstellung wird dauerhaft gespeichert.
- `mode servo` ist der Platzhalter für die geplante Servo-Erweiterung: Das Display zeigt `not implemented` und der Modus wird weder geändert noch gespeichert.

Die letzte Einstellung übersteht Neustarts und Stromunterbrechungen; beim Einschalten zeigt das Display den aktiven Modus kurz an. Weitere Tastendrücke während eines laufenden Impulses werden ignoriert. Sie starten oder verlängern den Impuls nicht.

Nicht über die serielle Schnittstelle änderbar und nur in `include\ProductConfig.h` per Neukompilieren und Flashen einstellbar sind:

- `outputMode = pony::FuncMode::LedPulse`: nur als Standardwert, wenn noch keine gespeicherte Einstellung vorliegt. Alternativ `pony::FuncMode::LedToggle` oder der Servo-Platzhalter `pony::FuncMode::Servo`. Die alten Namen `pony::OutputMode::Toggle` und `pony::OutputMode::Pulse` heißen jetzt `pony::FuncMode::LedToggle` beziehungsweise `pony::FuncMode::LedPulse`.
- `pulseDurationMillis = 1000`: Standard-Haltezeit (Parameterposition 1) des Impulses in Millisekunden.
- `outputActiveLow = true`: aktiv LOW und inaktiv HIGH.
- `outputActiveLow = false`: aktiv HIGH und inaktiv LOW.

## 11. Serielle Hilfe der Produktivfirmware

`Tools\product\Monitor.bat` öffnet COM9 mit 115200 Baud. Befehle mit der Eingabetaste abschließen:

| Befehl | Bedeutung |
| --- | --- |
| `help` | Alle Produktbefehle anzeigen |
| `status` | Zustand, Zuordnung, Modus, lokalen Bond, Linkstatus, Peer-ID und Ausgang prüfen |
| `mode` | Aktuellen Arbeitsmodus anzeigen (bei Impuls inkl. Haltezeit, z. B. `Impuls 750`) |
| `mode ledtoggle` | Arbeitsmodus auf Toggle umstellen und dauerhaft speichern (Kurzform `mode toggle`) |
| `mode ledpulse[,ms]` | Arbeitsmodus auf Impuls umstellen, optional mit Haltezeit (1–60000 ms), und dauerhaft speichern (Kurzform `mode pulse[,ms]`) |
| `mode servo` | Servo-Platzhalter: zeigt `not implemented` und ändert den Modus nicht |
| `pair` | Pairing starten |
| `confirm` | Gelerntes Profil bestätigen |
| `cancel` | Pairing abbrechen |
| `clear` | Zuordnung und Bonds löschen |

Bei der Wiederverbindung trennt Version 0.3.0 die Fehlerphasen Connect, Security, Linkstatus, Peer-ID, Endpunkt und Subscription. Scheitert die Endpunkt-Eindeutigkeit, folgt automatisch ein Diagnose-Block mit dem gespeicherten Endpunkt (Dienst, Charakteristik, Report-Referenz) sowie dem neu aufgelösten GATT-Aufbau: Dienste, Charakteristiken mit Handle, Eigenschaften, CCCD-Status, gelesene Report-Referenzen und das Match-Ergebnis. Schlüssel werden nicht ausgegeben; bei erfolgreicher Verbindung wird der Detailfehler gelöscht.

## 12. Diagnosefirmware verwenden

Die Diagnosefirmware ist für Auslöser gedacht, die sich nicht eindeutig anlernen oder wiederverbinden lassen. Sie schaltet GPIO8 beim Start sicher auf HIGH, verwendet aber weder Display noch BOOT-Taster zur Bedienung.

1. `C:\dev\OneKlickPony\Tools\diagnose\Flashen.bat` starten. Dieser reale Flashvorgang auf COM9 steht derzeit noch aus.
2. Danach `C:\dev\OneKlickPony\Tools\diagnose\Monitor.bat` öffnen.
3. `scan 10` eingeben.
4. Den Auslöser einschalten oder einen Knopf drücken.
5. Die passende Nummer mit `connect <Nummer>` verbinden, zum Beispiel `connect 0`.
6. Nacheinander `info`, `gatt read` und `hid` eingeben.
7. `subscribe` eingeben und den gewünschten Knopf mehrfach drücken und loslassen.
8. Falls ein Sicherheitsaufbau nötig ist, `secure` eingeben und angezeigte `passkey`- oder `confirm`-Schritte befolgen.
9. Mit `summary` eine kopierbare Zusammenfassung erzeugen.
10. Zusammenfassung und alle Zeilen, die mit `REPORT` beginnen, sichern.

Verfügbare Diagnosebefehle:

| Befehl | Bedeutung |
| --- | --- |
| `scan [Sekunden]` | Geräte suchen und nummeriert anzeigen |
| `connect <Nummer>` | Gewähltes Scan-Ergebnis verbinden |
| `disconnect` | Verbindung trennen |
| `gatt` / `gatt read` | Bluetooth-Dienste anzeigen, optional Werte lesen |
| `info` | Geräteinformationen lesen |
| `hid` | HID-Tastenreports untersuchen |
| `subscribe` | Eingehende Reports protokollieren |
| `secure` | Pairing und Bonding anfordern |
| `passkey <Zahl>` | Angeforderten Zahlencode eingeben |
| `confirm <ja\|nein>` | Angezeigten Zahlenvergleich beantworten |
| `bonds` | Gespeicherte Identitäten anzeigen |
| `forget <Nummer>` / `forget all` | Einen oder alle Bonds löschen |
| `summary` | Diagnosezusammenfassung ausgeben |
| `help` | Hilfe anzeigen |

## 13. Fehlerbehebung

### Fehlermeldungen auf dem Display (ERROR:####)

Bei einem Fehler zeigt das Display die Nummer im Format `ERROR:####`; den zugehörigen Fehlertext sowie eine Kurzempfehlung liefert die folgende Tabelle. Die vollständige, ausführliche Meldung steht zusätzlich im seriellen Monitor (`Fehler #N: Text`).

| Nummer | Fehlertext | Bedeutung / Empfehlung |
| --- | --- | --- |
| 1 | GPIO8 konnte nicht sicher initialisiert werden | Ausgang konnte nicht eingerichtet werden; Gerät neu starten. |
| 2 | Eventqueue fehlt | Interner Speicherfehler; Gerät neu starten. |
| 3 | NimBLE-Initialisierung fehlgeschlagen | Bluetooth-Stack ließ sich nicht starten; Gerät neu starten. |
| 4 | BLE-Scanner oder Client konnte nicht angelegt werden | Interner Fehler; Gerät neu starten. |
| 5 | NVS konnte nicht geöffnet werden | Gespeicherte Einstellungen nicht erreichbar; Gerät neu starten. |
| 6 | Binding ohne passenden Bond | Zuordnung ohne gespeicherten Bluetooth-Schlüssel; neu koppeln (BOOT länger halten). |
| 7 | Bonds konnten beim Start nicht bereinigt werden | Alte Bluetooth-Schlüssel ließen sich nicht löschen; Gerät neu starten. |
| 8 | Pairing-Scan konnte nicht sicher stoppen | Kopplung wurde abgebrochen; erneut zu koppeln versuchen. |
| 9 | Unbestätigter Bond konnte nicht gelöscht werden | Unbestätigte Kopplung konnte nicht entfernt werden; neu versuchen oder Gerät neu starten. |
| 10 | Scan konnte vor Pairing nicht stoppen | Kopplung konnte nicht beginnen; erneut versuchen. |
| 11 | Alte Bonds konnten nicht bereinigt werden | Alte Bluetooth-Schlüssel ließen sich nicht löschen; neu versuchen. |
| 12 | Alte Bonds konnten nach Bestätigung nicht bereinigt werden | Nach dem Koppeln ließen sich alte Schlüssel nicht löschen; erneut versuchen. |
| 13 | Scan konnte vor Löschen nicht stoppen | Löschen konnte nicht beginnen; erneut versuchen. |
| 14 | Löschmarkierung konnte nicht gespeichert werden | Speicherfehler beim Löschen der Zuordnung; neu versuchen oder Gerät neu starten. |
| 15 | Zuordnung gelöscht, Bond-Löschung fehlgeschlagen | Zuordnung ist gelöscht, Bluetooth-Schlüssel blieb aber erhalten; Schlüssel evtl. manuell pflegen. |
| 16 | Zuordnung gelöscht, Altbonds konnten nicht bereinigt werden | Zuordnung ist gelöscht, alte Schlüssel blieben; erneut Löschen versuchen. |
| 17 | Eventqueue überlaufen | Zu viele Meldungen in kurzer Zeit; Verbindung wird neu gestartet. |
| 18 | Client vor Reconnect noch im Trennen; späterer Retry | Wird automatisch erneut versucht. |
| 19 | Client vor Reconnect noch verbunden; Trennung und späterer Retry | Wird automatisch erneut versucht. |
| 20 | Connect zur gespeicherten ID fehlgeschlagen | Auslöser war nicht erreichbar; wird automatisch erneut versucht. Gerät möglichst näher bringen/einschalten. |
| 21 | Security fehlgeschlagen | Sicherheits-Aushandlung fehlgeschlagen; wird automatisch erneut versucht. |
| 22 | Link nicht verschlüsselt | Verbindung ist nicht verschlüsselt; wird automatisch erneut versucht. |
| 23 | Link nicht gebondet | Verbindung ist nicht dauerhaft gespeichert; wird automatisch erneut versucht. |
| 24 | Peer-ID weicht von gespeicherter ID ab | Angeschlossenes Gerät gleicht nicht der gespeicherten Zuordnung; neu koppeln. |
| 25 | Lokaler Bond für gespeicherte ID fehlt nach Security | Bluetooth-Schlüssel ging verloren; neu koppeln. |
| 26 | Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden | GATT-Aufbau passt nicht zur Zuordnung; ausführliche Diagnose steht im seriellen Monitor. |
| 27 | CCCD 0x2902 am gespeicherten Endpunkt fehlt | Notifications sind nicht verfügbar; neu koppeln. |
| 28 | CCCD-Subscription abgelehnt | Benachrichtigungen wurden abgelehnt; neu koppeln. |
| 29 | Lokaler Bond für gespeicherte ID fehlt vor Reconnect | Bluetooth-Schlüssel ging verloren; neu koppeln. |

### Display bleibt leer

- Prüfen, ob wirklich ein SSD1306 mit 72 × 40 Pixeln verbaut ist.
- SDA an GPIO5 und SCL an GPIO6 prüfen.
- Stromversorgung und gemeinsame Masse prüfen.
- Beachten, dass dies noch nicht real am vorhandenen Board validiert wurde.

### Pairing bricht mit einem mehrdeutigen Kandidaten ab

- Alle anderen BLE-Auslöser ausschalten.
- Nur den gewünschten Auslöser einschalten.
- Pairing erneut mit 3 bis 9 Sekunden BOOT oder seriell mit `pair` starten.

### Auslöser verbindet sich nach einem Neustart nicht

- Auslöser aufwecken und in Reichweite bringen.
- Im Produktmonitor `status` prüfen.
- Bei fehlendem Bond die Zuordnung mit mindestens 10 Sekunden BOOT löschen und neu anlernen.
- Mit der Diagnosefirmware Adresstyp, Bonding und aufgelöste Identität prüfen.

Private BLE-Adressen können wechseln. Eine Wiederverbindung ist nur möglich, wenn Auslöser und BLE-Stack die gebondete Identität zuverlässig auflösen. Bei einer nicht auflösbaren Adresse gibt es bewusst keinen unsicheren Fallback über Namen oder RSSI.

### Ausgang reagiert nicht

- Auf `Bereit` warten.
- Den Knopf einmal vollständig loslassen und danach erneut drücken.
- Im Produktmonitor `status` prüfen.
- Active-Low beachten: Der aktive Pegel ist standardmäßig LOW.
- Externe Treiberschaltung und gemeinsame Masse prüfen.

### Flashen schlägt fehl

- Seriellen Monitor schließen.
- COM9 beziehungsweise die Einstellung in `Tools\env.bat` prüfen.
- Datenfähiges USB-Kabel verwenden.
- Den beschriebenen manuellen BOOT-/RESET-Ablauf versuchen.

## 14. Aktueller Prüfstand

- **Kompiliert:** Produktivfirmware 0.5.0 und Diagnosefirmware wurden mit PlatformIO erfolgreich kompiliert.
- **Softwareseitig geprüft:** Alle 17 nativen Tests für Toggle, Impuls, Polarität, Disconnect-Sicherheit, BOOT-Grenzwerte, Laufzeit-Modusumschaltung, Modus- und Parameter-Parsing, die Servo-Platzhaltersperre (`not implemented`) und den zweistufigen Lernablauf sind erfolgreich.
- **Auf COM9 bestätigt:** Produktivfirmware 0.2.7 wurde real geflasht; dort gelang das Koppeln eines realen Auslösers, die direkte Wiederverbindung scheiterte jedoch mit `Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden`.
- **Realer Auslöser:** Pairing unter 0.2.7 erfolgreich; Nachweis einer erfolgreichen Wiederverbindung mit 0.5.0 steht noch aus.
- **Diagnosefirmware:** Erfolgreich gebaut, aber nicht geflasht.
- **Hardwareseitig noch NICHT mit 0.5.0 getestet:** erfolgreiche Wiederverbindung nach Koppeln und Neustart, neue Endpunkt-Diagnose, invertierte OLED-Anzeige und großes `ON`, Fehleranzeige `ERROR:####`, GPIO8-Pegel beziehungsweise Board-LED, Boot-Flash-Screen mit dem aktiven Modus, serial-basierte Modusumschaltung samt Haltezeit-Persistenz, BOOT-Gesten und RPA-Auflösung.
