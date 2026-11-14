#pragma once
// Snapshot of values the firmware already exposes, read by the Octa-core LCD UI.
#include <stdint.h>
struct OctaData {
    int n;                 // chips (8 on NerdOctAxe)
    float chipHr[8];       // GH/s per chip (HASHRATE_MONITOR)
    float chipT[8];        // degC per chip (board->getChipTemp)
    float hr;              // GH/s total
    float power;           // W
    float vin;             // V input
    float asicT, vrT;      // degC
    int rpm;               // fan 0
    int freq, mv;          // configured MHz / mV
    uint64_t acc, rej;     // shares
    uint64_t bestDiff;     // all-time best share difficulty
    double netDiff;        // network difficulty from the pool (0 = unknown)
    int64_t utc;           // unix time, 0 if not synced
    // ui7 additions (read-only getters, cached values)
    float vrIntT;          // degC, regulator internal sensor (POWER_MANAGEMENT_MODULE.getVRTempInt)
    float vout;            // V, measured ASIC core voltage (board->getVout, sampled <=1 Hz like stock display)
    int fanPct;            // fan 0 duty %
    uint32_t uptime;       // seconds since boot (same formula as /api/v2/dashboard)
    uint32_t foundBlocks;  // blocks found this session
    char ip[20];           // IPv4 string from SYSTEM_MODULE.getIPAddress()
    bool shutdown;
    const char *version;
    // 1.2.0: Costean OS runtime (profiles, governor, schedules, pool split)
    char profile[8];       // ECO / STOCK / TURBO / MANUAL
    char govState[12];     // HOLD / AUTO / TUNE +1 / THROTTLE
    char window[20];       // active schedule window name
    uint8_t lcdMode;       // 0 normal, 1 dim, 2 off
    bool throttled, scheduled;
    int limChip, limVr, limPow;
    bool split;            // dual-pool split mode
    bool failover;         // a pool's share is carried by another pool
    struct Pool { bool cfg, up; int weight, eff; float ghs; uint64_t acc, rej; char host[28]; } pool[2];
};
void octa_fill(OctaData &d);
