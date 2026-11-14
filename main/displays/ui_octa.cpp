// NerdOctAxe LCD UI, Costean OS 1.2 "Lode" look (1.2.0-costean.1).
// Graphite cards, copper and teal accents, crisp OpenSans figures. Own code and own drawings
// only. Fonts: OpenSans Bold + Montserrat already in stock ESP-Miner/LVGL.
//
// Object lifetime rule (boot-freeze fix from 1.1.2-costean.1): displayDriver.cpp deletes the
// children of the splash screens (lv_obj_clean) when it leaves them, and screens/overlays can be
// deleted under us at any time. Every lv_obj_t* this file keeps is registered with track(), which
// clears the pointer from an LV_EVENT_DELETE handler, and every use goes through a null check.
// Pages map onto the stock state machine, so the physical button / auto-cycle are unchanged:
//   MiningScreen   -> 1/4 HASHRATE gauge + vitals + hash trend
//   SettingsScreen -> 2/4 THERMAL & GOVERNOR (limits, throttle, profile, power)
//   BTCScreen      -> 3/4 OCTA MATRIX (8x BM1370 per-chip GH/s + C)
//   GlobalStats    -> 4/4 POOLS & BLOCKS (multi-pool split, best share, network)
// Each page is ONE code-drawn canvas (no per-string label objects) because the LVGL heap is 32 KB.
// Labels DisplayDriver writes to still exist (hidden), so displayDriver.cpp only gained the
// scheduled screen-off hook.
// Read-only: this file never changes frequency, voltage, fans or any setting.
#include "ui_select.h"
#if NERDOCTAXE_LCD_UI3
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "global_state.h"
#include "ui.h"
#include "ui_helpers.h"
#include "displayDriver.h"
#include "octa_data.h"
#include "octa_logic.h"

void on_screen_loaded(lv_event_t *e);

namespace {
// ---- "Lode" palette (own) ----
const uint32_t C_BG = 0x0A0B0E, C_CARD = 0x16181D, C_CARD2 = 0x111317, C_LINE = 0x2A2E36, C_LINE2 = 0x3A3F49,
               C_TEXT = 0xF3EDE4, C_MUTE = 0x8E939C, C_DIM = 0x5C616B, C_COPPER = 0xEE8A4C, C_COPPER2 = 0xB8602F,
               C_TEAL = 0x3FD8C2, C_TEAL2 = 0x229C8C, C_SKY = 0x8FB4FF, C_GREEN = 0x86E39A, C_AMBER = 0xFFB547,
               C_RED = 0xFF5A4E, C_GOLD = 0xF6CB6E;

const lv_font_t *F_XS = &lv_font_montserrat_10, *F_S = &ui_font_OpenSansBold13, *F_14 = &ui_font_OpenSansBold14,
                *F_M = &ui_font_OpenSansBold24, *F_L = &ui_font_OpenSansBold45;

OctaData D;
bool night = false;             // dimmed palette (schedule "dim" window)
uint32_t phase = 0;
const int HN = 64;              // hash trend: one sample / 10 s -> ~10.6 min
float hist[HN]; int hcount = 0; uint32_t hlast = 0;
float peakGh = 0;

lv_obj_t *scr1 = nullptr, *scr2 = nullptr, *scr3 = nullptr, *scr4 = nullptr;
lv_obj_t *cv1 = nullptr, *cv2 = nullptr, *cv3 = nullptr, *cv4 = nullptr;
lv_obj_t *oBurst = nullptr, *oLogo1 = nullptr, *oLogo2 = nullptr;
lv_obj_t *lPool = nullptr;      // hidden: DisplayDriver writes the pool host here

// ---- lifetime tracking: *ref is set to NULL when the object it points to is deleted ----
void onTrackedDelete(lv_event_t *e) {
    lv_obj_t **ref = (lv_obj_t **) lv_event_get_user_data(e);
    if (ref && *ref == lv_event_get_target(e)) *ref = nullptr;
}
lv_obj_t *track(lv_obj_t **ref) {
    if (ref && *ref) lv_obj_add_event_cb(*ref, onTrackedDelete, LV_EVENT_DELETE, ref);
    return ref ? *ref : nullptr;
}
inline void inval(lv_obj_t *o) { if (o) lv_obj_invalidate(o); }

inline lv_color_t C(uint32_t rgb) {
    if (night) { uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
        r = r * 50 / 100; g = g * 48 / 100; b = b * 52 / 100; return lv_color_make(r, g, b); }
    return lv_color_hex(rgb);
}
lv_obj_t *mkScreen() {
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    static lv_style_t st; static bool ini = false;
    if (!ini) { lv_style_init(&st); lv_style_set_bg_color(&st, lv_color_hex(C_BG)); lv_style_set_bg_opa(&st, 255); ini = true; }
    lv_obj_add_style(s, &st, 0);
    return s;
}
lv_obj_t *mkCanvas(lv_obj_t *p, int x, int y, int w, int h, lv_event_cb_t cb) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_event_cb(o, cb, LV_EVENT_DRAW_MAIN, NULL);
    return o;
}
lv_obj_t *g_sink = nullptr;
lv_obj_t *mkHidden(lv_obj_t *p) {
    if (!g_sink) { g_sink = lv_label_create(p); lv_obj_add_flag(g_sink, LV_OBJ_FLAG_HIDDEN); track(&g_sink); }
    return g_sink;
}
lv_obj_t *mkLabel(lv_obj_t *p, const lv_font_t *f, uint32_t col, int x, int y, const char *txt, lv_align_t al = LV_ALIGN_TOP_LEFT) {
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0); lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_label_set_text(l, txt); lv_obj_align(l, al, x, y);
    return l;
}

// ---- drawing primitives (absolute coords inside the draw ctx) ----
int OX = 0, OY = 0;   // origin of the canvas being drawn
void rect(lv_draw_ctx_t *dc, int x, int y, int w, int h, uint32_t col, lv_opa_t opa = 255, int rad = 0) {
    if (w <= 0 || h <= 0) return;
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color = C(col); d.bg_opa = opa; d.radius = rad;
    lv_area_t a = {(lv_coord_t) (OX + x), (lv_coord_t) (OY + y), (lv_coord_t) (OX + x + w - 1), (lv_coord_t) (OY + y + h - 1)};
    lv_draw_rect(dc, &d, &a);
}
void frame(lv_draw_ctx_t *dc, int x, int y, int w, int h, uint32_t col, int bw = 1, int rad = 6, lv_opa_t opa = 255) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_opa = 0; d.radius = rad; d.border_color = C(col); d.border_width = bw; d.border_opa = opa;
    lv_area_t a = {(lv_coord_t) (OX + x), (lv_coord_t) (OY + y), (lv_coord_t) (OX + x + w - 1), (lv_coord_t) (OY + y + h - 1)};
    lv_draw_rect(dc, &d, &a);
}
// graphite card with a short accent vein on the left edge (own detail)
void card(lv_draw_ctx_t *dc, int x, int y, int w, int h, uint32_t accent = C_COPPER, uint32_t fill = C_CARD) {
    rect(dc, x, y, w, h, fill, 255, 6);
    rect(dc, x, y + 7, 2, h > 22 ? 12 : h - 10, accent, 255, 1);
}
void line(lv_draw_ctx_t *dc, float x1, float y1, float x2, float y2, int w, uint32_t col, lv_opa_t opa = 255) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.color = C(col); d.width = w; d.opa = opa; d.round_start = d.round_end = 1;
    lv_point_t a = {(lv_coord_t) lroundf(OX + x1), (lv_coord_t) lroundf(OY + y1)}, b = {(lv_coord_t) lroundf(OX + x2), (lv_coord_t) lroundf(OY + y2)};
    lv_draw_line(dc, &d, &a, &b);
}
void arc(lv_draw_ctx_t *dc, int cx, int cy, int r, int w, int a0, int a1, uint32_t col, lv_opa_t opa = 255, bool rounded = false) {
    lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d); d.color = C(col); d.width = w; d.opa = opa; d.rounded = rounded;
    lv_point_t c = {(lv_coord_t) (OX + cx), (lv_coord_t) (OY + cy)};
    a0 = (a0 % 360 + 360) % 360; a1 = (a1 % 360 + 360) % 360; if (a0 == a1) a1 = (a1 + 1) % 360;
    lv_draw_arc(dc, &d, &c, r, a0, a1);
}
int tw(const lv_font_t *f, const char *s, int ls = 0) { lv_point_t sz; lv_txt_get_size(&sz, s, f, ls, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE); return sz.x; }
// text: al 0 left, 1 center (in w), 2 right (x is the right edge)
void text(lv_draw_ctx_t *dc, int x, int y, const char *s, const lv_font_t *f, uint32_t col, int al = 0, int w = 0, int ls = 0) {
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d); d.color = C(col); d.font = f; d.letter_space = ls;
    int W = tw(f, s, ls);
    int xx = al == 0 ? x : al == 1 ? x + (w - W) / 2 : x - W;
    lv_area_t a = {(lv_coord_t) (OX + xx), (lv_coord_t) (OY + y), (lv_coord_t) (OX + xx + W + 1), (lv_coord_t) (OY + y + lv_font_get_line_height(f))};
    lv_draw_label(dc, &d, &a, s, NULL);
}
// small caps label used on every card
void label(lv_draw_ctx_t *dc, int x, int y, const char *s, uint32_t col = C_MUTE) { text(dc, x, y, s, F_XS, col, 0, 0, 1); }
// value + unit, shrinking from 24 px to 14 px if it would not fit width w
void value(lv_draw_ctx_t *dc, int x, int y, int w, const char *v, const char *unit, uint32_t col) {
    const lv_font_t *f = F_M; int uw = unit && unit[0] ? tw(F_XS, unit) + 3 : 0;
    if (tw(F_M, v) + uw > w) { if (tw(F_M, v) <= w) uw = 0; else f = F_14; }
    int dy = f == F_M ? 0 : 6;
    text(dc, x, y + dy, v, f, col);
    if (uw) text(dc, x + tw(f, v) + 3, y + (f == F_M ? 13 : 9), unit, F_XS, C_MUTE);
}
int limChip() { return D.limChip > 0 ? D.limChip : 66; }
int limVr() { return D.limVr > 0 ? D.limVr : 78; }
int limPow() { return D.limPow > 0 ? D.limPow : 200; }
uint32_t tempCol(float t, int lim) { return t <= 0 ? C_MUTE : t >= lim ? C_RED : t >= lim - 4 ? C_AMBER : t >= lim - 12 ? C_TEXT : C_TEAL; }

// ---- derived state ----
int smallCores() { return 2040; } // BM1370 small cores per chip (ESP-Miner BM1370 driver)
float expectedGh() { return D.freq > 0 && D.n > 0 ? D.freq * (float) smallCores() * D.n / 1000.0f : 0; }
const char *profile() { return D.profile[0] ? D.profile : "MANUAL"; }
uint32_t profileCol() {
    const char *p = profile();
    return p[0] == 'E' ? C_TEAL : p[0] == 'T' ? C_COPPER : p[0] == 'S' ? C_TEXT : C_SKY;
}
// status banner (text, colour)
const char *status(uint32_t &col) {
    float ex = expectedGh();
    if (D.shutdown) { col = C_RED; return "ASIC SHUTDOWN"; }
    if (D.throttled) { col = C_AMBER; return "THROTTLED: COOLING DOWN"; }
    if (D.vrT >= limVr() || D.vrIntT >= 100) { col = C_RED; return "VR HOT: CHECK AIRFLOW"; }
    if (D.asicT >= limChip()) { col = C_RED; return "THERMAL LIMIT"; }
    if (D.hr <= 1) { col = C_AMBER; return "WAITING FOR SHARES"; }
    if (D.uptime < 300) { col = C_AMBER; return "RAMPING UP"; }
    if (D.failover) { col = C_AMBER; return "POOL FAILOVER ACTIVE"; }
    if (ex > 0 && D.hr >= 0.94f * ex) {
        col = C_GREEN;
        if (D.scheduled && D.window[0]) {
            static char b[32]; snprintf(b, sizeof b, "SCHEDULE: %s", D.window);
            for (char *p = b; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            col = C_TEAL; return b;
        }
        return "AT TARGET";
    }
    col = C_AMBER; return "BELOW TARGET";
}
void fmtUp(char *b, int n, uint32_t s) {
    uint32_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    if (d) snprintf(b, n, "UP %lud %luh %lum", (unsigned long) d, (unsigned long) h, (unsigned long) m);
    else snprintf(b, n, "UP %luh %lum", (unsigned long) h, (unsigned long) m);
}
void fmtHr(char *v, int n, const char **unit, float gh) {
    if (gh >= 1000) { snprintf(v, n, gh >= 10000 ? "%.1f" : "%.2f", gh / 1000.0f); *unit = "TH/s"; }
    else { snprintf(v, n, "%.0f", gh); *unit = "GH/s"; }
}

// ---- shared chrome ----
void origin(lv_event_t *e, lv_draw_ctx_t **dc) {
    lv_obj_t *o = lv_event_get_target(e); *dc = lv_event_get_draw_ctx(e);
    lv_area_t a; lv_obj_get_coords(o, &a); OX = a.x1; OY = a.y1;
}
// own mark: a copper lode diamond crossed by a teal vein
void logoMark(lv_draw_ctx_t *dc, int x, int y, int s) {
    float c = s / 2.0f, w = s >= 30 ? 4 : 2;
    line(dc, x + c, y, x + s, y + c, w, C_COPPER); line(dc, x + s, y + c, x + c, y + s, w, C_COPPER);
    line(dc, x + c, y + s, x, y + c, w, C_COPPER); line(dc, x, y + c, x + c, y, w, C_COPPER);
    line(dc, x + s * 0.28f, y + s * 0.62f, x + s * 0.72f, y + s * 0.38f, w, C_TEAL);
}
void wordmark(lv_draw_ctx_t *dc, int x, int y, const lv_font_t *f, int ls, int gap) {
    text(dc, x, y, "COSTEAN", f, C_TEXT, 0, 0, ls);
    text(dc, x + tw(f, "COSTEAN", ls) + gap, y, "OS", f, C_COPPER, 0, 0, ls);
}
int wordmarkW(const lv_font_t *f, int ls, int gap) { return tw(f, "COSTEAN", ls) + gap + tw(f, "OS", ls); }

void header(lv_draw_ctx_t *dc, int page, const char *title) {
    logoMark(dc, 6, 4, 14);
    wordmark(dc, 26, 3, F_S, 1, 4);
    int x = 26 + wordmarkW(F_S, 1, 4) + 8;
    // title only if it clears the profile chip (computed below)
    char tb[32]; bool st = D.govState[0] && strcmp(D.govState, "HOLD") != 0;
    snprintf(tb, sizeof tb, "%s%s%s", profile(), st ? "  " : "", st ? D.govState : "");
    if (x + tw(F_XS, title, 1) + 6 < 276 - (tw(F_XS, tb, 1) + 16)) {
        rect(dc, x - 4, 7, 1, 10, C_LINE2);
        text(dc, x + 1, 6, title, F_XS, C_MUTE, 0, 0, 1);
    }
    // page dots
    for (int i = 0; i < 4; i++) {
        int dx = 284 + i * 8;
        if (i == page - 1) rect(dc, dx - 2, 10, 8, 4, C_COPPER, 255, 2); else rect(dc, dx, 10, 4, 4, C_LINE2, 255, 2);
    }
    // profile chip: profile + governor state
    char b[32];
    bool showState = D.govState[0] && strcmp(D.govState, "HOLD") != 0;
    snprintf(b, sizeof b, "%s%s%s", profile(), showState ? "  " : "", showState ? D.govState : "");
    int w = tw(F_XS, b, 1) + 16, cx = 276 - w;
    uint32_t pc = D.throttled ? C_AMBER : profileCol();
    rect(dc, cx, 4, w, 16, C_CARD, 255, 8);
    frame(dc, cx, 4, w, 16, pc, 1, 8, 160);
    rect(dc, cx + 6, 10, 4, 4, pc, 255, 2);
    text(dc, cx + 13, 7, b, F_XS, C_TEXT, 0, 0, 1);
    rect(dc, 6, 24, 308, 1, C_LINE);
    rect(dc, 6, 24, 34, 1, C_COPPER);
}
void bottom(lv_draw_ctx_t *dc) { rect(dc, 4, 147, 312, 21, C_CARD2, 255, 6); }
void spark(lv_draw_ctx_t *dc, int x, int y, int w, int h, uint32_t col) {
    if (hcount < 2) { rect(dc, x, y + h / 2, w, 2, col, 140); return; }
    float mn = 1e9f, mx = 0; for (int i = 0; i < hcount; i++) { mn = fminf(mn, hist[i]); mx = fmaxf(mx, hist[i]); }
    if (mx - mn < mx * 0.02f) { float c = (mx + mn) / 2; mn = c * 0.98f; mx = c * 1.02f + 1; }
    float px = 0, py = 0;
    for (int i = 0; i < hcount; i++) {
        float xx = x + (float) i * (w - 1) / (HN - 1) + (w - 1) * (float) (HN - hcount) / (HN - 1);
        float yy = y + h - 2 - (hist[i] - mn) / (mx - mn) * (h - 4);
        if (i) line(dc, px, py, xx, yy, 2, col);
        px = xx; py = yy;
    }
}
// thin bar: value vs limit, with the limit as a tick at 100 %
void limitBar(lv_draw_ctx_t *dc, int x, int y, int w, float v, float lim, uint32_t col) {
    rect(dc, x, y, w, 3, C_LINE, 255, 1);
    float f = lim > 0 ? v / (lim * 1.1f) : 0; if (f > 1) f = 1; if (f < 0) f = 0;
    rect(dc, x, y, (int) (w * f), 3, col, 255, 1);
    int lx = x + (int) (w / 1.1f); rect(dc, lx, y - 2, 1, 7, C_MUTE);
}

// ================= 1/4 HASHRATE =================
void draw1(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc); header(dc, 1, "HASH");
    char b[48], v[16]; const char *u;
    card(dc, 4, 28, 150, 116, C_COPPER);
    label(dc, 12, 32, "MEASURED");
    const int cx = 79, cy = 96, R = 46;
    arc(dc, cx, cy, R, 5, 150, 30, C_LINE, 255, true);
    float ex = expectedGh(); float frac = ex > 0 ? D.hr / (ex * 1.15f) : 0; if (frac > 1) frac = 1; if (frac < 0) frac = 0;
    if (frac > 0.01f) {
        // copper -> teal gradient in 12 segments
        int segs = (int) ceilf(frac * 12);
        for (int i = 0; i < segs; i++) {
            float a0 = 150 + i * 20, a1 = fminf(150 + (i + 1) * 20, 150 + frac * 240);
            arc(dc, cx, cy, R, 5, (int) a0, (int) a1 + (i + 1 < segs ? 1 : 0), octa_lerp_rgb(C_COPPER, C_TEAL, i / 11.0f));
        }
        float ae = (150 + frac * 240) * (float) M_PI / 180;
        rect(dc, (int) (cx + R * cosf(ae)) - 3, (int) (cy + R * sinf(ae)) - 3, 7, 7, C_TEXT, 255, 4);
    }
    // expected marker (100 %)
    { float am = (150 + 240 / 1.15f) * (float) M_PI / 180;
      line(dc, cx + (R - 9) * cosf(am), cy + (R - 9) * sinf(am), cx + (R - 4) * cosf(am), cy + (R - 4) * sinf(am), 2, C_MUTE); }
    fmtHr(v, sizeof v, &u, D.hr);
    if (tw(F_L, v) <= 84) text(dc, cx - 60, cy - 37, v, F_L, C_TEXT, 1, 120);
    else text(dc, cx - 60, cy - 24, v, F_M, C_TEXT, 1, 120);
    text(dc, cx - 60, cy + 13, u, F_S, C_COPPER, 1, 120, 1);
    fmtUp(b, sizeof b, D.uptime); text(dc, cx - 60, cy + 31, b, F_XS, C_MUTE, 1, 120);
    // vitals 2x2
    const int X0 = 158, W = 77, H = 56, G = 4;
    struct { const char *l; float val; const char *fmt; const char *unit; uint32_t col; } c[4] = {
        {"CHIP MAX", D.asicT, "%.1f", "C", tempCol(D.asicT, limChip())}, {"VR BOARD", D.vrT, "%.1f", "C", tempCol(D.vrT, limVr())},
        {"POWER", D.power, "%.0f", "W", D.power >= limPow() ? C_AMBER : C_TEXT}, {"J/TH", D.hr > 0 ? D.power / (D.hr / 1000.0f) : 0, "%.1f", "", C_TEAL}};
    for (int i = 0; i < 4; i++) {
        int x = X0 + (i % 2) * (W + G), y = 28 + (i / 2) * (H + G);
        card(dc, x, y, W, H, i == 3 ? C_TEAL : C_COPPER2);
        label(dc, x + 8, y + 6, c[i].l);
        snprintf(v, sizeof v, c[i].val > 0 ? c[i].fmt : "--", c[i].val);
        value(dc, x + 8, y + 20, W - 10, v, c[i].unit, c[i].col);
        if (i == 0) limitBar(dc, x + 8, y + 49, W - 16, D.asicT, limChip(), c[i].col);
        if (i == 1) limitBar(dc, x + 8, y + 49, W - 16, D.vrT, limVr(), c[i].col);
        if (i == 2) limitBar(dc, x + 8, y + 49, W - 16, D.power, limPow(), C_COPPER);
    }
    bottom(dc);
    uint32_t sc; const char *st = status(sc);
    rect(dc, 11, 155, 5, 5, sc, 255, 3); text(dc, 21, 152, st, F_XS, sc, 0, 0, 1);
    spark(dc, 150, 150, 72, 15, C_TEAL);
    snprintf(b, sizeof b, "A/R %llu/%llu", (unsigned long long) D.acc, (unsigned long long) D.rej);
    text(dc, 310, 152, b, F_XS, C_TEXT, 2, 0, 1);
}

// ================= 2/4 THERMAL & GOVERNOR =================
void draw2(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc); header(dc, 2, "THERMAL");
    char v[24], s[48];
    struct { const char *l; float t; int lim; } t[3] = {{"CHIP", D.asicT, limChip()}, {"VR", D.vrT, limVr()}, {"VR INT", D.vrIntT, 105}};
    for (int i = 0; i < 3; i++) {
        int x = 4 + i * 105; uint32_t col = tempCol(t[i].t, t[i].lim);
        card(dc, x, 28, 101, 56, i == 0 ? C_COPPER : C_COPPER2);
        label(dc, x + 8, 34, t[i].l);
        snprintf(v, sizeof v, t[i].t > 0 ? "%.1f" : "--", t[i].t); value(dc, x + 8, 46, 70, v, "C", col);
        snprintf(s, sizeof s, "LIM %d", t[i].lim); text(dc, x + 95, 34, s, F_XS, C_DIM, 2);
        limitBar(dc, x + 8, 77, 85, t[i].t, t[i].lim, col);
    }
    float eff = D.hr > 0 ? D.power / (D.hr / 1000.0f) : 0;
    card(dc, 4, 88, 101, 56, C_TEAL); label(dc, 12, 94, "POWER");
    snprintf(v, sizeof v, D.power > 0 ? "%.1f" : "--", D.power); value(dc, 12, 106, 88, v, "W", D.power >= limPow() ? C_AMBER : C_TEXT);
    if (eff > 0) snprintf(s, sizeof s, "%.1f J/TH  LIM %d W", eff, limPow());
    else snprintf(s, sizeof s, "-- J/TH  LIM %d W", limPow());
    text(dc, 12, 133, s, F_XS, C_TEAL);
    card(dc, 109, 88, 101, 56, C_COPPER2); label(dc, 117, 94, "CORE VOLT");
    snprintf(v, sizeof v, D.vout > 0 ? "%.3f" : "--", D.vout); value(dc, 117, 106, 88, v, "V", C_TEXT);
    snprintf(s, sizeof s, "SET %.3f  VIN %.1f", D.mv / 1000.0f, D.vin); text(dc, 117, 133, s, F_XS, C_MUTE);
    card(dc, 214, 88, 102, 56, profileCol()); label(dc, 222, 94, "ASIC CLOCK");
    snprintf(v, sizeof v, "%d", D.freq); value(dc, 222, 106, 90, v, "MHz", C_TEXT);
    snprintf(s, sizeof s, "%s PROFILE", profile()); text(dc, 222, 133, s, F_XS, profileCol(), 0, 0, 1);
    bottom(dc);
    uint32_t sc; const char *st = status(sc);
    bool ok = sc == C_GREEN;
    if (ok) snprintf(s, sizeof s, "%s - HEADROOM OK", D.govState[0] ? D.govState : "HOLD"); else if (D.throttled) snprintf(s, sizeof s, "THROTTLED - COOLING"); else snprintf(s, sizeof s, "%s", st);
    rect(dc, 11, 155, 5, 5, ok ? C_GREEN : sc, 255, 3); text(dc, 21, 152, s, F_XS, ok ? C_GREEN : sc);
    snprintf(s, sizeof s, "FAN %d%%  %d RPM", D.fanPct, D.rpm); text(dc, 310, 152, s, F_XS, C_TEXT, 2);
}

// ================= 3/4 OCTA MATRIX =================
void draw3(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc); header(dc, 3, "CHIPS");
    float avg = 0, hot = 0; for (int i = 0; i < D.n; i++) { avg += D.chipHr[i]; hot = fmaxf(hot, D.chipT[i]); } avg = D.n ? avg / D.n : 0;
    char v[24], s[64];
    for (int i = 0; i < 8; i++) {
        int x = 4 + (i % 4) * 78, y = 28 + (i / 4) * 60;
        float t = i < D.n ? D.chipT[i] : 0, h = i < D.n ? D.chipHr[i] : 0;
        bool weak = avg > 0 && h < 0.9f * avg;
        uint32_t tc = tempCol(t, limChip());
        card(dc, x, y, 74, 56, weak ? C_AMBER : C_COPPER2);
        snprintf(s, sizeof s, "CHIP %d", i + 1); label(dc, x + 8, y + 6, s);
        if (weak) text(dc, x + 68, y + 6, "LOW", F_XS, C_AMBER, 2);
        snprintf(v, sizeof v, h > 0 ? "%.0f" : "--", h); text(dc, x + 8, y + 16, v, F_M, weak ? C_AMBER : C_TEXT);
        snprintf(v, sizeof v, t > 0 ? "%.1f C" : "-- C", t); text(dc, x + 8, y + 41, v, F_XS, tc, 0, 0, 1);
        float hf = t > 0 ? fminf(fmaxf((t - 40) / 35, 0.05f), 1) : 0;
        rect(dc, x + 44, y + 45, 24, 3, C_LINE, 255, 1); rect(dc, x + 44, y + 45, (int) (24 * hf), 3, tc, 255, 1);
    }
    bottom(dc);
    const char *u; fmtHr(v, sizeof v, &u, D.hr);
    snprintf(s, sizeof s, "TOTAL %s %s   AVG %.0f GH/s", v, u, avg); text(dc, 11, 152, s, F_XS, C_TEXT, 0, 0, 1);
    snprintf(s, sizeof s, "HOTTEST %.1f C", hot); text(dc, 310, 152, s, F_XS, tempCol(hot, limChip()), 2, 0, 1);
}

// ================= 4/4 POOLS & BLOCKS =================
void draw4(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc); header(dc, 4, "POOLS");
    char v[24], s[72];
    card(dc, 4, 28, 101, 50, C_COPPER); label(dc, 12, 34, "BEST SHARE");
    octa_fmt_big(v, sizeof v, (double) D.bestDiff); value(dc, 12, 46, 88, D.bestDiff ? v : "--", "", C_COPPER);
    card(dc, 109, 28, 101, 50, C_COPPER2); label(dc, 117, 34, "NETWORK");
    octa_fmt_big(v, sizeof v, D.netDiff); value(dc, 117, 46, 88, D.netDiff > 0 ? v : "--", "", C_TEXT);
    double lb = D.bestDiff > 0 ? log10((double) D.bestDiff) : 0, ln = D.netDiff > 1 ? log10(D.netDiff) : 15;
    float f = ln > 0 ? (float) (lb / ln) : 0; if (f > 1) f = 1; if (f < 0) f = 0;
    card(dc, 214, 28, 102, 50, C_GOLD); label(dc, 222, 34, D.foundBlocks ? "BLOCKS" : "BEST vs NET");
    if (D.foundBlocks) snprintf(v, sizeof v, "%lu", (unsigned long) D.foundBlocks); else snprintf(v, sizeof v, "%.0f", f * 100);
    value(dc, 222, 46, 90, v, D.foundBlocks ? "" : "% LOG", C_GOLD);
    // pool split
    card(dc, 4, 82, 312, 62, C_TEAL);
    if (D.split) {
        label(dc, 12, 88, "POOL SPLIT");
        snprintf(s, sizeof s, "%d / %d", D.pool[0].weight, D.pool[1].weight); text(dc, 92, 85, s, F_S, C_TEXT);
        if (D.failover) { int w = tw(F_XS, "FAILOVER", 1) + 12; rect(dc, 310 - w, 87, w, 12, C_AMBER, 255, 6); text(dc, 304, 88, "FAILOVER", F_XS, C_BG, 2, 0, 1); }
        int bx = 12, bw = 296, w0 = (bw * D.pool[0].eff) / 100;
        rect(dc, bx, 104, bw, 6, C_LINE, 255, 3);
        if (D.pool[0].eff > 0) rect(dc, bx, 104, w0, 6, C_COPPER, 255, 3);
        if (D.pool[1].eff > 0) rect(dc, bx + w0 + (w0 ? 2 : 0), 104, bw - w0 - (w0 ? 2 : 0), 6, C_TEAL, 255, 3);
        for (int i = 0; i < 2; i++) {
            const OctaData::Pool &p = D.pool[i]; int y = 115 + i * 14;
            uint32_t pc = i ? C_TEAL : C_COPPER;
            rect(dc, 12, y + 3, 6, 6, p.up ? pc : C_DIM, 255, 3);
            char hst[20]; const char *src = p.cfg && p.host[0] ? p.host : "not set"; size_t hn = strnlen(src, sizeof p.host);
            if (hn > 17) { memcpy(hst, src, 16); hst[16] = '.'; hst[17] = '.'; hst[18] = 0; } else { memcpy(hst, src, hn); hst[hn] = 0; }
            text(dc, 23, y, hst, F_XS, p.up ? C_TEXT : C_MUTE);
            snprintf(s, sizeof s, "%d%%", p.eff); text(dc, 150, y, s, F_XS, pc, 2);
            const char *u; fmtHr(v, sizeof v, &u, p.ghs); snprintf(s, sizeof s, "%s %s", p.up ? v : "--", u); text(dc, 230, y, s, F_XS, C_TEXT, 2);
            snprintf(s, sizeof s, "%llu/%llu", (unsigned long long) p.acc, (unsigned long long) p.rej); text(dc, 308, y, s, F_XS, C_MUTE, 2);
        }
    } else {
        label(dc, 12, 88, "POOL");
        const char *pool = lPool ? lv_label_get_text(lPool) : "";
        snprintf(s, sizeof s, "%s", pool && pool[0] ? pool : "--"); if (strlen(s) > 30) { s[28] = '.'; s[29] = '.'; s[30] = 0; }
        text(dc, 12, 102, s, F_S, C_TEXT);
        text(dc, 12, 124, "FAILOVER MODE - SPLIT OFF (WEB UI > POOL SPLIT)", F_XS, C_MUTE);
    }
    bottom(dc);
    snprintf(s, sizeof s, "IP %s", D.ip[0] ? D.ip : "--"); text(dc, 11, 152, s, F_XS, C_TEXT, 0, 0, 1);
    if (D.window[0]) snprintf(s, sizeof s, "SCHEDULE: %s", D.window); else snprintf(s, sizeof s, "NO SCHEDULE ACTIVE");
    for (char *p = s; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    text(dc, 310, 152, s, F_XS, D.window[0] ? C_TEAL : C_MUTE, 2, 0, 1);
}

// ---- splash / logo ----
void drawLogo(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc);
    int k = (int) (phase / 100) % 36;
    arc(dc, 35, 35, 33, 3, 0, 359, C_LINE);
    arc(dc, 35, 35, 33, 3, k * 10, k * 10 + 80, C_COPPER, 255, true);
    arc(dc, 35, 35, 33, 3, k * 10 + 180, k * 10 + 220, C_TEAL, 255, true);
    logoMark(dc, 17, 17, 36);
}
void drawBurst(lv_event_t *e) {
    lv_draw_ctx_t *dc; origin(e, &dc);
    rect(dc, 0, 0, 320, 170, C_BG);
    lv_opa_t op = (lv_opa_t) (140 + 115 * (0.5f + 0.5f * sinf(phase / 250.0f)));
    frame(dc, 6, 6, 308, 158, C_GOLD, 2, 10, op);
    // sparkles: small gold diamonds orbiting
    for (int i = 0; i < 8; i++) {
        float a = phase / 700.0f + i * 0.8f; int sx = (i < 4 ? 26 : 294) + (int) (8 * cosf(a * 1.3f)), sy = 28 + (i % 4) * 34 + (int) (6 * sinf(a));
        int r = 3 + (i % 2);
        line(dc, sx - r, sy, sx, sy - r, 2, i % 3 ? C_GOLD : C_COPPER); line(dc, sx, sy - r, sx + r, sy, 2, C_GOLD);
        line(dc, sx + r, sy, sx, sy + r, 2, C_GOLD); line(dc, sx, sy + r, sx - r, sy, 2, C_GOLD);
    }
    text(dc, 0, 26, "EVERY HASH IS A NEW CHANCE", F_XS, C_MUTE, 1, 320, 2);
    text(dc, 0, 46, "BLOCK FOUND", F_M, C_GOLD, 1, 320, 3);
    char v[24], s[48]; octa_fmt_big(v, sizeof v, (double) D.bestDiff); snprintf(s, sizeof s, "BEST %s  /  NET ", v);
    octa_fmt_big(v, sizeof v, D.netDiff); strcat(s, v); text(dc, 0, 88, s, F_S, C_TEXT, 1, 320, 1);
    text(dc, 0, 124, "COSTEAN OS  -  NERDOCTAXE 8x BM1370", F_XS, C_COPPER, 1, 320, 1);
}
void drawPlain(lv_event_t *e) {   // splash / portal / power-off backdrop
    lv_draw_ctx_t *dc; origin(e, &dc);
    lv_obj_t *o = lv_event_get_target(e); int mode = (int) (intptr_t) lv_obj_get_user_data(o);
    rect(dc, 6, 6, 308, 158, C_CARD2, 255, 10);
    rect(dc, 6, 160, 308, 2, C_LINE);
    rect(dc, 6, 160, 60, 2, C_COPPER);
    if (mode == 1) { // boot: logo canvas (24,46)-(94,116), text to its right
        const int tx = 110;
        wordmark(dc, tx, 38, F_M, 3, 10);
        rect(dc, tx, 72, 40, 2, C_COPPER);
        text(dc, tx, 82, "NERDOCTAXE GAMMA  REV 3.1", F_XS, C_MUTE, 0, 0, 1);
        text(dc, tx, 96, D.version ? D.version : "", F_XS, C_COPPER, 0, 0, 1);
        text(dc, tx, 114, "START 700 MHZ / 1150 MV", F_XS, C_TEAL, 0, 0, 1);
        text(dc, tx, 128, "AUTO-TUNE OFF  |  LIMITS ON", F_XS, C_DIM, 0, 0, 1);
    } else if (mode == 2) { // portal
        logoMark(dc, 18, 16, 16);
        text(dc, 42, 14, "WI-FI SETUP", F_S, C_TEXT, 0, 0, 2);
        rect(dc, 18, 38, 284, 1, C_LINE);
        text(dc, 18, 46, "1  JOIN THIS HOTSPOT", F_XS, C_COPPER, 0, 0, 1);
        text(dc, 18, 96, "2  OPEN THE SETUP PAGE THAT POPS UP", F_XS, C_COPPER, 0, 0, 1);
        text(dc, 30, 112, "OR BROWSE TO 192.168.4.1", F_XS, C_MUTE, 0, 0, 1);
    } else if (mode == 3) { // power off
        logoMark(dc, 146, 30, 28);
        text(dc, 0, 72, "SAFE TO POWER OFF", F_M, C_TEXT, 1, 320, 2);
        text(dc, 0, 108, "ASICS SHUT DOWN", F_XS, C_TEAL, 1, 320, 2);
    } else if (mode == 4) { // connecting
        wordmark(dc, 110, 52, F_M, 3, 10);
        rect(dc, 110, 86, 40, 2, C_COPPER);
    }
}

UI *g_ui;
lv_obj_t **g_ovl = nullptr, *g_sp1 = nullptr, *g_sp2 = nullptr;
uint32_t lastFull = 0;
void tick(lv_timer_t *tm) {
    phase += tm->period;
    octa_fill(D);
    if (D.hr > peakGh) peakGh = D.hr;
    if (phase - hlast >= 10000 || hcount == 0) {
        hlast = phase;
        if (D.hr > 0) { if (hcount < HN) hist[hcount++] = D.hr; else { memmove(hist, hist + 1, sizeof(float) * (HN - 1)); hist[HN - 1] = D.hr; } }
    }
    bool n = D.lcdMode != 0;    // schedule window "dim" (or "off" while woken by a button)
    if (n != night) { night = n; lv_obj_invalidate(lv_scr_act()); }
    lv_obj_t *act = lv_scr_act();
    if (!act) return;
    // Splash logos: only while their own screen is shown, and only while they still exist
    // (displayDriver lv_obj_clean()s the splash screens; track() nulls oLogo1/oLogo2 then).
    if (act == g_sp1) inval(oLogo1);
    else if (act == g_sp2) inval(oLogo2);
    if (phase - lastFull >= 1000) {            // data pages: 1 Hz full redraw
        lastFull = phase;
        if (act == scr1) inval(cv1); else if (act == scr2) inval(cv2);
        else if (act == scr3) inval(cv3); else if (act == scr4) inval(cv4);
    }
    if (g_ovl && *g_ovl) inval(oBurst);
}
lv_obj_t *page(lv_event_cb_t cb, lv_obj_t **cv) { lv_obj_t *s = mkScreen(); *cv = mkCanvas(s, 0, 0, 320, 170, cb); track(cv); return s; }
lv_obj_t *plain(int mode) { lv_obj_t *s = mkScreen(); lv_obj_t *c = mkCanvas(s, 0, 0, 320, 170, drawPlain); lv_obj_set_user_data(c, (void *) (intptr_t) mode); return s; }
} // namespace

void UI::octaInit()
{
    g_ui = this;
    lv_disp_t *disp = lv_disp_get_default();
    lv_disp_set_theme(disp, lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_DEEP_ORANGE), lv_palette_main(LV_PALETTE_TEAL), true, LV_FONT_DEFAULT));
    octa_fill(D);

    ui_Splash1 = plain(1); g_sp1 = ui_Splash1; track(&g_sp1);
    oLogo1 = mkCanvas(ui_Splash1, 24, 46, 70, 70, drawLogo); track(&oLogo1);
    ui_imgSplash1 = oLogo1; track(&ui_imgSplash1);

    ui_Splash2 = plain(4); g_sp2 = ui_Splash2; track(&g_sp2);
    oLogo2 = mkCanvas(ui_Splash2, 24, 46, 70, 70, drawLogo); track(&oLogo2);
    ui_Image1 = oLogo2; track(&ui_Image1);
    // DisplayDriver::updateWifiStatus() null-checks ui_lbConnect, so clearing it on delete is enough.
    ui_lbConnect = mkLabel(ui_Splash2, F_XS, C_TEAL, 110, 96, "Connecting..."); track(&ui_lbConnect);

    ui_PortalScreen = plain(2);
    ui_lbSSID = mkLabel(ui_PortalScreen, F_M, C_TEXT, 30, 62, "");
    ui_Image2 = NULL;

    scr1 = ui_MiningScreen = page(draw1, &cv1); track(&scr1);
    ui_imgNet = lv_img_create(scr1); lv_img_set_src(ui_imgNet, &ui_img_wifi_png); lv_obj_add_flag(ui_imgNet, LV_OBJ_FLAG_HIDDEN);
    track(&ui_imgNet);
    ui_lbIP = mkHidden(scr1); ui_lbTime = mkHidden(scr1); ui_lbHashrate = mkHidden(scr1); ui_lbEficiency = mkHidden(scr1);
    ui_lbPower = mkHidden(scr1); ui_lbTemp = mkHidden(scr1); ui_lbIntensidad = mkHidden(scr1); ui_lbVinput = mkHidden(scr1);
    ui_lbVcore = mkHidden(scr1); ui_lbBestDifficulty = mkHidden(scr1);

    scr2 = ui_SettingsScreen = page(draw2, &cv2); track(&scr2);
    lPool = ui_lbPoolSet = lv_label_create(scr2); lv_obj_add_flag(lPool, LV_OBJ_FLAG_HIDDEN); track(&lPool);
    ui_lbBestDifficultySet = mkHidden(scr2); ui_lbFreqSet = mkHidden(scr2); ui_lbVcoreSet = mkHidden(scr2); ui_lbFanSet = mkHidden(scr2);
    ui_lbIPSet = mkHidden(scr2); ui_lbPortSet = mkHidden(scr2); ui_lbPoolNr = mkHidden(scr2); ui_lbShares = mkHidden(scr2); ui_lbHashrateSet = mkHidden(scr2);

    scr3 = ui_BTCScreen = page(draw3, &cv3); track(&scr3);
    ui_lbRPM = mkHidden(scr3); ui_lblBTCPrice = mkHidden(scr3); ui_lblTempPrice = mkHidden(scr3); ui_lblHashPrice = mkHidden(scr3); ui_lblPriceInc = mkHidden(scr3);

    scr4 = ui_GlobalStats = page(draw4, &cv4); track(&scr4);
    ui_lblBlock = mkHidden(scr4); ui_lblBlocksToHalving = mkHidden(scr4); ui_lblHalvingPercent = mkHidden(scr4); ui_lblGlobalHash = mkHidden(scr4);
    ui_lblDifficulty = mkHidden(scr4); ui_lbllowFee = mkHidden(scr4); ui_lblmedFee = mkHidden(scr4); ui_lblhighFee = mkHidden(scr4);

    lv_obj_t *all[] = {ui_Splash1, ui_Splash2, ui_PortalScreen, ui_MiningScreen, ui_SettingsScreen, ui_BTCScreen, ui_GlobalStats};
    for (auto s : all) lv_obj_add_event_cb(s, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);

    g_ovl = &ui_imageOverlayContainer;
    lv_timer_create(tick, 100, NULL);
    lv_disp_load_scr(ui_Splash1);
}

void UI::octaPowerOff()
{
    ui_PowerOffScreen = plain(3); track(&ui_PowerOffScreen);
    lv_obj_add_event_cb(ui_PowerOffScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::octaShowError(const char *msg, uint32_t code)
{
    lv_obj_t *c = lv_obj_create(lv_scr_act());
    ui_errOverlayContainer = c; track(&ui_errOverlayContainer);   // its blink anim is removed by lv_obj_del
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, 308, 120); lv_obj_center(c);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x1A0D0B), 0); lv_obj_set_style_bg_opa(c, 255, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(C_RED), 0); lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    mkLabel(c, F_S, C_RED, 16, 12, "PROTECTION ACTIVE");
    lv_obj_t *t = mkLabel(c, F_M, C_TEXT, 16, 38, msg ? msg : "");
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_set_width(t, 280);
    char b[48]; snprintf(b, sizeof b, "CODE %08X - SEE WEB UI", (unsigned) code);
    mkLabel(c, F_XS, C_MUTE, 16, 90, b);
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, c); lv_anim_set_values(&a, 90, 255); lv_anim_set_time(&a, 900);
    lv_anim_set_playback_time(&a, 900); lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_obj_set_style_border_opa((lv_obj_t *) o, (lv_opa_t) v, 0); });
    lv_anim_start(&a);
}

void UI::octaShowBlock()
{
    lv_obj_t *c = lv_obj_create(lv_scr_act());
    ui_imageOverlayContainer = c; track(&ui_imageOverlayContainer);
    lv_obj_remove_style_all(c); lv_obj_set_size(c, 320, 170); lv_obj_set_pos(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    oBurst = mkCanvas(c, 0, 0, 320, 170, drawBurst); track(&oBurst);
}

void UI::octaHideBlock() { oBurst = nullptr; }
#endif
