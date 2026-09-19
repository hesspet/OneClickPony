#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <stddef.h>
#include <string.h>

#include "ErrorCodes.h"
#include "ProductConfig.h"

namespace {

using namespace product_config;

constexpr uint32_t bindingMagic = 0x594E4F50;
constexpr uint16_t bindingSchema = 2;
constexpr uint8_t bindingFlagDeleted = 0x01;
constexpr size_t maxUuidLength = 36;
constexpr size_t maxReportReferenceSize = 8;
constexpr size_t maxEndpoints = 8;
constexpr size_t eventQueueLength = 12;
constexpr uint32_t buttonDebounceMillis = 30;

enum class AppState : uint8_t {
    Unbound,
    PairScan,
    PairConnect,
    PairSettle,
    PairLearn,
    PairConfirm,
    Connecting,
    Ready,
    ReconnectWait,
    Error,
};

enum class RuntimeStatus : uint8_t { Starting, BleReady, Ready, Fatal };

enum class EventType : uint8_t { ScanDone, Disconnected, Report };

struct Event {
    EventType type;
    uint32_t connectionGeneration;
    uint32_t scanSession;
    int scanReason;
    bool requestedDisconnect;
    uint16_t endpoint;
    uint8_t length;
    uint8_t data[pony::maxReportSize];
};

struct __attribute__((packed)) BindingRecord {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    uint32_t sequence;
    uint8_t identity[6]; // Persisted exactly as NimBLE ble_addr_t.val (LSB-first).
    uint8_t identityType;
    uint8_t flags;
    uint8_t reportReferenceLength;
    uint8_t pressLength;
    uint8_t releaseLength;
    char serviceUuid[maxUuidLength + 1];
    char characteristicUuid[maxUuidLength + 1];
    uint8_t reportReference[maxReportReferenceSize];
    uint8_t press[pony::maxReportSize];
    uint8_t release[pony::maxReportSize];
    uint32_t crc;
};

struct Endpoint {
    NimBLERemoteCharacteristic* characteristic = nullptr;
    uint16_t handle = 0;
    char serviceUuid[maxUuidLength + 1]{};
    char characteristicUuid[maxUuidLength + 1]{};
    uint8_t reportReferenceLength = 0;
    uint8_t reportReference[maxReportReferenceSize]{};
};

static_assert(sizeof(BindingRecord) < 256, "BindingRecord must stay small");

U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, displaySclPin, displaySdaPin);
pony::OutputController output({outputMode, pulseDurationMillis});
pony::ReportLearner learner;
Preferences preferences;
QueueHandle_t eventQueue = nullptr;
NimBLEScan* scanner = nullptr;
NimBLEClient* client = nullptr;
BindingRecord binding{};
Endpoint endpoints[maxEndpoints];
NimBLEAddress pairIdentity;
size_t endpointCount = 0;
AppState state = AppState::Unbound;
RuntimeStatus runtimeStatus = RuntimeStatus::Starting;
String commandBuffer;
String errorDetail;
product_error::Code errorCode = product_error::Code::None;
String displayText;
uint32_t activeGeneration = 1;
volatile uint32_t callbackGeneration = 0;
uint32_t activeScanSession = 0;
volatile uint32_t completedScanSession = 0;
uint32_t stateStartedAt = 0;
uint32_t nextActionAt = 0;
uint32_t transientUntil = 0;
bool bindingLoaded = false;
bool preferencesReady = false;
bool displayReady = false;
bool physicalOutputActive = false;
bool reconnectPending = false;
volatile bool callbackOverflow = false;
volatile bool bondStoreRejected = false;
volatile bool clientDisconnectPending = false;

bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
    return static_cast<uint32_t>(now - since) >= duration;
}

bool due(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool validRecord(const BindingRecord& record) {
    if (record.magic != bindingMagic || record.schema != bindingSchema || record.size != sizeof(BindingRecord)) return false;
    if ((record.flags & ~bindingFlagDeleted) != 0) return false;
    if (record.crc != crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(BindingRecord, crc))) return false;
    if ((record.flags & bindingFlagDeleted) != 0) return true;
    if (record.pressLength == 0 || record.pressLength > pony::maxReportSize ||
        record.releaseLength == 0 || record.releaseLength > pony::maxReportSize ||
        record.reportReferenceLength > maxReportReferenceSize) return false;
    if (record.serviceUuid[0] == '\0' || record.characteristicUuid[0] == '\0' ||
        record.serviceUuid[maxUuidLength] != '\0' || record.characteristicUuid[maxUuidLength] != '\0') return false;
    return true;
}

bool loadSlot(const char* key, BindingRecord& record) {
    if (!preferences.isKey(key)) return false;
    if (preferences.getBytesLength(key) != sizeof(record)) return false;
    return preferences.getBytes(key, &record, sizeof(record)) == sizeof(record) && validRecord(record);
}

bool latestRecord(BindingRecord& record, bool& firstValid, bool& secondValid) {
    BindingRecord first{};
    BindingRecord second{};
    firstValid = loadSlot("slot0", first);
    secondValid = loadSlot("slot1", second);
    if (!firstValid && !secondValid) return false;
    if (firstValid && (!secondValid || static_cast<int32_t>(first.sequence - second.sequence) > 0)) record = first;
    else record = second;
    return true;
}

bool loadBinding() {
    preferencesReady = preferences.begin("pony-bind", false);
    if (!preferencesReady) return false;
    bool firstValid = false;
    bool secondValid = false;
    BindingRecord newest{};
    if (!latestRecord(newest, firstValid, secondValid) || (newest.flags & bindingFlagDeleted) != 0) return false;
    binding = newest;
    return true;
}

bool persistRecord(BindingRecord record, BindingRecord& verification) {
    BindingRecord first{};
    BindingRecord second{};
    const bool firstValid = loadSlot("slot0", first);
    const bool secondValid = loadSlot("slot1", second);
    uint32_t newestSequence = 0;
    if (firstValid) newestSequence = first.sequence;
    if (secondValid && (!firstValid || static_cast<int32_t>(second.sequence - newestSequence) > 0)) {
        newestSequence = second.sequence;
    }
    record.magic = bindingMagic;
    record.schema = bindingSchema;
    record.size = sizeof(BindingRecord);
    record.sequence = (firstValid || secondValid) ? newestSequence + 1 : 1;
    record.crc = crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(BindingRecord, crc));
    const char* target = nullptr;
    if (!firstValid) target = "slot0";
    else if (!secondValid) target = "slot1";
    else target = static_cast<int32_t>(first.sequence - second.sequence) > 0 ? "slot1" : "slot0";
    if (preferences.putBytes(target, &record, sizeof(record)) != sizeof(record)) return false;
    if (!loadSlot(target, verification) || verification.sequence != record.sequence) return false;
    return true;
}

bool saveBinding(BindingRecord record) {
    record.flags = 0;
    BindingRecord verification{};
    if (!persistRecord(record, verification)) return false;
    binding = verification;
    bindingLoaded = true;
    return true;
}

bool saveDeletedBinding() {
    BindingRecord tombstone{};
    tombstone.flags = bindingFlagDeleted;
    BindingRecord verification{};
    if (!persistRecord(tombstone, verification) || (verification.flags & bindingFlagDeleted) == 0) return false;
    memset(&binding, 0, sizeof(binding));
    bindingLoaded = false;
    return true;
}

NimBLEAddress bindingAddress() {
    ble_addr_t nativeAddress{};
    nativeAddress.type = binding.identityType;
    memcpy(nativeAddress.val, binding.identity, sizeof(nativeAddress.val));
    return NimBLEAddress(nativeAddress);
}

void renderScreen() {
    if (!displayReady) return;
    display.clearBuffer();
    if (output.isActive() && state == AppState::Ready) {
        display.setFont(u8g2_font_10x20_tf);
        const char* onText = "ON";
        const int onWidth = display.getStrWidth(onText);
        display.drawStr((72 - onWidth) / 2, 27, onText);
        display.sendBuffer();
        return;
    }
    display.setFont(u8g2_font_5x7_tf);
    display.setFontMode(1);
    const uint8_t textWidth = display.getStrWidth(displayText.c_str());
    const uint8_t x = (72 - textWidth) / 2;
    if (state != AppState::Ready) {
        display.drawStr(x, 23, displayText.c_str());
    } else {
        display.drawBox(0, 0, 72, 40);
        display.setDrawColor(0);
        display.drawStr(x, 23, displayText.c_str());
        display.setDrawColor(1);
    }
    display.sendBuffer();
}

void show(const char* text) {
    if (displayText == text) return;
    displayText = text;
    if (!displayReady) return;
    renderScreen();
}

void showTransient(const char* text, uint32_t now) {
    show(text);
    transientUntil = now + 500;
}

void showPairConfirmation(const NimBLEAddress& identity) {
    const std::string text = identity.toString();
    const char* suffix = text.size() >= 5 ? text.c_str() + text.size() - 5 : text.c_str();
    displayText = "BOOT bestaetigen";
    if (!displayReady) return;
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    char shortIdentity[12]{};
    snprintf(shortIdentity, sizeof(shortIdentity), "ID %s", suffix);
    display.drawStr((72 - display.getStrWidth(shortIdentity)) / 2, 15, shortIdentity);
    display.drawStr((72 - display.getStrWidth("BOOT")) / 2, 29, "BOOT");
    display.sendBuffer();
}

void applyOutput() {
    const bool active = output.isActive();
    if (active == physicalOutputActive) return;
    physicalOutputActive = active;
    gpio_set_level(static_cast<gpio_num_t>(outputPin), pony::physicalOutputLevel(active, outputActiveLow));
    renderScreen();
}

void forceInactive() {
    output.onDisconnected();
    applyOutput();
}

void enqueue(const Event& event) {
    if (eventQueue == nullptr || xQueueSend(eventQueue, &event, 0) != pdTRUE) callbackOverflow = true;
}

void notificationCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool) {
    if (length == 0 || length > pony::maxReportSize) {
        callbackOverflow = true;
        return;
    }
    Event event{};
    event.type = EventType::Report;
    event.connectionGeneration = callbackGeneration;
    event.endpoint = characteristic->getHandle();
    event.length = static_cast<uint8_t>(length);
    memcpy(event.data, data, length);
    enqueue(event);
}

class ProductScanCallbacks final : public NimBLEScanCallbacks {
  public:
    void setSession(uint32_t session) { session_ = session; }

    void onScanEnd(const NimBLEScanResults&, int reason) override {
        Event event{};
        event.type = EventType::ScanDone;
        event.scanSession = session_;
        event.scanReason = reason;
        completedScanSession = session_;
        enqueue(event);
    }

  private:
    volatile uint32_t session_ = 0;
};

class ProductClientCallbacks final : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*, int) override {
        Event event{};
        event.type = EventType::Disconnected;
        event.connectionGeneration = callbackGeneration;
        event.requestedDisconnect = clientDisconnectPending;
        enqueue(event);
    }
};

class ProductDeviceCallbacks final : public NimBLEDeviceCallbacks {
    int onStoreStatus(struct ble_store_status_event*, void*) override {
        bondStoreRejected = true;
        return BLE_HS_ESTORE_CAP;
    }
};

ProductScanCallbacks scanCallbacks;
ProductClientCallbacks clientCallbacks;
ProductDeviceCallbacks deviceCallbacks;

void setState(AppState next, const char* displayState) {
    state = next;
    stateStartedAt = millis();
    transientUntil = 0;
    show(displayState);
}

const char* errorDisplayString() {
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "ERROR:%u", product_error::number(errorCode));
    return buffer;
}

void setError(product_error::Code code) {
    forceInactive();
    reconnectPending = false;
    errorCode = code;
    errorDetail = product_error::text(code);
    setState(AppState::Error, errorDisplayString());
    Serial.printf("Fehler #%u: %s\n", product_error::number(code), errorDetail.c_str());
}

void setFatalError(product_error::Code code) {
    runtimeStatus = RuntimeStatus::Fatal;
    setError(code);
}

bool requireRuntime(const char* operation) {
    if (runtimeStatus == RuntimeStatus::Ready) return true;
    Serial.printf("%s nicht möglich: Firmware nicht vollständig initialisiert.\n", operation);
    return false;
}

void invalidateConnection() {
    ++activeGeneration;
    forceInactive();
    endpointCount = 0;
    if (client != nullptr && client->isConnected()) {
        clientDisconnectPending = true;
        if (!client->disconnect()) {
            clientDisconnectPending = false;
            Serial.printf("Client-Trennung fehlgeschlagen (LastError=%d)\n", client->getLastError());
        }
    }
}

bool copyUuid(char* destination, const NimBLEUUID& uuid) {
    const std::string text = uuid.toString();
    if (text.empty() || text.size() > maxUuidLength) return false;
    memset(destination, 0, maxUuidLength + 1);
    memcpy(destination, text.data(), text.size());
    return true;
}

bool parsePersistedUuid(const char* text, NimBLEUUID& uuid) {
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    uuid = NimBLEUUID(std::string(text));
    return uuid.bitSize() != 0;
}

bool readReportReference(NimBLERemoteCharacteristic* characteristic,
                         uint8_t* destination,
                         uint8_t& length,
                         bool& present,
                         bool& readError) {
    NimBLERemoteDescriptor* descriptor = characteristic->getDescriptor(NimBLEUUID("2908"));
    length = 0;
    present = descriptor != nullptr;
    readError = false;
    if (!present) return true;
    NimBLEAttValue value = descriptor->readValue();
    if (value.size() == 0 || value.size() > maxReportReferenceSize) {
        readError = true;
        return false;
    }
    memcpy(destination, value.data(), value.size());
    length = static_cast<uint8_t>(value.size());
    return true;
}

bool discoverLearningEndpoints() {
    endpointCount = 0;
    bool hasHidReports = false;
    size_t hidReportCount = 0;
    const auto& services = client->getServices(true);
    for (NimBLERemoteService* service : services) {
        const bool hidService = service->getUUID() == NimBLEUUID("1812");
        const auto& characteristics = service->getCharacteristics(true);
        for (NimBLERemoteCharacteristic* characteristic : characteristics) {
            if ((!characteristic->canNotify() && !characteristic->canIndicate()) ||
                (hidService && characteristic->getUUID() != NimBLEUUID("2a4d"))) continue;
            if (hidService) {
                hasHidReports = true;
                ++hidReportCount;
            }
        }
    }

    for (NimBLERemoteService* service : services) {
        const bool hidService = service->getUUID() == NimBLEUUID("1812");
        if (hasHidReports && !hidService) continue;
        const auto& characteristics = service->getCharacteristics();
        for (NimBLERemoteCharacteristic* characteristic : characteristics) {
            if ((!characteristic->canNotify() && !characteristic->canIndicate()) ||
                (hidService && characteristic->getUUID() != NimBLEUUID("2a4d"))) continue;
            Endpoint endpoint{};
            endpoint.characteristic = characteristic;
            endpoint.handle = characteristic->getHandle();
            if (!copyUuid(endpoint.serviceUuid, service->getUUID()) ||
                !copyUuid(endpoint.characteristicUuid, characteristic->getUUID())) return false;
            bool reportReferencePresent = false;
            bool reportReferenceReadError = false;
            if (!readReportReference(characteristic,
                                     endpoint.reportReference,
                                     endpoint.reportReferenceLength,
                                     reportReferencePresent,
                                     reportReferenceReadError)) return false;
            if (hidService && reportReferencePresent &&
                (endpoint.reportReferenceLength != 2 || endpoint.reportReference[1] != 1)) continue;
            if (hidService && !reportReferencePresent && hidReportCount != 1) continue;
            if (characteristic->getDescriptor(NimBLEUUID("2902")) == nullptr) {
                Serial.printf("Lern-Endpunkt 0x%04x abgelehnt: CCCD 0x2902 fehlt.\n",
                              static_cast<unsigned>(endpoint.handle));
                continue;
            }
            if (endpointCount >= maxEndpoints) return false;
            endpoints[endpointCount++] = endpoint;
        }
    }
    if (endpointCount == 0) return false;
    for (size_t index = 0; index < endpointCount; ++index) {
        const bool notifications = endpoints[index].characteristic->canNotify();
        if (!endpoints[index].characteristic->subscribe(notifications, notificationCallback, true)) {
            Serial.printf("Lern-Endpunkt 0x%04x abgelehnt: CCCD-Subscription abgelehnt.\n",
                          static_cast<unsigned>(endpoints[index].handle));
            return false;
        }
    }
    return true;
}

Endpoint* endpointByHandle(uint16_t handle) {
    for (size_t index = 0; index < endpointCount; ++index) {
        if (endpoints[index].handle == handle) return &endpoints[index];
    }
    return nullptr;
}

bool endpointIsUnambiguous(const Endpoint& selected) {
    size_t sameUuidCount = 0;
    size_t sameReferenceCount = 0;
    for (size_t index = 0; index < endpointCount; ++index) {
        if (strcmp(endpoints[index].serviceUuid, selected.serviceUuid) != 0 ||
            strcmp(endpoints[index].characteristicUuid, selected.characteristicUuid) != 0) continue;
        ++sameUuidCount;
        if (selected.reportReferenceLength != 0 &&
            endpoints[index].reportReferenceLength == selected.reportReferenceLength &&
            memcmp(endpoints[index].reportReference, selected.reportReference, selected.reportReferenceLength) == 0) {
            ++sameReferenceCount;
        }
    }
    if (selected.reportReferenceLength == 0) return sameUuidCount == 1;
    return sameReferenceCount == 1;
}

bool reportReferenceMatchesBinding(const uint8_t* reference, uint8_t referenceLength, bool referencePresent) {
    if (binding.reportReferenceLength == 0) return !referencePresent;
    if (!referencePresent || referenceLength != binding.reportReferenceLength) return false;
    return referenceLength == 0 || memcmp(reference, binding.reportReference, referenceLength) == 0;
}

NimBLERemoteCharacteristic* findBoundEndpoint() {
    // Die Zählung spiegelt exakt das Lernen (discoverLearningEndpoints + endpointIsUnambiguous):
    // Nur Endpunkte mit CCCD 0x2902, HID nur Report 2a4d, referenzloser HID-Report nur bei genau einem
    // notifizierenden Report im Dienst. Ein nicht lesbares 2908 gilt wie beim Lernen nicht als Referenz
    // und passt damit ausschließlich zu referenzlosen Bindings.
    NimBLERemoteCharacteristic* match = nullptr;
    size_t sourceCount = 0;
    size_t matchCount = 0;
    NimBLEUUID boundService;
    NimBLEUUID boundCharacteristic;
    if (!parsePersistedUuid(binding.serviceUuid, boundService) ||
        !parsePersistedUuid(binding.characteristicUuid, boundCharacteristic)) return nullptr;
    const bool expectsReference = binding.reportReferenceLength != 0;

    const auto& services = client->getServices(true);
    for (NimBLERemoteService* service : services) {
        if (service->getUUID() != boundService) continue;
        const bool hidService = service->getUUID() == NimBLEUUID("1812");
        const auto& characteristics = service->getCharacteristics(true);

        size_t hidNotifyReportCount = 0;
        if (hidService) {
            for (NimBLERemoteCharacteristic* characteristic : characteristics) {
                if (characteristic->getUUID() != NimBLEUUID("2a4d")) continue;
                if (!characteristic->canNotify() && !characteristic->canIndicate()) continue;
                ++hidNotifyReportCount;
            }
        }

        for (NimBLERemoteCharacteristic* characteristic : characteristics) {
            if (characteristic->getUUID() != boundCharacteristic) continue;
            if (!characteristic->canNotify() && !characteristic->canIndicate()) continue;
            if (characteristic->getDescriptor(NimBLEUUID("2902")) == nullptr) continue;

            uint8_t reference[maxReportReferenceSize]{};
            uint8_t referenceLength = 0;
            bool referencePresent = false;
            bool referenceReadError = false;
            if (!readReportReference(characteristic, reference, referenceLength, referencePresent, referenceReadError)) {
                referencePresent = false;
            }
            if (hidService && referencePresent && (referenceLength != 2 || reference[1] != 1)) continue;
            if (hidService && !referencePresent && hidNotifyReportCount != 1) continue;

            ++sourceCount;
            if (!reportReferenceMatchesBinding(reference, referenceLength, referencePresent)) continue;
            match = characteristic;
            ++matchCount;
        }
    }

    if (expectsReference) return matchCount == 1 ? match : nullptr;
    return sourceCount == 1 ? match : nullptr;
}

void dumpBoundEndpointDiagnostics() {
    NimBLEUUID boundService;
    NimBLEUUID boundCharacteristic;
    if (!parsePersistedUuid(binding.serviceUuid, boundService) ||
        !parsePersistedUuid(binding.characteristicUuid, boundCharacteristic)) {
        Serial.println("  Gespeicherte UUID ungueltig.");
        return;
    }
    Serial.printf("  Binding: Service=%s Char=%s RefLen=%u",
                  binding.serviceUuid,
                  binding.characteristicUuid,
                  static_cast<unsigned>(binding.reportReferenceLength));
    if (binding.reportReferenceLength != 0) {
        Serial.print(" Ref=");
        for (uint8_t index = 0; index < binding.reportReferenceLength; ++index) {
            if (binding.reportReference[index] < 16) Serial.print('0');
            Serial.print(binding.reportReference[index], HEX);
        }
    }
    Serial.println();
    const auto& services = client->getServices();
    Serial.printf("  Gefundene Dienste (%u):", static_cast<unsigned>(services.size()));
    for (const NimBLERemoteService* service : services) {
        Serial.printf(" %s", service->getUUID().toString().c_str());
    }
    Serial.println();
    for (NimBLERemoteService* service : services) {
        if (service->getUUID() != boundService) continue;
        const auto& characteristics = service->getCharacteristics(true);
        Serial.printf("  Dienst %s mit %u Charakteristik(en):",
                      service->getUUID().toString().c_str(),
                      static_cast<unsigned>(characteristics.size()));
        Serial.println();
        for (NimBLERemoteCharacteristic* characteristic : characteristics) {
            Serial.printf("    Handle 0x%04X UUID %s [%s%s] CCCD=%s",
                          characteristic->getHandle(),
                          characteristic->getUUID().toString().c_str(),
                          characteristic->canNotify() ? "notify " : "",
                          characteristic->canIndicate() ? "indicate" : "",
                          characteristic->getDescriptor(NimBLEUUID("2902")) != nullptr ? "ja" : "nein");
            if (characteristic->getUUID() != boundCharacteristic) {
                Serial.println();
                continue;
            }
            uint8_t reference[maxReportReferenceSize]{};
            uint8_t referenceLength = 0;
            bool referencePresent = false;
            bool referenceReadError = false;
            const bool readOk = readReportReference(characteristic,
                                                    reference,
                                                    referenceLength,
                                                    referencePresent,
                                                    referenceReadError);
            Serial.print(" 2908=");
            if (!readOk && referenceReadError) {
                Serial.print("LESEFEHLER");
            } else if (referencePresent) {
                for (uint8_t index = 0; index < referenceLength; ++index) {
                    if (reference[index] < 16) Serial.print('0');
                    Serial.print(reference[index], HEX);
                }
            } else {
                Serial.print("fehlt");
            }
            Serial.print(" -> ");
            Serial.println(reportReferenceMatchesBinding(reference, referenceLength, referencePresent) ? "passt"
                                                                                                      : "kein Match");
        }
        return;
    }
    Serial.printf("  Dienst %s nicht gefunden.\n", binding.serviceUuid);
}

bool pruneBonds(const NimBLEAddress* keep) {
    int count = NimBLEDevice::getNumBonds();
    if (count < 0) return false;
    int index = 0;
    while (index < count) {
        const NimBLEAddress address = NimBLEDevice::getBondedAddress(index);
        if (address.isNull()) return false;
        if (keep != nullptr && address == *keep) {
            ++index;
            continue;
        }
        if (!NimBLEDevice::deleteBond(address)) return false;
        const int remaining = NimBLEDevice::getNumBonds();
        if (remaining != count - 1) return false;
        count = remaining;
    }
    if (keep == nullptr) return count == 0;
    return count == 1 && NimBLEDevice::getBondedAddress(0) == *keep;
}

bool cleanupUnconfirmedBond() {
    if (!pairIdentity.isNull()) {
        const bool confirmed = bindingLoaded && pairIdentity == bindingAddress();
        if (confirmed) {
            pairIdentity = NimBLEAddress{};
            return true;
        }
        if (NimBLEDevice::isBonded(pairIdentity)) {
            if (!NimBLEDevice::deleteBond(pairIdentity) || NimBLEDevice::isBonded(pairIdentity)) return false;
        }
    }
    pairIdentity = NimBLEAddress{};
    return true;
}

bool stopScanBeforeTransition() {
    const uint32_t stoppingSession = activeScanSession;
    const bool wasScanning = scanner != nullptr && scanner->isScanning();
    ++activeScanSession;
    if (wasScanning) {
        if (!scanner->stop()) return false;
        const uint32_t stoppedAt = millis();
        while (completedScanSession != stoppingSession && !elapsed(millis(), stoppedAt, 200)) delay(1);
        if (completedScanSession != stoppingSession || scanner->isScanning()) return false;
    }
    if (eventQueue != nullptr) xQueueReset(eventQueue);
    return true;
}

bool isPairingState() {
    return state == AppState::PairScan || state == AppState::PairConnect || state == AppState::PairSettle ||
           state == AppState::PairLearn || state == AppState::PairConfirm;
}

void scheduleReconnect(const char* displayState = "Suche") {
    nextActionAt = millis() + reconnectDelayMillis;
    reconnectPending = bindingLoaded;
    setState(bindingLoaded ? AppState::ReconnectWait : AppState::Unbound, bindingLoaded ? displayState : "Koppeln");
}

void abortPairing(const char* reason) {
    const bool scanStopped = stopScanBeforeTransition();
    invalidateConnection();
    learner.cancel();
    const bool bondCleaned = cleanupUnconfirmedBond();
    errorDetail = reason;
    Serial.printf("Pairing abgebrochen: %s\n", reason);
    if (!scanStopped || !bondCleaned) {
        setError(scanStopped ? product_error::Code::UnconfirmedBondDeleteFailed : product_error::Code::PairScanAbortStopFailed);
        return;
    }
    scheduleReconnect("Abbruch");
}

void startPairing() {
    if (!requireRuntime("Pairing")) return;
    reconnectPending = false;
    invalidateConnection();
    if (!stopScanBeforeTransition()) {
        setError(product_error::Code::ScanStopBeforePairFailed);
        return;
    }
    if (!cleanupUnconfirmedBond()) {
        setError(product_error::Code::UnconfirmedBondDeleteFailed);
        return;
    }
    NimBLEAddress confirmedIdentity;
    const NimBLEAddress* keep = nullptr;
    if (bindingLoaded) {
        confirmedIdentity = bindingAddress();
        if (!NimBLEDevice::isBonded(confirmedIdentity)) {
            setError(product_error::Code::BindingWithoutBond);
            return;
        }
        keep = &confirmedIdentity;
    }
    if (!pruneBonds(keep)) {
        setError(product_error::Code::OldBondCleanupFailed);
        return;
    }
    learner.reset();
    pairIdentity = NimBLEAddress{};
    bondStoreRejected = false;
    scanner->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
    scanner->clearResults();
    ++activeScanSession;
    scanCallbacks.setSession(activeScanSession);
    setState(AppState::PairScan, "Suche");
    Serial.println("Koppeln: Nur den gewünschten Auslöser einschalten.");
    if (!scanner->start(scanDurationMillis, false, false)) abortPairing("Pairing-Scan konnte nicht starten");
}

const NimBLEAdvertisedDevice* selectPairCandidate(const NimBLEScanResults& results) {
    const NimBLEAdvertisedDevice* onlyConnectable = nullptr;
    const NimBLEAdvertisedDevice* onlyHid = nullptr;
    size_t connectableCount = 0;
    size_t hidCount = 0;
    for (int index = 0; index < results.getCount(); ++index) {
        const NimBLEAdvertisedDevice* device = results.getDevice(index);
        if (device == nullptr || !device->isConnectable()) continue;
        ++connectableCount;
        onlyConnectable = device;
        if (device->isAdvertisingService(NimBLEUUID("1812"))) {
            ++hidCount;
            onlyHid = device;
        }
    }
    if (hidCount == 1) return onlyHid;
    if (hidCount == 0 && connectableCount == 1) return onlyConnectable;
    return nullptr;
}

bool establishPairCandidate(const NimBLEAdvertisedDevice* device) {
    setState(AppState::PairConnect, "Verbinde");
    bondStoreRejected = false;
    if (!client->connect(device, true, false, true)) return false;
    const bool secured = client->secureConnection(false);
    const NimBLEConnInfo info = client->getConnInfo();
    pairIdentity = info.getIdAddress();
    if (!secured || bondStoreRejected || pairIdentity.isNull() || !info.isBonded() ||
        !NimBLEDevice::isBonded(pairIdentity)) return false;
    ++activeGeneration;
    callbackGeneration = activeGeneration;
    if (!discoverLearningEndpoints()) return false;
    setState(AppState::PairSettle, "Verbinde");
    return true;
}

void handlePairScanDone(int reason) {
    if (reason != 0) {
        String detail("Pairing-Scan beendet mit Grund ");
        detail += reason;
        abortPairing(detail.c_str());
        return;
    }
    const NimBLEScanResults results = scanner->getResults();
    const NimBLEAdvertisedDevice* candidate = selectPairCandidate(results);
    if (candidate == nullptr) {
        abortPairing("Kein eindeutiger Pairing-Kandidat");
        return;
    }
    if (clientDisconnectPending) {
        abortPairing("Client vor Pairing-Verbindung noch im Trennen");
        return;
    }
    if (client->isConnected()) {
        abortPairing("Client vor Pairing-Verbindung noch verbunden");
        return;
    }
    if (!establishPairCandidate(candidate)) {
        abortPairing(bondStoreRejected ? "Bond-Speicher voll oder fehlerhaft" : "Kandidat nicht sicher koppelbar");
    }
}

bool establishBoundDevice(const NimBLEAddress& expected) {
    setState(AppState::Connecting, "Verbinde");
    auto fail = [](product_error::Code code, const char* extra = nullptr) {
        errorCode = code;
        errorDetail = product_error::text(code);
        if (extra != nullptr && extra[0] != '\0') {
            errorDetail += " (";
            errorDetail += extra;
            errorDetail += ")";
        }
        Serial.printf("Reconnect: Fehler #%u: %s\n", product_error::number(code), errorDetail.c_str());
        return false;
    };
    if (clientDisconnectPending) return fail(product_error::Code::ReconnectClientDisconnectPending);
    if (client->isConnected()) return fail(product_error::Code::ReconnectClientAlreadyConnected);

    const std::string targetText = expected.toString();
    Serial.printf("Reconnect direkt: Ziel-ID=%s Typ=%u\n",
                  targetText.c_str(),
                  static_cast<unsigned>(expected.getType()));
    ++activeGeneration;
    callbackGeneration = activeGeneration;
    if (!client->connect(expected, true, false, true)) {
        String detail("LastError=");
        detail += client->getLastError();
        return fail(product_error::Code::ReconnectConnectFailed, detail.c_str());
    }
    clientDisconnectPending = false;

    // NimBLE kann bei PINKEY_MISSING den lokalen Schlüssel löschen und einmal neu sichern.
    // Deshalb werden Bond und exakte Identity nach dem Security-Aufruf erneut strikt geprüft.
    if (!client->secureConnection(false)) {
        String detail("LastError=");
        detail += client->getLastError();
        detail += ", lokaler Bond=";
        detail += NimBLEDevice::isBonded(expected) ? "vorhanden" : "fehlt";
        return fail(product_error::Code::ReconnectSecurityFailed, detail.c_str());
    }
    const NimBLEConnInfo info = client->getConnInfo();
    if (!info.isEncrypted()) return fail(product_error::Code::ReconnectLinkNotEncrypted);
    if (!info.isBonded()) return fail(product_error::Code::ReconnectLinkNotBonded);
    const NimBLEAddress actual = info.getIdAddress();
    if (actual != expected) {
        const std::string actualText = actual.toString();
        char detail[180]{};
        snprintf(detail,
                 sizeof(detail),
                 "Peer=%s Typ=%u, gespeichert=%s Typ=%u",
                 actualText.c_str(),
                 static_cast<unsigned>(actual.getType()),
                 targetText.c_str(),
                 static_cast<unsigned>(expected.getType()));
        return fail(product_error::Code::ReconnectPeerIdMismatch, detail);
    }
    if (!NimBLEDevice::isBonded(expected)) return fail(product_error::Code::ReconnectLocalBondMissingAfterSecurity);
    NimBLERemoteCharacteristic* characteristic = findBoundEndpoint();
    if (characteristic == nullptr) {
        Serial.println("Reconnect-Endpunkt-Diagnose:");
        dumpBoundEndpointDiagnostics();
        return fail(product_error::Code::ReconnectEndpointAmbiguous);
    }
    if (characteristic->getDescriptor(NimBLEUUID("2902")) == nullptr) {
        return fail(product_error::Code::ReconnectCccdMissing);
    }
    output.onConnected();
    applyOutput();
    if (characteristic->canRead()) {
        const NimBLEAttValue current = characteristic->readValue();
        const pony::ReportKind currentKind = pony::classifyReport(current.data(),
                                                                  current.size(),
                                                                  binding.press,
                                                                  binding.pressLength,
                                                                  binding.release,
                                                                  binding.releaseLength,
                                                                  output.isArmed());
        if (currentKind == pony::ReportKind::Release) output.onReport(currentKind, millis());
    }
    const bool notifications = characteristic->canNotify();
    if (!characteristic->subscribe(notifications, notificationCallback, true)) {
        forceInactive();
        return fail(product_error::Code::ReconnectSubscriptionRejected);
    }
    setState(AppState::Ready, "Bereit");
    errorDetail = "";
    const std::string identity = actual.toString();
    Serial.printf("Verbunden: ID=%s Typ=%u, verschlüsselt=ja, gebondet=ja\n",
                  identity.c_str(),
                  static_cast<unsigned>(actual.getType()));
    Serial.println(output.isArmed() ? "Gespeicherter Auslöser verbunden und neutral."
                                    : "Gespeicherter Auslöser verbunden. Neutralen Release-Report abwarten.");
    return true;
}

void startBoundReconnect() {
    if (!requireRuntime("Wiederverbindung")) return;
    forceInactive();
    if (!bindingLoaded) {
        reconnectPending = false;
        setState(AppState::Unbound, "Koppeln");
        return;
    }
    if (clientDisconnectPending) return;
    reconnectPending = false;
    const NimBLEAddress identity = bindingAddress();
    if (!NimBLEDevice::isBonded(identity)) {
        errorCode = product_error::Code::ReconnectLocalBondMissing;
        errorDetail = product_error::text(errorCode);
        Serial.printf("Reconnect: Fehler #%u: %s\n", product_error::number(errorCode), errorDetail.c_str());
        scheduleReconnect(errorDisplayString());
        return;
    }
    if (!establishBoundDevice(identity)) {
        invalidateConnection();
        if (clientDisconnectPending) {
            reconnectPending = false;
            setState(AppState::ReconnectWait, errorDisplayString());
        } else {
            scheduleReconnect(errorDisplayString());
        }
    }
}

void cancelPairing() {
    if (!isPairingState()) {
        Serial.println("Kein Pairing aktiv.");
        return;
    }
    abortPairing("Abbruch durch Benutzer");
}

void confirmPairing() {
    if (state != AppState::PairConfirm || !learner.confirm()) {
        Serial.println("Keine Pairing-Bestätigung ausstehend.");
        return;
    }
    Endpoint* selected = endpointByHandle(learner.endpoint());
    if (selected == nullptr || !endpointIsUnambiguous(*selected)) {
        abortPairing("Report-Endpunkt ist nicht eindeutig");
        return;
    }

    BindingRecord record{};
    memcpy(record.identity, pairIdentity.getVal(), sizeof(record.identity));
    record.identityType = pairIdentity.getType();
    memcpy(record.serviceUuid, selected->serviceUuid, sizeof(record.serviceUuid));
    memcpy(record.characteristicUuid, selected->characteristicUuid, sizeof(record.characteristicUuid));
    record.reportReferenceLength = selected->reportReferenceLength;
    memcpy(record.reportReference, selected->reportReference, selected->reportReferenceLength);
    record.pressLength = static_cast<uint8_t>(learner.pressLength());
    record.releaseLength = static_cast<uint8_t>(learner.releaseLength());
    memcpy(record.press, learner.pressData(), learner.pressLength());
    memcpy(record.release, learner.releaseData(), learner.releaseLength());
    if (!saveBinding(record)) {
        abortPairing("Binding konnte nicht atomar gespeichert werden");
        return;
    }
    const NimBLEAddress confirmedIdentity = bindingAddress();
    if (!pruneBonds(&confirmedIdentity)) {
        invalidateConnection();
        setError(product_error::Code::PostConfirmBondCleanupFailed);
        return;
    }
    Serial.println("Zuordnung gespeichert.");
    invalidateConnection();
    pairIdentity = NimBLEAddress{};
    scheduleReconnect("Bereit");
}

void clearBinding() {
    if (!requireRuntime("Löschen")) return;
    if (!stopScanBeforeTransition()) {
        setError(product_error::Code::ScanStopBeforeClearFailed);
        return;
    }
    invalidateConnection();
    if (!cleanupUnconfirmedBond()) {
        setError(product_error::Code::UnconfirmedBondDeleteFailed);
        return;
    }
    const bool hadBinding = bindingLoaded;
    const NimBLEAddress identity = hadBinding ? bindingAddress() : NimBLEAddress{};
    if (!saveDeletedBinding()) {
        setError(product_error::Code::ClearMarkerSaveFailed);
        return;
    }
    if (hadBinding && NimBLEDevice::isBonded(identity)) {
        if (!NimBLEDevice::deleteBond(identity) || NimBLEDevice::isBonded(identity)) {
            setError(product_error::Code::ClearBondDeleteFailed);
            return;
        }
    }
    if (!pruneBonds(nullptr)) {
        setError(product_error::Code::PostClearBondCleanupFailed);
        return;
    }
    setState(AppState::Unbound, "Koppeln");
    Serial.println("Zuordnung und passender Bond gelöscht.");
}

void processPairReport(const Event& event) {
    const pony::LearnResult result = learner.feed(event.endpoint, event.data, event.length);
    if (result == pony::LearnResult::CycleComplete) {
        show("Noch einmal");
    } else if (result == pony::LearnResult::Ready) {
        Endpoint* selected = endpointByHandle(learner.endpoint());
        if (selected == nullptr || !endpointIsUnambiguous(*selected)) {
            abortPairing("Reportquelle nicht eindeutig");
            return;
        }
        setState(AppState::PairConfirm, "BOOT bestaetigen");
        showPairConfirmation(pairIdentity);
        const std::string identity = pairIdentity.toString();
        Serial.printf("Profil gelernt. Identität ...%s; mit kurzem BOOT-Druck oder 'confirm' bestätigen.\n",
                      identity.size() >= 5 ? identity.c_str() + identity.size() - 5 : identity.c_str());
    } else if (result == pony::LearnResult::Rejected) {
        abortPairing("Zweiter Reportzyklus stimmt nicht überein");
    }
}

void processNormalReport(const Event& event, uint32_t now) {
    const pony::ReportKind kind = pony::classifyReport(event.data,
                                                       event.length,
                                                       binding.press,
                                                       binding.pressLength,
                                                       binding.release,
                                                       binding.releaseLength,
                                                       output.isArmed());
    output.onReport(kind, now);
    applyOutput();
    // LED aktiv: Das große "ON" erscheint automatisch über renderScreen/applyOutput.
}

void processEvents(uint32_t now) {
    Event event{};
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
        if (event.type == EventType::ScanDone) {
            if (event.scanSession != activeScanSession) continue;
            if (state == AppState::PairScan) handlePairScanDone(event.scanReason);
            continue;
        }
        if (event.type == EventType::Disconnected && event.requestedDisconnect) {
            clientDisconnectPending = false;
            forceInactive();
            if (!isPairingState() && bindingLoaded && runtimeStatus == RuntimeStatus::Ready) scheduleReconnect();
            continue;
        }
        if (event.connectionGeneration != activeGeneration) continue;
        if (event.type == EventType::Disconnected) {
            ++activeGeneration;
            forceInactive();
            if (isPairingState()) {
                abortPairing("Verbindung beim Lernen getrennt");
            } else {
                scheduleReconnect();
            }
        } else if (event.type == EventType::Report) {
            if (state == AppState::PairLearn) processPairReport(event);
            else if (state == AppState::Ready) processNormalReport(event, now);
        }
    }
}

class BootButton {
  public:
    void begin() {
        raw_ = digitalRead(bootButtonPin);
        stable_ = raw_;
        armed_ = stable_ == HIGH;
        changedAt_ = millis();
    }

    bool released(uint32_t now, uint32_t& heldMillis) {
        const bool current = digitalRead(bootButtonPin);
        if (current != raw_) {
            raw_ = current;
            changedAt_ = now;
        }
        if (current == stable_ || !elapsed(now, changedAt_, buttonDebounceMillis)) return false;
        stable_ = current;
        if (stable_ == HIGH) {
            if (!armed_) {
                armed_ = true;
                return false;
            }
            heldMillis = static_cast<uint32_t>(now - pressedAt_);
            return true;
        }
        if (armed_) pressedAt_ = now;
        return false;
    }

  private:
    bool raw_ = HIGH;
    bool stable_ = HIGH;
    bool armed_ = false;
    uint32_t changedAt_ = 0;
    uint32_t pressedAt_ = 0;
};

BootButton bootButton;

void handleButton(uint32_t now) {
    uint32_t held = 0;
    if (!bootButton.released(now, held)) return;
    switch (pony::classifyButtonHold(held, pairingHoldMinMillis, pairingHoldMaxMillis, clearHoldMinMillis)) {
        case pony::ButtonGesture::Clear: clearBinding(); break;
        case pony::ButtonGesture::Pair: startPairing(); break;
        case pony::ButtonGesture::Short:
            if (state == AppState::PairConfirm) confirmPairing();
            else if (isPairingState()) cancelPairing();
            break;
        case pony::ButtonGesture::None: break;
    }
}

const char* stateName() {
    switch (state) {
        case AppState::Unbound: return "ungebunden";
        case AppState::PairScan: return "Pairing-Suche";
        case AppState::PairConnect: return "Pairing-Verbindung";
        case AppState::PairSettle: return "Pairing-Beruhigung";
        case AppState::PairLearn: return "Lernen";
        case AppState::PairConfirm: return "Bestätigung";
        case AppState::Connecting: return "Verbindung";
        case AppState::Ready: return "bereit";
        case AppState::ReconnectWait: return "Wiederverbindung";
        case AppState::Error: return "Fehler";
    }
    return "unbekannt";
}

void printHelp() {
    Serial.println("Befehle: help, status, pair, confirm, cancel, clear");
}

void printStatus() {
    const bool runtimeReady = runtimeStatus == RuntimeStatus::Ready;
    const bool linkConnected = client != nullptr && client->isConnected();
    Serial.printf("Firmware %s, Runtime=%s, Zustand=%s, Zuordnung=%s, Ausgang=%s\n",
                  firmwareVersion,
                  runtimeReady ? "bereit" : runtimeStatus == RuntimeStatus::Fatal ? "fatal" : "Start",
                  stateName(),
                  bindingLoaded ? "ja" : "nein",
                  output.isActive() ? "aktiv" : "inaktiv");
    Serial.printf("Bond lokal: %s\n",
                  runtimeReady && bindingLoaded && NimBLEDevice::isBonded(bindingAddress()) ? "ja" : "nein");
    Serial.printf("Link verbunden: %s\n", linkConnected ? "ja" : "nein");
    if (linkConnected) {
        const NimBLEConnInfo info = client->getConnInfo();
        const NimBLEAddress identity = info.getIdAddress();
        const std::string identityText = identity.toString();
        Serial.printf("Link verschlüsselt: %s\n", info.isEncrypted() ? "ja" : "nein");
        Serial.printf("Link gebondet: %s\n", info.isBonded() ? "ja" : "nein");
        Serial.printf("Peer-ID: %s Typ=%u\n",
                      identityText.c_str(),
                      static_cast<unsigned>(identity.getType()));
    } else {
        Serial.println("Link verschlüsselt: nein");
        Serial.println("Link gebondet: nein");
        Serial.println("Peer-ID: nicht verbunden");
    }
    if (errorDetail.length() != 0) Serial.printf("Letzter Fehler: %s\n", errorDetail.c_str());
}

void processCommand(String command) {
    command.trim();
    command.toLowerCase();
    if (command == "help") printHelp();
    else if (command == "status") printStatus();
    else if (command == "pair") startPairing();
    else if (command == "confirm") confirmPairing();
    else if (command == "cancel") cancelPairing();
    else if (command == "clear") clearBinding();
    else if (command.length() != 0) Serial.println("Unbekannter Befehl. 'help' zeigt die Befehle.");
}

void readSerialCommands() {
    while (Serial.available() > 0) {
        const char input = static_cast<char>(Serial.read());
        if (input == '\r') continue;
        if (input == '\n') {
            processCommand(commandBuffer);
            commandBuffer = "";
        } else if (commandBuffer.length() < 80) {
            commandBuffer += input;
        }
    }
}

void updateState(uint32_t now) {
    if (state == AppState::PairSettle && elapsed(now, stateStartedAt, learningSettlingMillis)) {
        xQueueReset(eventQueue);
        if (client == nullptr || !client->isConnected()) {
            abortPairing("Verbindung während Beruhigungsphase getrennt");
            return;
        }
        learner.reset();
        setState(AppState::PairLearn, "Knopf druecken");
        Serial.println("Jetzt den Zielknopf drücken; Press und Release werden über den Nutzerzeitpunkt gelernt.");
        return;
    }
    if ((state == AppState::PairLearn && elapsed(now, stateStartedAt, learningTimeoutMillis)) ||
        (state == AppState::PairConfirm && elapsed(now, stateStartedAt, confirmationTimeoutMillis))) {
        abortPairing("Pairing-Zeitüberschreitung");
        return;
    }
    if (state == AppState::ReconnectWait && bindingLoaded && reconnectPending && !clientDisconnectPending &&
        due(now, nextActionAt)) {
        startBoundReconnect();
    }
    if (transientUntil != 0 && due(now, transientUntil)) {
        transientUntil = 0;
        show("Bereit");
    }
}

} // namespace

void setup() {
    // GPIO8/9 sind Strapping-Pins. Die Anwendung wertet BOOT erst nach einem normalen Start aus.
    const gpio_num_t outputGpio = static_cast<gpio_num_t>(outputPin);
    const esp_err_t outputLatchResult = gpio_set_level(outputGpio, pony::physicalOutputLevel(false, outputActiveLow));
    const esp_err_t outputModeResult = gpio_set_direction(outputGpio, GPIO_MODE_OUTPUT);
    pinMode(bootButtonPin, INPUT_PULLUP);
    bootButton.begin();

    Serial.begin(serialBaudRate);
    const uint32_t serialWaitStarted = millis();
    while (!Serial && !elapsed(millis(), serialWaitStarted, 1500)) delay(10);
    Serial.printf("\n%s %s\n", firmwareName, firmwareVersion);
    if (outputLatchResult != ESP_OK || outputModeResult != ESP_OK) {
        runtimeStatus = RuntimeStatus::Fatal;
        Serial.println("Fehler: GPIO8 konnte nicht sicher initialisiert werden.");
        return;
    }

    Wire.begin(displaySdaPin, displaySclPin);
    display.begin();
    displayReady = true;
    show("Koppeln");

    eventQueue = xQueueCreate(eventQueueLength, sizeof(Event));
    if (eventQueue == nullptr) {
        setFatalError(product_error::Code::EventQueueAllocationFailed);
        return;
    }
    NimBLEDevice::setDeviceCallbacks(&deviceCallbacks);
    if (!NimBLEDevice::init("OneKlickPony")) {
        setFatalError(product_error::Code::BleInitFailed);
        return;
    }
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setMTU(128);

    scanner = NimBLEDevice::getScan();
    client = NimBLEDevice::createClient();
    if (scanner == nullptr || client == nullptr) {
        setFatalError(product_error::Code::BleClientSetupFailed);
        return;
    }
    scanner->setScanCallbacks(&scanCallbacks, false);
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(60);
    scanner->setMaxResults(maximumScanResults);
    client->setClientCallbacks(&clientCallbacks, false);
    client->setConnectTimeout(connectTimeoutMillis);
    runtimeStatus = RuntimeStatus::BleReady;

    bindingLoaded = loadBinding();
    if (!preferencesReady) {
        setFatalError(product_error::Code::NvsOpenFailed);
        return;
    }
    runtimeStatus = RuntimeStatus::Ready;
    printHelp();
    NimBLEAddress confirmedIdentity;
    const NimBLEAddress* keep = nullptr;
    if (bindingLoaded) {
        confirmedIdentity = bindingAddress();
        if (!NimBLEDevice::isBonded(confirmedIdentity)) {
            setError(product_error::Code::BindingWithoutBond);
            return;
        }
        keep = &confirmedIdentity;
    }
    if (!pruneBonds(keep)) {
        setError(product_error::Code::BondCleanupAtStartFailed);
        return;
    }

    if (bindingLoaded) scheduleReconnect();
    else startPairing();
}

void loop() {
    const uint32_t now = millis();
    readSerialCommands();
    handleButton(now);

    if (callbackOverflow) {
        const bool resumePairing = !bindingLoaded || isPairingState();
        callbackOverflow = false;
        invalidateConnection();
        xQueueReset(eventQueue);
        setError(product_error::Code::EventQueueOverflow);
        if (resumePairing) {
            startPairing();
        } else {
            scheduleReconnect(errorDisplayString());
        }
    }
    if (eventQueue != nullptr) processEvents(now);
    const bool pulseEnded = output.tick(now);
    applyOutput();
    if (pulseEnded && state == AppState::Ready) {
        transientUntil = 0;
        show("Bereit");
    }
    updateState(millis());
    delay(2);
}
