// Host LVGL harness shared by the renderer and the lifetime test (DEMO data only).
#pragma once
#define protected public
#include "displays/ui.h"
#undef protected
#include "displayDriver.h"
#include "displays/ui_helpers.h"
#include <cstdio>
#include <cstdlib>
extern bool g_sim_night; extern int g_sim_fault; extern int g_sim_blocks;
static uint16_t fb[320 * 170];
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *c) {
  for (int y = a->y1; y <= a->y2; y++) for (int x = a->x1; x <= a->x2; x++) fb[y * 320 + x] = (c++)->full;
  lv_disp_flush_ready(d);
}
static void heapCheck(const char *where) {
#if !LV_MEM_CUSTOM
  if (lv_mem_test() != LV_RES_OK) { printf("HEAP CORRUPT after %s\n", where); fflush(stdout); abort(); }
#endif
  (void) where;
}
static void run(int ms) { for (int i = 0; i < ms / 20; i++) { lv_tick_inc(20); lv_timer_handler(); } heapCheck("run"); }
static void simInit() {
  lv_init();
  static lv_color_t buf[320 * 170]; static lv_disp_draw_buf_t db; lv_disp_draw_buf_init(&db, buf, NULL, 320 * 170);
  static lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.hor_res = 320; drv.ver_res = 170; drv.flush_cb = flush; drv.draw_buf = &db; drv.full_refresh = 1; lv_disp_drv_register(&drv);
}
