#include "ProductCore.h"

#include <string.h>

namespace pony {

void ReportLearner::reset() {
    stage_ = LearnStage::FirstPress;
    endpoint_ = 0;
    pressLength_ = 0;
    releaseLength_ = 0;
    memset(press_, 0, sizeof(press_));
    memset(release_, 0, sizeof(release_));
}

bool ReportLearner::equals(const uint8_t* expected,
                           size_t expectedLength,
                           const uint8_t* data,
                           size_t length) const {
    return expectedLength == length && memcmp(expected, data, length) == 0;
}

LearnResult ReportLearner::feed(uint16_t endpoint, const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0 || length > maxReportSize ||
        stage_ == LearnStage::AwaitConfirmation || stage_ == LearnStage::Complete || stage_ == LearnStage::Failed) {
        return LearnResult::Ignored;
    }

    if (stage_ == LearnStage::FirstPress) {
        endpoint_ = endpoint;
        pressLength_ = length;
        memcpy(press_, data, length);
        stage_ = LearnStage::FirstRelease;
        return LearnResult::Press;
    }
    if (endpoint != endpoint_) {
        stage_ = LearnStage::Failed;
        return LearnResult::Rejected;
    }

    if (stage_ == LearnStage::FirstRelease) {
        if (equals(press_, pressLength_, data, length)) return LearnResult::Repeat;
        releaseLength_ = length;
        memcpy(release_, data, length);
        stage_ = LearnStage::SecondPress;
        return LearnResult::CycleComplete;
    }
    if (stage_ == LearnStage::SecondPress) {
        if (equals(release_, releaseLength_, data, length)) return LearnResult::Repeat;
        if (!equals(press_, pressLength_, data, length)) {
            stage_ = LearnStage::Failed;
            return LearnResult::Rejected;
        }
        stage_ = LearnStage::SecondRelease;
        return LearnResult::Press;
    }
    if (stage_ == LearnStage::SecondRelease) {
        if (equals(press_, pressLength_, data, length)) return LearnResult::Repeat;
        if (!equals(release_, releaseLength_, data, length)) {
            stage_ = LearnStage::Failed;
            return LearnResult::Rejected;
        }
        stage_ = LearnStage::AwaitConfirmation;
        return LearnResult::Ready;
    }
    return LearnResult::Ignored;
}

bool ReportLearner::confirm() {
    if (stage_ != LearnStage::AwaitConfirmation) return false;
    stage_ = LearnStage::Complete;
    return true;
}

void ReportLearner::cancel() {
    stage_ = LearnStage::Failed;
}

ReportKind classifyReport(const uint8_t* data,
                          size_t length,
                          const uint8_t* press,
                          size_t pressLength,
                          const uint8_t* release,
                          size_t releaseLength,
                          bool armed) {
    if (data == nullptr || length == 0) return ReportKind::Unknown;
    if (length == pressLength && memcmp(data, press, length) == 0) {
        return armed ? ReportKind::Press : ReportKind::Repeat;
    }
    if (length == releaseLength && memcmp(data, release, length) == 0) return ReportKind::Release;
    return ReportKind::Unknown;
}

bool physicalOutputLevel(bool active, bool activeLow) {
    return activeLow ? !active : active;
}

ButtonGesture classifyButtonHold(uint32_t heldMillis,
                                 uint32_t pairingMinMillis,
                                 uint32_t pairingMaxMillis,
                                 uint32_t clearMinMillis) {
    if (heldMillis >= clearMinMillis) return ButtonGesture::Clear;
    if (heldMillis >= pairingMinMillis && heldMillis <= pairingMaxMillis) return ButtonGesture::Pair;
    if (heldMillis < pairingMinMillis) return ButtonGesture::Short;
    return ButtonGesture::None;
}

} // namespace pony
