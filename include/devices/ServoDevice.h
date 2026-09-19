#pragma once

#include "ModeDevice.h"

namespace pony {

class ServoDevice : public ModeDevice {
  protected:
    void activate(uint32_t now) override;
};

} // namespace pony