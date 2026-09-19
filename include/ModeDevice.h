#pragma once

#include <stdint.h>

#include "ProductCore.h"

namespace pony {

enum class FuncMode : uint8_t { LedToggle = 0, LedPulse = 1, Servo = 2 };

constexpr uint32_t pulseMinMillis = 1;
constexpr uint32_t pulseMaxMillis = 60000;

struct ModeConfig {
    FuncMode mode = FuncMode::LedToggle;
    uint32_t param0 = 0;
};

class ModeDevice {
  public:
    virtual ~ModeDevice() = default;
    void onConnected();
    void onDisconnected();
    void onReport(ReportKind kind, uint32_t now);
    virtual bool tick(uint32_t now);
    virtual bool isActive() const;
    bool isConnected() const;
    void setConnected(bool connected);
    void setArmed(bool armed);
    bool isArmed() const;

  protected:
    virtual void activate(uint32_t now);
    bool connected_ = false;
    bool armed_ = false;
    bool active_ = false;
};

class OutputController {
  public:
    explicit OutputController(ModeConfig config);
    ~OutputController();
    void onConnected();
    void onDisconnected();
    void onReport(ReportKind kind, uint32_t now);
    bool tick(uint32_t now);
    void setConfig(const ModeConfig& config);
    FuncMode mode() const;
    ModeConfig config() const;
    bool isActive() const;
    bool isArmed() const;

  private:
    ModeConfig config_;
    ModeDevice* device_ = nullptr;
};

ModeDevice* createModeDevice(const ModeConfig& config);

const char* funcModeDisplayName(FuncMode mode);
bool parseFuncMode(const char* name, FuncMode& mode);
bool parseModeParam(FuncMode mode, const char* params, uint32_t& param0);

} // namespace pony