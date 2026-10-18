#include "loop_pacer.h"

unsigned long LoopPacer::idleMsAfterPass(unsigned long passStartMs,
                                         unsigned long nowMs,
                                         unsigned long periodMs) {
  const unsigned long elapsedMs = nowMs - passStartMs;
  return elapsedMs >= periodMs ? 0 : periodMs - elapsedMs;
}
