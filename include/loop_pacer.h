#pragma once

// Pure timing logic for pacing loop(). Knows nothing about FreeRTOS or
// delay() — SnipsController.ino idles for the returned time at the end
// of each pass.
//
// Without pacing, loop() runs back to back and keeps the CPU at full
// power even though almost every pass finds nothing new. Idling between
// passes lets the FreeRTOS idle task halt the CPU until the next tick
// interrupt. Every timing-sensitive part of the firmware already runs on
// millis() rather than counting passes (button debounce is 20 ms, the
// uplink goes out every 50 ms), so a few milliseconds of idle between
// passes never shows up as lag.
namespace LoopPacer {

// One loop pass every 5 ms (200 Hz): well under the 20 ms button
// debounce and the 50 ms uplink interval.
constexpr unsigned long kPassPeriodMs = 5;

// How long to idle after a pass that started at `passStartMs`, so passes
// start every `periodMs`. Zero if the pass already ran that long (e.g. a
// blocking RSSI query or OLED redraw). Correct across millis() rollover.
unsigned long idleMsAfterPass(unsigned long passStartMs, unsigned long nowMs,
                              unsigned long periodMs = kPassPeriodMs);

}  // namespace LoopPacer
