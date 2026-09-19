Du bist Embedded-Entwickler für ESP32, C++ und NimBLE. Entwickle eine kleine Firmware für einen ESP32-C3, die handelsübliche BLE-Kameraauslöser beziehungsweise Selfie-Shutter-Buttons empfängt und damit einen GPIO-Ausgang schaltet.

## 0. Testfirmware

Zusätzlich soll eine separate Test- und Diagnosefirmware entstehen, mit der sich unterschiedliche Auslöser untersuchen und eindeutig einem Empfänger zuordnen lassen.

## Allgemeine Rahmenbedingungen

- Verwende NimBLE-Arduino und C++.
- Prüfe zu Beginn die aktuellen APIs und kompatiblen Bibliotheksversionen. Verwende keine ungeprüften Beispiele älterer NimBLE-Versionen.
- Die Anwendung arbeitet eigenständig ohne Smartphone, Home Assistant, WLAN oder Cloud.
- Einstellungen beim Kompilieren reichen aus. Eine Weboberfläche ist nicht erforderlich.
- Halte die Implementierung klein und nachvollziehbar. Keine unnötigen Frameworks oder Zusatzfunktionen.
- Konkrete Boardauswahl und Pinbelegung werden später festgelegt. Kapsle hardwareabhängige Einstellungen und erfinde keine feste Belegung.
- Unterstützt werden echte BLE-Auslöser. Bluetooth Classic gehört nicht zum Projektumfang.
- Projekt soll unter Plattform.IO und VisualStudioCode realisiert werden
- Lege ein lokales GIT an. Überstellung nach GIThub macht der Nutzer.

## 1. Produktivfirmware

Ein Tastendruck des zugeordneten Auslösers steuert einen konfigurierbaren GPIO.

Implementiere zwei auswählbare Betriebsarten:

**Toggle**

- Jeder neue Tastendruck wechselt zwischen aktiv und inaktiv.
- Der Zustand bleibt bis zum nächsten Tastendruck erhalten.

**Impuls**

- Ein Tastendruck aktiviert den Ausgang für eine einstellbare Haltezeit in Millisekunden.
- Danach wird der Ausgang automatisch inaktiv.
- Weitere Tastendrücke während eines laufenden Impulses werden standardmäßig ignoriert. Dokumentiere dieses Verhalten.

**Ausgangspolarität**

- Active-High: inaktiv LOW, aktiv HIGH.
- Active-Low: inaktiv HIGH, aktiv LOW.
- Nach dem Start ist der Ausgang logisch inaktiv.
- Bei Verbindungsverlust wird der Ausgang inaktiv; ein laufender Impuls wird beendet.
- Nach Wiederverbindung bleibt der Ausgang bis zu einem neuen Tastendruck inaktiv.

Die Zeitsteuerung muss nicht blockierend arbeiten.

Unterscheide Tastendruck, Loslassen und gegebenenfalls Wiederholungsreports. Ein physischer Tastendruck darf nicht mehrfach toggeln. Berücksichtige unterschiedliche Reportformate anhand tatsächlich beobachteter Daten. Erfinde keine allgemeingültige Tastencodierung.

## 2. Test- und Diagnosefirmware

Erstelle zuerst eine eigenständig nutzbare Diagnosefirmware mit serieller Bedienung.

Sie soll:

- BLE-Geräte suchen und mit einer auswählbaren Nummer auflisten.
- Gerätenamen, aktuelle BLE-Adresse, Adresstyp, RSSI und verfügbare Advertising-Daten ausgeben.
- Eine gezielte Verbindung zum ausgewählten Gerät erlauben.
- Pairing und dauerhaftes Bonding unterstützen, soweit das Gerät dies unterstützt beziehungsweise verlangt.
- Verbindungs-, Sicherheits- und Authentifizierungsereignisse verständlich protokollieren.
- GATT-Services, Characteristics, Eigenschaften und Deskriptoren auflisten.
- Das HID-Profil untersuchen, insbesondere Report Map, Report References und relevante Eingabereports.
- Relevante Notifications beziehungsweise Indications abonnieren.
- Empfangene Reports mit Zeitstempel, Quelle, Länge und Hexdaten ausgeben.
- Tastendrücken und Loslassen anhand der Aufzeichnung nachvollziehbar machen.
- Verfügbare Geräteinformationen, etwa Hersteller, Modell oder Seriennummer, auslesen.
- Gespeicherte Zuordnungen anzeigen sowie gezielt trennen, entkoppeln und löschen können.
- Eine kopierbare Diagnosezusammenfassung für die spätere Konfiguration der Produktivfirmware ausgeben.

Fehlende Services, nicht lesbare Attribute oder abgelehnte Sicherheitsanforderungen dürfen keinen Absturz verursachen.

## 3. Eindeutige Zuordnung gleicher Auslöser

Ich besitze mehrere baugleiche Auslöser mit identischen Gerätenamen. Mehrere Empfänger-Auslöser-Paare sollen gleichzeitig nebeneinander funktionieren.

- Ein Empfänger soll im Normalbetrieb ausschließlich seinen ausdrücklich zugeordneten Auslöser akzeptieren.
- Der Gerätename allein darf niemals Auswahl- oder Identitätskriterium sein.
- Ein Empfänger darf sich bei Verbindungsverlust nicht automatisch einen anderen gleichnamigen Auslöser suchen.
- Die Zuordnung soll Neustarts und Stromunterbrechungen überstehen.
- Implementiere automatische Wiederverbindung zum zugeordneten Gerät.

Prüfe ausdrücklich die Unterscheidung zwischen aktueller BLE-Adresse und dauerhafter Geräteidentität:

- Untersuche Adresstypen und mögliche wechselnde private Adressen.
- Verwende die Identitäts- und Bonding-Funktionen des BLE-Stacks, soweit verfügbar.
- Behaupte nicht, dass eine beim Scan gefundene MAC-Adresse grundsätzlich dauerhaft ist.
- Falls das Gerät keine zuverlässig wiedererkennbare Identität bereitstellt, dokumentiere die Grenze und eine konkret getestete Vorgehensweise.

Gib keine geheimen Bonding-Schlüssel im Diagnoseprotokoll aus.

## 4. Pairing mit minimaler Bedienung

Plane zusätzlich ein Bedienkonzept für einen Empfänger, der nur Reset und gegebenenfalls einen einzigen nutzbaren Taster besitzt. Dieser Taster kann gleichzeitig der Bootloader-Taster sein.

Prüfe und beschreibe:

- Eintritt in einen zeitlich begrenzten Pairing-Modus durch langes Drücken während des normalen Betriebs.
- Auswahl beziehungsweise Bestätigung des gewünschten Auslösers.
- Vermeidung einer falschen Zuordnung bei mehreren gleichzeitig sichtbaren, gleichnamigen Geräten.
- Abbruch und Timeout.
- Bewusstes Löschen einer bestehenden Zuordnung.
- Verhalten ohne frei nutzbaren Taster; hierfür genügt zunächst die serielle Bedienung.

Eine Auswahl nur anhand des stärksten RSSI oder des ersten gefundenen Geräts genügt nicht als zuverlässige Zuordnung. Prüfe, ob eine Bestätigung durch Tastendruck am verbundenen Auslöser praktikabel ist, und benenne gegebenenfalls erforderliche Schritte wie das vorübergehende Ausschalten anderer Auslöser.

Beachte die Bootloader-/Strapping-Funktion des Tasters. Setze Reset nicht ungeprüft mit einem normalen Anwendungstaster gleich. Eine LED darf optional genutzt, aber nicht vorausgesetzt werden.

Während Pairing und Diagnose bleibt der Schaltausgang inaktiv.

## Statusausgabe auf Display

Das Board hat ein über I2C angeschlossenes Display. (ESP32-C3 OLED Board (72×40 SSD1306, I2C))

Gebe auf dem Display einfache Informationen über den aktuellen Systemstatus mit aus

In Der Art wie:

* Wait Pairing
* Paired
* Aktive
* Button Pressed
* Vorerst einfache Textausgabe

## 5. Vorgehen und Lieferumfang

1. Prüfe NimBLE-Unterstützung und skizziere kurz Architektur, Gerätebindung und Pairing-Ablauf.
2. Implementiere die Diagnosefirmware.
3. Implementiere die Produktivfirmware mit gemeinsam genutzten BLE-Bausteinen, soweit dies sinnvoll ist.
4. Wenn reale Reports für die Auswertung fehlen, liefere die ausführbare Diagnosefirmware und genaue Schritte zur Datenerfassung. Frage gezielt nach diesen Daten, statt Gerätekompatibilität zu erfinden.
5. Dokumentiere Konfiguration, serielle Befehle, Pairing, Wiederverbindung und die Übernahme eines untersuchten Auslösers in die Produktivfirmware.
6. Schreibe eine readme.md im Github Style
7. Schreibe eine Anwenderdokumentation für Laien

Prüfe insbesondere:

- Toggle genau einmal pro Tastendruck.
- Impulsdauer und beide Ausgangspolaritäten.
- Wiederholungs- und Loslassreports.
- Verbindungsabbruch und Wiederverbindung.
- Neustart beider Geräte.
- Mehrere identisch benannte Auslöser mit verschiedenen Empfängern.
- Ausschluss unbeabsichtigter Schaltvorgänge beim Start und Pairing.

Unterscheide im Ergebnis klar zwischen kompiliert, softwareseitig geprüft und tatsächlich mit Hardware getestet. Liefere Quellcode und eine kurze Bedien- und Testanleitung.

## 6. Flashen und Deployment

Aktuell ist an COM9 ein Board angeschlossen.

An PIN 8 ist laut Boardbeschreibung eine LED angebunden. Diese LED soll auch

Das Board entspricht den definitionen unter Platform.IO 

* C:\dev\TheStampSizeDiyPeekDevice\BlePrompter

ESP32-C3 OLED Board (72×40 SSD1306, I2C)

Nach fertigstellung Flashe das Board

Erstelle zusätzlich eine Batchroutine mit der der Nutzer durch Aufruf den Code compilieren kann, und eine weitere Batchroutine mit der das Flashen durchgeführt werden kann.

Lege diese Routinen im Verzeichnis ./Tools ab

Es kann auch Powershell genutzt werden, aber dann soll eine zusätzliche Batchroutine zum Start bereitgestellt werden.

Bsp: C:\dev\LanzSoundGenerator\tools

Hier sollen Tools vorhanden sein, die erlauben die Testfirmware und die Produktivfirmware entsprechend zu deployen.

