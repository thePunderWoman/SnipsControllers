#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "xbee_role.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

class FakeRoleTransport : public XbeeRoleTransport {
 public:
  // Current module values, starting as already configured.
  uint8_t ce = XbeeRole::kCoordinatorEnable;
  uint8_t sm = XbeeRole::kSleepMode;
  uint8_t et = XbeeRole::kEndDeviceTimeout;

  std::string failQueryOf;  // command whose query fails, if any
  std::string failWriteOf;  // command whose write fails, if any
  bool commitResult = true;

  std::vector<std::string> writes;
  int commitCount = 0;

  bool queryByteParameter(const char *command, uint8_t *outValue) override {
    if (failQueryOf == command) return false;
    uint8_t *slot = slotFor(command);
    if (slot == nullptr) return false;
    *outValue = *slot;
    return true;
  }

  bool writeByteParameter(const char *command, uint8_t value) override {
    writes.push_back(std::string(command) + "=" + std::to_string(value));
    if (failWriteOf == command) return false;
    uint8_t *slot = slotFor(command);
    if (slot == nullptr) return false;
    *slot = value;
    return true;
  }

  bool commitAndApply() override {
    ++commitCount;
    return commitResult;
  }

 private:
  uint8_t *slotFor(const char *command) {
    if (std::strcmp(command, "CE") == 0) return &ce;
    if (std::strcmp(command, "SM") == 0) return &sm;
    if (std::strcmp(command, "ET") == 0) return &et;
    return nullptr;
  }
};

}  // namespace

void test_role_constants_match_sleepy_end_device() {
  TEST_ASSERT_EQUAL_UINT8(0, XbeeRole::kCoordinatorEnable);
  TEST_ASSERT_EQUAL_UINT8(1, XbeeRole::kSleepMode);
  TEST_ASSERT_EQUAL_UINT8(4, XbeeRole::kEndDeviceTimeout);
}

void test_configured_module_is_left_alone() {
  FakeRoleTransport transport;
  TEST_ASSERT_TRUE(XbeeRoleResult::kAlreadyConfigured ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(0, transport.writes.size());
  TEST_ASSERT_EQUAL(0, transport.commitCount);
}

void test_factory_router_gets_sleep_mode_and_timeout_with_one_commit() {
  // A factory-default module that a previous firmware already set to
  // CE=0: SM=0 (never sleeps, router) and ET=1 (2 minutes).
  FakeRoleTransport transport;
  transport.sm = 0;
  transport.et = 1;
  TEST_ASSERT_TRUE(XbeeRoleResult::kReconfigured ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(2, transport.writes.size());
  TEST_ASSERT_EQUAL_STRING("SM=1", transport.writes[0].c_str());
  TEST_ASSERT_EQUAL_STRING("ET=4", transport.writes[1].c_str());
  TEST_ASSERT_EQUAL(1, transport.commitCount);
}

void test_coordinator_is_switched_to_join() {
  FakeRoleTransport transport;
  transport.ce = 1;
  TEST_ASSERT_TRUE(XbeeRoleResult::kReconfigured ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(1, transport.writes.size());
  TEST_ASSERT_EQUAL_STRING("CE=0", transport.writes[0].c_str());
  TEST_ASSERT_EQUAL(1, transport.commitCount);
}

void test_any_wrong_value_is_rewritten() {
  FakeRoleTransport transport;
  transport.ce = 2;
  transport.sm = 4;  // cyclic sleep
  transport.et = 0;
  TEST_ASSERT_TRUE(XbeeRoleResult::kReconfigured ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(3, transport.writes.size());
  TEST_ASSERT_EQUAL_UINT8(0, transport.ce);
  TEST_ASSERT_EQUAL_UINT8(1, transport.sm);
  TEST_ASSERT_EQUAL_UINT8(4, transport.et);
}

void test_query_failure_writes_nothing() {
  // The last query fails after earlier ones show a mismatch: still no
  // writes, so the module is never left half-configured.
  FakeRoleTransport transport;
  transport.ce = 1;
  transport.failQueryOf = "ET";
  TEST_ASSERT_TRUE(XbeeRoleResult::kQueryFailed ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(0, transport.writes.size());
  TEST_ASSERT_EQUAL(0, transport.commitCount);
}

void test_write_failure_is_reported_without_commit() {
  FakeRoleTransport transport;
  transport.sm = 0;
  transport.et = 1;
  transport.failWriteOf = "SM";
  TEST_ASSERT_TRUE(XbeeRoleResult::kSetFailed ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(0, transport.commitCount);
}

void test_commit_failure_is_reported() {
  FakeRoleTransport transport;
  transport.sm = 0;
  transport.commitResult = false;
  TEST_ASSERT_TRUE(XbeeRoleResult::kSetFailed ==
                   ensureSleepyEndDeviceRole(&transport));
  TEST_ASSERT_EQUAL(1, transport.commitCount);
}

void test_null_transport_fails_without_crashing() {
  TEST_ASSERT_TRUE(XbeeRoleResult::kNoTransport ==
                   ensureSleepyEndDeviceRole(nullptr));
}

// XbeeControl itself (the real XbeeRoleTransport, using XbeeSpi) is
// hardware-dependent and excluded from native builds — see platformio.ini.

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_role_constants_match_sleepy_end_device);
  RUN_TEST(test_configured_module_is_left_alone);
  RUN_TEST(test_factory_router_gets_sleep_mode_and_timeout_with_one_commit);
  RUN_TEST(test_coordinator_is_switched_to_join);
  RUN_TEST(test_any_wrong_value_is_rewritten);
  RUN_TEST(test_query_failure_writes_nothing);
  RUN_TEST(test_write_failure_is_reported_without_commit);
  RUN_TEST(test_commit_failure_is_reported);
  RUN_TEST(test_null_transport_fails_without_crashing);
  return UNITY_END();
}
