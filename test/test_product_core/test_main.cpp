#include <unity.h>

#include <stdint.h>

#include "ModeDevice.h"

using pony::LearnResult;
using pony::LearnStage;
using pony::ButtonGesture;
using pony::FuncMode;
using pony::ModeConfig;
using pony::OutputController;
using pony::ReportKind;
using pony::ReportLearner;

void test_toggle_press_repeat_release() {
    OutputController output(ModeConfig{FuncMode::LedToggle, 250});
    output.onConnected();
    TEST_ASSERT_FALSE(output.isActive());
    TEST_ASSERT_FALSE(output.isArmed());

    output.onReport(ReportKind::Press, 10);
    TEST_ASSERT_FALSE(output.isActive());
    output.onReport(ReportKind::Release, 20);
    output.onReport(ReportKind::Press, 30);
    TEST_ASSERT_TRUE(output.isActive());
    output.onReport(ReportKind::Repeat, 40);
    output.onReport(ReportKind::Press, 50);
    TEST_ASSERT_TRUE(output.isActive());
    output.onReport(ReportKind::Release, 60);
    output.onReport(ReportKind::Press, 70);
    TEST_ASSERT_FALSE(output.isActive());
}

void test_pulse_duration_and_press_while_active() {
    OutputController output(ModeConfig{FuncMode::LedPulse, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, 900);
    output.onReport(ReportKind::Press, 1000);
    TEST_ASSERT_TRUE(output.isActive());
    output.tick(1249);
    TEST_ASSERT_TRUE(output.isActive());

    output.onReport(ReportKind::Release, 1200);
    output.onReport(ReportKind::Press, 1220);
    TEST_ASSERT_TRUE(output.isActive());
    output.tick(1250);
    TEST_ASSERT_FALSE(output.isActive());
}

void test_disconnect_is_safe_and_requires_release() {
    OutputController output(ModeConfig{FuncMode::LedToggle, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, 1);
    output.onReport(ReportKind::Press, 2);
    TEST_ASSERT_TRUE(output.isActive());
    output.onDisconnected();
    TEST_ASSERT_FALSE(output.isActive());
    output.onConnected();
    output.onReport(ReportKind::Unknown, 3);
    output.onReport(ReportKind::Press, 3);
    TEST_ASSERT_FALSE(output.isActive());
    TEST_ASSERT_FALSE(output.isArmed());
    output.onReport(ReportKind::Release, 4);
    TEST_ASSERT_TRUE(output.isArmed());
    output.onReport(ReportKind::Press, 5);
    TEST_ASSERT_TRUE(output.isActive());
}

void test_unknown_report_does_nothing() {
    OutputController output(ModeConfig{FuncMode::LedToggle, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, 1);
    output.onReport(ReportKind::Unknown, 2);
    TEST_ASSERT_FALSE(output.isActive());
    TEST_ASSERT_TRUE(output.isArmed());
}

void test_pulse_millis_wraparound() {
    OutputController output(ModeConfig{FuncMode::LedPulse, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, UINT32_MAX - 200);
    output.onReport(ReportKind::Press, UINT32_MAX - 100);
    output.tick(100);
    TEST_ASSERT_TRUE(output.isActive());
    output.tick(149);
    TEST_ASSERT_FALSE(output.isActive());
}

void test_pulse_tick_reports_deactivation_once() {
    OutputController output(ModeConfig{FuncMode::LedPulse, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, 1);
    output.onReport(ReportKind::Press, 10);
    TEST_ASSERT_FALSE(output.tick(259));
    TEST_ASSERT_TRUE(output.tick(260));
    TEST_ASSERT_FALSE(output.tick(261));
}

void test_learning_two_cycles_and_confirmation() {
    ReportLearner learner;
    learner.reset();
    const uint8_t press[] = {0x01, 0x20};
    const uint8_t release[] = {0x00, 0x00};
    TEST_ASSERT_EQUAL(LearnResult::Press, learner.feed(42, press, sizeof(press)));
    TEST_ASSERT_EQUAL(LearnResult::Repeat, learner.feed(42, press, sizeof(press)));
    TEST_ASSERT_EQUAL(LearnResult::CycleComplete, learner.feed(42, release, sizeof(release)));
    TEST_ASSERT_EQUAL(LearnResult::Repeat, learner.feed(42, release, sizeof(release)));
    TEST_ASSERT_EQUAL(LearnResult::Press, learner.feed(42, press, sizeof(press)));
    TEST_ASSERT_EQUAL(LearnResult::Ready, learner.feed(42, release, sizeof(release)));
    TEST_ASSERT_EQUAL(LearnStage::AwaitConfirmation, learner.stage());
    TEST_ASSERT_TRUE(learner.confirm());
    TEST_ASSERT_EQUAL(LearnStage::Complete, learner.stage());
}

void test_learning_rejects_mismatched_second_cycle() {
    ReportLearner learner;
    learner.reset();
    const uint8_t press[] = {0x01};
    const uint8_t release[] = {0x00};
    const uint8_t other[] = {0x02};
    learner.feed(7, press, sizeof(press));
    learner.feed(7, release, sizeof(release));
    TEST_ASSERT_EQUAL(LearnResult::Rejected, learner.feed(7, other, sizeof(other)));
    TEST_ASSERT_FALSE(learner.confirm());
}

void test_learning_rejects_second_endpoint() {
    ReportLearner learner;
    learner.reset();
    const uint8_t press[] = {0x01};
    const uint8_t other[] = {0x00};
    learner.feed(7, press, sizeof(press));
    TEST_ASSERT_EQUAL(LearnResult::Rejected, learner.feed(8, other, sizeof(other)));
    TEST_ASSERT_EQUAL(LearnStage::Failed, learner.stage());
}

void test_learning_cancel_blocks_confirmation() {
    ReportLearner learner;
    learner.reset();
    learner.cancel();
    TEST_ASSERT_EQUAL(LearnStage::Failed, learner.stage());
    TEST_ASSERT_FALSE(learner.confirm());
}

void test_output_polarities() {
    TEST_ASSERT_FALSE(pony::physicalOutputLevel(false, false));
    TEST_ASSERT_TRUE(pony::physicalOutputLevel(true, false));
    TEST_ASSERT_TRUE(pony::physicalOutputLevel(false, true));
    TEST_ASSERT_FALSE(pony::physicalOutputLevel(true, true));
}

void test_button_hold_boundaries() {
    TEST_ASSERT_EQUAL(ButtonGesture::Short, pony::classifyButtonHold(2999, 3000, 9000, 10000));
    TEST_ASSERT_EQUAL(ButtonGesture::Pair, pony::classifyButtonHold(3000, 3000, 9000, 10000));
    TEST_ASSERT_EQUAL(ButtonGesture::Pair, pony::classifyButtonHold(9000, 3000, 9000, 10000));
    TEST_ASSERT_EQUAL(ButtonGesture::None, pony::classifyButtonHold(9001, 3000, 9000, 10000));
    TEST_ASSERT_EQUAL(ButtonGesture::None, pony::classifyButtonHold(9999, 3000, 9000, 10000));
    TEST_ASSERT_EQUAL(ButtonGesture::Clear, pony::classifyButtonHold(10000, 3000, 9000, 10000));
}

void test_set_config_switches_and_resets_active() {
    OutputController output(ModeConfig{FuncMode::LedToggle, 250});
    output.onConnected();
    TEST_ASSERT_EQUAL(FuncMode::LedToggle, output.mode());
    output.onReport(ReportKind::Release, 1);
    output.onReport(ReportKind::Press, 2);
    TEST_ASSERT_TRUE(output.isActive());
    output.setConfig(ModeConfig{FuncMode::LedPulse, 250});
    TEST_ASSERT_EQUAL(FuncMode::LedPulse, output.mode());
    TEST_ASSERT_FALSE(output.isActive());
    output.onReport(ReportKind::Release, 3);
    output.onReport(ReportKind::Press, 4);
    TEST_ASSERT_TRUE(output.isActive());
    output.tick(254);
    TEST_ASSERT_FALSE(output.isActive());
}

void test_set_config_keeps_armed() {
    OutputController output(ModeConfig{FuncMode::LedPulse, 250});
    output.onConnected();
    output.onReport(ReportKind::Release, 1);
    TEST_ASSERT_TRUE(output.isArmed());
    output.setConfig(ModeConfig{FuncMode::LedToggle, 250});
    TEST_ASSERT_TRUE(output.isArmed());
    output.onReport(ReportKind::Press, 2);
    TEST_ASSERT_TRUE(output.isActive());
}

void test_parse_mode_tokens() {
    pony::FuncMode mode{};
    TEST_ASSERT_TRUE(pony::parseFuncMode("ledtoggle", mode));
    TEST_ASSERT_EQUAL(pony::FuncMode::LedToggle, mode);
    TEST_ASSERT_TRUE(pony::parseFuncMode("toggle", mode));
    TEST_ASSERT_EQUAL(pony::FuncMode::LedToggle, mode);
    TEST_ASSERT_TRUE(pony::parseFuncMode("LedPulse", mode));
    TEST_ASSERT_EQUAL(pony::FuncMode::LedPulse, mode);
    TEST_ASSERT_TRUE(pony::parseFuncMode("pulse", mode));
    TEST_ASSERT_EQUAL(pony::FuncMode::LedPulse, mode);
    TEST_ASSERT_TRUE(pony::parseFuncMode("servo", mode));
    TEST_ASSERT_EQUAL(pony::FuncMode::Servo, mode);
    TEST_ASSERT_FALSE(pony::parseFuncMode("led", mode));
    TEST_ASSERT_FALSE(pony::parseFuncMode("", mode));
}

void test_parse_ledpulse_param() {
    uint32_t param = 1000;
    TEST_ASSERT_TRUE(pony::parseModeParam(pony::FuncMode::LedPulse, "750", param));
    TEST_ASSERT_EQUAL_UINT32(750, param);
    TEST_ASSERT_TRUE(pony::parseModeParam(pony::FuncMode::LedPulse, "", param));
    TEST_ASSERT_EQUAL_UINT32(750, param);
    TEST_ASSERT_TRUE(pony::parseModeParam(pony::FuncMode::LedPulse, ",500", param));
    TEST_ASSERT_EQUAL_UINT32(500, param);
    TEST_ASSERT_FALSE(pony::parseModeParam(pony::FuncMode::LedPulse, "abc", param));
    TEST_ASSERT_FALSE(pony::parseModeParam(pony::FuncMode::LedPulse, "0", param));
    TEST_ASSERT_FALSE(pony::parseModeParam(pony::FuncMode::LedPulse, "60001", param));
    TEST_ASSERT_FALSE(pony::parseModeParam(pony::FuncMode::LedToggle, "500", param));
    TEST_ASSERT_TRUE(pony::parseModeParam(pony::FuncMode::LedToggle, "", param));
}

void test_servo_dummy_never_activates() {
    OutputController output(ModeConfig{FuncMode::Servo, 0});
    output.onConnected();
    output.onReport(ReportKind::Release, 1);
    TEST_ASSERT_TRUE(output.isArmed());
    output.onReport(ReportKind::Press, 2);
    TEST_ASSERT_FALSE(output.isActive());
    TEST_ASSERT_FALSE(output.isArmed());
    TEST_ASSERT_FALSE(output.tick(1000));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_toggle_press_repeat_release);
    RUN_TEST(test_pulse_duration_and_press_while_active);
    RUN_TEST(test_disconnect_is_safe_and_requires_release);
    RUN_TEST(test_unknown_report_does_nothing);
    RUN_TEST(test_pulse_millis_wraparound);
    RUN_TEST(test_pulse_tick_reports_deactivation_once);
    RUN_TEST(test_learning_two_cycles_and_confirmation);
    RUN_TEST(test_learning_rejects_mismatched_second_cycle);
    RUN_TEST(test_learning_rejects_second_endpoint);
    RUN_TEST(test_learning_cancel_blocks_confirmation);
    RUN_TEST(test_output_polarities);
    RUN_TEST(test_button_hold_boundaries);
    RUN_TEST(test_set_config_switches_and_resets_active);
    RUN_TEST(test_set_config_keeps_armed);
    RUN_TEST(test_parse_mode_tokens);
    RUN_TEST(test_parse_ledpulse_param);
    RUN_TEST(test_servo_dummy_never_activates);
    return UNITY_END();
}
