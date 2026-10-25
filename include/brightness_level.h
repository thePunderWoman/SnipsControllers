#pragma once

// A brightness setting as a percentage, 10% to 100% in steps of 10,
// shared by the screen and the status LED and adjusted from the
// Brightness menu. Knows nothing about either device: 100% is each one's
// own ceiling (see screenContrastFor() and ledBrightnessScale()), not
// necessarily full hardware drive. There is no 0%; the dim tier and
// power-off handle "darker than the setting".
namespace BrightnessLevel {

constexpr int kMinPercent = 10;
constexpr int kMaxPercent = 100;
constexpr int kStepPercent = 10;

// Clamps to the range and rounds to the nearest step, so an off-grid or
// out-of-range value (e.g. corrupt NVS data) still lands on a real level.
int sanitize(int percent);

// One step up or down, stopping at the ends rather than wrapping — the
// Brightness menu's Right and Left.
int increase(int percent);
int decrease(int percent);

// One step up, wrapping from 100% back to 10% — the Brightness menu's
// Enter, for stepping through every level with one button.
int cycle(int percent);

}  // namespace BrightnessLevel
