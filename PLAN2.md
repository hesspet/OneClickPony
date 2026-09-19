# Umsetzungsplan: Webanwendung für OneKlickPony

Stand: 19.09.2026

## 1. Ziel

Es entsteht eine öffentlich über GitHub Pages bereitgestellte Webanwendung, mit der ein technisch grundsätzlich erfahrener, aber nicht entwickelnder Anwender einen OneKlickPony-Empfänger über USB:

1. auf Browser- und Schnittstellenkompatibilität prüfen,
2. als ESP32-C3 erkennen,
3. mit einer freigegebenen Produktivfirmware flashen,
4. über die bestehende serielle Schnittstelle konfigurieren und prüfen,
5. bebilderte Integrationsanleitungen lesen und
6. eingebettete Anwendungsvideos ansehen kann.

Die Bedienoberfläche ist vollständig auf Deutsch und Englisch verfügbar. Deutsch ist immer die Standardsprache. Die Anwendung benötigt weder Backend noch Benutzerkonto und überträgt keine Gerätedaten an einen Server.

## 2. Geprüfter Ausgangsstand

- Die vorhandene Produktivfirmware ist Version 0.5.0 für `esp32-c3-devkitm-1` mit USB-CDC und 115200 Baud.
- Die Firmware bietet bereits die seriellen Befehle `help`, `status`, `mode`, `mode ledtoggle`, `mode ledpulse[,ms]`, `mode servo`, `pair`, `confirm`, `cancel` und `clear`.
- Die Impulsdauer ist auf 1 bis 60000 Millisekunden begrenzt.
- `status` liefert unter anderem Firmwareversion, Laufzeitstatus, Zuordnung, Modus, Ausgang, Bond- und Linkstatus sowie den letzten Fehler.
- Das serielle Protokoll ist zeilenorientiert: Befehle werden mit Zeilenumbruch abgeschlossen; die aktuelle Firmware akzeptiert Befehlszeilen bis 80 Zeichen.
- Neben der Produktivfirmware existiert eine Diagnosefirmware. Für die erste Webversion wird nur die Produktivfirmware öffentlich angeboten. Die Diagnosefirmware bleibt ein Entwicklerwerkzeug.
- Die bestehende Seite [Zauberhaft](https://hesspet.github.io/Zauberhaft/) verwendet ein dunkles, violett-schwarzes Erscheinungsbild mit heller Schrift, goldenen Akzenten, großzügigen Abständen, einer festen Navigation und systemnaher serifenloser Schrift. Diese Gestaltungsmerkmale werden übernommen, ohne die Seite pixelgenau zu kopieren.

## 3. Technische Grenzen und verbindliche Auslegung

### 3.1 Geräteauswahl

Eine Webseite darf einen neuen seriellen Anschluss nicht ohne eine aktive Handlung des Anwenders öffnen. Deshalb bedeutet „automatisch erkennen“ in diesem Projekt:

1. Bereits zuvor freigegebene und aktuell verbundene Anschlüsse werden beim Seitenaufruf über `navigator.serial.getPorts()` angezeigt.
2. Für ein neues Gerät betätigt der Anwender die Schaltfläche „Gerät auswählen“ und wählt es im Browserdialog aus.
3. Danach erkennt die Anwendung den Chip, prüft auf ESP32-C3 und zeigt das Ergebnis verständlich an.
4. Beim An- oder Abstecken wird die Anzeige über die `connect`- und `disconnect`-Ereignisse aktualisiert.

Der Browserdialog selbst wird vom Browser bereitgestellt und kann nicht durch eine eigene, vollständig lokalisierte Geräteliste ersetzt werden.

### 3.2 Unterstützte Endgeräte

Es wird keine feste Betriebssystem- oder Browserliste versprochen. Entscheidend ist eine zur Laufzeit durchgeführte Funktionsprüfung:

- sicherer Kontext über HTTPS,
- vorhandenes `navigator.serial`,
- Zugriff auf `requestPort()`, `getPorts()` und serielle Lese-/Schreibströme,
- funktionierende Steuerleitungen für den ESP32-Bootloader.

Ist eine Voraussetzung nicht erfüllt, zeigt die Anwendung auf der jeweiligen Seite eine lokalisierte Erklärung und nennt als Ausweichweg einen aktuellen Browser mit Web-Serial-Unterstützung auf einem USB-fähigen Gerät. iOS-Geräte werden nicht als unterstützt vorausgesetzt. Android wird nur dann angeboten, wenn die tatsächliche Browserprüfung positiv ist.

### 3.3 Trennung von Flashen und Konfigurieren

„Flashen“ und „Konfigurieren“ sind eigene Menüpunkte und eigene Zustandsabläufe. Ein serieller Anschluss kann immer nur von einem Ablauf gleichzeitig geöffnet sein. Vor dem Wechsel wird er sauber geschlossen. Nach dem Flashen wird das Gerät neu gestartet und für die Konfiguration erneut verbunden.

### 3.4 Erhalt vorhandener Einstellungen

Ein normales Firmware-Update darf die gespeicherte Zuordnung, Bonds und Moduseinstellungen nicht absichtlich löschen. Ein vollständiges Löschen des Flash-Speichers gehört nicht in den normalen Ablauf. Falls später eine saubere Neuinstallation angeboten wird, liegt sie hinter einer zusätzlichen Warnung und Bestätigung.

## 4. Bewusster Umfang der ersten Version

### Enthalten

- Startseite mit kurzer, geführter Auswahl der nächsten Aktion
- Menüpunkt „Flashen“
- Menüpunkt „Konfigurieren“
- Menüpunkt „Anleitungen“ mit zwei zweisprachigen Dummy-Artikeln in Markdown und lokalen Beispielbildern
- Menüpunkt „YouTube“ mit den beiden vorgegebenen Testvideos
- Sprachwahl Deutsch/Englisch im Menü
- Browser-, HTTPS- und Web-Serial-Prüfung
- Erkennung des ESP32-Chips und Ablehnung einer falschen Chipfamilie
- serielles Lesen, Schreiben, Statusabfrage und verständliche Fehlerführung
- lokalisierte Oberfläche, Hilfetexte und Fehlermeldungen
- responsive Bedienung mit Tastatur und Screenreader-Grundunterstützung
- GitHub-Pages-Deployment

### Nicht enthalten

- automatischer Firmware-Build in GitHub Actions
- Backend, Datenbank, Anmeldung, Telemetrie oder Cloudspeicherung
- drahtloses Flashen oder Konfigurieren
- Unterstützung beliebiger ESP32-Projekte oder frei hochladbarer Binärdateien
- öffentlicher Zugriff auf die Diagnosefirmware
- vollständiges Entwicklerterminal als eigene Funktion
- Content-Management-System
- Offline-PWA, Service Worker oder Update-Server

Diese Begrenzung folgt dem YAGNI-Prinzip.

## 5. Empfohlene technische Architektur

### 5.1 Web-Stack

- Vite als schlankes Build-Werkzeug
- TypeScript ohne UI-Framework
- semantisches HTML und eigenes CSS
- lokal eingebundene, fest versionierte Abhängigkeiten; keine zur Laufzeit erforderlichen Skripte von einem CDN
- `@espressif/esptool-js` für Chipkommunikation und Flashen
- Web Serial API für die Konfiguration
- kleiner Markdown-Renderer mit deaktiviertem Roh-HTML oder anschließender Sanitierung
- einfache JSON-basierte Lokalisierung

Ein React-, Vue- oder Svelte-Framework bringt für fünf kleine Seiten keinen notwendigen Nutzen und wird daher nicht eingeplant.

### 5.2 Vorgesehene Verzeichnisstruktur

```text
web/
  index.html
  package.json
  vite.config.ts
  src/
    main.ts
    router.ts
    styles/
      tokens.css
      main.css
    i18n/
      de.json
      en.json
      i18n.ts
    serial/
      serialCapability.ts
      serialConnection.ts
      productProtocol.ts
    flash/
      firmwareCatalog.ts
      firmwareFlasher.ts
    pages/
      homePage.ts
      flashPage.ts
      configurationPage.ts
      guidesPage.ts
      videosPage.ts
    components/
      applicationHeader.ts
      capabilityNotice.ts
      progressSteps.ts
      statusMessage.ts
    content/
      de/
        integration-beispiel-1.md
        integration-beispiel-2.md
      en/
        integration-example-1.md
        integration-example-2.md
    assets/
      guides/
        beispiel-anschluss.svg
        beispiel-einbau.svg
  public/
    firmware/
      catalog.json
      releases/
        0.5.0/
          manifest.json
          bootloader.bin
          partitions.bin
          boot_app0.bin
          firmware.bin
          SHA256SUMS.txt
Tools/
  web/
    FirmwareReleaseVorbereiten.ps1
    FirmwareReleaseVorbereiten.bat
.github/
  workflows/
    github-pages.yml
```

Die endgültigen Flash-Dateinamen und Offsets werden nicht aus diesem Beispiel übernommen, sondern aus dem tatsächlich von PlatformIO erzeugten Upload-Befehl der Umgebung `product-c3` verifiziert.

### 5.3 Zuständigkeiten

- `serialCapability` prüft ausschließlich Browser- und Kontextfähigkeiten.
- `serialConnection` besitzt Öffnen, Schließen, Lesen, Schreiben und Verbindungsereignisse.
- `productProtocol` kapselt die vorhandenen OneKlickPony-Befehle, Timeout-Behandlung und Antwortauswertung.
- `firmwareCatalog` lädt und validiert ausschließlich den veröffentlichten Firmwarekatalog.
- `firmwareFlasher` kapselt `esptool-js`, Chipprüfung, Fortschritt, Neustart und Fehlerabbildung.
- Seiten kennen keine Browserdetails, sondern verwenden diese Dienste über kleine, testbare Schnittstellen.

## 6. Navigation und Gestaltung

### 6.1 Hauptnavigation

Desktop:

- Start
- Flashen
- Konfigurieren
- Anleitungen
- YouTube
- Sprache: Deutsch / English

Auf kleinen Bildschirmen wird daraus ein tastaturbedienbares Menü. Die Spracheinstellung bleibt dauerhaft über `localStorage` erhalten; fehlt eine gespeicherte Einstellung, wird immer Deutsch verwendet.

### 6.2 Gestaltungsrichtung

- Hintergrund in sehr dunklem Violett-Schwarz, orientiert an `rgb(15, 10, 26)`
- helle Hauptschrift, orientiert an `rgb(225, 220, 232)`
- Gold als Akzentfarbe für aktive Navigation, wichtige Schaltflächen und Fortschritt
- feste, leicht transparente Navigation
- klar abgegrenzte Karten für Handlungsblöcke
- große Primärschaltflächen und kurze Absätze
- keine Fachbegriffe ohne unmittelbar sichtbare Erklärung
- Status nie nur durch Farbe ausdrücken; zusätzlich Symbol und Text verwenden
- Bewegungen bei `prefers-reduced-motion` reduzieren

### 6.3 Inhaltliche Tonalität

Texte sprechen den Anwender direkt und freundlich an. Jeder Arbeitsablauf beantwortet sichtbar:

1. Was wird benötigt?
2. Was soll jetzt getan werden?
3. Woran ist Erfolg zu erkennen?
4. Was ist bei einem Fehler zu tun?

## 7. Ablauf „Flashen“

### 7.1 Vorprüfung

Die Seite zeigt vor Beginn eine kurze Checkliste:

- datenfähiges USB-Kabel verwenden,
- OneKlickPony direkt anschließen,
- andere Programme mit geöffnetem seriellen Monitor schließen,
- Stromversorgung während des Vorgangs nicht trennen.

Danach werden HTTPS und Web Serial automatisch geprüft. Bei fehlender Unterstützung endet der Ablauf mit konkreter Hilfe, ohne eine wirkungslose Flash-Schaltfläche zu zeigen.

### 7.2 Gerät auswählen und erkennen

1. Der Anwender wählt „Gerät auswählen“.
2. Der Browser zeigt seine Geräteauswahl.
3. Die Anwendung verbindet sich über `esptool-js` mit dem Bootloader.
4. Chipfamilie und erkannter Chipname werden angezeigt.
5. Nur ein erkannter ESP32-C3 wird für die freigegebene Firmware akzeptiert.
6. Bei einem falschen Chip wird nicht geflasht; die Meldung nennt erkanntes und erwartetes Gerät.

USB-Hersteller- und Produktkennungen dürfen den Browserdialog sinnvoll eingrenzen, aber keine kompatiblen Boards mit abweichendem USB-Seriell-Wandler ausschließen. Die eigentliche Freigabe erfolgt deshalb erst über die Chiperkennung.

### 7.3 Firmware auswählen

Der Firmwarekatalog enthält für jede Version mindestens:

- sichtbaren Produktnamen,
- Version,
- Veröffentlichungsdatum im Format `DD.MM.YYYY`,
- Zielchip `ESP32-C3`,
- Status `stable` oder `testing`,
- Dateien mit Flash-Offset, Dateigröße und SHA-256-Prüfsumme,
- zugehörigen Git-Commit,
- kurze zweisprachige Änderungshinweise.

In der ersten Version wird standardmäßig genau eine stabile Produktivfirmware angeboten. Ältere oder experimentelle Versionen erscheinen nicht, solange dafür kein konkreter Bedarf besteht.

### 7.4 Flashvorgang

1. Manifest und Dateien laden.
2. Schema, Zielchip, Dateigröße und SHA-256-Prüfsummen prüfen.
3. Vor dem Schreiben noch einmal Firmwareversion und Gerät anzeigen.
4. Flashen starten und Fortschritt in Prozent sowie als Schritttext anzeigen.
5. Während des Schreibens Navigation und konkurrierende Geräteaktionen sperren.
6. Gerät neu starten.
7. Seriellen Anschluss schließen und den Anwender zur erneuten Verbindung für die Prüfung auffordern.
8. Nach erneuter Verbindung `status` senden und die gemeldete Firmwareversion mit dem Katalog abgleichen.

Ein abgebrochener oder fehlgeschlagener Vorgang darf nicht als Erfolg erscheinen. Die Seite erklärt bei Bedarf den manuellen BOOT-/RESET-Ablauf aus der Anwenderdokumentation.

### 7.5 Schutz gespeicherter Daten

Der Katalog listet die einzelnen tatsächlich benötigten Binärdateien und Offsets. NVS- und andere persistente Datenbereiche werden beim normalen Update nicht beschrieben. Ein implizites „gesamten Flash löschen“ ist verboten.

## 8. Ablauf „Konfigurieren“

### 8.1 Verbindung

1. Fähigkeit prüfen.
2. Gerät auswählen.
3. Port mit 115200 Baud öffnen.
4. Leser und zeilenweisen Decoder starten.
5. `status` und anschließend `mode` senden.
6. Antwort mit Timeout auswerten.
7. Firmwareversion und erkannten Zustand sichtbar anzeigen.

Kann die Antwort nicht als OneKlickPony erkannt werden, bleibt die Konfiguration gesperrt und die Anwendung bietet „Erneut prüfen“ sowie eine kopierbare technische Ausgabe an.

### 8.2 Konfigurationsoberfläche

Die Oberfläche bildet nur bereits vorhandene, belastbare Firmwarefunktionen ab:

- Status aktualisieren
- Modus „Umschalten“ über `mode ledtoggle`
- Modus „Impuls“ über `mode ledpulse,<Millisekunden>`
- Impulsdauer als Zahlenfeld von 1 bis 60000 Millisekunden
- Pairing starten über `pair`
- gelerntes Profil bestätigen über `confirm`
- Pairing abbrechen über `cancel`
- Zuordnung und Bonds löschen über `clear`

Der Servo-Platzhalter wird nicht als auswählbare Anwenderfunktion angeboten, weil er nicht implementiert ist.

### 8.3 Sichere Änderungen

- Die Anwendung sendet einen Änderungsbefehl erst nach bewusster Betätigung von „Speichern“.
- Nach jeder Modusänderung fragt sie `status` und `mode` erneut ab und zeigt die Änderung erst danach als gespeichert an.
- Ungültige Impulsdauern werden bereits im Formular verhindert und zusätzlich vor dem Senden geprüft.
- `clear` erhält eine eigene Warnung mit zweistufiger Bestätigung, weil Zuordnung und Bonds verloren gehen.
- Pairing wird als geführter Schrittprozess mit Abbruchmöglichkeit dargestellt.
- Beim Verlassen der Seite wird die serielle Verbindung geschlossen.

### 8.4 Technische Ausgabe

Ein standardmäßig eingeklappter Bereich zeigt die unbearbeitete serielle Ausgabe mit Zeitstempel. Er besitzt nur „Kopieren“ und „Leeren“. Freie Befehlseingabe ist in Version 1 nicht vorgesehen.

### 8.5 Robustheit des bestehenden Protokolls

Vor der Webimplementierung wird mit realer Firmware geprüft, ob die Antworten auf `status`, `mode` und Modusänderungen eindeutig und vollständig auswertbar sind. Falls nicht, ist ausschließlich folgende kleine Firmwareergänzung zulässig:

- ein zusätzlicher rückwärtskompatibler Maschinenbefehl, beispielsweise `webstatus`,
- eine einzelne stabile, versionsbehaftete Antwortzeile mit Schlüssel-Wert-Feldern,
- unveränderte bestehende Befehle und Anwenderausgaben.

Eine allgemeine Protokollschicht oder ein JSON-RPC-System wird nicht gebaut.

## 9. Anleitungen aus Markdown

### 9.1 Inhalt

Es werden zwei bewusst als Beispiele gekennzeichnete Dummy-Anleitungen erstellt:

1. „Beispiel: Anschluss eines externen Verbrauchers“
2. „Beispiel: Einbau in ein Requisit“

Beide Artikel existieren auf Deutsch und Englisch, enthalten Überschriften, Listen, Warnhinweise und mindestens ein lokales Beispielbild. Die Texte dürfen nicht als echte, geprüfte Verdrahtungsanleitung missverstanden werden.

### 9.2 Darstellung

- Markdown wird beim Web-Build oder kontrolliert im Browser gerendert.
- Rohes HTML aus Markdown ist deaktiviert oder wird zuverlässig bereinigt.
- Bilder besitzen lokalisierte Alternativtexte und Bildunterschriften.
- Ein Inhaltsverzeichnis wird nur angezeigt, wenn ein Artikel mehrere Abschnitte hat.
- Die Artikelliste zeigt Titel und Kurzbeschreibung in der aktuell gewählten Sprache.

## 10. YouTube-Seite

Die erste Version enthält:

- „Dummy1“: `https://www.youtube.com/watch?v=Bc23cLfiF6I`
- „Dummy2“: `https://www.youtube.com/shorts/OBm9twsoMng`

Die Videos werden responsiv über den datenschutzfreundlicheren Einbettungsmodus `youtube-nocookie.com` eingebunden. Zunächst erscheint ein lokalisierter Vorschaubereich. Erst nach Betätigung von „Video laden“ wird der externe Player geladen; der Hinweis erklärt die dann entstehende Verbindung zu YouTube. Beide Titel und Beschreibungstexte liegen in Deutsch und Englisch vor.

## 11. Lokalisierung

- Jeder sichtbare Text verwendet einen Lokalisierungsschlüssel.
- Dazu gehören Navigation, Schaltflächen, Formularbeschriftungen, Hilfetexte, Fortschritt, Warnungen, Fehler, Erfolgsmeldungen, Bildtexte und Metadaten.
- Deutsch und Englisch müssen dieselbe Schlüsselmenge besitzen; ein automatischer Test prüft fehlende oder zusätzliche Schlüssel.
- Deutsch ist die feste Standardsprache, nicht die Browsersprache.
- Die Auswahl wird lokal gespeichert und ändert Inhalte ohne Seitenneuladung.
- Technische Rohdaten aus der Firmware werden als solche gekennzeichnet und nicht verfälschend übersetzt.
- Sichtbare Datumsangaben verwenden `DD.MM.YYYY`.

## 12. Firmwareablage und manuelle Freigabe

### 12.1 Grundsatz

Firmware wird nicht beim Web-Build erzeugt. Das Repository enthält nur bewusst freigegebene, zuvor lokal gebaute und geprüfte Binärdateien. Der Web-Workflow darf PlatformIO nicht aufrufen.

### 12.2 Manueller Freigabeablauf

1. `product-c3` mit den vorhandenen Werkzeugen lokal kompilieren.
2. PlatformIO-Ausgabe und exakten Upload-Befehl sichern.
3. Produktivfirmware auf realer Hardware flashen und Mindesttest durchführen.
4. Erzeugte Binärdateien mit ihren verifizierten Offsets in ein neues Versionsverzeichnis kopieren.
5. SHA-256-Prüfsummen erzeugen.
6. `manifest.json` und `catalog.json` mit Version, Datum, Zielchip, Commit und Änderungshinweisen aktualisieren.
7. Das Hilfswerkzeug validiert Dateien, Offsets, Prüfsummen, doppelte Versionen und JSON-Schema. Es kompiliert nicht.
8. Webanwendung lokal bauen und den Flashablauf mit den veröffentlichten Dateien testen.
9. Erst danach Dokumente, Katalog und Firmware gemeinsam committen.

`FirmwareReleaseVorbereiten.bat` startet das PowerShell-Hilfswerkzeug, damit der Ablauf unter Windows ohne manuelle PowerShell-Eingabe nutzbar bleibt.

## 13. GitHub Pages

- Vite erhält den korrekten Basis-Pfad `/OneKlickPony/`.
- Eine GitHub-Actions-Datei baut ausschließlich die Webanwendung und veröffentlicht das Ergebnis als GitHub Pages.
- Der Workflow wird nur bei Änderungen an Webdateien, Firmwarekatalog oder dem Deployment-Workflow ausgelöst.
- Er führt keinen PlatformIO-Build durch.
- Pull Requests führen Build, Typprüfung und Tests aus, veröffentlichen aber keine produktive Seite.
- GitHub Pages liefert HTTPS und erfüllt damit die Secure-Context-Voraussetzung der Web Serial API.
- Direkte Aufrufe von Unterseiten dürfen nicht zu einem 404 führen; deshalb wird entweder Hash-Routing oder ein für GitHub Pages geeigneter Fallback verwendet. Für diese kleine Anwendung wird Hash-Routing bevorzugt.

## 14. Fehlerbehandlung

Mindestens folgende Fälle erhalten eigene lokalisierte Meldungen und Handlungsempfehlungen:

- Web Serial nicht verfügbar
- Seite nicht über HTTPS geöffnet
- Geräteauswahl abgebrochen
- kein oder falsches Gerät ausgewählt
- ESP32-C3 nicht erkannt
- Anschluss bereits durch ein anderes Programm belegt
- Bootloader nicht erreichbar
- Firmwaredatei oder Manifest fehlt
- Prüfsumme stimmt nicht
- Flashen abgebrochen oder fehlgeschlagen
- Gerät während des Vorgangs getrennt
- Neustart oder Wiederverbindung fehlgeschlagen
- serielle Antwort läuft in ein Timeout
- Antwort gehört nicht zu OneKlickPony
- Firmwareversion stimmt nach dem Flashen nicht
- ungültige Impulsdauer
- Pairing abgebrochen oder nicht bestätigt

Technische Fehlerdetails stehen in einem aufklappbaren Bereich und können kopiert werden. Die Hauptmeldung bleibt kurz und handlungsorientiert.

## 15. Tests und Abnahmekriterien

### 15.1 Automatisierte Tests

- Typprüfung und Produktions-Build
- gleiche Lokalisierungsschlüssel in Deutsch und Englisch
- Firmwarekatalog- und Manifest-Schema
- Prüfsummenlogik
- Parser für `status`- und `mode`-Antworten einschließlich Teilzeilen und Zeilenumbrüchen
- Zeitüberschreitung und Verbindungsabbruch
- Validierung der Impulsdauer 1, 60000 sowie ungültiger Grenzwerte
- Seitennavigation und Sprachwechsel
- Markdown-Sanitierung
- Umwandlung normaler YouTube- und Shorts-Links in Einbettungsadressen

Browser- und Hardwarezugriff werden in automatisierten Tests über kleine Adapter simuliert. Die echte Hardwareprüfung bleibt zusätzlich erforderlich.

### 15.2 Manuelle Browserprüfung

- aktuelles Chrome und Edge auf Windows
- mindestens ein weiteres tatsächlich Web-Serial-fähiges Desktop-System, sofern verfügbar
- kleiner und großer Bildschirm
- Tastaturbedienung
- Verhalten eines Browsers ohne Web Serial
- direkter Aufruf jeder Route auf GitHub Pages

### 15.3 Hardwareprüfung mit ESP32-C3

- neues Gerät erstmals auswählen
- zuvor freigegebenes Gerät nach erneutem Seitenaufruf erkennen
- falsche Chipfamilie ablehnen
- Produktivfirmware vollständig flashen
- Abbruch durch Trennen des USB-Kabels verständlich behandeln
- Firmwareversion nach Neustart prüfen
- Modus auf Umschalten stellen und nach Neustart erneut auslesen
- Modus auf Impuls mit 1, 750 und 60000 Millisekunden stellen und erneut auslesen
- ungültige Werte nicht senden
- Pairing starten, bestätigen und abbrechen
- Löschen nur nach zweiter Bestätigung
- bestehende Zuordnung und Einstellung bei normalem Firmware-Update erhalten

### 15.4 Fertig-Definition

Die erste Version ist fertig, wenn:

1. alle fünf Inhaltsseiten und die Sprachwahl vorhanden sind,
2. alle sichtbaren Texte auf Deutsch und Englisch verfügbar sind,
3. ein ESP32-C3 mit der freigegebenen Firmware über GitHub Pages geflasht werden kann,
4. die Firmwareversion danach seriell bestätigt wird,
5. Status, Umschaltmodus, Impulsmodus und Impulsdauer bedienbar und nachweislich persistent sind,
6. der Lösch- und Pairingablauf verständlich abgesichert ist,
7. die beiden Markdown-Beispiele und Testvideos dargestellt werden,
8. inkompatible Browser nicht in einen Sackgassen-Ablauf geraten,
9. Build, Tests und manuelle Hardware-Checkliste erfolgreich sind und
10. `README.md`, `ANWENDERDOKUMENTATION.md` und `PROJEKTUEBERSICHT.md` den tatsächlichen Stand widerspruchsfrei wiedergeben.

## 16. Umsetzungsschritte in Reihenfolge

### Phase 1: Technischer Nachweis

- Minimalprojekt unter `web/` anlegen.
- `esptool-js` mit dem vorhandenen ESP32-C3 und dessen USB-CDC-Verhalten prüfen.
- tatsächliche PlatformIO-Binärdateien und Offsets ermitteln.
- Flashen ohne Überschreiben persistenter Bereiche nachweisen.
- vorhandene serielle Antworten für `status`, `mode` und Modusänderungen auf Eignung prüfen.
- Ergebnis dokumentieren; nur bei uneindeutigen Antworten den kleinen `webstatus`-Befehl ergänzen.

Abschlusskriterium: Ein lokaler technischer Prototyp kann den Chip erkennen, eine Testfirmware schreiben und den Firmwarestatus lesen.

### Phase 2: Grundgerüst und Gestaltung

- Vite/TypeScript, Routing und Seitenrahmen einrichten.
- Gestaltungstoken aus der Zauberhaft-Seite ableiten.
- Navigation, responsive Darstellung und zentrale Statuskomponenten umsetzen.
- vollständige Lokalisierungsstruktur mit Deutsch als Standard einrichten.

Abschlusskriterium: Alle Seiten sind navigierbar, responsiv und in beiden Sprachen als Gerüst vorhanden.

### Phase 3: Flashen

- Firmwarekatalog und Schema implementieren.
- Geräteauswahl, Chiperkennung, Fortschritt, Fehler und Neustart umsetzen.
- Prüfsummen und Nachprüfung der Firmwareversion ergänzen.
- Schutz persistenter Daten nachweisen.

Abschlusskriterium: Der geführte Flashablauf besteht die Hardware-Checkliste.

### Phase 4: Konfigurieren

- serielle Verbindung und zeilenweisen Decoder umsetzen.
- Protokolladapter, Statusansicht, Modusformular und Verifikation implementieren.
- Pairing-Ablauf und abgesichertes Löschen ergänzen.
- technische Ausgabe mit Kopierfunktion hinzufügen.

Abschlusskriterium: Alle angebotenen Änderungen werden auf realer Hardware gesetzt, erneut ausgelesen und nach Neustart bestätigt.

### Phase 5: Inhalte

- zwei zweisprachige Dummy-Anleitungen und Beispielbilder hinzufügen.
- Markdown-Darstellung und Navigation umsetzen.
- YouTube-Testvideos mit Zustimmung vor externem Laden einbetten.

Abschlusskriterium: Inhalte sind zweisprachig, responsiv und ohne zusätzliche Dokumentation verständlich.

### Phase 6: Qualität und Veröffentlichung

- automatisierte Tests vervollständigen.
- Barrierefreiheit, Tastaturbedienung und Fehlerfälle prüfen.
- manuellen Firmware-Freigabehelfer erstellen.
- GitHub-Pages-Workflow einrichten und öffentliche Bereitstellung prüfen.
- bestehende Projekt- und Anwenderdokumentation aktualisieren.

Abschlusskriterium: Alle Punkte der Fertig-Definition sind belegt und die öffentliche Seite ist über HTTPS erreichbar.

## 17. Vor der Implementierung zu bestätigende Annahmen

Falls keine andere Entscheidung getroffen wird, gelten folgende YAGNI-orientierte Vorgaben:

1. Öffentlich angeboten wird ausschließlich die stabile Produktivfirmware; die Diagnosefirmware bleibt lokal.
2. Normale Updates erhalten Pairing, Bonds und Einstellungen; ein vollständiges Löschen wird in Version 1 nicht angeboten.
3. „Konfigurieren“ umfasst Modus, Impulsdauer, Status, Pairing und das bewusst abgesicherte Löschen der Zuordnung.
4. Die Anwendung wird im bestehenden Repository unter `web/` entwickelt und über dasselbe GitHub-Repository veröffentlicht.
5. Das Zauberhaft-Layout dient als visuelle Familie; Quellcode und Medien der Referenzseite werden nicht ungeprüft kopiert.

## 18. Technische Referenzen

- [MDN: Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [Espressif: esptool-js](https://github.com/espressif/esptool-js)
- [ESP Web Tools: Firmwaremanifeste und Browsergrundlagen](https://esphome.github.io/esp-web-tools/)
- [Gestaltungsreferenz Zauberhaft](https://hesspet.github.io/Zauberhaft/)
