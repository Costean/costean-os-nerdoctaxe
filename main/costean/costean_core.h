#pragma once
// Costean OS runtime: tuning profiles, safety governor, auto-tune, weekly schedules and the
// long (48 h) history that survives a reboot. ESP-IDF glue around the host-tested logic in
// tuning.h / scheduler.h / rollup.h.
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
#include "ArduinoJson.h"

struct CosteanLcdInfo {
    char profile[8];     // "ECO", "STOCK", "TURBO", "MANUAL"
    char state[12];      // "HOLD", "THROTTLE", "TUNE +1", ...
    char window[20];     // active schedule window name, "" if none
    uint8_t lcdMode;     // 0 normal, 1 dim, 2 off
    bool throttled;
    bool scheduled;
    int tuneSteps;
    int limitChip, limitVr, limitPower;
};

void costean_init();                       // after the board and STRATUM_MANAGER exist
void costean_before_restart();             // flush history (waits up to 1.5 s)
uint8_t costean_lcd_mode();                // 0 normal, 1 dim, 2 off
void costean_lcd_info(CosteanLcdInfo &out);

// HTTP (under /api/costean)
void costean_status_json(JsonObject o);
void costean_config_json(JsonObject o);
void costean_history_json(JsonObject o);
// Validates and queues a config change. Returns nullptr on success, else an error string.
const char *costean_patch_config(const JsonDocument &doc);
#endif
