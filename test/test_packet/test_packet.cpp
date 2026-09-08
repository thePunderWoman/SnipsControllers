#include <unity.h>
#include <cstring>

#include "packet.h"

void setUp(void) {}
void tearDown(void) {}

// ---- uplink ----------------------------------------------------------

void test_uplink_round_trip() {
  UplinkPacket packet;
  packet.buttonMask = 0xABCD;
  packet.triggerPercent = 42;
  packet.stickXPercent = -73;
  packet.stickYPercent = 100;
  packet.batteryPercent = 88;
  packet.chargeState = ChargeState::kCharging;

  uint8_t buf[Packet::kUplinkEncodedSize];
  TEST_ASSERT_EQUAL_UINT(Packet::kUplinkEncodedSize,
                        Packet::encodeUplink(packet, buf, sizeof(buf)));

  UplinkPacket decoded;
  TEST_ASSERT_TRUE(Packet::decodeUplink(buf, sizeof(buf), &decoded));
  TEST_ASSERT_EQUAL_UINT16(0xABCD, decoded.buttonMask);
  TEST_ASSERT_EQUAL_UINT8(42, decoded.triggerPercent);
  TEST_ASSERT_EQUAL_INT8(-73, decoded.stickXPercent);
  TEST_ASSERT_EQUAL_INT8(100, decoded.stickYPercent);
  TEST_ASSERT_EQUAL_UINT8(88, decoded.batteryPercent);
  TEST_ASSERT_TRUE(ChargeState::kCharging == decoded.chargeState);
}

void test_uplink_encode_fails_when_buffer_too_small() {
  UplinkPacket packet;
  uint8_t buf[3];
  TEST_ASSERT_EQUAL_UINT(0, Packet::encodeUplink(packet, buf, sizeof(buf)));
}

void test_uplink_decode_fails_when_length_too_short() {
  uint8_t buf[3] = {};
  UplinkPacket decoded;
  TEST_ASSERT_FALSE(Packet::decodeUplink(buf, sizeof(buf), &decoded));
}

// ---- downlink ----------------------------------------------------------

void test_downlink_round_trip() {
  DownlinkPacket packet;
  packet.handedness = DownlinkPacket::Handedness::kLeft;
  std::strncpy(packet.leftLabel, "Volume", sizeof(packet.leftLabel));
  std::strncpy(packet.leftValue, "42%", sizeof(packet.leftValue));
  std::strncpy(packet.rightLabel, "Throttle", sizeof(packet.rightLabel));
  std::strncpy(packet.rightValue, "88%", sizeof(packet.rightValue));

  uint8_t buf[Packet::kDownlinkEncodedSize];
  TEST_ASSERT_EQUAL_UINT(Packet::kDownlinkEncodedSize,
                        Packet::encodeDownlink(packet, buf, sizeof(buf)));

  DownlinkPacket decoded;
  TEST_ASSERT_TRUE(Packet::decodeDownlink(buf, sizeof(buf), &decoded));
  TEST_ASSERT_TRUE(DownlinkPacket::Handedness::kLeft == decoded.handedness);
  TEST_ASSERT_EQUAL_STRING("Volume", decoded.leftLabel);
  TEST_ASSERT_EQUAL_STRING("42%", decoded.leftValue);
  TEST_ASSERT_EQUAL_STRING("Throttle", decoded.rightLabel);
  TEST_ASSERT_EQUAL_STRING("88%", decoded.rightValue);
}

void test_downlink_truncates_overly_long_field() {
  DownlinkPacket packet;
  std::strncpy(packet.leftLabel, "WayTooLongForEightChars",
              sizeof(packet.leftLabel));
  packet.leftLabel[sizeof(packet.leftLabel) - 1] = '\0';

  uint8_t buf[Packet::kDownlinkEncodedSize];
  Packet::encodeDownlink(packet, buf, sizeof(buf));

  DownlinkPacket decoded;
  Packet::decodeDownlink(buf, sizeof(buf), &decoded);
  TEST_ASSERT_EQUAL_UINT(DownlinkPacket::kFieldLength,
                        std::strlen(decoded.leftLabel));
  TEST_ASSERT_EQUAL_STRING("WayTooLo", decoded.leftLabel);
}

void test_downlink_short_field_is_clean_not_garbage() {
  DownlinkPacket packet;
  std::strncpy(packet.leftLabel, "Hi", sizeof(packet.leftLabel));

  uint8_t buf[Packet::kDownlinkEncodedSize];
  Packet::encodeDownlink(packet, buf, sizeof(buf));

  DownlinkPacket decoded;
  Packet::decodeDownlink(buf, sizeof(buf), &decoded);
  TEST_ASSERT_EQUAL_STRING("Hi", decoded.leftLabel);
}

void test_downlink_encode_fails_when_buffer_too_small() {
  DownlinkPacket packet;
  uint8_t buf[5];
  TEST_ASSERT_EQUAL_UINT(0, Packet::encodeDownlink(packet, buf, sizeof(buf)));
}

void test_downlink_decode_fails_when_length_too_short() {
  uint8_t buf[5] = {};
  DownlinkPacket decoded;
  TEST_ASSERT_FALSE(Packet::decodeDownlink(buf, sizeof(buf), &decoded));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_uplink_round_trip);
  RUN_TEST(test_uplink_encode_fails_when_buffer_too_small);
  RUN_TEST(test_uplink_decode_fails_when_length_too_short);
  RUN_TEST(test_downlink_round_trip);
  RUN_TEST(test_downlink_truncates_overly_long_field);
  RUN_TEST(test_downlink_short_field_is_clean_not_garbage);
  RUN_TEST(test_downlink_encode_fails_when_buffer_too_small);
  RUN_TEST(test_downlink_decode_fails_when_length_too_short);
  return UNITY_END();
}
