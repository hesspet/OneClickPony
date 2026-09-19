#pragma once

#include <stdint.h>

#include "ModeDevice.h"

namespace pony {

class LedPulseDevice : public ModeDevice {
  public:
    explicit LedPulseDevice(uint32_t pulseMillis);
    bool tick(uint32_t now) override;

  protected:
    void activate(uint32_t now) override;

  private:
    uint32_t pulseMillis_;
    uint32_t pulseStartedAt_ = 0;
};

} // namespace pony