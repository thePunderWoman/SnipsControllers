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
  packet.flags = UplinkPacket::kFlagShuttingDown;

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
  TEST_ASSERT_EQUAL_UINT8(UplinkPacket::kFlagShuttingDown, decoded.flags);
}

void test_uplink_flags_default_to_zero() {
  UplinkPacket packet;
  uint8_t buf[Packet::kUplinkEncodedSize];
  Packet::encodeUplink(packet, buf, sizeof(buf));

  UplinkPacket decoded;
  Packet::decodeUplink(buf, sizeof(buf), &decoded);
  TEST_ASSERT_EQUAL_UINT8(0, decoded.flags);
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
  packet.volumePercent = 70;
  packet.throttlePercent = 65;
  packet.flags = DownlinkPacket::kFlagEstop | DownlinkPacket::kFlagAutoDome;
  packet.droidBatteryPercent = 74;
  packet.sequenceId = 9;
  packet.sequenceProgressPercent = 40;
  std::strncpy(packet.sequenceName, "Leia msg", sizeof(packet.sequenceName));

  uint8_t buf[Packet::kDownlinkEncodedSize];
  TEST_ASSERT_EQUAL_UINT(Packet::kDownlinkEncodedSize,
                        Packet::encodeDownlink(packet, buf, sizeof(buf)));

  DownlinkPacket decoded;
  TEST_ASSERT_TRUE(Packet::decodeDownlink(buf, sizeof(buf), &decoded));
  TEST_ASSERT_TRUE(DownlinkPacket::Handedness::kLeft == decoded.handedness);
  TEST_ASSERT_EQUAL_UINT8(70, decoded.volumePercent);
  TEST_ASSERT_EQUAL_UINT8(65, decoded.throttlePercent);
  TEST_ASSERT_EQUAL_UINT8(
      DownlinkPacket::kFlagEstop | DownlinkPacket::kFlagAutoDome,
      decoded.flags);
  TEST_ASSERT_EQUAL_UINT8(74, decoded.droidBatteryPercent);
  TEST_ASSERT_EQUAL_UINT8(9, decoded.sequenceId);
  TEST_ASSERT_EQUAL_UINT8(40, decoded.sequenceProgressPercent);
  TEST_ASSERT_EQUAL_STRING("Leia msg", decoded.sequenceName);
}

void test_downlink_wire_layout() {
  DownlinkPacket packet;
  packet.handedness = DownlinkPacket::Handedness::kRight;
  packet.volumePercent = 1;
  packet.throttlePercent = 2;
  packet.flags = 3;
  packet.droidBatteryPercent = 4;
  packet.sequenceId = 5;
  packet.sequenceProgressPercent = 6;
  std::strncpy(packet.sequenceName, "AB", sizeof(packet.sequenceName));

  uint8_t buf[Packet::kDownlinkEncodedSize];
  Packet::encodeDownlink(packet, buf, sizeof(buf));
  const uint8_t expected[] = {DownlinkPacket::kVersion, 2, 1, 2, 3, 4, 5, 6,
                              'A', 'B', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT(sizeof(expected), Packet::kDownlinkEncodedSize);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(expected));
}

void test_downlink_defaults_to_unknown_values() {
  DownlinkPacket packet;
  uint8_t buf[Packet::kDownlinkEncodedSize];
  Packet::encodeDownlink(packet, buf, sizeof(buf));

  DownlinkPacket decoded;
  Packet::decodeDownlink(buf, sizeof(buf), &decoded);
  TEST_ASSERT_EQUAL_UINT8(DownlinkPacket::kUnknown, decoded.volumePercent);
  TEST_ASSERT_EQUAL_UINT8(DownlinkPacket::kUnknown, decoded.throttlePercent);
  TEST_ASSERT_EQUAL_UINT8(DownlinkPacket::kUnknown,
                          decoded.droidBatteryPercent);
  TEST_ASSERT_EQUAL_UINT8(DownlinkPacket::kUnknown,
                          decoded.sequenceProgressPercent);
  TEST_ASSERT_EQUAL_UINT8(0, decoded.flags);
  TEST_ASSERT_EQUAL_STRING("", decoded.sequenceName);
}

void test_downlink_truncates_overly_long_sequence_name() {
  DownlinkPacket packet;
  std::strncpy(packet.sequenceName, "WayTooLongForTheField",
              sizeof(packet.sequenceName));
  packet.sequenceName[sizeof(packet.sequenceName) - 1] = '\0';

  uint8_t buf[Packet::kDownlinkEncodedSize];
  Packet::encodeDownlink(packet, buf, sizeof(buf));

  DownlinkPacket decoded;
  Packet::decodeDownlink(buf, sizeof(buf), &decoded);
  TEST_ASSERT_EQUAL_UINT(DownlinkPacket::kSequenceNameLength,
                        std::strlen(decoded.sequenceName));
  TEST_ASSERT_EQUAL_STRING("WayTooLongForTh", decoded.sequenceName);
}

// The previous downlink format started with a handedness byte (0-2) and
// was 33 bytes long. A controller on this firmware must not misread one
// from an Amidala that hasn't been updated.
void test_downlink_decode_rejects_previous_format() {
  for (uint8_t handedness = 0; handedness <= 2; ++handedness) {
    uint8_t buf[33] = {};
    buf[0] = handedness;
    DownlinkPacket decoded;
    TEST_ASSERT_FALSE(Packet::decodeDownlink(buf, sizeof(buf), &decoded));
  }
}

void test_downlink_encode_fails_when_buffer_too_small() {
  DownlinkPacket packet;
  uint8_t buf[5];
  TEST_ASSERT_EQUAL_UINT(0, Packet::encodeDownlink(packet, buf, sizeof(buf)));
}

void test_downlink_decode_fails_when_length_too_short() {
  uint8_t buf[5] = {DownlinkPacket::kVersion};
  DownlinkPacket decoded;
  TEST_ASSERT_FALSE(Packet::decodeDownlink(buf, sizeof(buf), &decoded));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_uplink_round_trip);
  RUN_TEST(test_uplink_flags_default_to_zero);
  RUN_TEST(test_uplink_encode_fails_when_buffer_too_small);
  RUN_TEST(test_uplink_decode_fails_when_length_too_short);
  RUN_TEST(test_downlink_round_trip);
  RUN_TEST(test_downlink_wire_layout);
  RUN_TEST(test_downlink_defaults_to_unknown_values);
  RUN_TEST(test_downlink_truncates_overly_long_sequence_name);
  RUN_TEST(test_downlink_decode_rejects_previous_format);
  RUN_TEST(test_downlink_encode_fails_when_buffer_too_small);
  RUN_TEST(test_downlink_decode_fails_when_length_too_short);
  return UNITY_END();
}
