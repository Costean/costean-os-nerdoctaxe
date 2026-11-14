#pragma once
// Costean OS one-time "safe start" clamp for the NerdOctAxe Gamma.
// Pure logic, host-testable: given saved values (or none), decide clamped values.
#include <stdint.h>

// 1.1.2-costean.1: fresh NVS key (<=15 chars) so the one-time 700/1150 clamp runs once after
// installing this build, by USB factory image or by OTA.
#define SAFE_BOOT_NVS_KEY "safe_boot_c112"
#define SAFE_BOOT_FREQ_MHZ 700
#define SAFE_BOOT_VOLT_MV 1150

struct SafeBootResult {
    bool setFreq;
    bool setVolt;
    uint16_t freq;
    uint16_t volt;
};

// alreadyDone: flag present -> never touch user settings again.
// hasF/hasV: whether a value is stored in NVS (absent => board default, never above 700/1150, applies anyway).
static inline SafeBootResult safeBootDecide(bool alreadyDone, bool hasF, uint16_t f, bool hasV, uint16_t v)
{
    SafeBootResult r = {false, false, f, v};
    if (alreadyDone) return r;
    if (hasF && f > SAFE_BOOT_FREQ_MHZ) { r.setFreq = true; r.freq = SAFE_BOOT_FREQ_MHZ; }
    if (hasV && v > SAFE_BOOT_VOLT_MV) { r.setVolt = true; r.volt = SAFE_BOOT_VOLT_MV; }
    return r;
}
