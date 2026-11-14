// LCD object-lifetime stress test for the Costean OS LCD UI (ui_octa.cpp).
// Replays DisplayDriver::enterState() from main/displays/displayDriver.cpp (v1.1.2) line for line:
// lv_scr_load_anim(...) + lv_obj_clean(ui_Splash1/2) + "= NULL", the null-checked
// updateWifiStatus() write to ui_lbConnect, and the overlay show/hide calls, while the 100 ms
// Costean tick timer keeps running. Built with -fsanitize=address (LVGL heap = malloc) any access to
// a deleted LVGL object aborts; the device-like build uses LVGL's own 32 KB TLSF heap with
// LV_MEM_ADD_JUNK (freed memory poisoned) and lv_mem_test() after every step.
#include "sim_common.h"
#include <cstring>
static UI ui; static DisplayDriver dd;
static void scr(lv_obj_t *s, lv_scr_load_anim_t a, int spd) { _ui_screen_change(s, a, spd, 0); }  // = DisplayDriver::safe_screen_change
static void enterSplash2() { scr(ui.ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500); if (ui.ui_Splash1) { lv_obj_clean(ui.ui_Splash1); ui.ui_Splash1 = NULL; } }
static void enterPortal()  { if (ui.ui_Splash2) { lv_obj_clean(ui.ui_Splash2); ui.ui_Splash2 = NULL; } scr(ui.ui_PortalScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500); }
static void enterMining(bool fromStats) { if (ui.ui_Splash2) { lv_obj_clean(ui.ui_Splash2); ui.ui_Splash2 = NULL; } scr(ui.ui_MiningScreen, fromStats ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_FADE_ON, fromStats ? 350 : 500); }
static void wifiStatus(const char *m) { if (ui.ui_lbConnect != NULL) lv_label_set_text(ui.ui_lbConnect, m); }
static void cyclePages(int rounds, int dwell) {
  for (int r = 0; r < rounds; r++) {
    scr(ui.ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350); run(dwell);
    scr(ui.ui_BTCScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350); run(dwell);
    scr(ui.ui_GlobalStats, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350); run(dwell);
    enterMining(true); run(dwell);
  }
}
static void powerOff() { if (!ui.ui_PowerOffScreen) ui.powerOffScreenInit(); scr(ui.ui_PowerOffScreen, LV_SCR_LOAD_ANIM_NONE, 0); run(1000); }
#define STEP(s) do { printf("  step: %s\n", s); } while (0)
int main(int argc, char **argv) {
  setvbuf(stdout, 0, _IONBF, 0);
  const char *sc = argc > 1 ? argv[1] : "portal";
  simInit();
  static ThemeNerdoctaxegamma theme; Board b{&theme, "NerdOCTAXE-Gamma", "BM1370"};
  ui.init(&b, &dd);
  lv_label_set_text(ui.ui_lbSSID, "NerdOCTAXE_DEMO"); lv_label_set_text(ui.ui_lbPoolSet, "pool.example.org");
  printf("scenario %s\n", sc);
  STEP("splash1"); run(1500);
  if (!strcmp(sc, "portal")) {
    STEP("splash2 + clean splash1"); enterSplash2(); for (int i = 0; i < 30; i++) { wifiStatus(i & 1 ? "Connecting..." : "Wi-Fi: retry"); run(100); }
    STEP("wait"); run(2000);
    STEP("portal + clean splash2"); enterPortal(); wifiStatus("late status after clean"); run(3000);
    STEP("mining"); enterMining(false); run(2000); cyclePages(3, 1200);
  } else if (!strcmp(sc, "mining")) {
    STEP("splash2 + clean splash1"); enterSplash2(); run(3000);
    STEP("mining + clean splash2"); enterMining(false); for (int i = 0; i < 20; i++) { wifiStatus("Wi-Fi reconnect"); run(100); }
    cyclePages(3, 1200);
  } else if (!strcmp(sc, "midanim")) {   // transitions fired while the previous fade is still running
    STEP("splash2 + clean splash1 (20 ms)"); enterSplash2(); run(20);
    STEP("mining + clean splash2 mid-fade"); enterMining(false); run(40); cyclePages(4, 60); run(2000);
  } else if (!strcmp(sc, "overlays")) {  // fault and block overlays parented to screens that get cleaned
    STEP("splash2 + clean splash1"); enterSplash2(); run(600);
    STEP("fault + block overlay on splash2"); ui.hideErrorOverlay(); ui.showErrorOverlay("MINER OVERHEATED", 7); ui.hideImageOverlay(); ui.showImageOverlay(&ui_img_found_block_png); run(1500);
    STEP("mining + clean splash2 (overlays deleted underneath)"); enterMining(false); run(1500);
    STEP("hide overlays after their parent died"); ui.hideErrorOverlay(); ui.hideImageOverlay(); run(1000);
    STEP("overlays on mining page, switch page, hide"); ui.showErrorOverlay("VR HOT", 3); ui.showImageOverlay(&ui_img_found_block_png); run(800);
    cyclePages(1, 800); ui.hideErrorOverlay(); ui.hideImageOverlay(); run(800);
  } else if (!strcmp(sc, "shutdown")) {  // overheat shutdown (queued message) arrives mid page swipe
    STEP("splash2"); enterSplash2(); run(3000); STEP("mining"); enterMining(false); run(1500);
    ui.showErrorOverlay("MINER OVERHEATED", 1); scr(ui.ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350); run(100);
    STEP("power off mid-swipe"); powerOff(); run(1000); printf("PASS %s\n", sc); return 0;
  } else { printf("unknown scenario\n"); return 2; }
  STEP("night dim"); g_sim_night = true; run(1500); g_sim_night = false; run(500);
  STEP("block found + hide"); ui.hideImageOverlay(); ui.showImageOverlay(&ui_img_found_block_png); run(1500); ui.hideImageOverlay(); run(500);
  STEP("fault + hide"); ui.hideErrorOverlay(); ui.showErrorOverlay("ASIC FAULT", 2); run(1500); ui.hideErrorOverlay(); run(500);
  STEP("power off"); powerOff();
#if !LV_MEM_CUSTOM
  { lv_mem_monitor_t m; lv_mem_monitor(&m); printf("  LVGL heap: used %u%%, max used %u B, frag %u%%\n", m.used_pct, (unsigned) m.max_used, m.frag_pct); }
#endif
  printf("PASS %s\n", sc); return 0;
}
