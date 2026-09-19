#include <Arduino.h>
#include <NimBLEDevice.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <limits.h>

#include "FirmwareConfig.h"

namespace {

NimBLEScan* scan = nullptr;
NimBLEClient* client = nullptr;
String commandBuffer;
constexpr gpio_num_t diagnoseOutputPin = GPIO_NUM_8;
constexpr size_t maximumReportPayload = 256;
constexpr size_t maximumReportEndpoints = 32;

struct ReportEvent {
    uint32_t timestamp;
    uint16_t handle;
    uint16_t length;
    bool isNotify;
    uint8_t data[maximumReportPayload];
};

struct ReportEndpoint {
    uint16_t handle;
    char uuid[37];
};

QueueHandle_t reportQueue = nullptr;
bool diagnoseReady = false;
ReportEndpoint reportEndpoints[maximumReportEndpoints]{};
size_t reportEndpointCount = 0;
std::atomic<uint32_t> droppedReports{0};

String addressType(const NimBLEAddress& address) {
    if (address.isPublic()) return "public";
    if (address.isRpa()) return "random private resolvable";
    if (address.isNrpa()) return "random private non-resolvable";
    if (address.isStatic()) return "random static";
    return "unbekannt";
}

String hex(const uint8_t* data, size_t length) {
    const char digits[] = "0123456789ABCDEF";
    String result;
    result.reserve(length * 3);
    for (size_t index = 0; index < length; ++index) {
        if (index != 0) result += ' ';
        result += digits[(data[index] >> 4) & 0x0F];
        result += digits[data[index] & 0x0F];
    }
    return result;
}

String hex(const std::string& value) {
    return hex(reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

String uuid(const NimBLEUUID& value) {
    return String(value.toString().c_str());
}

void printAdvertisement(const NimBLEAdvertisedDevice& device) {
    const NimBLEAddress& address = device.getAddress();
    Serial.printf("  Name: %s\n", device.haveName() ? device.getName().c_str() : "<nicht vorhanden>");
    Serial.printf("  Adresse: %s (%s, Typ %u)\n", address.toString().c_str(), addressType(address).c_str(), address.getType());
    Serial.printf("  RSSI: %d dBm, connectable: %s, scannable: %s\n",
                  device.getRSSI(), device.isConnectable() ? "ja" : "nein", device.isScannable() ? "ja" : "nein");
    Serial.printf("  Advertising-Rohdaten (%u Byte): %s\n", device.getAdvLength(), hex(device.getPayload().data(), device.getPayload().size()).c_str());
    if (device.haveManufacturerData()) {
        for (uint8_t index = 0; index < device.getManufacturerDataCount(); ++index) {
            Serial.printf("  Herstellerdaten[%u]: %s\n", index, hex(device.getManufacturerData(index)).c_str());
        }
    }
    if (device.haveServiceData()) {
        for (uint8_t index = 0; index < device.getServiceDataCount(); ++index) {
            Serial.printf("  Service-Daten[%u] %s: %s\n", index, uuid(device.getServiceDataUUID(index)).c_str(), hex(device.getServiceData(index)).c_str());
        }
    }
    if (device.haveServiceUUID()) {
        Serial.print("  Beworbene Services:");
        for (uint8_t index = 0; index < device.getServiceUUIDCount(); ++index) Serial.printf(" %s", uuid(device.getServiceUUID(index)).c_str());
        Serial.println();
    }
}

void printScanResults(const NimBLEScanResults& results) {
    Serial.printf("\nScan beendet: %d Geraet(e)\n", results.getCount());
    for (int index = 0; index < results.getCount(); ++index) {
        const NimBLEAdvertisedDevice* device = results.getDevice(index);
        if (device == nullptr) continue;
        Serial.printf("\n[%d]\n", index);
        printAdvertisement(*device);
    }
    Serial.println("Mit 'connect <Nummer>' verbinden.");
}

class ScanCallbacks final : public NimBLEScanCallbacks {
    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("Scan-Ende, Grund: %d\n", reason);
        printScanResults(results);
    }
};

void printSecurityState(const NimBLEConnInfo& info, const char* event) {
    const NimBLEAddress otaAddress = info.getAddress();
    const NimBLEAddress idAddress = info.getIdAddress();
    Serial.printf("Sicherheit %s: OTA=%s (%s), Identitaet=%s (%s), verschluesselt=%s, authentifiziert=%s, gebondet=%s, Schluesselgroesse=%u\n",
                  event,
                  otaAddress.toString().c_str(), addressType(otaAddress).c_str(),
                  idAddress.toString().c_str(), addressType(idAddress).c_str(),
                  info.isEncrypted() ? "ja" : "nein", info.isAuthenticated() ? "ja" : "nein",
                  info.isBonded() ? "ja" : "nein", info.getSecKeySize());
}

class ClientCallbacks final : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* connectedClient) override {
        Serial.printf("Verbunden mit %s, MTU=%u, RSSI=%d dBm\n", connectedClient->getPeerAddress().toString().c_str(), connectedClient->getMTU(), connectedClient->getRssi());
    }

    void onConnectFail(NimBLEClient*, int reason) override {
        Serial.printf("Verbindung fehlgeschlagen, Grund: %d\n", reason);
    }

    void onDisconnect(NimBLEClient*, int reason) override {
        Serial.printf("Verbindung getrennt, Grund: %d\n", reason);
    }

    void onPassKeyEntry(NimBLEConnInfo& info) override {
        printSecurityState(info, "Passkey wird erwartet");
        Serial.println("Passkey mit 'passkey <sechsstellige Zahl>' eingeben.");
    }

    void onConfirmPasskey(NimBLEConnInfo& info, uint32_t pin) override {
        printSecurityState(info, "Zahlenvergleich");
        Serial.printf("Vergleiche die Zahl %06lu und bestaetige mit 'confirm ja' oder lehne mit 'confirm nein' ab.\n", static_cast<unsigned long>(pin));
    }

    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        printSecurityState(info, "abgeschlossen");
    }

    void onIdentity(NimBLEConnInfo& info) override {
        printSecurityState(info, "Identitaet aufgeloest");
    }

    void onMTUChange(NimBLEClient*, uint16_t mtu) override {
        Serial.printf("MTU geaendert: %u\n", mtu);
    }
};

ScanCallbacks scanCallbacks;
ClientCallbacks clientCallbacks;

void reportCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
    if (reportQueue == nullptr || length > maximumReportPayload) {
        droppedReports.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    ReportEvent event{};
    event.timestamp = millis();
    event.handle = characteristic->getHandle();
    event.length = static_cast<uint16_t>(length);
    event.isNotify = isNotify;
    memcpy(event.data, data, length);
    if (xQueueSend(reportQueue, &event, 0) != pdTRUE) droppedReports.fetch_add(1, std::memory_order_relaxed);
}

const char* reportUuid(uint16_t handle) {
    for (size_t index = 0; index < reportEndpointCount; ++index) {
        if (reportEndpoints[index].handle == handle) return reportEndpoints[index].uuid;
    }
    return "unbekannt";
}

void printQueuedReports() {
    if (reportQueue == nullptr) return;
    ReportEvent event{};
    while (xQueueReceive(reportQueue, &event, 0) == pdTRUE) {
        Serial.printf("REPORT %lu ms %s Handle 0x%04X UUID %s Laenge %u: %s\n",
                      static_cast<unsigned long>(event.timestamp), event.isNotify ? "Notification" : "Indication",
                      event.handle, reportUuid(event.handle), static_cast<unsigned>(event.length),
                      hex(event.data, event.length).c_str());
    }
    const uint32_t dropped = droppedReports.exchange(0, std::memory_order_relaxed);
    if (dropped != 0) {
        Serial.printf("WARNUNG: %lu Report(s) wegen voller Queue verworfen.\n", static_cast<unsigned long>(dropped));
    }
}

String properties(const NimBLERemoteCharacteristic& characteristic) {
    String value;
    if (characteristic.canRead()) value += "read ";
    if (characteristic.canWrite()) value += "write ";
    if (characteristic.canWriteNoResponse()) value += "write-no-response ";
    if (characteristic.canNotify()) value += "notify ";
    if (characteristic.canIndicate()) value += "indicate ";
    return value.length() == 0 ? "keine" : value;
}

void printGatt(bool readValues) {
    if (client == nullptr || !client->isConnected()) {
        Serial.println("Nicht verbunden.");
        return;
    }

    const auto& services = client->getServices(true);
    Serial.printf("GATT: %u Service(s)\n", static_cast<unsigned>(services.size()));
    for (const NimBLERemoteService* service : services) {
        Serial.printf("Service Handle 0x%04X-0x%04X UUID %s\n", service->getStartHandle(), service->getEndHandle(), uuid(service->getUUID()).c_str());
        const auto& characteristics = service->getCharacteristics(true);
        for (const NimBLERemoteCharacteristic* characteristic : characteristics) {
            Serial.printf("  Characteristic Handle 0x%04X UUID %s [%s]\n", characteristic->getHandle(), uuid(characteristic->getUUID()).c_str(), properties(*characteristic).c_str());
            if (readValues && characteristic->canRead()) {
                NimBLEAttValue value = const_cast<NimBLERemoteCharacteristic*>(characteristic)->readValue();
                Serial.printf("    Wert: %s\n", hex(value.data(), value.size()).c_str());
            }
            const auto& descriptors = characteristic->getDescriptors(true);
            for (const NimBLERemoteDescriptor* descriptor : descriptors) {
                Serial.printf("    Descriptor Handle 0x%04X UUID %s", descriptor->getHandle(), uuid(descriptor->getUUID()).c_str());
                if (readValues) {
                    NimBLEAttValue value = const_cast<NimBLERemoteDescriptor*>(descriptor)->readValue();
                    Serial.printf(" Wert: %s", hex(value.data(), value.size()).c_str());
                }
                Serial.println();
            }
        }
    }
}

void inspectHid() {
    if (client == nullptr || !client->isConnected()) {
        Serial.println("Nicht verbunden.");
        return;
    }
    NimBLERemoteService* hid = client->getService("1812");
    if (hid == nullptr) {
        Serial.println("Kein HID-Service (0x1812) vorhanden.");
        return;
    }
    Serial.println("HID-Service 0x1812:");
    NimBLERemoteCharacteristic* reportMap = hid->getCharacteristic("2a4b");
    if (reportMap != nullptr && reportMap->canRead()) {
        NimBLEAttValue value = reportMap->readValue();
        Serial.printf("  Report Map (%u Byte): %s\n", static_cast<unsigned>(value.size()), hex(value.data(), value.size()).c_str());
    } else {
        Serial.println("  Report Map (0x2A4B) fehlt oder ist nicht lesbar.");
    }
    const auto& characteristics = hid->getCharacteristics(true);
    for (NimBLERemoteCharacteristic* characteristic : characteristics) {
        if (uuid(characteristic->getUUID()) != "2a4d") continue;
        Serial.printf("  Report Handle 0x%04X [%s]\n", characteristic->getHandle(), properties(*characteristic).c_str());
        NimBLERemoteDescriptor* reference = characteristic->getDescriptor(NimBLEUUID("2908"));
        if (reference != nullptr) {
            NimBLEAttValue value = reference->readValue();
            Serial.printf("    Report Reference: %s\n", hex(value.data(), value.size()).c_str());
        } else {
            Serial.println("    Report Reference (0x2908) fehlt.");
        }
    }
}

void inspectDeviceInformation() {
    if (client == nullptr || !client->isConnected()) {
        Serial.println("Nicht verbunden.");
        return;
    }
    NimBLERemoteService* information = client->getService("180a");
    if (information == nullptr) {
        Serial.println("Kein Device-Information-Service (0x180A) vorhanden.");
        return;
    }
    struct InformationField {
        const char* uuid;
        const char* label;
    };
    const InformationField fields[] = {
        {"2a29", "Hersteller"}, {"2a24", "Modell"}, {"2a25", "Seriennummer"},
        {"2a27", "Hardware"}, {"2a26", "Firmware"}, {"2a28", "Software"},
        {"2a23", "System-ID"}, {"2a50", "PnP-ID"},
    };

    Serial.println("Geraeteinformationen:");
    for (const InformationField& field : fields) {
        NimBLERemoteCharacteristic* characteristic = information->getCharacteristic(field.uuid);
        if (characteristic == nullptr) continue;
        if (!characteristic->canRead()) {
            Serial.printf("  %s: nicht lesbar\n", field.label);
            continue;
        }
        NimBLEAttValue value = characteristic->readValue();
        bool printable = value.size() > 0;
        for (size_t index = 0; index < value.size(); ++index) {
            if (value[index] < 32 || value[index] > 126) printable = false;
        }
        if (printable) Serial.printf("  %s: %.*s\n", field.label, static_cast<int>(value.size()), value.data());
        else Serial.printf("  %s: %s\n", field.label, hex(value.data(), value.size()).c_str());
    }
}

void subscribeReports() {
    if (client == nullptr || !client->isConnected()) {
        Serial.println("Nicht verbunden.");
        return;
    }
    if (reportQueue != nullptr) xQueueReset(reportQueue);
    reportEndpointCount = 0;
    uint16_t subscribed = 0;
    const auto& services = client->getServices();
    for (NimBLERemoteService* service : services) {
        const auto& characteristics = service->getCharacteristics();
        for (NimBLERemoteCharacteristic* characteristic : characteristics) {
            const bool notifications = characteristic->canNotify();
            if (!notifications && !characteristic->canIndicate()) continue;
            if (reportEndpointCount >= maximumReportEndpoints) {
                Serial.println("Zu viele Report-Kanaele; weitere werden nicht abonniert.");
                continue;
            }
            const bool success = characteristic->subscribe(notifications, reportCallback, true);
            Serial.printf("Abo Handle 0x%04X UUID %s: %s\n", characteristic->getHandle(), uuid(characteristic->getUUID()).c_str(), success ? "aktiv" : "fehlgeschlagen");
            if (success) {
                ReportEndpoint& endpoint = reportEndpoints[reportEndpointCount++];
                endpoint.handle = characteristic->getHandle();
                const std::string uuidText = characteristic->getUUID().toString();
                const size_t copyLength = uuidText.size() < sizeof(endpoint.uuid) - 1 ? uuidText.size() : sizeof(endpoint.uuid) - 1;
                memcpy(endpoint.uuid, uuidText.data(), copyLength);
                endpoint.uuid[copyLength] = '\0';
                ++subscribed;
            }
        }
    }
    Serial.printf("%u Report-Kanal/Kanaele abonniert. Jetzt Tasten druecken und loslassen.\n", subscribed);
}

bool parseIndex(const String& argument, int& value) {
    if (argument.length() == 0) return false;
    uint32_t parsed = 0;
    for (size_t index = 0; index < argument.length(); ++index) {
        const char character = argument[index];
        if (character < '0' || character > '9') return false;
        parsed = parsed * 10u + static_cast<uint32_t>(character - '0');
        if (parsed > INT_MAX) return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

void printBonds() {
    const int count = NimBLEDevice::getNumBonds();
    Serial.printf("Gespeicherte Bond(s): %d\n", count);
    for (int index = 0; index < count; ++index) {
        const NimBLEAddress address = NimBLEDevice::getBondedAddress(index);
        Serial.printf("[%d] %s (%s, Typ %u)\n", index, address.toString().c_str(), addressType(address).c_str(), address.getType());
    }
}

void printHelp() {
    Serial.println("\nBefehle:");
    Serial.println("  scan [Sekunden]      Aktiven BLE-Scan starten und Ergebnisse nummeriert ausgeben");
    Serial.println("  connect <Nummer>     Mit einem Ergebnis des letzten Scans verbinden");
    Serial.println("  disconnect           Verbindung trennen");
    Serial.println("  gatt                 Services, Characteristics und Deskriptoren auflisten");
    Serial.println("  gatt read            GATT auflisten und lesbare Werte auslesen");
    Serial.println("  info                 Hersteller, Modell, Seriennummer und Versionen auslesen");
    Serial.println("  hid                  HID Report Map, Report References und Reports untersuchen");
    Serial.println("  subscribe            Alle Notification-/Indication-Kanaele abonnieren");
    Serial.println("  secure               Pairing/Sicherheitsaufbau explizit starten");
    Serial.println("  passkey <Zahl>       Angeforderten Passkey eingeben");
    Serial.println("  confirm <ja|nein>    Zahlenvergleich bestaetigen oder ablehnen");
    Serial.println("  bonds                Gespeicherte Bond-Identitaeten anzeigen");
    Serial.println("  forget <Nummer>      Einen Bond anhand seiner bonds-Nummer loeschen");
    Serial.println("  forget all           Alle gespeicherten Bonds loeschen");
    Serial.println("  summary              Kopierbare Diagnosezusammenfassung ausgeben");
    Serial.println("  help                 Diese Hilfe anzeigen\n");
}

void printSummary() {
    Serial.println("\n=== OneKlickPony Diagnosezusammenfassung ===");
    Serial.printf("Firmware: %s %s\n", firmwareName, firmwareVersion);
    Serial.printf("Lokale BLE-Adresse: %s\n", NimBLEDevice::getAddress().toString().c_str());
    if (client != nullptr && client->isConnected()) {
        const NimBLEConnInfo info = client->getConnInfo();
        printSecurityState(info, "aktueller Stand");
        Serial.printf("Verbundenes Geraet: %s, RSSI=%d dBm, MTU=%u\n", client->getPeerAddress().toString().c_str(), client->getRssi(), client->getMTU());
        Serial.println("HID- und GATT-Daten mit 'hid' sowie 'gatt read' erfassen und diesem Block beifuegen.");
    } else {
        Serial.println("Kein Geraet verbunden. Erst scan, connect, hid, subscribe ausfuehren.");
    }
    printBonds();
    Serial.println("Hinweis: Eine beim Scan sichtbare Adresse ist keine garantiert dauerhafte Identitaet. Fuer die Produktivfirmware nur aufgeloeste Identitaeten/Bonds verwenden.");
    Serial.println("=== Ende Diagnosezusammenfassung ===\n");
}

void connectToScanResult(int index) {
    if (scan->isScanning()) {
        Serial.println("Scan zuerst mit 'scan' abwarten oder neu starten.");
        return;
    }
    const NimBLEScanResults results = scan->getResults();
    if (index < 0 || index >= results.getCount()) {
        Serial.println("Ungueltige Scan-Nummer.");
        return;
    }
    const NimBLEAdvertisedDevice* device = results.getDevice(index);
    if (device == nullptr || !device->isConnectable()) {
        Serial.println("Dieses Scan-Ergebnis ist nicht verbindbar.");
        return;
    }
    if (client != nullptr && client->isConnected()) client->disconnect();
    if (client == nullptr) {
        client = NimBLEDevice::createClient();
        client->setClientCallbacks(&clientCallbacks, false);
        client->setConnectTimeout(10000);
    }
    Serial.printf("Verbinde mit [%d] %s ...\n", index, device->getAddress().toString().c_str());
    if (!client->connect(device, true, false, true)) {
        Serial.printf("Verbindung fehlgeschlagen, Fehler: %d\n", client->getLastError());
        return;
    }
    printGatt(false);
    Serial.println("Mit 'hid' und danach 'subscribe' die Ausloeserreports aufzeichnen.");
}

void processCommand(String command) {
    command.trim();
    if (command.length() == 0) return;
    String argument;
    const int separator = command.indexOf(' ');
    if (separator >= 0) {
        argument = command.substring(separator + 1);
        argument.trim();
        command = command.substring(0, separator);
    }
    command.toLowerCase();

    if (command == "help") {
        printHelp();
        return;
    }
    if (!diagnoseReady) {
        Serial.println("Diagnose nicht initialisiert.");
        return;
    }

    if (command == "scan") {
        uint32_t duration = argument.length() == 0 ? defaultScanDurationMillis : static_cast<uint32_t>(argument.toInt()) * 1000;
        if (duration == 0) duration = defaultScanDurationMillis;
        scan->clearResults();
        Serial.printf("Aktiver Scan fuer %lu ms gestartet ...\n", static_cast<unsigned long>(duration));
        if (!scan->start(duration, false, true)) Serial.println("Scan konnte nicht gestartet werden.");
    } else if (command == "connect") {
        int index = 0;
        if (!parseIndex(argument, index)) Serial.println("Ungueltige Scan-Nummer.");
        else connectToScanResult(index);
    } else if (command == "disconnect") {
        if (client != nullptr && client->isConnected()) client->disconnect();
        else Serial.println("Nicht verbunden.");
    } else if (command == "gatt") {
        printGatt(argument == "read");
    } else if (command == "info") {
        inspectDeviceInformation();
    } else if (command == "hid") {
        inspectHid();
    } else if (command == "subscribe") {
        subscribeReports();
    } else if (command == "secure") {
        if (client == nullptr || !client->isConnected()) Serial.println("Nicht verbunden.");
        else if (!NimBLEDevice::startSecurity(client->getConnHandle())) Serial.println("Sicherheitsaufbau konnte nicht gestartet werden.");
    } else if (command == "passkey") {
        if (client == nullptr || !client->isConnected()) Serial.println("Nicht verbunden.");
        else if (!NimBLEDevice::injectPassKey(client->getConnInfo(), static_cast<uint32_t>(argument.toInt()))) Serial.println("Passkey konnte nicht uebergeben werden.");
    } else if (command == "confirm") {
        if (client == nullptr || !client->isConnected()) Serial.println("Nicht verbunden.");
        else if (!NimBLEDevice::injectConfirmPasskey(client->getConnInfo(), argument == "ja")) Serial.println("Bestaetigung konnte nicht uebergeben werden.");
    } else if (command == "bonds") {
        printBonds();
    } else if (command == "forget") {
        if (argument == "all") {
            Serial.printf("Alle Bonds geloescht: %s\n", NimBLEDevice::deleteAllBonds() ? "ja" : "nein");
        } else {
            int index = 0;
            const int count = NimBLEDevice::getNumBonds();
            if (!parseIndex(argument, index) || index < 0 || index >= count) {
                Serial.println("Ungueltige Bond-Nummer.");
            } else {
                const NimBLEAddress address = NimBLEDevice::getBondedAddress(index);
                Serial.printf("Bond geloescht: %s\n", NimBLEDevice::deleteBond(address) ? "ja" : "nein");
            }
        }
    } else if (command == "summary") printSummary();
    else {
        Serial.println("Unbekannter Befehl. 'help' zeigt die Befehle.");
    }
}

void readSerialCommands() {
    while (Serial.available() > 0) {
        const char input = static_cast<char>(Serial.read());
        if (input == '\r') continue;
        if (input == '\n') {
            processCommand(commandBuffer);
            commandBuffer = "";
        } else if (commandBuffer.length() < 120) {
            commandBuffer += input;
        }
    }
}

} // namespace

void setup() {
    gpio_set_level(diagnoseOutputPin, 1);
    gpio_set_direction(diagnoseOutputPin, GPIO_MODE_OUTPUT);
    Serial.begin(serialBaudRate);
    const uint32_t waitUntil = millis() + 2000;
    while (!Serial && millis() < waitUntil) delay(10);

    Serial.printf("\n%s %s\n", firmwareName, firmwareVersion);
    Serial.println("ESP32-C3 BLE-Test- und Diagnosefirmware");
    reportQueue = xQueueCreate(8, sizeof(ReportEvent));
    if (reportQueue == nullptr) {
        Serial.println("FEHLER: Report-Queue konnte nicht angelegt werden.");
        return;
    }
    if (!NimBLEDevice::init("OneKlickPony-Diagnose")) {
        Serial.println("FEHLER: NimBLE konnte nicht initialisiert werden.");
        return;
    }
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setMTU(256);

    scan = NimBLEDevice::getScan();
    if (scan == nullptr) {
        Serial.println("FEHLER: BLE-Scanner konnte nicht angelegt werden.");
        return;
    }
    scan->setScanCallbacks(&scanCallbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(50);
    scan->setMaxResults(maximumScanResults);
    diagnoseReady = true;

    Serial.printf("Lokale BLE-Adresse: %s\n", NimBLEDevice::getAddress().toString().c_str());
    printHelp();
}

void loop() {
    readSerialCommands();
    if (reportQueue != nullptr) printQueuedReports();
    delay(10);
}
