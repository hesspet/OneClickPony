#include "ModeDevice.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "devices/LedPulseDevice.h"
#include "devices/LedToggleDevice.h"
#include "devices/ServoDevice.h"

namespace pony {

void ModeDevice::onConnected() {
    connected_ = true;
    active_ = false;
    armed_ = false;
}

void ModeDevice::onDisconnected() {
    connected_ = false;
    active_ = false;
    armed_ = false;
}

void ModeDevice::onReport(ReportKind kind, uint32_t now) {
    if (!connected_) return;
    if (kind == ReportKind::Release) {
        armed_ = true;
        return;
    }
    if (kind != ReportKind::Press || !armed_) return;

    armed_ = false;
    activate(now);
}

bool ModeDevice::tick(uint32_t now) {
    return false;
}

bool ModeDevice::isActive() const {
    return active_;
}

bool ModeDevice::isConnected() const {
    return connected_;
}

void ModeDevice::setConnected(bool connected) {
    connected_ = connected;
}

void ModeDevice::setArmed(bool armed) {
    armed_ = armed;
}

bool ModeDevice::isArmed() const {
    return armed_;
}

void ModeDevice::activate(uint32_t now) {}

OutputController::OutputController(ModeConfig config) : config_(config) {
    device_ = createModeDevice(config_);
}

OutputController::~OutputController() {
    delete device_;
}

void OutputController::setConfig(const ModeConfig& config) {
    const bool wasConnected = device_->isConnected();
    const bool wasArmed = device_->isArmed();
    ModeDevice* next = createModeDevice(config);
    next->setConnected(wasConnected);
    next->setArmed(wasArmed);
    delete device_;
    device_ = next;
    config_ = config;
}

void OutputController::onConnected() {
    device_->onConnected();
}

void OutputController::onDisconnected() {
    device_->onDisconnected();
}

void OutputController::onReport(ReportKind kind, uint32_t now) {
    device_->onReport(kind, now);
}

bool OutputController::tick(uint32_t now) {
    return device_->tick(now);
}

FuncMode OutputController::mode() const {
    return config_.mode;
}

ModeConfig OutputController::config() const {
    return config_;
}

bool OutputController::isActive() const {
    return device_->isActive();
}

bool OutputController::isArmed() const {
    return device_->isArmed();
}

ModeDevice* createModeDevice(const ModeConfig& config) {
    switch (config.mode) {
        case FuncMode::LedPulse: return new LedPulseDevice(config.param0);
        case FuncMode::Servo: return new ServoDevice;
        case FuncMode::LedToggle: return new LedToggleDevice;
    }
    return new LedToggleDevice;
}

const char* funcModeDisplayName(FuncMode mode) {
    switch (mode) {
        case FuncMode::LedToggle: return "Toggle";
        case FuncMode::LedPulse: return "Impuls";
        case FuncMode::Servo: return "Servo";
    }
    return "Toggle";
}

bool parseFuncMode(const char* name, FuncMode& mode) {
    if (name == nullptr || name[0] == '\0') return false;
    char buffer[10];
    size_t index = 0;
    for (; name[index] != '\0' && index < sizeof(buffer) - 1; ++index) {
        buffer[index] = static_cast<char>(tolower(static_cast<unsigned char>(name[index])));
    }
    if (name[index] != '\0') return false;
    buffer[index] = '\0';
    if (strcmp(buffer, "ledtoggle") == 0 || strcmp(buffer, "toggle") == 0) {
        mode = FuncMode::LedToggle;
        return true;
    }
    if (strcmp(buffer, "ledpulse") == 0 || strcmp(buffer, "pulse") == 0) {
        mode = FuncMode::LedPulse;
        return true;
    }
    if (strcmp(buffer, "servo") == 0) {
        mode = FuncMode::Servo;
        return true;
    }
    return false;
}

bool parseModeParam(FuncMode mode, const char* params, uint32_t& param0) {
    if (params == nullptr) return true;
    while (*params == ' ' || *params == ',') ++params;
    if (*params == '\0') return true;
    if (mode != FuncMode::LedPulse) return false;
    char* end = nullptr;
    const unsigned long value = strtoul(params, &end, 10);
    if (end == params || *end != '\0') return false;
    if (value < pulseMinMillis || value > pulseMaxMillis) return false;
    param0 = static_cast<uint32_t>(value);
    return true;
}

} // namespace pony