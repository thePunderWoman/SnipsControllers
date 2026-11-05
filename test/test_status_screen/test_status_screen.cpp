#include <unity.h>
#include <cstring>

#include "snips_logo_small_bitmap.h"
#include "status_font.h"
#include "status_screen.h"

void setUp(void) {}
void tearDown(void) {}

// A connected controller with every value known — the usual screen.
StatusData driving() {
  StatusData d;
  d.batteryPercent = 82;
  d.connected = true;
  d.signalBars = 3;
  d.handedness = DownlinkPacket::Handedness::kLeft;
  std::strncpy(d.droidName, "R2-D2", sizeof(d.droidName) - 1);
  d.volumePercent = 70;
  d.throttlePercent = 65;
  d.droidBatteryPercent = 74;
  return d;
}

FrameBuffer render(const StatusData &d,
                   StatusLayout layout = StatusLayout::kStatusBars) {
  FrameBuffer frame;
  renderStatusScreen(d, layout, &frame);
  return frame;
}

int litIn(const FrameBuffer &f, int x, int y, int w, int h) {
  int count = 0;
  for (int row = y; row < y + h; ++row) {
    for (int col = x; col < x + w; ++col) count += f.pixel(col, row);
  }
  return count;
}

// True if the box `text` occupies at (x, y) holds exactly that text.
bool hasText(const FrameBuffer &f, int x, int y, const char *text,
             int size = 1) {
  FrameBuffer expected;
  StatusFont::drawText(&expected, x, y, text, size);
  const int w = StatusFont::textWidth(text, size);
  for (int row = y; row < y + 7 * size; ++row) {
    for (int col = x; col < x + w; ++col) {
      if (f.pixel(col, row) != expected.pixel(col, row)) return false;
    }
  }
  return true;
}

bool hasSmallLogo(const FrameBuffer &f, int x, int y) {
  FrameBuffer expected;
  expected.drawBitmap(x, y, SnipsLogoSmall::kBitmap, SnipsLogoSmall::kWidth,
                      SnipsLogoSmall::kHeight);
  for (int row = y; row < y + SnipsLogoSmall::kHeight; ++row) {
    for (int col = x; col < x + SnipsLogoSmall::kWidth; ++col) {
      if (f.pixel(col, row) != expected.pixel(col, row)) return false;
    }
  }
  return true;
}

// ---- helpers ---------------------------------------------------------------

void test_signal_bars_for_dbm() {
  TEST_ASSERT_EQUAL_INT(0, signalBarsForDbm(-40, false));
  TEST_ASSERT_EQUAL_INT(4, signalBarsForDbm(-40, true));
  TEST_ASSERT_EQUAL_INT(4, signalBarsForDbm(-65, true));
  TEST_ASSERT_EQUAL_INT(3, signalBarsForDbm(-66, true));
  TEST_ASSERT_EQUAL_INT(3, signalBarsForDbm(-75, true));
  TEST_ASSERT_EQUAL_INT(2, signalBarsForDbm(-85, true));
  TEST_ASSERT_EQUAL_INT(1, signalBarsForDbm(-95, true));
  TEST_ASSERT_EQUAL_INT(0, signalBarsForDbm(-96, true));
}

void test_layout_labels() {
  TEST_ASSERT_EQUAL_STRING("Status bars",
                           statusLayoutLabel(StatusLayout::kStatusBars));
  TEST_ASSERT_EQUAL_STRING("Throttle first",
                           statusLayoutLabel(StatusLayout::kThrottleFirst));
  TEST_ASSERT_EQUAL_STRING("Side gauges",
                           statusLayoutLabel(StatusLayout::kSideGauges));
  TEST_ASSERT_EQUAL_STRING("Snips logo", statusLayoutLabel(StatusLayout::kLogo));
  TEST_ASSERT_EQUAL_STRING("Unknown", statusLayoutLabel(StatusLayout::kCount));
}

void test_layout_from_stored_falls_back_to_status_bars() {
  TEST_ASSERT_TRUE(StatusLayout::kSideGauges == statusLayoutFromStored(2));
  TEST_ASSERT_TRUE(StatusLayout::kStatusBars == statusLayoutFromStored(-1));
  TEST_ASSERT_TRUE(StatusLayout::kStatusBars ==
                   statusLayoutFromStored(static_cast<int>(StatusLayout::kCount)));
}

void test_apply_downlink_copies_values_and_flags() {
  DownlinkPacket downlink;
  downlink.handedness = DownlinkPacket::Handedness::kRight;
  downlink.volumePercent = 40;
  downlink.throttlePercent = 150;  // clamped
  downlink.flags = DownlinkPacket::kFlagMotorsDisabled | DownlinkPacket::kFlagMuse;
  StatusData d;
  applyDownlink(downlink, &d);
  TEST_ASSERT_TRUE(DownlinkPacket::Handedness::kRight == d.handedness);
  TEST_ASSERT_EQUAL_INT(40, d.volumePercent);
  TEST_ASSERT_EQUAL_INT(100, d.throttlePercent);
  TEST_ASSERT_EQUAL_INT(-1, d.droidBatteryPercent);  // kUnknown
  TEST_ASSERT_FALSE(d.estop);
  TEST_ASSERT_TRUE(d.motorsDisabled);
  TEST_ASSERT_TRUE(d.muse);
  TEST_ASSERT_FALSE(d.autoDome);

  downlink.flags = DownlinkPacket::kFlagEstop | DownlinkPacket::kFlagAutoDome;
  applyDownlink(downlink, &d);
  TEST_ASSERT_TRUE(d.estop);
  TEST_ASSERT_FALSE(d.motorsDisabled);
  TEST_ASSERT_FALSE(d.muse);
  TEST_ASSERT_TRUE(d.autoDome);
}

// ---- top bar ----------------------------------------------------------------

void test_top_bar_shows_battery_percent() {
  const FrameBuffer f = render(driving());
  TEST_ASSERT_TRUE(hasText(f, 128 - 17 - 2 - StatusFont::textWidth("82%"), 1,
                           "82%"));
  TEST_ASSERT_TRUE(f.pixel(111, 0));  // battery outline
}

void test_low_battery_blink_hides_battery() {
  StatusData d = driving();
  d.batteryVisible = false;
  const FrameBuffer f = render(d);
  TEST_ASSERT_EQUAL_INT(0, litIn(f, 80, 0, 48, 9));
}

void test_charging_adds_bolt() {
  StatusData d = driving();
  d.charging = true;
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(f.pixel(111 - 7 + 3, 0));  // bolt's top-right
  TEST_ASSERT_TRUE(hasText(f, 111 - 9 - StatusFont::textWidth("82%"), 1, "82%"));
}

void test_usb_power_shows_usb_instead_of_percent() {
  StatusData d = driving();
  d.usbPower = UsbPower::kHost;
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasText(f, 111 - 2 - StatusFont::textWidth("USB"), 1, "USB"));
}

void test_hand_badge_is_inverted_letter() {
  StatusData d = driving();
  // XBee icon (8) + gap (2) + bars (11) + gap (3): badge at x = 24.
  FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(f.pixel(24, 0));
  TEST_ASSERT_FALSE(f.pixel(26, 1));  // 'L' stem, drawn dark
  TEST_ASSERT_TRUE(f.pixel(27, 1));

  d.handedness = DownlinkPacket::Handedness::kRight;
  const FrameBuffer right = render(d);
  TEST_ASSERT_TRUE(f != right);
  d.handedness = DownlinkPacket::Handedness::kUnknown;
  TEST_ASSERT_TRUE(right != render(d));
}

void test_signal_bars_follow_strength_and_connection() {
  StatusData d = driving();
  d.signalBars = 4;
  TEST_ASSERT_TRUE(render(d).pixel(19, 0));  // tallest bar's top
  d.signalBars = 3;
  TEST_ASSERT_FALSE(render(d).pixel(19, 0));
  d.signalBars = 4;
  d.connected = false;
  TEST_ASSERT_FALSE(render(d).pixel(19, 0));
}

void test_link_type_changes_icon() {
  StatusData d = driving();
  const FrameBuffer xbee = render(d);
  d.link = LinkType::kWifi;
  const FrameBuffer wifi = render(d);
  d.link = LinkType::kBluetooth;
  const FrameBuffer bluetooth = render(d);
  TEST_ASSERT_TRUE(xbee != wifi);
  TEST_ASSERT_TRUE(wifi != bluetooth);
}

void test_mode_icons_only_while_on() {
  StatusData d = driving();
  TEST_ASSERT_EQUAL_INT(0, litIn(render(d), 34, 0, 50, 9));
  d.muse = true;
  const int museOnly = litIn(render(d), 34, 0, 50, 9);
  TEST_ASSERT_TRUE(museOnly > 0);
  d.autoDome = true;
  TEST_ASSERT_TRUE(litIn(render(d), 34, 0, 50, 9) > museOnly);
}

// ---- status bars ------------------------------------------------------------

void test_status_bars_layout() {
  const FrameBuffer f = render(driving());
  TEST_ASSERT_TRUE(hasText(f, (128 - StatusFont::textWidth("R2-D2", 2)) / 2,
                           18, "R2-D2", 2));
  TEST_ASSERT_TRUE(hasText(f, 11, 56, "70%"));
  TEST_ASSERT_TRUE(hasText(f, 127 - StatusFont::textWidth("65%"), 56, "65%"));
  TEST_ASSERT_TRUE(litIn(f, 30, 38, 70, 7) > 0);  // droid battery line
}

void test_unknown_values_show_dashes_and_no_droid_battery() {
  StatusData d = driving();
  d.volumePercent = -1;
  d.throttlePercent = -1;
  d.droidBatteryPercent = -1;
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasText(f, 11, 56, "--"));
  TEST_ASSERT_TRUE(hasText(f, 127 - StatusFont::textWidth("--"), 56, "--"));
  TEST_ASSERT_EQUAL_INT(0, litIn(f, 0, 36, 128, 10));
}

void test_long_droid_name_falls_back_to_small_font() {
  StatusData d = driving();
  std::strncpy(d.droidName, "CHOPPER C1 10P", sizeof(d.droidName) - 1);
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasText(f, (128 - StatusFont::textWidth("CHOPPER C1 10P")) / 2,
                           22, "CHOPPER C1 10P"));
}

void test_motors_disabled_replaces_throttle_with_off() {
  StatusData d = driving();
  d.motorsDisabled = true;
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasText(f, 127 - StatusFont::textWidth("OFF"), 56, "OFF"));
}

void test_zero_volume_shows_muted_speaker() {
  StatusData d = driving();
  const FrameBuffer loud = render(d);
  d.volumePercent = 0;
  const FrameBuffer muted = render(d);
  TEST_ASSERT_TRUE(loud.pixel(6, 57));  // a sound wave
  TEST_ASSERT_FALSE(muted.pixel(6, 57));
}

// ---- idle middle -----------------------------------------------------------

void test_no_droid_shows_logo_and_hint() {
  StatusData d = driving();
  d.droidName[0] = '\0';
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasSmallLogo(f, 4, 11 + (43 - SnipsLogoSmall::kHeight) / 2));
  const int textX = 4 + SnipsLogoSmall::kWidth + 4;
  const int textW = 128 - textX;
  TEST_ASSERT_TRUE(hasText(
      f, textX + (textW - StatusFont::textWidth("No droid")) / 2, 22, "No droid"));
}

void test_link_lost_shows_logo_and_searching() {
  StatusData d = driving();
  d.connected = false;
  const FrameBuffer f = render(d);
  const int textX = 4 + SnipsLogoSmall::kWidth + 4;
  const int textW = 128 - textX;
  TEST_ASSERT_TRUE(hasText(
      f, textX + (textW - StatusFont::textWidth("R2-D2")) / 2, 22, "R2-D2"));
  TEST_ASSERT_TRUE(hasText(
      f, textX + (textW - StatusFont::textWidth("Searching...")) / 2, 34,
      "Searching..."));
}

void test_every_layout_shows_idle_logo() {
  StatusData d = driving();
  d.connected = false;
  for (int i = 0; i < static_cast<int>(StatusLayout::kCount); ++i) {
    const auto layout = static_cast<StatusLayout>(i);
    const int boxX = layout == StatusLayout::kSideGauges ? 10 : 0;
    TEST_ASSERT_TRUE(hasSmallLogo(render(d, layout), boxX + 4,
                                  11 + (43 - SnipsLogoSmall::kHeight) / 2));
  }
}

// ---- other layouts ---------------------------------------------------------

void test_logo_layout_puts_logo_beside_name() {
  const FrameBuffer f = render(driving(), StatusLayout::kLogo);
  TEST_ASSERT_TRUE(hasSmallLogo(f, 2, 14));
  TEST_ASSERT_TRUE(hasText(
      f, 26 + (102 - StatusFont::textWidth("R2-D2", 2)) / 2, 18, "R2-D2", 2));
  TEST_ASSERT_TRUE(hasText(f, 11, 56, "70%"));
}

void test_throttle_first_shows_big_throttle() {
  const FrameBuffer f = render(driving(), StatusLayout::kThrottleFirst);
  const int width = StatusFont::textWidth("65", 3) + 3 + StatusFont::textWidth("%", 2);
  TEST_ASSERT_TRUE(hasText(f, (128 - width) / 2, 22, "65", 3));
  TEST_ASSERT_TRUE(hasText(f, (128 - StatusFont::textWidth("R2-D2")) / 2, 12,
                           "R2-D2"));
  TEST_ASSERT_TRUE(f.pixel(6, 48));     // bar fill starts
  TEST_ASSERT_FALSE(f.pixel(120, 48));  // ...and stops short at 65%
  TEST_ASSERT_TRUE(hasText(f, 127 - StatusFont::textWidth("74%"), 56, "74%"));
}

void test_throttle_first_unknown_and_locked() {
  StatusData d = driving();
  d.throttlePercent = -1;
  const FrameBuffer unknown = render(d, StatusLayout::kThrottleFirst);
  const int width = StatusFont::textWidth("--", 3);  // no "%" when unknown
  TEST_ASSERT_TRUE(hasText(unknown, (128 - width) / 2, 22, "--", 3));
  TEST_ASSERT_EQUAL_INT(0, litIn(unknown, (128 + width) / 2 + 1, 22, 20, 21));
  TEST_ASSERT_FALSE(unknown.pixel(6, 48));

  d.motorsDisabled = true;
  const FrameBuffer locked = render(d, StatusLayout::kThrottleFirst);
  TEST_ASSERT_TRUE(hasText(locked, (128 - 14 - 4 - 70) / 2 + 18, 27, "LOCKED", 2));
}

void test_throttle_first_volume_bars() {
  StatusData d = driving();
  d.volumePercent = 30;
  const FrameBuffer f = render(d, StatusLayout::kThrottleFirst);
  TEST_ASSERT_TRUE(f.pixel(11 + 2 * 3, 62));   // third bar lit
  TEST_ASSERT_FALSE(f.pixel(11 + 3 * 3, 61));  // fourth bar only a stub
}

void test_side_gauges_fill_with_values() {
  StatusData d = driving();
  d.volumePercent = 100;
  d.throttlePercent = 0;
  const FrameBuffer f = render(d, StatusLayout::kSideGauges);
  TEST_ASSERT_TRUE(f.pixel(2, 23));       // volume gauge full
  TEST_ASSERT_FALSE(f.pixel(124, 61));    // throttle gauge empty
  TEST_ASSERT_TRUE(hasText(f, 10, 56, "100%"));
  TEST_ASSERT_TRUE(hasText(f, 118 - StatusFont::textWidth("0%"), 56, "0%"));
  TEST_ASSERT_TRUE(hasText(f, (128 - StatusFont::textWidth("R2-D2", 2)) / 2,
                           20, "R2-D2", 2));
}

void test_side_gauges_motors_disabled_empties_throttle() {
  StatusData d = driving();
  d.throttlePercent = 100;
  d.motorsDisabled = true;
  const FrameBuffer f = render(d, StatusLayout::kSideGauges);
  TEST_ASSERT_FALSE(f.pixel(124, 40));
  TEST_ASSERT_TRUE(hasText(f, 118 - StatusFont::textWidth("OFF"), 56, "OFF"));
}

// ---- overlays --------------------------------------------------------------

void test_estop_takes_over_middle_in_both_phases() {
  StatusData d = driving();
  d.estop = true;
  d.sequenceVisible = true;  // e-stop wins
  std::strncpy(d.sequenceName, "Happy", sizeof(d.sequenceName) - 1);
  for (int i = 0; i < static_cast<int>(StatusLayout::kCount); ++i) {
    const auto layout = static_cast<StatusLayout>(i);
    d.estopSolidPhase = true;
    const FrameBuffer solid = render(d, layout);
    TEST_ASSERT_TRUE(solid.pixel(0, 11));
    TEST_ASSERT_TRUE(solid.pixel(64, 15));
    d.estopSolidPhase = false;
    const FrameBuffer outline = render(d, layout);
    TEST_ASSERT_TRUE(outline.pixel(0, 11));
    TEST_ASSERT_FALSE(outline.pixel(64, 15));
    TEST_ASSERT_TRUE(hasText(outline, (128 - StatusFont::textWidth("E-STOP", 3)) / 2,
                             22, "E-STOP", 3));
  }
}

void test_sequence_takes_over_middle() {
  StatusData d = driving();
  d.sequenceVisible = true;
  std::strncpy(d.sequenceName, "Happy", sizeof(d.sequenceName) - 1);
  const FrameBuffer f = render(d);
  TEST_ASSERT_TRUE(hasText(f, (128 - StatusFont::textWidth("Happy", 2)) / 2,
                           25, "Happy", 2));
  TEST_ASSERT_EQUAL_INT(0, litIn(f, 0, 44, 128, 8));  // no progress bar
  TEST_ASSERT_TRUE(hasText(f, 11, 56, "70%"));        // bottom row kept

  d.sequenceProgressPercent = 50;
  const FrameBuffer progress = render(d);
  TEST_ASSERT_TRUE(progress.pixel(10, 48));
  TEST_ASSERT_FALSE(progress.pixel(100, 48));
}

void test_sequence_hidden_while_disconnected() {
  StatusData d = driving();
  d.connected = false;
  const FrameBuffer idle = render(d);
  d.sequenceVisible = true;
  std::strncpy(d.sequenceName, "Happy", sizeof(d.sequenceName) - 1);
  TEST_ASSERT_TRUE(idle == render(d));
}

void test_sequence_in_side_gauges_keeps_gauges() {
  StatusData d = driving();
  d.volumePercent = 100;
  d.sequenceVisible = true;
  std::strncpy(d.sequenceName, "Cantina dance", sizeof(d.sequenceName) - 1);
  const FrameBuffer f = render(d, StatusLayout::kSideGauges);
  TEST_ASSERT_TRUE(f.pixel(2, 30));  // volume gauge still drawn
  TEST_ASSERT_TRUE(hasText(f, 10 + (108 - StatusFont::textWidth("Cantina dance")) / 2,
                           29, "Cantina dance"));
}

void test_rendering_clears_previous_frame() {
  FrameBuffer frame;
  frame.fillRect(0, 0, 128, 64);
  renderStatusScreen(driving(), StatusLayout::kStatusBars, &frame);
  TEST_ASSERT_TRUE(frame == render(driving()));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_signal_bars_for_dbm);
  RUN_TEST(test_layout_labels);
  RUN_TEST(test_layout_from_stored_falls_back_to_status_bars);
  RUN_TEST(test_apply_downlink_copies_values_and_flags);
  RUN_TEST(test_top_bar_shows_battery_percent);
  RUN_TEST(test_low_battery_blink_hides_battery);
  RUN_TEST(test_charging_adds_bolt);
  RUN_TEST(test_usb_power_shows_usb_instead_of_percent);
  RUN_TEST(test_hand_badge_is_inverted_letter);
  RUN_TEST(test_signal_bars_follow_strength_and_connection);
  RUN_TEST(test_link_type_changes_icon);
  RUN_TEST(test_mode_icons_only_while_on);
  RUN_TEST(test_status_bars_layout);
  RUN_TEST(test_unknown_values_show_dashes_and_no_droid_battery);
  RUN_TEST(test_long_droid_name_falls_back_to_small_font);
  RUN_TEST(test_motors_disabled_replaces_throttle_with_off);
  RUN_TEST(test_zero_volume_shows_muted_speaker);
  RUN_TEST(test_no_droid_shows_logo_and_hint);
  RUN_TEST(test_link_lost_shows_logo_and_searching);
  RUN_TEST(test_every_layout_shows_idle_logo);
  RUN_TEST(test_logo_layout_puts_logo_beside_name);
  RUN_TEST(test_throttle_first_shows_big_throttle);
  RUN_TEST(test_throttle_first_unknown_and_locked);
  RUN_TEST(test_throttle_first_volume_bars);
  RUN_TEST(test_side_gauges_fill_with_values);
  RUN_TEST(test_side_gauges_motors_disabled_empties_throttle);
  RUN_TEST(test_estop_takes_over_middle_in_both_phases);
  RUN_TEST(test_sequence_takes_over_middle);
  RUN_TEST(test_sequence_hidden_while_disconnected);
  RUN_TEST(test_sequence_in_side_gauges_keeps_gauges);
  RUN_TEST(test_rendering_clears_previous_frame);
  return UNITY_END();
}
