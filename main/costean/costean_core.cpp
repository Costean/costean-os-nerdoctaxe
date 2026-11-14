#include "costean_core.h"

#include <algorithm>
#include <math.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "global_state.h"
#include "nvs_config.h"
#include "psram_allocator.h"
#include "rollup.h"
#include "scheduler.h"
#include "tuning.h"
#include "../stratum/stratum_manager_dual_pool.h"

using namespace costean;

static const char *TAG = "costean";
static const char *NVS_NS = "costean";
#define TICK_MS 5000

namespace {

struct Cfg {
    Profile profile = Profile::CUSTOM;   // "Manual": the saved Mining & Tuning values (stock behaviour)
    Limits limits;
    AutoTune autotune;
    Schedule schedule;
    char tz[32] = "America/New_York";
};

SemaphoreHandle_t s_mx = nullptr;
Cfg s_cfg;
bool s_cfgDirty = false;            // save config to NVS from the task
Presets s_presets;
Governor s_gov;
Rollup s_roll;
ScheduleResult s_sched;
Profile s_active = Profile::CUSTOM;
OpPoint s_target{}, s_applied{};
Telemetry s_tel;
bool s_failover = false;
bool s_started = false;
volatile bool s_saveReq = false;
SemaphoreHandle_t s_saved = nullptr;
int64_t s_lastHistSave = 0;
uint64_t s_snapAcc = 0, s_snapRej = 0; int64_t s_snapT = 0; float s_rejPct = 0;

struct Lock {
    Lock() { if (s_mx) xSemaphoreTakeRecursive(s_mx, portMAX_DELAY); }
    ~Lock() { if (s_mx) xSemaphoreGiveRecursive(s_mx); }
};

int64_t utcNow() { time_t t = time(nullptr); return t > 1700000000 ? (int64_t) t : 0; }
uint64_t msNow() { return (uint64_t) (esp_timer_get_time() / 1000); }

const Zone &zone() { const Zone *z = zoneById(s_cfg.tz); return z ? *z : defaultZone(); }

void defaultSchedule(Schedule &s)
{
    s = Schedule();
    s.enabled = true;
    s.count = 1;
    Window &w = s.windows[0];
    w.enabled = true; w.days = 0x7f; w.start = 22 * 60; w.end = 7 * 60;
    w.profile = Profile::NONE; w.lcd = LcdMode::DIM;
    strcpy(w.name, "Night screen");
}

// ---------- JSON <-> config ----------
const char *lcdName(LcdMode m) { return m == LcdMode::DIM ? "dim" : m == LcdMode::OFF ? "off" : "normal"; }
LcdMode lcdFrom(const char *s) { return s && !strcmp(s, "dim") ? LcdMode::DIM : s && !strcmp(s, "off") ? LcdMode::OFF : LcdMode::NORMAL; }

void cfgToJson(const Cfg &c, JsonObject o)
{
    o["profile"] = profileName(c.profile);
    JsonObject l = o["limits"].to<JsonObject>();
    l["chipTemp"] = c.limits.chipTempC; l["vrTemp"] = c.limits.vrTempC; l["power"] = c.limits.powerW;
    JsonObject a = o["autotune"].to<JsonObject>();
    a["enabled"] = c.autotune.enabled; a["maxSteps"] = c.autotune.maxStepsUp;
    a["maxErrorPct"] = c.autotune.maxErrorPct; a["maxRejectPct"] = c.autotune.maxRejectPct;
    a["holdMin"] = (int) (c.autotune.holdMs / 60000);
    JsonObject s = o["schedule"].to<JsonObject>();
    s["enabled"] = c.schedule.enabled; s["tz"] = c.tz;
    JsonArray w = s["windows"].to<JsonArray>();
    for (int i = 0; i < c.schedule.count; i++) {
        const Window &x = c.schedule.windows[i];
        JsonObject j = w.add<JsonObject>();
        j["on"] = x.enabled; j["days"] = x.days; j["start"] = x.start; j["end"] = x.end;
        j["profile"] = profileName(x.profile); j["lcd"] = lcdName(x.lcd); j["name"] = x.name;
    }
}

// strict: returns error string or nullptr; only touches fields present
const char *cfgFromJson(Cfg &c, JsonVariantConst o)
{
    if (o["profile"].is<const char *>()) {
        Profile p = profileFromName(o["profile"].as<const char *>());
        if (p == Profile::NONE) return "unknown profile";
        c.profile = p;
    }
    JsonVariantConst l = o["limits"];
    if (l.is<JsonObjectConst>()) {
        if (l["chipTemp"].is<int>()) { int v = l["chipTemp"]; if (v < 45 || v > 75) return "chip limit must be 45-75 C"; c.limits.chipTempC = v; }
        if (l["vrTemp"].is<int>()) { int v = l["vrTemp"]; if (v < 50 || v > 100) return "regulator limit must be 50-100 C"; c.limits.vrTempC = v; }
        if (l["power"].is<int>()) { int v = l["power"]; if (v < 40 || v > 280) return "power limit must be 40-280 W"; c.limits.powerW = v; }
    }
    JsonVariantConst a = o["autotune"];
    if (a.is<JsonObjectConst>()) {
        if (a["enabled"].is<bool>()) c.autotune.enabled = a["enabled"];
        if (a["maxSteps"].is<int>()) { int v = a["maxSteps"]; if (v < 1 || v > 4) return "auto-tune steps must be 1-4"; c.autotune.maxStepsUp = v; }
        if (a["maxErrorPct"].is<float>()) { float v = a["maxErrorPct"]; if (v < 0.2f || v > 3) return "error ceiling must be 0.2-3 %"; c.autotune.maxErrorPct = v; }
        if (a["maxRejectPct"].is<float>()) { float v = a["maxRejectPct"]; if (v < 0.2f || v > 5) return "reject ceiling must be 0.2-5 %"; c.autotune.maxRejectPct = v; }
        if (a["holdMin"].is<int>()) { int v = a["holdMin"]; if (v < 5 || v > 120) return "dwell must be 5-120 min"; c.autotune.holdMs = (uint32_t) v * 60000u; }
    }
    JsonVariantConst s = o["schedule"];
    if (s.is<JsonObjectConst>()) {
        if (s["enabled"].is<bool>()) c.schedule.enabled = s["enabled"];
        if (s["tz"].is<const char *>()) {
            if (!zoneById(s["tz"].as<const char *>())) return "unknown time zone";
            strncpy(c.tz, s["tz"].as<const char *>(), sizeof(c.tz) - 1);
        }
        if (s["windows"].is<JsonArrayConst>()) {
            JsonArrayConst arr = s["windows"].as<JsonArrayConst>();
            if (arr.size() > (size_t) Schedule::kMaxWindows) return "at most 8 windows";
            Schedule ns = c.schedule; ns.count = 0;
            for (JsonVariantConst j : arr) {
                Window w;
                w.enabled = j["on"] | true;
                w.days = (uint8_t) (j["days"] | 0x7f);
                w.start = (uint16_t) (j["start"] | 0);
                w.end = (uint16_t) (j["end"] | 0);
                Profile p = Profile::NONE;
                const char *pn = j["profile"] | "none";
                if (strcmp(pn, "none")) { p = profileFromName(pn); if (p == Profile::NONE) return "unknown profile in window"; }
                w.profile = p;
                w.lcd = lcdFrom(j["lcd"] | "normal");
                strncpy(w.name, j["name"] | "", sizeof(w.name) - 1);
                const char *err = Schedule::validate(w);
                if (err) return err;
                ns.windows[ns.count++] = w;
            }
            c.schedule = ns;
        }
    }
    return nullptr;
}

// ---------- NVS (only from the Costean task: it has an internal-RAM stack) ----------
bool s_cfgLoaded = false;   // a saved config exists: never overwrite the user's limits with board defaults

void nvsLoad()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = 0;
    if (nvs_get_blob(h, "cfg", nullptr, &len) == ESP_OK && len > 0 && len < 4096) {
        char *buf = (char *) malloc(len + 1);
        if (buf && nvs_get_blob(h, "cfg", buf, &len) == ESP_OK) {
            buf[len] = 0;
            PSRAMAllocator al; JsonDocument d(&al);
            if (!deserializeJson(d, buf)) {
                Cfg c = s_cfg;
                const char *err = cfgFromJson(c, d.as<JsonVariantConst>());
                if (!err) { s_cfg = c; s_cfgLoaded = true; } else ESP_LOGW(TAG, "saved config ignored: %s", err);
            }
        }
        free(buf);
    }
    len = Rollup::kBlobSize;
    uint8_t *hb = (uint8_t *) malloc(len);
    if (hb && nvs_get_blob(h, "hist", hb, &len) == ESP_OK) {
        if (s_roll.restore(hb, len)) ESP_LOGI(TAG, "history restored: %d points", s_roll.size());
        else ESP_LOGW(TAG, "saved history invalid, starting empty");
    }
    free(hb);
    nvs_close(h);
}

void nvsSaveCfg()
{
    PSRAMAllocator al; JsonDocument d(&al);
    { Lock l; cfgToJson(s_cfg, d.to<JsonObject>()); s_cfgDirty = false; }
    size_t n = measureJson(d) + 1;
    char *buf = (char *) malloc(n);
    if (!buf) return;
    serializeJson(d, buf, n);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_blob(h, "cfg", buf, n - 1) == ESP_OK) nvs_commit(h);
        nvs_close(h);
    }
    free(buf);
}

void nvsSaveHist()
{
    uint8_t *buf = (uint8_t *) malloc(Rollup::kBlobSize);
    if (!buf) return;
    size_t n;
    { Lock l; n = s_roll.save(buf, Rollup::kBlobSize); }
    nvs_handle_t h;
    if (n && nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_blob(h, "hist", buf, n) == ESP_OK) nvs_commit(h);
        nvs_close(h);
    }
    free(buf);
}

// ---------- board envelope / presets ----------
void setupBoard(Board *b)
{
    Envelope e;
    const auto &fl = b->getFrequencyOptions();
    const auto &vl = b->getVoltageOptions();
    if (!fl.empty()) { e.minMhz = (int) fl.front(); e.maxMhz = (int) fl.back(); for (auto f : fl) { e.minMhz = std::min(e.minMhz, (int) f); e.maxMhz = std::max(e.maxMhz, (int) f); } }
    if (!vl.empty()) { e.minMv = (int) vl.front(); e.maxMv = (int) vl.back(); for (auto v : vl) { e.minMv = std::min(e.minMv, (int) v); e.maxMv = std::max(e.maxMv, (int) v); } }
    if (b->getAbsMaxAsicVoltageMillis()) e.maxMv = std::min(e.maxMv, b->getAbsMaxAsicVoltageMillis());
    e.stepMhz = 25;
    s_presets.stock = {b->getDefaultAsicFrequency(), b->getDefaultAsicVoltageMillis()};
    if (b->getEcoAsicFrequency() > 0) s_presets.eco = {b->getEcoAsicFrequency(), b->getEcoAsicVoltageMillis()};
    else s_presets.eco = {std::max(e.minMhz, s_presets.stock.mhz - 100), std::max(e.minMv, s_presets.stock.mv - 50)};
    s_presets.turbo = {std::min(e.maxMhz, s_presets.stock.mhz + 50), std::min(e.maxMv, s_presets.stock.mv + 30)};
    // power limit default follows the regulator (6-phase 300 W class -> 200 W, 4-phase -> 170 W)
    if (!s_cfgLoaded && b->getMaxPin() > 0 && b->getMaxPin() < 280) s_cfg.limits.powerW = 170;
    s_gov.configure(e, s_cfg.limits, s_cfg.autotune);
}

Limits effectiveLimits()
{
    // never above the stock overheat shutdown: stay 2 C below it
    Limits l = s_cfg.limits;
    int oh0 = Config::getFanOverheatTemp(0), oh1 = Config::getFanOverheatTemp(1);
    if (oh0 > 0) l.chipTempC = std::min(l.chipTempC, oh0 - 2);
    if (oh1 > 0) l.vrTempC = std::min(l.vrTempC, oh1 - 2);
    return l;
}

// ---------- the 5 s loop ----------
void readTelemetry(Board *b)
{
    Telemetry t;
    t.chipTempC = POWER_MANAGEMENT_MODULE.getChipTempMax();
    t.vrTempC = POWER_MANAGEMENT_MODULE.getVRTemp();
    t.powerW = POWER_MANAGEMENT_MODULE.getPower();
    float ok = 0, err = 0;
    for (int i = 0; i < b->getAsicCount(); i++) { ok += HASHRATE_MONITOR.getChipHashrate(i); err += HASHRATE_MONITOR.getChipErrorRate(i); }
    t.errorPct = ok + err > 1 ? 100.0f * err / (ok + err) : 0;
    t.mining = !POWER_MANAGEMENT_MODULE.isShutdown() && b->isInitialized() && SYSTEM_MODULE.getBoardError() == Board::Error::NONE;
    // pool reject % over ~10 min windows
    if (STRATUM_MANAGER) {
        uint64_t a = STRATUM_MANAGER->getSharesAccepted(), r = STRATUM_MANAGER->getSharesRejected();
        int64_t now = (int64_t) msNow();
        if (s_snapT == 0 || a < s_snapAcc || r < s_snapRej) { s_snapT = now; s_snapAcc = a; s_snapRej = r; }
        else if (now - s_snapT >= 600000) {
            uint64_t da = a - s_snapAcc, dr = r - s_snapRej;
            s_rejPct = da + dr >= 20 ? 100.0f * dr / (float) (da + dr) : 0;
            s_snapT = now; s_snapAcc = a; s_snapRej = r;
        }
    }
    t.rejectPct = s_rejPct;
    s_tel = t;
}

void step(Board *b)
{
    readTelemetry(b);
    uint64_t now = msNow();
    int64_t utc = utcNow();
    OpPoint out;
    {
        Lock l;
        s_gov.configure(s_gov.envelope(), effectiveLimits(), s_cfg.autotune);
        s_sched = s_cfg.schedule.evaluate(utc, zone());
        s_active = s_sched.profile != Profile::NONE ? s_sched.profile : s_cfg.profile;
        s_presets.custom = {b->getAsicFrequency(), b->getAsicVoltageMillis()};
        s_target = s_presets.of(s_active);
        out = s_gov.step(s_target, s_tel, now);
    }
    // apply: when the clock goes down, lower it first and the voltage on the next tick;
    // when it goes up, raise the voltage first (the power task applies voltage before clock).
    int curF = b->getEffectiveAsicFrequency(), curV = b->getEffectiveAsicVoltageMillis();
    OpPoint apply = out;
    if (out.mhz < curF && out.mv < curV) apply.mv = curV;
    if (apply.mhz != curF || apply.mv != curV) {
        ESP_LOGI(TAG, "operating point %d MHz / %d mV (%s, %s%s%s)", apply.mhz, apply.mv, profileName(s_active),
                 govStateName(s_gov.state()), s_gov.reason()[0] ? ": " : "", s_gov.reason());
    }
    bool isSaved = apply.mhz == b->getAsicFrequency() && apply.mv == b->getAsicVoltageMillis();
    b->setOperatingOverride(isSaved ? 0 : apply.mhz, isSaved ? 0 : apply.mv);
    s_applied = apply;

    s_failover = false;
    if (STRATUM_MANAGER && STRATUM_MANAGER->isDualPool()) s_failover = ((StratumManagerDualPool *) STRATUM_MANAGER)->isFailoverActive();

    // long history
    if (utc && s_tel.mining) {
        uint8_t fl = (s_gov.throttleSteps() > 0 ? 1 : 0) | (s_failover ? 2 : 0) | (s_sched.profile != Profile::NONE ? 4 : 0) |
                     (s_gov.tuneSteps() > 0 ? 8 : 0);
        Lock l;
        s_roll.add(utc, SYSTEM_MODULE.getCurrentHashrate(), s_tel.powerW, s_tel.chipTempC, s_tel.vrTempC, fl);
    }
}

void task(void *)
{
    nvsLoad();
    Board *b = SYSTEM_MODULE.getBoard();
    { Lock l; setupBoard(b); }
    s_started = true;
    ESP_LOGI(TAG, "started: profile %s, auto-tune %s, limits %d C / %d C / %d W", profileName(s_cfg.profile),
             s_cfg.autotune.enabled ? "on" : "off", s_cfg.limits.chipTempC, s_cfg.limits.vrTempC, s_cfg.limits.powerW);
    while (1) {
        if (!POWER_MANAGEMENT_MODULE.isShutdown()) step(b);
        if (s_cfgDirty) nvsSaveCfg();
        int64_t utc = utcNow();
        // save the long history every 30 min (about 1.5 KB) or when a restart is coming
        if (s_saveReq || (utc && utc - s_lastHistSave >= 1800 && s_roll.size() > 0)) {
            nvsSaveHist();
            s_lastHistSave = utc;
            if (s_saveReq) { s_saveReq = false; if (s_saved) xSemaphoreGive(s_saved); }
        }
        for (int i = 0; i < TICK_MS / 250 && !s_saveReq; i++) vTaskDelay(pdMS_TO_TICKS(250));
    }
}

} // namespace

void costean_init()
{
    if (s_mx) return;
    s_mx = xSemaphoreCreateRecursiveMutex();
    s_saved = xSemaphoreCreateBinary();
    defaultSchedule(s_cfg.schedule);
    // internal-RAM stack: this task writes NVS (flash), which must not run on a PSRAM stack
    xTaskCreate(task, "costean", 6144, nullptr, 4, nullptr);
}

void costean_before_restart()
{
    if (!s_started || !s_saved) return;
    s_saveReq = true;
    xSemaphoreTake(s_saved, pdMS_TO_TICKS(1500));
}

uint8_t costean_lcd_mode() { return (uint8_t) s_sched.lcd; }

void costean_lcd_info(CosteanLcdInfo &o)
{
    memset(&o, 0, sizeof o);
    Lock l;
    const char *p = s_active == Profile::ECO ? "ECO" : s_active == Profile::STOCK ? "STOCK" : s_active == Profile::TURBO ? "TURBO" : "MANUAL";
    snprintf(o.profile, sizeof o.profile, "%s", p);
    if (s_gov.throttleSteps() > 0) snprintf(o.state, sizeof o.state, "THROTTLE");
    else if (s_gov.tuneSteps() > 0) snprintf(o.state, sizeof o.state, "TUNE +%d", s_gov.tuneSteps() & 7);
    else if (s_cfg.autotune.enabled) snprintf(o.state, sizeof o.state, "AUTO");
    else snprintf(o.state, sizeof o.state, "HOLD");
    if (s_sched.profileWindow >= 0) snprintf(o.window, sizeof o.window, "%s", s_cfg.schedule.windows[s_sched.profileWindow].name);
    else if (s_sched.lcdWindow >= 0) snprintf(o.window, sizeof o.window, "%s", s_cfg.schedule.windows[s_sched.lcdWindow].name);
    o.lcdMode = (uint8_t) s_sched.lcd;
    o.throttled = s_gov.throttleSteps() > 0;
    o.scheduled = s_sched.profileWindow >= 0;
    o.tuneSteps = s_gov.tuneSteps();
    Limits lim = effectiveLimits();
    o.limitChip = lim.chipTempC; o.limitVr = lim.vrTempC; o.limitPower = lim.powerW;
}

void costean_status_json(JsonObject o)
{
    Lock l;
    o["activeProfile"] = profileName(s_active);
    o["profileSource"] = s_sched.profile != Profile::NONE ? "schedule" : "user";
    JsonObject t = o["target"].to<JsonObject>(); t["mhz"] = s_target.mhz; t["mv"] = s_target.mv;
    JsonObject a = o["applied"].to<JsonObject>(); a["mhz"] = s_applied.mhz; a["mv"] = s_applied.mv;
    JsonObject g = o["governor"].to<JsonObject>();
    g["state"] = govStateName(s_gov.state()); g["reason"] = s_gov.reason();
    g["throttleSteps"] = s_gov.throttleSteps(); g["tuneSteps"] = s_gov.tuneSteps(); g["bestStableSteps"] = s_gov.bestStableSteps();
    g["errorPct"] = s_tel.errorPct; g["rejectPct"] = s_tel.rejectPct; g["mining"] = s_tel.mining;
    Limits lim = effectiveLimits();
    JsonObject el = o["effectiveLimits"].to<JsonObject>(); el["chipTemp"] = lim.chipTempC; el["vrTemp"] = lim.vrTempC; el["power"] = lim.powerW;
    JsonObject pr = o["presets"].to<JsonObject>();
    for (Profile p : {Profile::ECO, Profile::STOCK, Profile::TURBO, Profile::CUSTOM}) {
        JsonObject x = pr[profileName(p)].to<JsonObject>(); OpPoint op = s_presets.of(p); x["mhz"] = op.mhz; x["mv"] = op.mv;
    }
    JsonObject env = o["envelope"].to<JsonObject>();
    env["minMhz"] = s_gov.envelope().minMhz; env["maxMhz"] = s_gov.envelope().maxMhz; env["stepMhz"] = s_gov.envelope().stepMhz;
    JsonObject sc = o["schedule"].to<JsonObject>();
    sc["clockValid"] = s_sched.clockValid; sc["profileWindow"] = s_sched.profileWindow; sc["lcdWindow"] = s_sched.lcdWindow;
    sc["lcd"] = lcdName(s_sched.lcd); sc["profile"] = profileName(s_sched.profile);
    int64_t utc = utcNow();
    if (utc) {
        const Zone &z = zone();
        sc["localTime"] = toLocal(utc, z);
        sc["dst"] = isDst(utc, z);
        if (s_sched.nextChangeLocal) sc["nextChangeLocal"] = s_sched.nextChangeLocal;
    }
    JsonArray zs = o["zones"].to<JsonArray>();
    for (int i = 0; zoneAt(i); i++) zs.add(zoneAt(i)->id);
    o["failover"] = s_failover;
    o["poolMode"] = Config::getPoolMode();
    o["maxPools"] = 2; o["designMaxPools"] = kMaxPools;
}

void costean_config_json(JsonObject o) { Lock l; cfgToJson(s_cfg, o); }

void costean_history_json(JsonObject o)
{
    Lock l;
    o["slotSec"] = Rollup::kSlotSec;
    o["start"] = s_roll.size() ? s_roll.slotTime(0) : 0;
    JsonArray gh = o["ghs"].to<JsonArray>(), w = o["watts"].to<JsonArray>(), c = o["chipTemp"].to<JsonArray>(),
              v = o["vrTemp"].to<JsonArray>(), e = o["jth"].to<JsonArray>(), f = o["flags"].to<JsonArray>();
    for (int i = 0; i < s_roll.size(); i++) {
        const RollupPoint &p = s_roll.at(i);
        if (p.ghs == 0xFFFF) { gh.add(nullptr); w.add(nullptr); c.add(nullptr); v.add(nullptr); e.add(nullptr); f.add(0); continue; }
        gh.add(p.ghs); w.add(p.watts10 / 10.0f); c.add(p.chip2 / 2.0f); v.add(p.vr2 / 2.0f);
        if (p.eff == 255) e.add(nullptr); else e.add(p.eff);
        f.add(p.flags);
    }
}

const char *costean_patch_config(const JsonDocument &doc)
{
    Lock l;
    Cfg c = s_cfg;
    const char *err = cfgFromJson(c, doc.as<JsonVariantConst>());
    if (err) return err;
    bool tuneWasOn = s_cfg.autotune.enabled;
    s_cfg = c;
    if (doc["autotune"]["reset"].is<bool>() && doc["autotune"]["reset"].as<bool>()) s_gov.resetTune();
    if (tuneWasOn != s_cfg.autotune.enabled) ESP_LOGW(TAG, "auto-tune %s", s_cfg.autotune.enabled ? "ENABLED" : "disabled");
    s_cfgDirty = true;
    return nullptr;
}
