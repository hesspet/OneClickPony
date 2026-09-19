#pragma once

#include "ModeDevice.h"

namespace pony {

class LedToggleDevice : public ModeDevice {
  protected:
    void activate(uint32_t now) override;
};

} // namespace pony