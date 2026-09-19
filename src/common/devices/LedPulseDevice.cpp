#include "devices/LedPulseDevice.h"

namespace pony {

LedPulseDevice::LedPulseDevice(uint32_t pulseMillis) : pulseMillis_(pulseMillis) {}

bool LedPulseDevice::tick(uint32_t now) {
    if (active_ && static_cast<uint32_t>(now - pulseStartedAt_) >= pulseMillis_) {
        active_ = false;
        return true;
    }
    return false;
}

void LedPulseDevice::activate(uint32_t now) {
    if (!active_) {
        active_ = true;
        pulseStartedAt_ = now;
    }
}

} // namespace pony