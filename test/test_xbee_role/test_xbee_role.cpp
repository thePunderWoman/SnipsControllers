#include <unity.h>

#include "xbee_role.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

class FakeRoleTransport : public XbeeRoleTransport {
 public:
  bool queryResult = true;
  uint8_t coordinatorEnable = 0;
  bool setResult = true;

  bool setCalled = false;
  uint8_t lastSetValue = 0xFF;

  bool queryCoordinatorEnable(uint8_t *outValue) override {
    if (!queryResult) return false;
    *outValue = coordinatorEnable;
    return true;
  }
  bool setCoordinatorEnable(uint8_t value) override {
    setCalled = true;
    lastSetValue = value;
    return setResult;
  }
};

}  // namespace

void test_router_is_left_alone() {
  FakeRoleTransport transport;
  transport.coordinatorEnable = 0;
  TEST_ASSERT_TRUE(XbeeRoleResult::kAlreadyRouter ==
                    ensureRouterRole(&transport));
  TEST_ASSERT_FALSE(transport.setCalled);
}

void test_coordinator_is_switched_to_router() {
  FakeRoleTransport transport;
  transport.coordinatorEnable = 1;
  TEST_ASSERT_TRUE(XbeeRoleResult::kSwitchedToRouter ==
                    ensureRouterRole(&transport));
  TEST_ASSERT_TRUE(transport.setCalled);
  TEST_ASSERT_EQUAL_UINT8(0, transport.lastSetValue);
}

void test_any_nonzero_ce_is_switched_to_router() {
  FakeRoleTransport transport;
  transport.coordinatorEnable = 2;
  TEST_ASSERT_TRUE(XbeeRoleResult::kSwitchedToRouter ==
                    ensureRouterRole(&transport));
  TEST_ASSERT_EQUAL_UINT8(0, transport.lastSetValue);
}

void test_query_failure_does_not_write() {
  FakeRoleTransport transport;
  transport.queryResult = false;
  TEST_ASSERT_TRUE(XbeeRoleResult::kQueryFailed ==
                    ensureRouterRole(&transport));
  TEST_ASSERT_FALSE(transport.setCalled);
}

void test_set_failure_is_reported() {
  FakeRoleTransport transport;
  transport.coordinatorEnable = 1;
  transport.setResult = false;
  TEST_ASSERT_TRUE(XbeeRoleResult::kSetFailed == ensureRouterRole(&transport));
}

void test_null_transport_fails_without_crashing() {
  TEST_ASSERT_TRUE(XbeeRoleResult::kNoTransport == ensureRouterRole(nullptr));
}

// XbeeControl itself (the real XbeeRoleTransport, using XbeeSpi) is
// hardware-dependent and excluded from native builds — see platformio.ini.

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_router_is_left_alone);
  RUN_TEST(test_coordinator_is_switched_to_router);
  RUN_TEST(test_any_nonzero_ce_is_switched_to_router);
  RUN_TEST(test_query_failure_does_not_write);
  RUN_TEST(test_set_failure_is_reported);
  RUN_TEST(test_null_transport_fails_without_crashing);
  return UNITY_END();
}
