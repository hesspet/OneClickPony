#include "devices/LedToggleDevice.h"

namespace pony {

void LedToggleDevice::activate(uint32_t now) {
    active_ = !active_;
}

} // namespace pony