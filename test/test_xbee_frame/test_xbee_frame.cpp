#include <unity.h>

#include "xbee_frame.h"

void setUp(void) {}
void tearDown(void) {}

// ---- checksum ---------------------------------------------------------

void test_checksum_valid_round_trip() {
  const uint8_t frame[] = {0x08, 0x01, 'I', 'D'};
  const uint8_t checksum = XbeeFrame::computeChecksum(frame, sizeof(frame));
  TEST_ASSERT_TRUE(
      XbeeFrame::checksumValid(frame, sizeof(frame), checksum));
}

void test_checksum_invalid_when_wrong() {
  const uint8_t frame[] = {0x08, 0x01, 'I', 'D'};
  const uint8_t checksum = XbeeFrame::computeChecksum(frame, sizeof(frame));
  TEST_ASSERT_FALSE(
      XbeeFrame::checksumValid(frame, sizeof(frame), checksum + 1));
}

// ---- buildAtCommandFrame ------------------------------------------------

void test_build_at_command_frame_query_no_value() {
  uint8_t buf[16];
  const uint16_t length =
      XbeeFrame::buildAtCommandFrame(buf, sizeof(buf), 0x01, "SL", nullptr, 0);
  TEST_ASSERT_EQUAL_UINT16(4, length);
  TEST_ASSERT_EQUAL_UINT8(XbeeFrame::kFrameTypeAtCommand, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, buf[1]);
  TEST_ASSERT_EQUAL_UINT8('S', buf[2]);
  TEST_ASSERT_EQUAL_UINT8('L', buf[3]);
}

void test_build_at_command_frame_with_value() {
  uint8_t buf[16];
  const uint8_t value[] = {0xAA, 0xBB, 0xCC};
  const uint16_t length = XbeeFrame::buildAtCommandFrame(
      buf, sizeof(buf), 0x02, "ID", value, sizeof(value));
  TEST_ASSERT_EQUAL_UINT16(7, length);
  TEST_ASSERT_EQUAL_UINT8('I', buf[2]);
  TEST_ASSERT_EQUAL_UINT8('D', buf[3]);
  TEST_ASSERT_EQUAL_UINT8(0xAA, buf[4]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, buf[5]);
  TEST_ASSERT_EQUAL_UINT8(0xCC, buf[6]);
}

void test_build_at_command_frame_returns_zero_when_buffer_too_small() {
  uint8_t buf[3];  // needs at least 4 for a valueless command
  const uint16_t length =
      XbeeFrame::buildAtCommandFrame(buf, sizeof(buf), 0x01, "SL", nullptr, 0);
  TEST_ASSERT_EQUAL_UINT16(0, length);
}

// ---- parseAtCommandResponse ----------------------------------------------

void test_parse_at_command_response_ok_with_value() {
  const uint8_t frame[] = {0x88, 0x01, 'S', 'L', 0x00,
                           0x41, 0xA7, 0xB3, 0xC2};
  XbeeFrame::AtCommandResponse response{};
  TEST_ASSERT_TRUE(
      XbeeFrame::parseAtCommandResponse(frame, sizeof(frame), &response));
  TEST_ASSERT_EQUAL_UINT8(0x01, response.frameId);
  TEST_ASSERT_EQUAL_INT('S', response.atCmd[0]);
  TEST_ASSERT_EQUAL_INT('L', response.atCmd[1]);
  TEST_ASSERT_TRUE(XbeeFrame::AtCommandStatus::kOk == response.status);
  TEST_ASSERT_EQUAL_UINT8(4, response.valueLength);
  TEST_ASSERT_EQUAL_UINT8(0x41, response.value[0]);
  TEST_ASSERT_EQUAL_UINT8(0xC2, response.value[3]);
}

void test_parse_at_command_response_without_value() {
  const uint8_t frame[] = {0x88, 0x02, 'N', 'R', 0x00};
  XbeeFrame::AtCommandResponse response{};
  TEST_ASSERT_TRUE(
      XbeeFrame::parseAtCommandResponse(frame, sizeof(frame), &response));
  TEST_ASSERT_EQUAL_UINT8(0, response.valueLength);
  TEST_ASSERT_TRUE(response.value == nullptr);
}

void test_parse_at_command_response_reports_error_status() {
  const uint8_t frame[] = {0x88, 0x01, 'I', 'D', 0x01};
  XbeeFrame::AtCommandResponse response{};
  TEST_ASSERT_TRUE(
      XbeeFrame::parseAtCommandResponse(frame, sizeof(frame), &response));
  TEST_ASSERT_TRUE(XbeeFrame::AtCommandStatus::kError == response.status);
}

void test_parse_at_command_response_rejects_too_short_frame() {
  const uint8_t frame[] = {0x88, 0x01, 'I', 'D'};  // missing status byte
  XbeeFrame::AtCommandResponse response{};
  TEST_ASSERT_FALSE(
      XbeeFrame::parseAtCommandResponse(frame, sizeof(frame), &response));
}

void test_parse_at_command_response_rejects_wrong_frame_type() {
  const uint8_t frame[] = {0x90, 0x01, 'I', 'D', 0x00};
  XbeeFrame::AtCommandResponse response{};
  TEST_ASSERT_FALSE(
      XbeeFrame::parseAtCommandResponse(frame, sizeof(frame), &response));
}

// ---- buildTransmitRequestFrame -------------------------------------------

void test_build_transmit_request_frame_defaults_to_coordinator() {
  uint8_t buf[32];
  const uint8_t payload[] = {0xAA, 0xBB};
  const uint16_t length = XbeeFrame::buildTransmitRequestFrame(
      buf, sizeof(buf), 0x01, payload, sizeof(payload));
  TEST_ASSERT_EQUAL_UINT16(16, length);
  TEST_ASSERT_EQUAL_UINT8(XbeeFrame::kFrameTypeTransmitRequest, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, buf[1]);
  for (int i = 2; i < 10; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, buf[i]);  // dest64 == kCoordinatorAddress64
  }
  TEST_ASSERT_EQUAL_UINT8(0, buf[10]);  // dest16 hi
  TEST_ASSERT_EQUAL_UINT8(0, buf[11]);  // dest16 lo
  TEST_ASSERT_EQUAL_UINT8(0xAA, buf[14]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, buf[15]);
}

void test_build_transmit_request_frame_with_explicit_destination() {
  uint8_t buf[32];
  const uint8_t payload[] = {0x01};
  const uint16_t length = XbeeFrame::buildTransmitRequestFrame(
      buf, sizeof(buf), 0x02, payload, sizeof(payload),
      0x0013A20041A7B3C2ULL, 0x1234);
  TEST_ASSERT_EQUAL_UINT16(15, length);
  TEST_ASSERT_EQUAL_UINT8(0x00, buf[2]);
  TEST_ASSERT_EQUAL_UINT8(0x13, buf[3]);
  TEST_ASSERT_EQUAL_UINT8(0xA2, buf[4]);
  TEST_ASSERT_EQUAL_UINT8(0x00, buf[5]);
  TEST_ASSERT_EQUAL_UINT8(0x41, buf[6]);
  TEST_ASSERT_EQUAL_UINT8(0xA7, buf[7]);
  TEST_ASSERT_EQUAL_UINT8(0xB3, buf[8]);
  TEST_ASSERT_EQUAL_UINT8(0xC2, buf[9]);
  TEST_ASSERT_EQUAL_UINT8(0x12, buf[10]);
  TEST_ASSERT_EQUAL_UINT8(0x34, buf[11]);
}

void test_build_transmit_request_frame_returns_zero_when_buffer_too_small() {
  uint8_t buf[13];  // needs at least 14 for an empty payload
  const uint16_t length =
      XbeeFrame::buildTransmitRequestFrame(buf, sizeof(buf), 0x01, nullptr, 0);
  TEST_ASSERT_EQUAL_UINT16(0, length);
}

// ---- parseReceivePacket ---------------------------------------------------

void test_parse_receive_packet_extracts_source_and_payload() {
  const uint8_t frame[] = {
      0x90,                                            // type
      0x00, 0x13, 0xA2, 0x00, 0x41, 0xA7, 0xB3, 0xC2,  // source64
      0x00, 0x00,                                      // source16
      0x01,                                            // options
      0xDE, 0xAD, 0xBE, 0xEF,                          // payload
  };
  XbeeFrame::ReceivePacket packet{};
  TEST_ASSERT_TRUE(
      XbeeFrame::parseReceivePacket(frame, sizeof(frame), &packet));
  TEST_ASSERT_TRUE(0x0013A20041A7B3C2ULL == packet.sourceAddress64);
  TEST_ASSERT_EQUAL_UINT16(4, packet.payloadLength);
  TEST_ASSERT_EQUAL_UINT8(0xDE, packet.payload[0]);
  TEST_ASSERT_EQUAL_UINT8(0xEF, packet.payload[3]);
}

void test_parse_receive_packet_handles_empty_payload() {
  const uint8_t frame[] = {0x90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
  XbeeFrame::ReceivePacket packet{};
  TEST_ASSERT_TRUE(
      XbeeFrame::parseReceivePacket(frame, sizeof(frame), &packet));
  TEST_ASSERT_EQUAL_UINT16(0, packet.payloadLength);
  TEST_ASSERT_TRUE(packet.payload == nullptr);
}

void test_parse_receive_packet_rejects_too_short_frame() {
  const uint8_t frame[] = {0x90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // 11 bytes
  XbeeFrame::ReceivePacket packet{};
  TEST_ASSERT_FALSE(
      XbeeFrame::parseReceivePacket(frame, sizeof(frame), &packet));
}

void test_parse_receive_packet_rejects_wrong_frame_type() {
  const uint8_t frame[] = {0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  XbeeFrame::ReceivePacket packet{};
  TEST_ASSERT_FALSE(
      XbeeFrame::parseReceivePacket(frame, sizeof(frame), &packet));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_valid_round_trip);
  RUN_TEST(test_checksum_invalid_when_wrong);
  RUN_TEST(test_build_at_command_frame_query_no_value);
  RUN_TEST(test_build_at_command_frame_with_value);
  RUN_TEST(test_build_at_command_frame_returns_zero_when_buffer_too_small);
  RUN_TEST(test_parse_at_command_response_ok_with_value);
  RUN_TEST(test_parse_at_command_response_without_value);
  RUN_TEST(test_parse_at_command_response_reports_error_status);
  RUN_TEST(test_parse_at_command_response_rejects_too_short_frame);
  RUN_TEST(test_parse_at_command_response_rejects_wrong_frame_type);
  RUN_TEST(test_build_transmit_request_frame_defaults_to_coordinator);
  RUN_TEST(test_build_transmit_request_frame_with_explicit_destination);
  RUN_TEST(test_build_transmit_request_frame_returns_zero_when_buffer_too_small);
  RUN_TEST(test_parse_receive_packet_extracts_source_and_payload);
  RUN_TEST(test_parse_receive_packet_handles_empty_payload);
  RUN_TEST(test_parse_receive_packet_rejects_too_short_frame);
  RUN_TEST(test_parse_receive_packet_rejects_wrong_frame_type);
  return UNITY_END();
}
