# OneKlickPony

OneKlickPony ist eine eigenständige ESP32-C3-Firmware, die einen ausdrücklich zugeordneten BLE-Kameraauslöser lernt und damit GPIO8 schaltet. Das Projekt enthält zusätzlich eine separate Diagnosefirmware zum Untersuchen unbekannter Auslöser.

**Projektstand:** 0.5.0. Die Produktivfirmware 0.5.0 mit Fehlernummern-Katalog, neuer Anzeige und erweiterbarem Funktions-Modus-Konzept (LedToggle, LedPulse mit einstellbarer Haltezeit, Servo-Dummy) ist gebaut; 0.2.7 wurde real geflasht und gekoppelt, die direkte Wiederverbindung scheiterte dort an der Endpunkt-Eindeutigkeit. Die separat gepflegte Diagnosefirmware meldet in ihrer seriellen Ausgabe Version 0.1.0.

## Firmware-Umgebungen

| PlatformIO-Umgebung | Zweck | Einstiegspunkt |
| --- | --- | --- |
| `product-c3` | Produktivbetrieb mit Display, selbstlernender Zuordnung und GPIO-Ausgang; Standardumgebung | `src/product/main.cpp` |
| `diagnose-c3` | Manuelle BLE-, GATT-, HID- und Bond-Diagnose über die serielle Schnittstelle | `src/diagnose/main.cpp` |
| `native` | Softwaretests der hardwareunabhängigen Ausgangs- und Lernlogik | `test/test_product_core/test_main.cpp` |

Ohne Angabe einer Umgebung baut PlatformIO `product-c3`, da diese Umgebung in `platformio.ini` als `default_envs` eingetragen ist. Beide Gerätefirmwares verwenden das Arduino-Framework, das Board `esp32-c3-devkitm-1`, USB-CDC und 115200 Baud. BLE wird mit NimBLE-Arduino 2.5.1 umgesetzt; Bluetooth Classic wird nicht unterstützt.

## Hardware und Pinbelegung

| Funktion | Anschluss | Verhalten |
| --- | --- | --- |
| Ausgang/Board-LED | GPIO8 | Standardmäßig Impuls (LedPulse) und Active-Low: inaktiv HIGH, aktiv LOW; Modus zur Laufzeit umschaltbar |
| BOOT-Taster | GPIO9 | Active-Low mit internem Pull-up; Gesten werden beim Loslassen ausgewertet |
| OLED | SSD1306, 72 × 40 Pixel | I²C: SDA GPIO5, SCL GPIO6 |

GPIO8 und GPIO9 sind Strapping-Pins. Externe Beschaltung darf beim Reset keine falschen Bootpegel erzwingen. Für GPIO9 ist bei störanfälliger oder externer Tasterverdrahtung ein geeigneter externer Pull-up vorzusehen. Lasten nicht direkt aus GPIO8 versorgen: Für Relais, Lampen oder andere größere Lasten einen passenden Lasttreiber mit Schutzbeschaltung und gemeinsamer Masse verwenden. Vor dem Anschließen die tatsächliche Board-Schaltung und die Active-Low-Board-LED prüfen.

## Konfiguration

Die kompilierbaren Produkteinstellungen stehen zentral in [`include/ProductConfig.h`](include/ProductConfig.h):

- `outputMode`: `pony::FuncMode::LedPulse` (Standard), alternativ `pony::FuncMode::LedToggle` oder der Servo-Dummy `pony::FuncMode::Servo`. Die alten Namen `pony::OutputMode::Toggle` und `pony::OutputMode::Pulse` heißen jetzt `pony::FuncMode::LedToggle` beziehungsweise `pony::FuncMode::LedPulse`.
- `outputActiveLow`: `true` für Active-Low, `false` für Active-High
- `pulseDurationMillis`: Standard-Haltezeit für LedPulse in Millisekunden (entspricht Parameterposition 1), standardmäßig 1000 ms
- Display-, BOOT- und Ausgangspins sowie Pairing-, Lern- und Wiederverbindungszeiten

Der Ausgangsmodus ist ein erweiterbares Funktions-Modus-Konzept: Jeder Mode bildet eine Funktion ab und ist als eigene Device-Klasse in `include/devices/*.h` beziehungsweise `src/common/devices/*.cpp` implementiert. Folgende Modi gibt es:

- **LedToggle** (Alias `toggle`): jeder vollständig erkannte neue Tastendruck wechselt den Ausgang genau einmal um.
- **LedPulse** (Alias `pulse`, Standard): ein Tastendruck aktiviert den Ausgang für die eingestellte Haltezeit; danach wird der Ausgang automatisch inaktiv. Weitere Tastendrücke während eines laufenden Impulses werden ignoriert und verlängern den Impuls nicht. Die Haltezeit ist die feste Parameterposition 1 (1–60000 ms), zum Beispiel `mode ledpulse,750`.
- **Servo** (Dummy): bewusster Datenträger für die geplante Servo-Erweiterung. Wird er gewählt, zeigt das Display `not implemented` und der Modus wird weder geändert noch gespeichert.

Neue Modi lassen sich über eine eigene Device-Klasse plus einen Eintrag in der Fabrik [`src/common/ModeDevice.cpp`](src/common/ModeDevice.cpp) ergänzen.

Der Ausgangsmodus lässt sich seit 0.5.0 zur Laufzeit per seriellem Befehl umschalten (`mode ledtoggle` beziehungsweise der Alias `mode toggle`, `mode ledpulse[,ms]` beziehungsweise der Alias `mode pulse[,ms]`, `mode servo`). Die letzte Einstellung, bei LedPulse einschließlich der eingegebenen Haltezeit, wird dauerhaft im nichtflüchtigen Speicher abgelegt und beim Start geladen; beim Einschalten zeigt das Display den aktiven Modus kurz als Flash-Screen an (`Modus` mit `Toggle` oder `Impuls` inkl. Haltezeit). Bereits gespeicherte Werte der Vorversionen (0 = Toggle, 1 = Impuls) bleiben gültig. `outputMode` in [ProductConfig.h](include/ProductConfig.h) dient nur als Ausgangswert bei fehlender gespeicherter Einstellung.

## Kompilieren, Flashen und Monitor

Voraussetzungen sind Visual Studio Code mit PlatformIO oder PlatformIO Core 6 sowie ein ESP32-C3-OLED-Board. `Tools\env.bat` setzt derzeit sowohl `PRODUCT_COM_PORT` als auch `DIAGNOSE_COM_PORT` auf `COM9`.

Die folgenden Batchdateien werden aus dem Projektverzeichnis aufgerufen und enthalten die jeweils exakte PlatformIO-Umgebung:

| Aktion | Produktivfirmware | Diagnosefirmware |
| --- | --- | --- |
| Kompilieren | `Tools\product\Kompilieren.bat` | `Tools\diagnose\Kompilieren.bat` |
| Kompilieren und auf COM9 flashen | `Tools\product\Flashen.bat` | `Tools\diagnose\Flashen.bat` |
| Serieller Monitor, 115200 Baud, COM9 | `Tools\product\Monitor.bat` | `Tools\diagnose\Monitor.bat` |

Vor dem Flashen einen laufenden seriellen Monitor schließen. Falls der automatische Reset nicht funktioniert: BOOT gedrückt halten, RESET kurz drücken und loslassen, danach BOOT loslassen und den Flashvorgang erneut starten. Unter 0.2.7 wurde ein realer gekoppelter Auslöser auf COM9 geflasht; die Wiederverbindung dort scheiterte an der Endpunkt-Eindeutigkeit. Version 0.2.8 setzt die Endpunktprüfung symmetrisch zur Lernseite um und protokolliert bei Fehlschlag den GATT-Aufbau. Stand 0.5.0 wurde noch nicht geflasht.

## Produktivbetrieb

### Sicheres Ausgangsverhalten

- Beim Start wird GPIO8 vor der übrigen Initialisierung logisch inaktiv gesetzt. Ein früherer Toggle-Zustand wird nicht wiederhergestellt.
- Während Pairing, Suche und Fehlerbehandlung bleibt der Ausgang inaktiv.
- Bei Verbindungsverlust wird der Ausgang sofort inaktiv; ein laufender Impuls endet.
- Nach jeder Verbindung ist zunächst ein gelernter Release-Report erforderlich. Ein bereits gedrückt gehaltener Auslöser kann dadurch beim Verbinden nicht unbeabsichtigt schalten.
- Unbekannte, wiederholte oder nicht zum gelernten Profil passende Reports schalten nicht.

### BOOT-Gesten

| Dauer bis zum Loslassen | Aktion |
| --- | --- |
| Kürzer als 3 Sekunden | Ausstehende lokale Zuordnung bestätigen; in einer anderen Pairing-Phase Pairing abbrechen; sonst keine Aktion |
| 3 bis einschließlich 9 Sekunden | Pairing starten |
| Mehr als 9, aber weniger als 10 Sekunden | Keine Aktion |
| Mindestens 10 Sekunden | Gespeicherte Zuordnung und Bonds löschen |

GPIO9 wird erst nach einem normalen Start als Anwendungstaster ausgewertet; der Taster behält seine Bootloader-/Strapping-Funktion.

### Selbstlernendes Pairing

Ohne gespeicherte Zuordnung startet das Pairing beim Einschalten automatisch. Bei vorhandener Zuordnung wird es mit einer BOOT-Geste von 3 bis 9 Sekunden oder dem seriellen Befehl `pair` gestartet.

1. Nur den gewünschten Auslöser einschalten. Bei genau einem sichtbaren HID-Gerät wird dieses gewählt; ohne HID-Kennzeichnung ist genau ein verbindbares Gerät erforderlich. Mehrdeutige Ergebnisse brechen ab. Name, erstes Ergebnis und stärkstes RSSI sind keine Auswahlkriterien.
2. Die Firmware verbindet, baut eine sichere gebondete Verbindung auf und übernimmt die vom BLE-Stack aufgelöste Geräteidentität einschließlich Adresstyp.
3. Nach 400 ms Settling werden ältere Verbindungsreports verworfen. Das Display fordert zum Drücken auf.
4. Den gewünschten Auslöser einmal drücken und loslassen. Der erste neue Report wird als Press gelernt; identische Wiederholungen werden ignoriert; der erste davon abweichende Report wird als Release gelernt.
5. Nach `Noch einmal` denselben Knopf erneut drücken und loslassen. Press, Release und Report-Endpunkt müssen mit dem ersten Zyklus identisch sein.
6. Bei `BOOT` die Zuordnung mit einem BOOT-Druck unter 3 Sekunden oder `confirm` lokal bestätigen. Erst jetzt werden die aufgelöste Identität einschließlich Adresstyp, Endpunkt und Reportmuster redundant mit Sequenz und CRC im nichtflüchtigen Speicher gesichert. Alte Bonds werden entfernt.

Die Lernphase läuft höchstens 30 Sekunden, die Bestätigung höchstens 20 Sekunden. Ein BOOT-Druck unter 3 Sekunden während einer noch nicht bestätigungsbereiten Pairing-Phase oder `cancel` bricht ab. Unbestätigte Bonds werden bereinigt.

### Wiederverbindung und Identitätsgrenzen

Die automatische Wiederverbindung verbindet direkt ausschließlich die persistent gespeicherte, gebondete BLE-Identität einschließlich ihres Adresstyps. Nach dem Sicherheitsaufbau werden Bondstatus, Verschlüsselung und aufgelöste Geräteidentität erneut exakt geprüft. Gerätename und RSSI werden weder zur Wiedererkennung noch als Fallback verwendet.

Eine sichtbare Scan-Adresse kann eine auflösbare private Adresse (RPA) sein und regelmäßig wechseln. Die dauerhafte Wiedererkennung funktioniert nur, wenn Auslöser und BLE-Stack Bonding sowie Identitätsauflösung zuverlässig unterstützen und NimBLE den Scan-Kandidaten passend zur gespeicherten Identität liefert. Bei nicht auflösbaren privaten Adressen, fehlendem Bonding, gelöschten Schlüsseln oder nicht aufgelösten RPAs kann die Firmware das Gerät bewusst nicht automatisch zuordnen. Dann ist erneutes Pairing beziehungsweise eine Diagnose erforderlich; es wird niemals ersatzweise ein gleichnamiges oder stärkeres Gerät gewählt.

Version 0.3.0 verbindet direkt zur gespeicherten Identität und unterscheidet die Reconnect-Fehlerphasen Connect, Security, Linkverschlüsselung, Link-Bond, Peer-ID, lokalen Bond, Endpunkt und Subscription. Seit 0.5.0 zeigt `status` zusätzlich den aktiven Funktionsmodus an (bei LedPulse inkl. Haltezeit). Scheitert die Endpunkt-Eindeutigkeit, gibt die Firmware automatisch den gespeicherten Endpunkt sowie den neu aufgelösten GATT-Dienst mit Charakteristiken, CCCD-Status, Report-Referenzen und Match-Ergebnis aus. Es werden keine Schlüssel ausgegeben.

### Serielle Produktbefehle

| Befehl | Funktion |
| --- | --- |
| `help` | Befehlsübersicht anzeigen |
| `status` | Firmwareversion, Zustand, Zuordnung, Modus (bei LedPulse inkl. Haltezeit), lokalen Bond, Linkstatus, Peer-ID und Ausgang anzeigen |
| `mode` | Aktuellen Funktionsmodus anzeigen (bei LedPulse inkl. Haltezeit, z. B. `Impuls 750`) |
| `mode ledtoggle` | Funktionsmodus auf LedToggle umstellen und dauerhaft speichern (Kurzform `mode toggle`) |
| `mode ledpulse[,ms]` | Funktionsmodus auf LedPulse umstellen, optional mit Haltezeit (1–60000 ms), und dauerhaft speichern (Kurzform `mode pulse[,ms]`; ohne Haltezeit gilt die zuletzt gespeicherte bzw. der Standardwert `pulseDurationMillis`) |
| `mode servo` | Servo-Dummy: zeigt `not implemented` und ändert den Modus nicht |
| `pair` | Pairing starten |
| `confirm` | Gelerntes Profil lokal bestätigen |
| `cancel` | Laufendes Pairing abbrechen |
| `clear` | Zuordnung und Bonds löschen |

Das OLED zeigt kompakte Zustände wie `Koppeln`, `Suche`, `Verbinde`, `Knopf druecken`, `Noch einmal`, `BOOT` oder `Abbruch`. Beim Start erscheint zunächst als Flash-Screen für etwa zwei Sekunden `Modus` mit `Toggle` oder `Impuls` (bei Impuls inkl. Haltezeit, z. B. `Impuls 750`); danach der reguläre Zustand. Bei aufgebauter Verbindung wird die Anzeige invertiert dargestellt (heller Hintergrund, dunkler Text) und zeigt `Bereit`. Ist der Ausgang aktiv, wird der gesamte Bildschirm groß mit `ON` beschriftet. Tritt ein Fehler auf, erscheint `ERROR:####` mit einem dokumentierten Fehlercode; die zugehörige Fehlertabelle steht in der [ANWENDERDOKUMENTATION](ANWENDERDOKUMENTATION.md).

## Diagnosefirmware

Die Diagnosefirmware untersucht BLE-Auslöser manuell. Sie setzt GPIO8 beim Start auf HIGH als sicheren inaktiven Pegel, verwendet aber weder das OLED noch BOOT zur Bedienung.

### Diagnoseablauf

1. `Tools\diagnose\Flashen.bat` ausführen. Der reale Flashvorgang auf COM9 ist derzeit noch ausstehend.
2. `Tools\diagnose\Monitor.bat` starten.
3. `scan 10` eingeben und den Auslöser einschalten beziehungsweise eine Taste drücken.
4. Mit `connect <Nummer>` das gewünschte Scan-Ergebnis verbinden.
5. `info`, `gatt read` und `hid` ausführen.
6. `subscribe` ausführen und jede Auslösertaste mehrfach drücken und loslassen.
7. Falls nötig `secure` ausführen und Anforderungen mit `passkey` oder `confirm` beantworten.
8. `summary` ausführen und die Zusammenfassung zusammen mit allen `REPORT`-Zeilen sichern.

`REPORT`-Zeilen enthalten Zeitstempel, Notification oder Indication, Handle, UUID, Länge und Hexdaten. Bonding-Schlüssel werden nicht ausgegeben.

### Diagnosebefehle

| Befehl | Funktion |
| --- | --- |
| `scan [Sekunden]` | Aktiver BLE-Scan mit Adresse, Adresstyp, Name, RSSI und Advertising-Rohdaten |
| `connect <Nummer>` | Mit einem Ergebnis des letzten Scans verbinden |
| `disconnect` | Aktive Verbindung trennen |
| `gatt` / `gatt read` | GATT-Struktur ausgeben, optional lesbare Werte abrufen |
| `info` | Hersteller, Modell, Seriennummer und Versionsangaben auslesen |
| `hid` | HID Report Map, Report References und HID-Reports untersuchen |
| `subscribe` | Alle Notification- und Indication-Kanäle abonnieren |
| `secure` | Sicherheitsaufbau und Bonding anfordern |
| `passkey <Zahl>` | Angeforderten Passkey eingeben |
| `confirm <ja\|nein>` | Zahlenvergleich bestätigen oder ablehnen |
| `bonds` | Gespeicherte Bond-Identitäten anzeigen |
| `forget <Nummer>` / `forget all` | Einen oder alle Bonds löschen |
| `summary` | Kopierbare Diagnosezusammenfassung ausgeben |
| `help` | Befehlsübersicht anzeigen |

## Tests und Prüfstand

Der native Testlauf wird mit folgendem Befehl gestartet:

```powershell
python -m platformio test --project-dir "C:\dev\OneKlickPony" --environment native
```

Die Tests decken Toggle, Impulsdauer, ignorierte Presses während eines Impulses, `millis()`-Überlauf, Disconnect-Sicherheit, beide Polaritäten, die BOOT-Grenzwerte, die Laufzeit-Modusumschaltung (`setConfig` resettet den aktiven Zustand und erhält die Scharfschaltung) sowie Lernen, Wiederholungsreports, Abweichungen, Endpunktwechsel, Abbruch und Bestätigung ab. Drei neue Tests prüfen das Mode-Parsing (`ledtoggle`/`ledpulse`/`servo` samt Alias `toggle`/`pulse`), die Parameter-Parsing inkl. Validierung (Haltezeit 1–60000 ms) und den Servo-Dummy, der nie aktiviert wird. Zusammen sind es 17 Tests.

Klare Abgrenzung des aktuellen Prüfstands:

- **Kompiliert:** `product-c3` 0.5.0 und `diagnose-c3` wurden mit PlatformIO erfolgreich kompiliert.
- **Softwareseitig geprüft:** Alle 17 nativen Unity-Tests der hardwareunabhängigen Kernlogik sind erfolgreich; zusätzlich sind sichere Zustandsübergänge und Identitätsprüfungen im Code umgesetzt.
- **Auf COM9 bestätigt:** `product-c3` 0.2.7 wurde real geflasht; das Koppeln eines realen Auslösers gelang und wurde gespeichert.
- **Realer Auslöser:** Unter 0.2.7 scheiterte die unmittelbar nach dem Koppeln folgende direkte Wiederverbindung wiederholt mit `Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden`. Version 0.2.8 stellt die Endpunktprüfung symmetrisch zur Lernseite und protokolliert bei Fehlschlag den GATT-Aufbau; ein Hardwarelauf mit 0.5.0 steht aus.
- **Diagnosefirmware:** Erfolgreich gebaut, aber nicht geflasht.
- **Hardwareseitig noch NICHT mit 0.5.0 getestet:** erfolgreiche Wiederverbindung nach Koppeln und Neustart, neue Endpunkt-Diagnose, invertierte OLED-Anzeige und großes `ON`, Fehleranzeige `ERROR:####`, GPIO-Pegel beziehungsweise Board-LED, Boot-Flash-Screen mit dem aktiven Modus, serial-basierte Modusumschaltung und Haltezeit-Persistenz, BOOT-Gesten und RPA-Auflösung.

Eine schrittweise Bedienanleitung für Anwender steht in [`ANWENDERDOKUMENTATION.md`](ANWENDERDOKUMENTATION.md).

## Webanwendung

Die zweisprachige Vite-Webanwendung liegt in [`web/`](web/). Sie läuft ausschließlich im Browser, benötigt kein Konto und überträgt keine Gerätedaten an einen Server. Sie enthält die Seiten Start, Flashen, Konfigurieren, Anleitungen und YouTube. Deutsch ist die Standardsprache; Englisch kann im Menü ausgewählt werden.

Die Flash-Funktion bietet ausschließlich die freigegebene Produktivfirmware 0.5.0 an. Der veröffentlichte Dateisatz beschreibt nur `0x0000`, `0x8000`, `0xE000` und `0x10000`; die NVS-Partition ab `0x9000` bleibt bei normalen Updates unangetastet. Der Browser muss HTTPS und Web Serial unterstützen. Nach einem Flashvorgang wird die Firmwareversion über eine erneute serielle Verbindung abgefragt.

```powershell
Set-Location "C:\dev\OneKlickPony\web"
npm ci
npm run check
```

Die Firmwarefreigabe wird bewusst lokal vorbereitet und nie im GitHub-Pages-Workflow kompiliert:

```powershell
.\Tools\web\FirmwareReleaseVorbereiten.ps1 -ReleaseVersion 0.5.0
```

Details zu Offsets, Prüfsummen und der noch erforderlichen Hardwareabnahme stehen in [`web/FIRMWARE-FREIGABE.md`](web/FIRMWARE-FREIGABE.md).
