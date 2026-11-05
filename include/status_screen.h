#pragma once

#include <cstdint>

#include "battery.h"
#include "droid_store.h"
#include "frame_buffer.h"
#include "packet.h"

// The "normal operating" screen, shown whenever the on-device menu is
// closed. Pure: SnipsController.ino fills a StatusData each tick from the
// battery, the radio, the droid list and the latest downlink, and
// OledDisplay draws the resulting FrameBuffer. The layout is the user's
// choice, picked on the Display Config menu screen.
//
// Every layout shares the same top bar (rows 0-8): link icon and signal
// bars, an inverted L/R badge, mode icons (muse, auto dome) while they're
// on, and the controller battery on the right. E-stop and a running
// sequence take over the middle (rows 11-53) the same way in every
// layout; the layouts differ only in how they show the droid, volume and
// throttle.
enum class StatusLayout : uint8_t {
  kStatusBars = 0,  // droid name in the middle, small volume/throttle readouts
  kThrottleFirst,   // throttle as a big number, droid name on a small line
  kSideGauges,      // volume and throttle as vertical bars on the edges
  kLogo,            // kStatusBars with the Snips logo beside the droid name
  kCount,
};

const char *statusLayoutLabel(StatusLayout layout);

// Maps a stored value back to a layout, falling back to kStatusBars for
// anything out of range (e.g. NVS from a newer firmware).
StatusLayout statusLayoutFromStored(int stored);

enum class LinkType : uint8_t { kXbee, kWifi, kBluetooth };

// 0-4 bars for the top bar. Thresholds sit across the XBee 3's usable
// range (about -100 dBm sensitivity), so 4 bars is a strong link and 1 bar
// is close to dropping.
int signalBarsForDbm(int dbm, bool known);

struct StatusData {
  // Controller battery. batteryVisible = false blanks the indicator for
  // the low-battery warning's blink (see low_battery.h).
  int batteryPercent = 0;
  bool batteryVisible = true;
  bool charging = false;
  UsbPower usbPower = UsbPower::kNotDetected;

  LinkType link = LinkType::kXbee;
  // False once no downlink has arrived for a while: the middle shows
  // "Searching" and the signal bars drop to empty.
  bool connected = false;
  int signalBars = 0;

  DownlinkPacket::Handedness handedness = DownlinkPacket::Handedness::kUnknown;
  // Empty when no droid is selected.
  char droidName[DroidEntry::kMaxNameLength + 1] = {};

  // 0-100, or -1 when Amidala hasn't reported one.
  int volumePercent = -1;
  int throttlePercent = -1;
  int droidBatteryPercent = -1;

  bool estop = false;
  bool motorsDisabled = false;
  bool muse = false;
  bool autoDome = false;
  // The e-stop banner alternates between solid and outlined; this picks
  // the phase.
  bool estopSolidPhase = true;

  // See sequence_banner.h for when a sequence is shown.
  bool sequenceVisible = false;
  char sequenceName[DownlinkPacket::kSequenceNameLength + 1] = {};
  int sequenceProgressPercent = -1;  // -1 = no progress bar
};

// Copies a downlink's droid fields into `data`: handedness, volume,
// throttle, droid battery and the mode/alert flags. A percentage of
// DownlinkPacket::kUnknown becomes -1, and anything over 100 is clamped.
// The sequence fields go through SequenceBanner instead.
void applyDownlink(const DownlinkPacket &downlink, StatusData *data);

void renderStatusScreen(const StatusData &data, StatusLayout layout,
                        FrameBuffer *frame);
