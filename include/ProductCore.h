#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pony {

constexpr size_t maxReportSize = 20;

enum class OutputMode : uint8_t { Toggle, Pulse };
enum class ReportKind : uint8_t { Unknown, Press, Release, Repeat };
enum class ButtonGesture : uint8_t { None, Short, Pair, Clear };

struct OutputConfig {
    OutputMode mode;
    uint32_t pulseDurationMillis;
};

class OutputController {
  public:
    explicit OutputController(OutputConfig config);

    void onConnected();
    void onDisconnected();
    void onReport(ReportKind kind, uint32_t now);
    bool tick(uint32_t now);

    bool isActive() const { return active_; }
    bool isArmed() const { return armed_; }

  private:
    OutputConfig config_;
    uint32_t pulseStartedAt_ = 0;
    bool connected_ = false;
    bool active_ = false;
    bool armed_ = false;
};

enum class LearnStage : uint8_t {
    FirstPress,
    FirstRelease,
    SecondPress,
    SecondRelease,
    AwaitConfirmation,
    Complete,
    Failed,
};

enum class LearnResult : uint8_t { Ignored, Press, Repeat, Release, CycleComplete, Ready, Rejected };

class ReportLearner {
  public:
    void reset();
    LearnResult feed(uint16_t endpoint, const uint8_t* data, size_t length);
    bool confirm();
    void cancel();

    LearnStage stage() const { return stage_; }
    uint16_t endpoint() const { return endpoint_; }
    size_t pressLength() const { return pressLength_; }
    size_t releaseLength() const { return releaseLength_; }
    const uint8_t* pressData() const { return press_; }
    const uint8_t* releaseData() const { return release_; }

  private:
    bool equals(const uint8_t* expected, size_t expectedLength, const uint8_t* data, size_t length) const;

    LearnStage stage_ = LearnStage::FirstPress;
    uint16_t endpoint_ = 0;
    size_t pressLength_ = 0;
    size_t releaseLength_ = 0;
    uint8_t press_[maxReportSize]{};
    uint8_t release_[maxReportSize]{};
};

ReportKind classifyReport(const uint8_t* data,
                          size_t length,
                          const uint8_t* press,
                          size_t pressLength,
                          const uint8_t* release,
                          size_t releaseLength,
                          bool armed);

bool physicalOutputLevel(bool active, bool activeLow);
ButtonGesture classifyButtonHold(uint32_t heldMillis,
                                 uint32_t pairingMinMillis,
                                 uint32_t pairingMaxMillis,
                                 uint32_t clearMinMillis);

} // namespace pony
