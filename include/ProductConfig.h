#pragma once

#include <stdint.h>

#include "ProductCore.h"

namespace product_config {

constexpr const char* firmwareName = "OneKlickPony";
constexpr const char* firmwareVersion = "0.3.0";
constexpr uint32_t serialBaudRate = 115200;

constexpr uint8_t displaySdaPin = 5;
constexpr uint8_t displaySclPin = 6;
constexpr uint8_t bootButtonPin = 9;
constexpr uint8_t outputPin = 8;

constexpr pony::OutputMode outputMode = pony::OutputMode::Pulse;
constexpr bool outputActiveLow = true;
constexpr uint32_t pulseDurationMillis = 1000;

constexpr uint32_t pairingHoldMinMillis = 3000;
constexpr uint32_t pairingHoldMaxMillis = 9000;
constexpr uint32_t clearHoldMinMillis = 10000;
constexpr uint32_t scanDurationMillis = 8000;
constexpr uint32_t learningTimeoutMillis = 30000;
constexpr uint32_t learningSettlingMillis = 400;
constexpr uint32_t confirmationTimeoutMillis = 20000;
constexpr uint32_t reconnectDelayMillis = 2000;
constexpr uint32_t connectTimeoutMillis = 10000;
constexpr uint8_t maximumScanResults = 24;

static_assert(displaySdaPin <= 21 && displaySclPin <= 21 && bootButtonPin <= 21 && outputPin <= 21,
              "Pins must be valid ESP32-C3 GPIO numbers");
static_assert(displaySdaPin != displaySclPin && displaySdaPin != bootButtonPin && displaySdaPin != outputPin &&
                  displaySclPin != bootButtonPin && displaySclPin != outputPin && bootButtonPin != outputPin,
              "Product pins must be distinct");
static_assert(maximumScanResults > 0, "At least one scan result is required");
static_assert(scanDurationMillis > 0 && learningTimeoutMillis > 0 && learningSettlingMillis > 0 &&
                   confirmationTimeoutMillis > 0 && reconnectDelayMillis > 0 && connectTimeoutMillis == 10000,
              "Scan, learning and reconnect times must be positive");
static_assert(pulseDurationMillis > 0, "Pulse duration must be positive");
static_assert(pairingHoldMinMillis > 0 && pairingHoldMinMillis <= pairingHoldMaxMillis &&
                  pairingHoldMaxMillis < clearHoldMinMillis,
              "BOOT hold thresholds must not overlap");

} // namespace product_config
