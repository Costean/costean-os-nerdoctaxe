// Host unit test for main/displays/octa_logic.h: zig c++ -std=c++17 -I main test/host/octa_logic_test.cpp && ./a.out
#include "displays/octa_logic.h"
#include <cstdio>
#include <cstring>
#include <cmath>
static int fails = 0;
#define CHECK(c) do { if (c) printf("ok   %s\n", #c); else { printf("FAIL %s\n", #c); fails++; } } while (0)
int main() {
    CHECK(octa_lerp_rgb(0x22D3EE, 0xF43F5E, 0) == 0x22D3EE);
    CHECK(octa_lerp_rgb(0x22D3EE, 0xF43F5E, 1) == 0xF43F5E);
    CHECK(octa_lerp_rgb(0xFF0000, 0x00FF00, 0.5f) == 0x807F00);
    CHECK(octa_temp_rgb(40) == 0x22D3EE && octa_temp_rgb(75) == 0xF43F5E && octa_temp_rgb(0) == 0x3A4466);
    double es = octa_expected_s(1.27e14, 6150);   // 6.15 TH/s
    CHECK(es > 8.8e10 && es < 8.9e10);             // ~2810 years
    CHECK(fabs(octa_prob(86400, 86400) - (1 - exp(-1.0))) < 1e-9);
    CHECK(octa_expected_s(0, 6150) == 0);
    char b[24]; octa_fmt_big(b, sizeof b, 1234567); CHECK(strcmp(b, "1.23M") == 0);
    // Eastern Time: 2026-11-02 15:00 EST = 20:00 UTC ; 2026-07-01 12:00 EDT = 16:00 UTC ; DST start 2026-03-08 07:00 UTC -> 03:00 EDT
    CHECK(octa_eastern_hour(1793649600) == 15);
    CHECK(octa_eastern_hour(1782921600) == 12);
    CHECK(octa_eastern_hour(1772953200) == 3);
    CHECK(octa_is_night(23) && octa_is_night(3) && !octa_is_night(7) && !octa_is_night(15));
    printf(fails ? "FAILED (%d)\n" : "PASSED (%d failures)\n", fails); return fails != 0;
}
