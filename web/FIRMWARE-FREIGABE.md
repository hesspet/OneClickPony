# Firmware-Freigabe

Der Webkatalog enthält ausschließlich die Produktivfirmware `0.5.0` für `ESP32-C3`.
Die vier Dateien im Manifest stammen aus dem vorhandenen PlatformIO-Artefakt
`.pio/build/product-c3` sowie aus der dazugehörigen installierten Arduino-Plattform.

## Verifizierte Flash-Belegung

| Datei | Offset |
| --- | --- |
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `firmware.bin` | `0x10000` |

Die erzeugte Partitionstabelle reserviert NVS von `0x9000` bis ausschließlich
`0xE000`. Die Freigabedateien überschreiben diesen Bereich nicht; damit bleiben
Zuordnung, Bonds und Moduseinstellungen erhalten. `boot_app0.bin` schreibt den
benachbarten OTA-Auswahlbereich ab `0xE000`, jedoch keine Nutzereinstellungen.

## Freigabe prüfen

Nach dem lokal gebauten und hardwareseitig geprüften `product-c3`-Artefakt im
passenden Versionsordner ablegen, Manifest und Katalog aktualisieren und dann
ausführen:

```powershell
.\Tools\web\FirmwareReleaseVorbereiten.ps1
```

Alternativ startet `Tools\web\FirmwareReleaseVorbereiten.bat` dieselbe
Prüfung. Der Helfer baut nichts: Er prüft ausschließlich JSON-Struktur,
Versionsduplikate, Datumsformat, Hashes, Dateigrößen, Flash-Überlappungen und
den Schutz deklarierter Bereiche. Eine echte Hardware-Abnahme bleibt vor der
öffentlichen Veröffentlichung erforderlich.
