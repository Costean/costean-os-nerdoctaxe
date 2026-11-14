#pragma once
// Pure helpers for the Octa-core LCD UI (host-testable, no LVGL).
#include <math.h>
#include <stdint.h>
#include <stdio.h>

// Temperature -> 0xRRGGBB ramp: cyan <50, green 55, amber 65, red >=70
static inline uint32_t octa_lerp_rgb(uint32_t a, uint32_t b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int ar = (int) ((a >> 16) & 255), ag = (int) ((a >> 8) & 255), ab = (int) (a & 255);
    int br = (int) ((b >> 16) & 255), bg = (int) ((b >> 8) & 255), bb = (int) (b & 255);
    int r = ar + (int) ((br - ar) * t), g = ag + (int) ((bg - ag) * t), bl = ab + (int) ((bb - ab) * t);
    return ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) bl;
}
static inline uint32_t octa_temp_rgb(float t)
{
    if (t <= 0) return 0x3A4466;               // unknown
    if (t < 50) return 0x22D3EE;
    if (t < 58) return octa_lerp_rgb(0x22D3EE, 0x34D399, (t - 50) / 8);
    if (t < 65) return octa_lerp_rgb(0x34D399, 0xFBBF24, (t - 58) / 7);
    if (t < 70) return octa_lerp_rgb(0xFBBF24, 0xF43F5E, (t - 65) / 5);
    return 0xF43F5E;
}
// Expected seconds to find a block solo: D * 2^32 / H  (H in GH/s)
static inline double octa_expected_s(double netDiff, double ghs)
{
    if (netDiff <= 0 || ghs <= 0) return 0;
    return netDiff * 4294967296.0 / (ghs * 1e9);
}
// Probability of at least one block in `window_s` seconds
static inline double octa_prob(double expected_s, double window_s)
{
    if (expected_s <= 0) return 0;
    return 1.0 - exp(-window_s / expected_s);
}
// Compact number: 1.05M, 12.4K, 3.2T ...
static inline void octa_fmt_big(char *buf, int n, double v)
{
    const char *u[] = {"", "K", "M", "B", "T", "P", "E"};
    int i = 0;
    while (v >= 1000.0 && i < 6) { v /= 1000.0; i++; }
    if (i == 0) snprintf(buf, n, "%.0f", v);
    else snprintf(buf, n, v < 10 ? "%.2f%s" : v < 100 ? "%.1f%s" : "%.0f%s", v, u[i]);
}
// Human duration for expected time
static inline void octa_fmt_dur(char *buf, int n, double s)
{
    if (s <= 0) { snprintf(buf, n, "--"); return; }
    double y = s / 31557600.0;
    if (y >= 1) { char b[16]; octa_fmt_big(b, sizeof b, y); snprintf(buf, n, "%s yrs", b); }
    else if (s >= 86400) snprintf(buf, n, "%.0f days", s / 86400);
    else if (s >= 3600) snprintf(buf, n, "%.1f h", s / 3600);
    else snprintf(buf, n, "%.0f min", s / 60);
}
// days since 1970-01-01 for civil date (Howard Hinnant)
static inline int64_t octa_days(int y, unsigned m, unsigned d)
{
    y -= m <= 2; const int64_t era = (y >= 0 ? y : y - 399) / 400; const unsigned yoe = (unsigned) (y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t) doe - 719468;
}
static inline int octa_wday(int64_t days) { return (int) ((days % 7 + 11) % 7); } // 0=Sunday
// North American Eastern Time hour (EST/EDT, current US DST rule) from unix time; -1 if unknown.
static inline int octa_eastern_hour(int64_t utc)
{
    if (utc <= 0) return -1;
    int64_t days = utc / 86400; int hour = (int) ((utc % 86400) / 3600);
    // civil year from days
    int y = 1970; while (octa_days(y + 1, 1, 1) <= days) y++;
    int64_t m1 = octa_days(y, 3, 1); int64_t marSun2 = m1 + (7 - octa_wday(m1)) % 7 + 7;
    int64_t n1 = octa_days(y, 11, 1); int64_t novSun1 = n1 + (7 - octa_wday(n1)) % 7;
    bool dst = utc >= marSun2 * 86400 + 7 * 3600 && utc < novSun1 * 86400 + 6 * 3600;
    return (hour + (dst ? -4 : -5) + 24) % 24;
}
static inline bool octa_is_night(int hour) { return hour >= 0 && (hour >= 22 || hour < 7); }
