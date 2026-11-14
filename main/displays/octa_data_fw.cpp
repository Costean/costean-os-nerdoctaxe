// Firmware data provider for the Octa-core LCD UI. Read-only; uses existing getters only.
#include "octa_data.h"
#include <time.h>
#include <string.h>
#include "esp_app_desc.h"
#include "global_state.h"
#include "../boards/board.h"
#include "esp_timer.h"
#include "../costean/costean_core.h"
#include "../stratum/stratum_manager_dual_pool.h"
#include <stdio.h>

void octa_fill(OctaData &d)
{
    memset(&d, 0, sizeof(d));
    Board *b = SYSTEM_MODULE.getBoard();
    if (!b) return;
    d.n = b->getAsicCount(); if (d.n > 8) d.n = 8; if (d.n < 0) d.n = 0;
    for (int i = 0; i < d.n; i++) { d.chipHr[i] = HASHRATE_MONITOR.getChipHashrate(i); d.chipT[i] = b->getChipTemp(i); }
    d.hr = SYSTEM_MODULE.getCurrentHashrate();
    d.power = POWER_MANAGEMENT_MODULE.getPower();
    d.vin = POWER_MANAGEMENT_MODULE.getVoltage() / 1000.0f;
    d.asicT = POWER_MANAGEMENT_MODULE.getChipTempMax();
    d.vrT = POWER_MANAGEMENT_MODULE.getVRTemp();
    d.rpm = POWER_MANAGEMENT_MODULE.getFanRPM(0);
    d.freq = b->getEffectiveAsicFrequency();   // what the governor runs now
    d.mv = b->getEffectiveAsicVoltageMillis();
    d.shutdown = POWER_MANAGEMENT_MODULE.isShutdown();
    // ui7: extra read-only telemetry
    d.vrIntT = POWER_MANAGEMENT_MODULE.getVRTempInt();
    d.fanPct = POWER_MANAGEMENT_MODULE.getFanPerc(0);
    {   // measured Vout: same getter stock displayDriver.cpp uses, rate-limited to 1 Hz
        static float s_vout = 0; static int64_t s_last = -2000000;
        int64_t now = esp_timer_get_time();
        if (now - s_last >= 1000000) { s_vout = b->getVout(); s_last = now; }
        d.vout = s_vout;
    }
    d.uptime = (uint32_t) ((esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000);
    { const char *ip = SYSTEM_MODULE.getIPAddress(); snprintf(d.ip, sizeof d.ip, "%s", ip ? ip : ""); }
    if (STRATUM_MANAGER) {
        d.acc = STRATUM_MANAGER->getSharesAccepted();
        d.rej = STRATUM_MANAGER->getSharesRejected();
        d.bestDiff = STRATUM_MANAGER->getBestDiff();
        d.netDiff = STRATUM_MANAGER->getNetworkDifficulty();
        d.foundBlocks = STRATUM_MANAGER->getFoundBlocks();
    }
    {
        CosteanLcdInfo ci; costean_lcd_info(ci);
        memcpy(d.profile, ci.profile, sizeof d.profile); memcpy(d.govState, ci.state, sizeof d.govState);
        memcpy(d.window, ci.window, sizeof d.window);
        d.lcdMode = ci.lcdMode; d.throttled = ci.throttled; d.scheduled = ci.scheduled;
        d.limChip = ci.limitChip; d.limVr = ci.limitVr; d.limPow = ci.limitPower;
    }
    if (STRATUM_MANAGER && STRATUM_MANAGER->isDualPool()) {
        StratumManagerDualPool *m = (StratumManagerDualPool *) STRATUM_MANAGER;
        CosteanPoolView v[2]; memset(v, 0, sizeof v);
        m->getSplitView(v, 2);
        d.split = true;
        for (int i = 0; i < 2; i++) {
            d.pool[i].cfg = v[i].configured; d.pool[i].up = v[i].healthy; d.pool[i].weight = v[i].weight; d.pool[i].eff = v[i].effective;
            d.pool[i].ghs = d.hr * v[i].jobShare; d.pool[i].acc = v[i].accepted; d.pool[i].rej = v[i].rejected;
            const char *h = m->getPoolHost(i); snprintf(d.pool[i].host, sizeof d.pool[i].host, "%s", h ? h : "");
            if (d.pool[i].weight > 0 && !d.pool[i].up) d.failover = true;
        }
    }
    time_t t = time(NULL);
    d.utc = (t > 1700000000) ? (int64_t) t : 0;
    d.version = esp_app_get_description()->version;
}
