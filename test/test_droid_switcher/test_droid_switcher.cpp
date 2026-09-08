#include <unity.h>

#include "droid_switcher.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

// Configurable fake so tests can force a failure at any step and verify
// the orchestration stops there instead of calling subsequent steps.
class FakeTransport : public XbeeTransport {
 public:
  bool leaveResult = true;
  bool setPanResult = true;
  bool rejoinResult = true;

  bool leaveCalled = false;
  bool setPanCalled = false;
  bool rejoinCalled = false;
  const char *lastPanId = nullptr;

  bool leaveNetwork() override {
    leaveCalled = true;
    return leaveResult;
  }
  bool setPanId(const char *panId) override {
    setPanCalled = true;
    lastPanId = panId;
    return setPanResult;
  }
  bool rejoinNetwork() override {
    rejoinCalled = true;
    return rejoinResult;
  }
};

}  // namespace

void test_switch_succeeds_when_every_step_succeeds() {
  FakeTransport transport;
  TEST_ASSERT_TRUE(DroidSwitchResult::kSuccess ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.leaveCalled);
  TEST_ASSERT_TRUE(transport.setPanCalled);
  TEST_ASSERT_TRUE(transport.rejoinCalled);
  TEST_ASSERT_EQUAL_STRING("1111111111111111", transport.lastPanId);
}

void test_switch_with_null_transport_fails_without_crashing() {
  TEST_ASSERT_TRUE(DroidSwitchResult::kNoTransport ==
                    DroidSwitcher::switchTo("1111111111111111", nullptr));
}

void test_switch_stops_after_leave_failure() {
  FakeTransport transport;
  transport.leaveResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kLeaveFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.leaveCalled);
  TEST_ASSERT_FALSE(transport.setPanCalled);
  TEST_ASSERT_FALSE(transport.rejoinCalled);
}

void test_switch_stops_after_set_pan_failure() {
  FakeTransport transport;
  transport.setPanResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kSetPanFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.setPanCalled);
  TEST_ASSERT_FALSE(transport.rejoinCalled);
}

void test_switch_reports_rejoin_failure() {
  FakeTransport transport;
  transport.rejoinResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kRejoinFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
}

// XbeeControl itself (the real XbeeTransport, using XbeeSpi) is
// hardware-dependent now and excluded from native builds — see
// platformio.ini. Nothing here instantiates it directly; DroidSwitcher is
// tested purely against FakeTransport above.

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_switch_succeeds_when_every_step_succeeds);
  RUN_TEST(test_switch_with_null_transport_fails_without_crashing);
  RUN_TEST(test_switch_stops_after_leave_failure);
  RUN_TEST(test_switch_stops_after_set_pan_failure);
  RUN_TEST(test_switch_reports_rejoin_failure);
  return UNITY_END();
}
