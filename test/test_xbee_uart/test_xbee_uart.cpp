#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "xbee_frame.h"
#include "xbee_frame_reader.h"
#include "xbee_uart_setup.h"

void setUp(void) {}
void tearDown(void) {}

// ---- frame reader -------------------------------------------------------------

std::vector<uint8_t> wire(std::vector<uint8_t> data) {
  std::vector<uint8_t> out = {0x7E, static_cast<uint8_t>(data.size() >> 8),
                              static_cast<uint8_t>(data.size() & 0xFF)};
  out.insert(out.end(), data.begin(), data.end());
  out.push_back(XbeeFrame::computeChecksum(data.data(), static_cast<uint16_t>(data.size())));
  return out;
}

XbeeFrameReader::Result feedAll(XbeeFrameReader *r, const std::vector<uint8_t> &bytes) {
  XbeeFrameReader::Result last = XbeeFrameReader::Result::kNeedMore;
  for (uint8_t b : bytes) {
    last = r->feed(b);
    if (last != XbeeFrameReader::Result::kNeedMore) return last;
  }
  return last;
}

void test_reader_assembles_frame_after_noise() {
  XbeeFrameReader r;
  std::vector<uint8_t> bytes = {0x00, 0xFF, 0x13};
  auto frame = wire({0x88, 0x01, 'A', 'P', 0x00, 0x7E});  // 0x7E inside is data
  bytes.insert(bytes.end(), frame.begin(), frame.end());
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kFrame == feedAll(&r, bytes));
  TEST_ASSERT_EQUAL_UINT16(6, r.length());
  TEST_ASSERT_EQUAL_HEX8(0x7E, r.frame()[5]);
}

void test_reader_rejects_bad_checksum_then_recovers() {
  XbeeFrameReader r;
  auto bad = wire({0x90, 0x01});
  bad.back() ^= 0xFF;
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kBadFrame == feedAll(&r, bad));
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kFrame == feedAll(&r, wire({0x90, 0x02})));
}

void test_reader_rejects_impossible_lengths() {
  XbeeFrameReader r;
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kBadFrame == feedAll(&r, {0x7E, 0x00, 0x00}));
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kBadFrame == feedAll(&r, {0x7E, 0x01, 0x00}));
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kFrame == feedAll(&r, wire({0x01})));
}

void test_reader_reset_drops_partial_frame() {
  XbeeFrameReader r;
  feedAll(&r, {0x7E, 0x00, 0x05, 0x01});
  r.reset();
  TEST_ASSERT_TRUE(XbeeFrameReader::Result::kFrame == feedAll(&r, wire({0x42})));
}

// ---- setup sequence -------------------------------------------------------------

// A pretend XBee: answers API frames only in API mode at its baud, and
// command mode only in transparent mode at its baud. Records what it was
// asked.
class FakePort : public XbeeUartPort {
 public:
  uint32_t moduleBaud = 9600;
  bool moduleApi = false;
  bool present = true;
  bool failWrite = false;
  uint32_t baud = 0;
  bool inCommandMode = false;
  std::vector<std::string> log;

  void setBaud(uint32_t b) override { baud = b; }

  bool apiAtCommand(const char *cmd, const uint8_t *value, uint8_t valueLength,
                    uint8_t *outValue, uint8_t outCapacity, uint8_t *outLength) override {
    if (!present || !moduleApi || baud != moduleBaud) return false;
    std::string c(cmd, 2);
    log.push_back("api " + c);
    if (c == "AP") {
      if (outValue && outCapacity) outValue[0] = 1;
      if (outLength) *outLength = 1;
    } else if (c == "BD" && valueLength == 1) {
      pendingBaud_ = value[0] == 7 ? 115200 : moduleBaud;
    } else if (c == "WR" && failWrite) {
      return false;
    } else if (c == "AC") {
      moduleBaud = pendingBaud_;
    }
    return true;
  }

  bool enterCommandMode() override {
    if (!present || moduleApi || baud != moduleBaud) return false;
    log.push_back("+++");
    inCommandMode = true;
    return true;
  }

  bool commandModeLine(const char *line) override {
    if (!inCommandMode) return false;
    log.push_back(line);
    std::string l(line);
    if (l == "ATAP1") pendingApi_ = true;
    if (l == "ATBD7") pendingBaud_ = 115200;
    if (l == "ATCN") {
      inCommandMode = false;
      moduleApi = pendingApi_;
      moduleBaud = pendingBaud_;
    }
    return true;
  }

 private:
  uint32_t pendingBaud_ = 9600;
  bool pendingApi_ = false;
};

void test_setup_already_ready() {
  FakePort port;
  port.moduleApi = true;
  port.moduleBaud = 115200;
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kReady == XbeeUartSetup::run(&port));
  TEST_ASSERT_EQUAL_UINT32(115200, port.baud);
}

void test_setup_factory_transparent_module() {
  FakePort port;  // AP=0 at 9600
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kReconfigured == XbeeUartSetup::run(&port));
  TEST_ASSERT_TRUE(port.moduleApi);
  TEST_ASSERT_EQUAL_UINT32(115200, port.moduleBaud);
  TEST_ASSERT_EQUAL_UINT32(115200, port.baud);
  bool wrote = false;
  for (const auto &l : port.log) wrote |= l == "ATWR";
  TEST_ASSERT_TRUE(wrote);
}

void test_setup_api_mode_at_factory_baud_raises_baud() {
  FakePort port;
  port.moduleApi = true;  // API at 9600
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kReconfigured == XbeeUartSetup::run(&port));
  TEST_ASSERT_EQUAL_UINT32(115200, port.moduleBaud);
}

void test_setup_transparent_at_high_baud() {
  FakePort port;
  port.moduleBaud = 115200;  // AP=0 at 115200
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kReconfigured == XbeeUartSetup::run(&port));
  TEST_ASSERT_TRUE(port.moduleApi);
}

void test_setup_no_module() {
  FakePort port;
  port.present = false;
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kNotFound == XbeeUartSetup::run(&port));
}

void test_setup_failed_write_is_not_found() {
  FakePort port;
  port.moduleApi = true;
  port.failWrite = true;
  TEST_ASSERT_TRUE(XbeeUartSetup::Result::kNotFound == XbeeUartSetup::run(&port));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_reader_assembles_frame_after_noise);
  RUN_TEST(test_reader_rejects_bad_checksum_then_recovers);
  RUN_TEST(test_reader_rejects_impossible_lengths);
  RUN_TEST(test_reader_reset_drops_partial_frame);
  RUN_TEST(test_setup_already_ready);
  RUN_TEST(test_setup_factory_transparent_module);
  RUN_TEST(test_setup_api_mode_at_factory_baud_raises_baud);
  RUN_TEST(test_setup_transparent_at_high_baud);
  RUN_TEST(test_setup_no_module);
  RUN_TEST(test_setup_failed_write_is_not_found);
  return UNITY_END();
}
