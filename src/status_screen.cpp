#include "status_screen.h"

#include <cstdio>
#include <cstring>

#include "snips_logo_small_bitmap.h"
#include "status_font.h"

namespace {

using StatusFont::drawText;
using StatusFont::textWidth;

constexpr int kW = FrameBuffer::kWidth;

// The middle area every layout draws its main content in, between the
// top bar (rows 0-8) and the bottom row (rows 55-63).
struct Box {
  int x, y, w, h;
};
constexpr Box kFullMiddle = {0, 11, kW, 43};
// Side gauges keeps its middle clear of the two gauges.
constexpr Box kGaugeMiddle = {10, 11, 108, 43};
constexpr int kBottomRowY = 56;

// Icons, '#' = lit. Each is drawn with FrameBuffer::drawPattern().
struct Icon {
  const char *const *rows;
  int height;
  int width;
};
// The first row's literal length sets the width, so every row of an icon
// must be the same length.
#define FIRST(a, ...) a
#define ICON(name, ...)                                                   \
  constexpr const char *name##Rows[] = {__VA_ARGS__};                     \
  constexpr Icon name = {name##Rows, sizeof(name##Rows) / sizeof(char *), \
                         static_cast<int>(sizeof(FIRST(__VA_ARGS__)) - 1)};

ICON(kXbeeIcon, "..####..", ".#....#.", "#.####.#", "#...#..#", "#..#...#",
     "#.####.#", ".#....#.", "..####..")
ICON(kWifiIcon, "..#####..", ".#.....#.", "#..###..#", "..#...#..",
     ".........", "....#....")
ICON(kBluetoothIcon, "..#..", "..##.", "#.#.#", ".###.", "..#..", ".###.",
     "#.#.#", "..##.", "..#..")
ICON(kMuseIcon, "..#####", "..#...#", "..#...#", "..#...#", "###.###",
     "###.###")
ICON(kAutoDomeIcon, "...#####...", ".##.....##.", "#....#....#",
     "#...#.#...#", "#...###...#", "#...#.#...#", "###########")
ICON(kSpeakerIcon, "...#....", "..##..#.", "####...#", "####.#.#",
     "####...#", "..##..#.", "...#....")
ICON(kMutedIcon, "...#....", "..##....", "####.#.#", "####..#.",
     "####.#.#", "..##....", "...#....")
ICON(kThrottleIcon, "..#####..", ".#.....#.", "#.....#.#", "#....#..#",
     "#..##...#", "#..##...#", "#.......#")
ICON(kLockIcon, "..###..", ".#...#.", ".#...#.", "#######", "###.###",
     "###.###", "#######")
ICON(kPlayIcon, "#...", "##..", "###.", "####", "###.", "##..", "#...")
ICON(kBoltIcon, "...##", "..##.", ".##..", "#####", "..##.", ".##..",
     "##...")

#undef ICON
#undef FIRST

void drawIcon(FrameBuffer *f, const Icon &icon, int x, int y, int scale = 1) {
  f->drawPattern(x, y, icon.rows, icon.height, true, scale);
}

void centerText(FrameBuffer *f, const char *text, int x0, int width, int y,
                int size = 1, bool on = true) {
  drawText(f, x0 + (width - textWidth(text, size)) / 2, y, text, size, on);
}

// Picks size 2 when `text` fits in `maxWidth`, otherwise size 1, truncated
// to fit.
int fitText(const char *text, int maxWidth, char *out, size_t outCapacity) {
  std::snprintf(out, outCapacity, "%s", text);
  if (textWidth(out, 2) <= maxWidth) return 2;
  const int max = StatusFont::maxChars(maxWidth, 1);
  if (max >= 0 && static_cast<size_t>(max) < outCapacity) out[max] = '\0';
  return 1;
}

void formatPercent(int percent, char *out, size_t outCapacity) {
  if (percent < 0) {
    std::snprintf(out, outCapacity, "--");
  } else {
    std::snprintf(out, outCapacity, "%d%%", percent);
  }
}

// ---- shared pieces --------------------------------------------------------

void drawBattery(FrameBuffer *f, int x, int y, int percent) {  // 17x8
  f->drawRect(x, y, 15, 8);
  f->fillRect(x + 15, y + 2, 2, 4);
  int segments = percent <= 0 ? 0 : (percent + 24) / 25;
  if (segments > 4) segments = 4;
  for (int i = 0; i < segments; ++i) f->fillRect(x + 2 + i * 3, y + 2, 2, 4);
}

void drawMiniBattery(FrameBuffer *f, int x, int y, int percent) {  // 11x6
  f->drawRect(x, y, 9, 6);
  f->fillRect(x + 9, y + 2, 2, 2);
  f->fillRect(x + 2, y + 2, (5 * percent + 50) / 100, 2);
}

void drawSignalBars(FrameBuffer *f, int x, int y, int bars) {  // 11x8
  for (int i = 0; i < 4; ++i) {
    const int height = 2 * (i + 1);
    if (i < bars) {
      f->fillRect(x + i * 3, y + 8 - height, 2, height);
    } else {
      f->fillRect(x + i * 3, y + 7, 2, 1);
    }
  }
}

void drawVolumeBars(FrameBuffer *f, int x, int y, int percent) {  // 29x7
  const int lit = percent < 0 ? 0 : (percent + 5) / 10;
  for (int i = 0; i < 10; ++i) {
    const int height = 1 + (i * 6 + 4) / 9;
    if (i < lit) {
      f->fillRect(x + i * 3, y + 7 - height, 2, height);
    } else {
      f->fillRect(x + i * 3, y + 6, 2, 1);
    }
  }
}

const Icon &linkIcon(LinkType link) {
  switch (link) {
    case LinkType::kWifi: return kWifiIcon;
    case LinkType::kBluetooth: return kBluetoothIcon;
    default: return kXbeeIcon;
  }
}

const Icon &volumeIcon(const StatusData &d) {
  return d.volumePercent == 0 ? kMutedIcon : kSpeakerIcon;
}

char handLetter(DownlinkPacket::Handedness hand) {
  switch (hand) {
    case DownlinkPacket::Handedness::kLeft: return 'L';
    case DownlinkPacket::Handedness::kRight: return 'R';
    default: return '?';
  }
}

void drawTopBar(FrameBuffer *f, const StatusData &d) {
  const Icon &link = linkIcon(d.link);
  drawIcon(f, link, 0, d.link == LinkType::kWifi ? 1 : 0);
  int x = link.width + 2;
  drawSignalBars(f, x, 0, d.connected ? d.signalBars : 0);
  x += 14;

  f->fillRect(x, 0, 9, 9);
  const char hand[2] = {handLetter(d.handedness), '\0'};
  drawText(f, x + 2, 1, hand, 1, false);
  const int leftEnd = x + 9;

  const int batteryX = kW - 17;
  int rightStart = batteryX - 30;
  if (d.batteryVisible) {
    drawBattery(f, batteryX, 0, d.batteryPercent);
    char text[6];
    if (!d.charging && d.usbPower != UsbPower::kNotDetected) {
      std::snprintf(text, sizeof(text), "USB");
    } else {
      formatPercent(d.batteryPercent, text, sizeof(text));
    }
    int textX = batteryX - 2 - textWidth(text);
    if (d.charging) {
      drawIcon(f, kBoltIcon, batteryX - 7, 0);
      textX = batteryX - 9 - textWidth(text);
    }
    drawText(f, textX, 1, text);
    rightStart = textX;
  }

  const Icon *modes[2];
  int modeCount = 0;
  if (d.muse) modes[modeCount++] = &kMuseIcon;
  if (d.autoDome) modes[modeCount++] = &kAutoDomeIcon;
  int total = 0;
  for (int i = 0; i < modeCount; ++i) total += modes[i]->width + (i ? 4 : 0);
  int modeX = (leftEnd + rightStart) / 2 - total / 2;
  for (int i = 0; i < modeCount; ++i) {
    drawIcon(f, *modes[i], modeX, 1);
    modeX += modes[i]->width + 4;
  }
}

// "Droid [batt] 74%", centered on cx.
void drawDroidBattery(FrameBuffer *f, int cx, int y, int percent) {
  const char *label = "Droid";
  char text[6];
  formatPercent(percent, text, sizeof(text));
  const int width = textWidth(label) + 4 + 11 + 3 + textWidth(text);
  int x = cx - width / 2;
  drawText(f, x, y, label);
  x += textWidth(label) + 4;
  drawMiniBattery(f, x, y + 1, percent);
  drawText(f, x + 14, y, text);
}

// Throttle icon and value, right-aligned at rightX. A padlock and "OFF"
// while motors are disabled.
void drawThrottleReadout(FrameBuffer *f, const StatusData &d, int rightX,
                         int y) {
  char text[6];
  if (d.motorsDisabled) {
    std::snprintf(text, sizeof(text), "OFF");
  } else {
    formatPercent(d.throttlePercent, text, sizeof(text));
  }
  const Icon &icon = d.motorsDisabled ? kLockIcon : kThrottleIcon;
  const int textX = rightX - textWidth(text);
  drawText(f, textX, y, text);
  drawIcon(f, icon, textX - 3 - icon.width, y);
}

void drawVolumeReadout(FrameBuffer *f, const StatusData &d, int x, int y) {
  char text[6];
  formatPercent(d.volumePercent, text, sizeof(text));
  drawIcon(f, volumeIcon(d), x, y);
  drawText(f, x + 11, y, text);
}

// The droid name centered in `area`: size 2 at y2 when it fits, else
// size 1 at y1.
void drawDroidName(FrameBuffer *f, const StatusData &d, const Box &area,
                   int y2, int y1) {
  char name[DroidEntry::kMaxNameLength + 1];
  const int size = fitText(d.droidName, area.w, name, sizeof(name));
  centerText(f, name, area.x, area.w, size == 2 ? y2 : y1, size);
}

// No droid selected, or the link is down: the Snips logo with a short
// message beside it, in place of the layout's normal middle.
bool drawIdleMiddle(FrameBuffer *f, const StatusData &d, const Box &box) {
  const bool noDroid = d.droidName[0] == '\0';
  if (!noDroid && d.connected) return false;

  const int logoX = box.x + 4;
  const int logoY = box.y + (box.h - SnipsLogoSmall::kHeight) / 2;
  f->drawBitmap(logoX, logoY, SnipsLogoSmall::kBitmap, SnipsLogoSmall::kWidth,
                SnipsLogoSmall::kHeight);
  const int textX = logoX + SnipsLogoSmall::kWidth + 4;
  const int textW = box.x + box.w - textX;

  char line1[DroidEntry::kMaxNameLength + 1];
  std::snprintf(line1, sizeof(line1), "%s", noDroid ? "No droid" : d.droidName);
  const int max = StatusFont::maxChars(textW);
  if (max >= 0 && static_cast<size_t>(max) < sizeof(line1)) line1[max] = '\0';
  centerText(f, line1, textX, textW, box.y + 11);
  centerText(f, noDroid ? "Pick one in" : "Searching...", textX, textW,
             box.y + 23);
  if (noDroid) centerText(f, "the menu", textX, textW, box.y + 32);
  return true;
}

// ---- layouts --------------------------------------------------------------

void drawStatusBars(FrameBuffer *f, const StatusData &d) {
  if (!drawIdleMiddle(f, d, kFullMiddle)) {
    drawDroidName(f, d, kFullMiddle, 18, 22);
    if (d.droidBatteryPercent >= 0) drawDroidBattery(f, kW / 2, 38, d.droidBatteryPercent);
  }
  drawVolumeReadout(f, d, 0, kBottomRowY);
  drawThrottleReadout(f, d, kW - 1, kBottomRowY);
}

void drawLogo(FrameBuffer *f, const StatusData &d) {
  if (!drawIdleMiddle(f, d, kFullMiddle)) {
    f->drawBitmap(2, 14, SnipsLogoSmall::kBitmap, SnipsLogoSmall::kWidth,
                  SnipsLogoSmall::kHeight);
    const Box area = {26, kFullMiddle.y, kW - 26, kFullMiddle.h};
    drawDroidName(f, d, area, 18, 22);
    if (d.droidBatteryPercent >= 0) {
      drawDroidBattery(f, area.x + area.w / 2, 38, d.droidBatteryPercent);
    }
  }
  drawVolumeReadout(f, d, 0, kBottomRowY);
  drawThrottleReadout(f, d, kW - 1, kBottomRowY);
}

void drawThrottleFirst(FrameBuffer *f, const StatusData &d) {
  if (!drawIdleMiddle(f, d, kFullMiddle)) {
    char name[DroidEntry::kMaxNameLength + 1];
    std::snprintf(name, sizeof(name), "%s", d.droidName);
    centerText(f, name, 0, kW, 12);

    if (d.motorsDisabled) {
      const int width = kLockIcon.width * 2 + 4 + textWidth("LOCKED", 2);
      const int x = (kW - width) / 2;
      drawIcon(f, kLockIcon, x, 26, 2);
      drawText(f, x + kLockIcon.width * 2 + 4, 27, "LOCKED", 2);
    } else {
      const bool known = d.throttlePercent >= 0;
      char number[5];
      if (known) {
        std::snprintf(number, sizeof(number), "%d", d.throttlePercent);
      } else {
        std::snprintf(number, sizeof(number), "--");
      }
      const int width =
          textWidth(number, 3) + (known ? 3 + textWidth("%", 2) : 0);
      const int x = (kW - width) / 2;
      drawText(f, x, 22, number, 3);
      if (known) drawText(f, x + textWidth(number, 3) + 3, 29, "%", 2);
      f->drawRect(4, 46, 120, 5);
      if (d.throttlePercent > 0) {
        f->fillRect(6, 48, (116 * d.throttlePercent + 50) / 100, 1);
      }
    }
  }

  drawIcon(f, volumeIcon(d), 0, kBottomRowY);
  drawVolumeBars(f, 11, kBottomRowY, d.volumePercent);
  if (d.connected && d.droidBatteryPercent >= 0) {
    char text[6];
    formatPercent(d.droidBatteryPercent, text, sizeof(text));
    const int textX = kW - 1 - textWidth(text);
    drawText(f, textX, kBottomRowY, text);
    drawMiniBattery(f, textX - 14, kBottomRowY + 1, d.droidBatteryPercent);
  }
}

// A 6x43 vertical gauge from row 21 to the bottom, filled from below.
void drawGauge(FrameBuffer *f, int x, int percent) {
  f->drawRect(x, 21, 6, 43);
  if (percent > 0) {
    const int height = (39 * percent + 50) / 100;
    f->fillRect(x + 2, 62 - height, 2, height);
  }
}

void drawSideGauges(FrameBuffer *f, const StatusData &d) {
  drawIcon(f, volumeIcon(d), 0, 12);
  drawGauge(f, 0, d.volumePercent);
  if (d.motorsDisabled) {
    drawIcon(f, kLockIcon, kW - kLockIcon.width, 12);
    drawGauge(f, kW - 6, 0);
  } else {
    drawIcon(f, kThrottleIcon, kW - kThrottleIcon.width, 12);
    drawGauge(f, kW - 6, d.throttlePercent);
  }

  if (!drawIdleMiddle(f, d, kGaugeMiddle)) {
    drawDroidName(f, d, kGaugeMiddle, 20, 24);
    if (d.droidBatteryPercent >= 0) drawDroidBattery(f, kW / 2, 38, d.droidBatteryPercent);
  }

  char text[6];
  formatPercent(d.volumePercent, text, sizeof(text));
  drawText(f, 10, kBottomRowY, text);
  if (d.motorsDisabled) {
    std::snprintf(text, sizeof(text), "OFF");
  } else {
    formatPercent(d.throttlePercent, text, sizeof(text));
  }
  drawText(f, 118 - textWidth(text), kBottomRowY, text);
}

// ---- overlays -------------------------------------------------------------

void drawSequence(FrameBuffer *f, const StatusData &d, const Box &box) {
  f->fillRect(box.x, box.y, box.w, box.h, false);
  const char *label = "Playing";
  const int width = kPlayIcon.width + 4 + textWidth(label);
  const int x = box.x + (box.w - width) / 2;
  drawIcon(f, kPlayIcon, x, box.y + 2);
  drawText(f, x + kPlayIcon.width + 4, box.y + 2, label);

  char name[DownlinkPacket::kSequenceNameLength + 1];
  const int size = fitText(d.sequenceName, box.w, name, sizeof(name));
  centerText(f, name, box.x, box.w, box.y + (size == 2 ? 14 : 18), size);

  if (d.sequenceProgressPercent >= 0) {
    const int barX = box.x + 8, barW = box.w - 16, barY = box.y + 35;
    f->drawRect(barX, barY, barW, 5);
    f->fillRect(barX + 2, barY + 2,
                ((barW - 4) * d.sequenceProgressPercent + 50) / 100, 1);
  }
}

void drawEstop(FrameBuffer *f, bool solid) {
  f->fillRect(kFullMiddle.x, kFullMiddle.y, kFullMiddle.w, kFullMiddle.h,
              solid);
  if (!solid) {
    f->drawRect(kFullMiddle.x, kFullMiddle.y, kFullMiddle.w, kFullMiddle.h);
  }
  centerText(f, "E-STOP", 0, kW, 22, 3, !solid);
}

}  // namespace

const char *statusLayoutLabel(StatusLayout layout) {
  switch (layout) {
    case StatusLayout::kStatusBars: return "Status bars";
    case StatusLayout::kThrottleFirst: return "Throttle first";
    case StatusLayout::kSideGauges: return "Side gauges";
    case StatusLayout::kLogo: return "Snips logo";
    default: return "Unknown";
  }
}

StatusLayout statusLayoutFromStored(int stored) {
  if (stored < 0 || stored >= static_cast<int>(StatusLayout::kCount)) {
    return StatusLayout::kStatusBars;
  }
  return static_cast<StatusLayout>(stored);
}

int signalBarsForDbm(int dbm, bool known) {
  if (!known) return 0;
  if (dbm >= -65) return 4;
  if (dbm >= -75) return 3;
  if (dbm >= -85) return 2;
  if (dbm >= -95) return 1;
  return 0;
}

namespace {
int downlinkPercent(uint8_t value) {
  if (value == DownlinkPacket::kUnknown) return -1;
  return value > 100 ? 100 : value;
}
}  // namespace

void applyDownlink(const DownlinkPacket &downlink, StatusData *data) {
  data->handedness = downlink.handedness;
  data->volumePercent = downlinkPercent(downlink.volumePercent);
  data->throttlePercent = downlinkPercent(downlink.throttlePercent);
  data->droidBatteryPercent = downlinkPercent(downlink.droidBatteryPercent);
  data->estop = downlink.flags & DownlinkPacket::kFlagEstop;
  data->motorsDisabled = downlink.flags & DownlinkPacket::kFlagMotorsDisabled;
  data->muse = downlink.flags & DownlinkPacket::kFlagMuse;
  data->autoDome = downlink.flags & DownlinkPacket::kFlagAutoDome;
}

void renderStatusScreen(const StatusData &data, StatusLayout layout,
                        FrameBuffer *frame) {
  frame->clear();
  drawTopBar(frame, data);
  switch (layout) {
    case StatusLayout::kThrottleFirst: drawThrottleFirst(frame, data); break;
    case StatusLayout::kSideGauges: drawSideGauges(frame, data); break;
    case StatusLayout::kLogo: drawLogo(frame, data); break;
    default: drawStatusBars(frame, data); break;
  }

  if (data.estop) {
    drawEstop(frame, data.estopSolidPhase);
  } else if (data.sequenceVisible && data.connected) {
    drawSequence(frame, data,
                 layout == StatusLayout::kSideGauges ? kGaugeMiddle : kFullMiddle);
  }
}
