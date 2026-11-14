// Host unit test for main/safe_boot.h: g++ -std=c++17 -I main test/host/safe_boot_test.cpp && ./a.out
#include "safe_boot.h"
#include <cstdio>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } else printf("ok   %s\n", #c); } while (0)
int main() {
    auto r = safeBootDecide(false, true, 950, true, 1250);           // user's old OC settings
    CHECK(r.setFreq && r.freq == 700 && r.setVolt && r.volt == 1150);
    r = safeBootDecide(false, true, 600, true, 1100);                // already low: untouched
    CHECK(!r.setFreq && !r.setVolt && r.freq == 600 && r.volt == 1100);
    r = safeBootDecide(false, true, 700, true, 1150);                // exactly at limit
    CHECK(!r.setFreq && !r.setVolt);
    r = safeBootDecide(false, true, 725, true, 1120);                // only freq high
    CHECK(r.setFreq && !r.setVolt && r.volt == 1120);
    r = safeBootDecide(false, true, 650, true, 1210);                // only volt high (upstream default volt)
    CHECK(!r.setFreq && r.setVolt && r.volt == 1150 && r.freq == 650);
    CHECK(sizeof(SAFE_BOOT_NVS_KEY) - 1 <= 15);                       // NVS key length limit
    r = safeBootDecide(false, false, 0, false, 0);                   // fresh/factory NVS: defaults apply
    CHECK(!r.setFreq && !r.setVolt);
    r = safeBootDecide(true, true, 950, true, 1250);                 // flag set: never override user
    CHECK(!r.setFreq && !r.setVolt && r.freq == 950 && r.volt == 1250);
    printf("%s (%d failures)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
