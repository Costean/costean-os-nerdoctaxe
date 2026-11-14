// DEMO data provider for the host renderer (never a real miner). Start settings 700 MHz / 1150 mV.
#include "displays/octa_data.h"
#include "lvgl.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
bool g_sim_night = false; int g_sim_fault = 0; int g_sim_blocks = 0; int g_sim_failover = 0; int g_sim_throttle = 0; int g_sim_eco = 0;
void octa_fill(OctaData &d) {
    memset(&d, 0, sizeof d);
    float t = lv_tick_get() / 1000.0f;
    const float base[8] = {1428, 1419, 1436, 1411, 1425, 1347, 1433, 1422};
    const float tb[8] = {55.1, 56.4, 58.8, 59.2, 60.4, 63.1, 57.6, 54.9};
    float k = g_sim_eco ? 0.86f : g_sim_throttle ? 0.93f : 1.0f;
    d.n = 8; d.hr = 0;
    for (int i = 0; i < 8; i++) { d.chipHr[i] = k * (base[i] + 11 * sinf(t * 0.7f + i)); d.chipT[i] = tb[i] + 0.4f * sinf(t * 0.3f + i) + (g_sim_fault ? 12 : 0) + (g_sim_throttle ? 5 : 0) - (g_sim_eco ? 6 : 0); d.hr += d.chipHr[i]; }
    d.power = d.hr / 1000 * (g_sim_eco ? 14.9f : 16.4f); d.vin = 12.03f; d.asicT = 0; for (int i = 0; i < 8; i++) if (d.chipT[i] > d.asicT) d.asicT = d.chipT[i];
    d.vrT = g_sim_throttle ? 74.2f : 64.8f; d.vrIntT = 71.2f; d.vout = g_sim_eco ? 1.097f : 1.147f; d.rpm = 4180; d.fanPct = 62;
    d.freq = g_sim_eco ? 600 : g_sim_throttle ? 650 : 700; d.mv = g_sim_eco ? 1100 : 1150;
    d.uptime = 10 * 3600 + 24 * 60 + (uint32_t) t;
    d.acc = 1204 + (uint64_t) (t / 1.3f); d.rej = 2; d.bestDiff = 1890000000ULL; d.netDiff = 1.27e14; d.foundBlocks = g_sim_blocks;
    snprintf(d.ip, sizeof d.ip, "192.0.2.10");
    d.utc = g_sim_night ? 1793691000 : 1793649600;
    d.version = "1.2.0-costean.1";
    snprintf(d.profile, sizeof d.profile, "%s", g_sim_eco ? "ECO" : "STOCK");
    snprintf(d.govState, sizeof d.govState, "%s", g_sim_throttle ? "THROTTLE" : "HOLD");
    if (g_sim_eco) snprintf(d.window, sizeof d.window, "Quiet night");
    else if (g_sim_night) snprintf(d.window, sizeof d.window, "Night screen");
    d.lcdMode = g_sim_night ? 1 : 0; d.throttled = g_sim_throttle; d.scheduled = g_sim_eco;
    d.limChip = 66; d.limVr = 78; d.limPow = 200;
    d.split = true;
    const char *host[2] = {"pool.example.org", "backup.example.net"};
    int w[2] = {70, 30};
    for (int i = 0; i < 2; i++) {
        d.pool[i].cfg = true; d.pool[i].weight = w[i];
        d.pool[i].up = !(g_sim_failover && i == 1);
        snprintf(d.pool[i].host, sizeof d.pool[i].host, "%s", host[i]);
    }
    d.failover = g_sim_failover;
    d.pool[0].eff = g_sim_failover ? 100 : 70; d.pool[1].eff = g_sim_failover ? 0 : 30;
    d.pool[0].ghs = d.hr * d.pool[0].eff / 100.0f; d.pool[1].ghs = d.hr * d.pool[1].eff / 100.0f;
    d.pool[0].acc = d.acc * 7 / 10; d.pool[1].acc = d.acc - d.pool[0].acc; d.pool[0].rej = 1; d.pool[1].rej = 1;
}
