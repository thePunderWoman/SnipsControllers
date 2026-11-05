#include <unity.h>

#include <cstring>
#include <vector>

#include "xbee_api.h"
#include "xbee_frame.h"

// A link whose incoming frames are queued by the test and whose outgoing
// frames are recorded.
class FakeLink : public XbeeLink {
 public:
  std::vector<std::vector<uint8_t>> incoming;
  std::vector<std::vector<uint8_t>> written;
  int badFrames = 0;  // returned as 0 before the queued frames

  bool frameAvailable() override { return badFrames > 0 || !incoming.empty(); }
  int32_t readFrame(uint8_t *buf, uint16_t maxLen) override {
    if (badFrames > 0) {
      --badFrames;
      return 0;
    }
    if (incoming.empty()) return -1;
    std::vector<uint8_t> f = incoming.front();
    incoming.erase(incoming.begin());
    if (f.size() > maxLen) return 0;
    std::memcpy(buf, f.data(), f.size());
    return static_cast<int32_t>(f.size());
  }
  void writeFrame(const uint8_t *data, uint16_t length) override {
    written.emplace_back(data, data + length);
  }
};

static unsigned long gNow = 0;
static int gIdles = 0;
unsigned long fakeClock() { return gNow; }
void fakeIdle() {
  ++gIdles;
  gNow += 10;  // time passes while waiting
}

void setUp(void) {
  gNow = 0;
  gIdles = 0;
}
void tearDown(void) {}

std::vector<uint8_t> atResponse(uint8_t frameId, const char *cmd, uint8_t status,
                                std::vector<uint8_t> value = {}) {
  std::vector<uint8_t> f = {XbeeFrame::kFrameTypeAtCommandResponse, frameId,
                            static_cast<uint8_t>(cmd[0]), static_cast<uint8_t>(cmd[1]),
                            status};
  f.insert(f.end(), value.begin(), value.end());
  return f;
}

std::vector<uint8_t> receivePacket(std::vector<uint8_t> payload) {
  std::vector<uint8_t> f = {XbeeFrame::kFrameTypeReceivePacket, 0, 0, 0, 0, 0, 0, 0, 0,
                            0xFF, 0xFE, 0x01};
  f.insert(f.end(), payload.begin(), payload.end());
  return f;
}

// ---- AT commands ------------------------------------------------------------

void test_at_command_writes_request_and_returns_value() {
  FakeLink link;
  link.incoming.push_back(atResponse(0x01, "SL", 0, {0x41, 0x42, 0x43, 0x44}));
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t value[4];
  uint8_t length = 0;
  TEST_ASSERT_TRUE(api.sendAtCommand("SL", nullptr, 0, value, sizeof(value), &length));
  TEST_ASSERT_EQUAL_UINT8(4, length);
  TEST_ASSERT_EQUAL_HEX8(0x44, value[3]);
  TEST_ASSERT_EQUAL(1, (int)link.written.size());
  TEST_ASSERT_EQUAL_HEX8(XbeeFrame::kFrameTypeAtCommand, link.written[0][0]);
  TEST_ASSERT_EQUAL_HEX8('S', link.written[0][2]);
}

void test_at_command_skips_other_frames_and_ids() {
  FakeLink link;
  link.badFrames = 1;
  link.incoming.push_back(receivePacket({1, 2}));
  link.incoming.push_back(atResponse(0x07, "SL", 0, {9}));  // someone else's
  link.incoming.push_back(atResponse(0x01, "AP", 0, {1}));
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t value = 0;
  TEST_ASSERT_TRUE(api.sendAtCommand("AP", nullptr, 0, &value, 1, nullptr));
  TEST_ASSERT_EQUAL_UINT8(1, value);
}

void test_at_command_error_status_fails() {
  FakeLink link;
  link.incoming.push_back(atResponse(0x01, "ID", 2));
  XbeeApi api(&link, fakeClock, fakeIdle);
  TEST_ASSERT_FALSE(api.sendAtCommand("ID", nullptr, 0, nullptr, 0, nullptr));
}

void test_at_command_times_out_with_no_reply() {
  FakeLink link;
  XbeeApi api(&link, fakeClock, fakeIdle);
  TEST_ASSERT_FALSE(api.sendAtCommand("ID", nullptr, 0, nullptr, 0, nullptr, 200));
  TEST_ASSERT_TRUE(gIdles >= 20);
}

void test_at_command_truncates_value_to_capacity() {
  FakeLink link;
  link.incoming.push_back(atResponse(0x01, "ID", 0, {1, 2, 3, 4}));
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t value[2];
  uint8_t length = 0;
  TEST_ASSERT_TRUE(api.sendAtCommand("ID", nullptr, 0, value, sizeof(value), &length));
  TEST_ASSERT_EQUAL_UINT8(2, length);
}

// ---- packets ------------------------------------------------------------------

void test_send_packet_wraps_payload_in_transmit_request() {
  FakeLink link;
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t payload[17] = {};  // today's uplink size
  api.sendPacket(payload, sizeof(payload));
  TEST_ASSERT_EQUAL(1, (int)link.written.size());
  TEST_ASSERT_EQUAL_HEX8(XbeeFrame::kFrameTypeTransmitRequest, link.written[0][0]);
  TEST_ASSERT_EQUAL(14 + 17, (int)link.written[0].size());
}

void test_send_packet_drops_oversized_payload() {
  FakeLink link;
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t payload[64] = {};
  api.sendPacket(payload, sizeof(payload));
  TEST_ASSERT_EQUAL(0, (int)link.written.size());
}

void test_poll_returns_receive_packet_payload() {
  FakeLink link;
  link.incoming.push_back(receivePacket({0x10, 0x20, 0x30}));
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t out[8];
  uint16_t length = 0;
  TEST_ASSERT_TRUE(api.pollForPacket(out, sizeof(out), &length));
  TEST_ASSERT_EQUAL_UINT16(3, length);
  TEST_ASSERT_EQUAL_HEX8(0x30, out[2]);
}

void test_poll_ignores_other_frames_and_empty_link() {
  FakeLink link;
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t out[8];
  uint16_t length = 0;
  TEST_ASSERT_FALSE(api.pollForPacket(out, sizeof(out), &length));
  link.incoming.push_back(atResponse(0x01, "AP", 0, {1}));
  TEST_ASSERT_FALSE(api.pollForPacket(out, sizeof(out), &length));
  link.badFrames = 1;
  TEST_ASSERT_FALSE(api.pollForPacket(out, sizeof(out), &length));
}

void test_poll_truncates_to_capacity() {
  FakeLink link;
  link.incoming.push_back(receivePacket({1, 2, 3, 4, 5}));
  XbeeApi api(&link, fakeClock, fakeIdle);
  uint8_t out[2];
  uint16_t length = 0;
  TEST_ASSERT_TRUE(api.pollForPacket(out, sizeof(out), &length));
  TEST_ASSERT_EQUAL_UINT16(2, length);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_at_command_writes_request_and_returns_value);
  RUN_TEST(test_at_command_skips_other_frames_and_ids);
  RUN_TEST(test_at_command_error_status_fails);
  RUN_TEST(test_at_command_times_out_with_no_reply);
  RUN_TEST(test_at_command_truncates_value_to_capacity);
  RUN_TEST(test_send_packet_wraps_payload_in_transmit_request);
  RUN_TEST(test_send_packet_drops_oversized_payload);
  RUN_TEST(test_poll_returns_receive_packet_payload);
  RUN_TEST(test_poll_ignores_other_frames_and_empty_link);
  RUN_TEST(test_poll_truncates_to_capacity);
  return UNITY_END();
}
