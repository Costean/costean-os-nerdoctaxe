#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "global_state.h"
#include "create_jobs_task.h"
#include "macros.h"
#include "nvs_config.h"
#include "stratum_manager_dual_pool.h"
#include "utils.h"
#include "utils.h"

StratumManagerDualPool::StratumManagerDualPool() : StratumManager(PoolMode::DUAL)
{
    // NOP
}

void StratumManagerDualPool::reconnectTimerCallback(int index)
{
    PThreadGuard lock(m_mutex);

    // verify-blocked pool must not reconnect until the block is cleared
    if (isVerifyBlocked(index)) return;

    // dual pool: both pools always try to stay alive
    m_stratumTasks[index]->connect();
}

void StratumManagerDualPool::connectedCallback(int index)
{
    PThreadGuard lock(m_mutex);

    // Blocked pool connected (e.g. after settings save) — kick it off immediately
    if (isVerifyBlocked(index)) {
        m_stratumTasks[index]->disconnect();
        return;
    }

    // reset poolDiffErr
    if (index >= 0 && index < 2) {
        m_poolDiffErr[index] = false;
    }
}

void StratumManagerDualPool::disconnectedCallback(int index)
{
    PThreadGuard lock(m_mutex);
    create_job_invalidate(index);
    m_stratumTasks[index]->m_validNotify = false;
    m_stratumTasks[index]->startReconnectTimer();
}

// Costean OS: the split itself lives in costean::PoolSplit (smooth weighted round robin
// per ASIC job, failover hands a dead pool's share to the healthy ones, 30 s rejoin delay).
static uint64_t nowMs() { return (uint64_t) (esp_timer_get_time() / 1000); }

int StratumManagerDualPool::getNextActivePool()
{
    PThreadGuard lock(m_mutex);
    costean::PoolHealth h[2];
    for (int i = 0; i < 2; i++) {
        h[i].configured = m_stratumConfig[i] && m_stratumConfig[i]->getHost() && m_stratumConfig[i]->getHost()[0];
        h[i].hasWork = m_stratumTasks[i] && m_stratumTasks[i]->m_validNotify;
        h[i].blocked = isVerifyBlocked(i);
    }
    if (m_split.update(h, nowMs())) {
        ESP_LOGI(m_tag, "pool split now %d%% / %d%%%s", m_split.effective(0), m_split.effective(1),
                 m_split.failoverActive() ? " (failover)" : "");
    }
    int p = m_split.next();
    return p < 0 ? PRIMARY : p;   // nobody has work: same as stock (job is skipped as invalid)
}

void StratumManagerDualPool::acceptedShare(int pool)
{
    if (pool < 0 || pool >= 2) return;
    m_accepted[pool]++;
    m_split.onAccepted(pool, (double) m_poolDifficulty[pool], nowMs());
}

void StratumManagerDualPool::getSplitView(CosteanPoolView *v, int n)
{
    PThreadGuard lock(m_mutex);
    uint64_t t = nowMs();
    for (int i = 0; i < n && i < 2; i++) {
        v[i].configured = m_stratumConfig[i] && m_stratumConfig[i]->getHost() && m_stratumConfig[i]->getHost()[0];
        v[i].healthy = m_split.healthy(i);
        v[i].weight = m_split.weight(i);
        v[i].effective = m_split.effective(i);
        v[i].jobShare = m_split.jobShare(i);
        v[i].creditedGhs = (float) m_split.creditedGhs(i, t);
        v[i].accepted = m_accepted[i];
        v[i].rejected = m_rejected[i];
    }
}

bool StratumManagerDualPool::isFailoverActive()
{
    PThreadGuard lock(m_mutex);
    return m_split.failoverActive();
}

const char *StratumManagerDualPool::getPoolHost(int pool)
{
    if (!m_stratumConfig[pool]) {
        return "-";
    }
    return m_stratumConfig[pool]->getHost();
}

int StratumManagerDualPool::getPoolPort(int pool)
{
    if (!m_stratumConfig[pool]) {
        return 0;
    }
    return m_stratumConfig[pool]->getPort();
}

uint64_t StratumManagerDualPool::getSharesAccepted(int pool) {
    return m_accepted[pool];
}

uint64_t StratumManagerDualPool::getSharesRejected(int pool) {
    return m_rejected[pool];
}

uint32_t StratumManagerDualPool::selectAsicDiff(int pool, uint32_t poolDiff)
{
    Board *board = SYSTEM_MODULE.getBoard();
    uint32_t asicMax = board->getAsicMaxDifficulty();
    uint32_t asicMin = board->getAsicMinDifficultyDualPool();

    static uint32_t poolDiffs[2] = {0xffffffffu, 0xffffffffu};

    // shouldn't happen
    if (pool < 0 || pool >= 2) {
        return asicMax;
    }

    m_poolDiffErr[pool] = poolDiff < asicMin;

    poolDiffs[pool] = poolDiff;

    uint32_t minDiff = std::min(poolDiffs[0], poolDiffs[1]);

    // clamp to ASIC range
    if (minDiff < asicMin) {
        return asicMin;
    }
    if (minDiff > asicMax) {
        return asicMax;
    }
    return minDiff;
}

bool StratumManagerDualPool::acceptsNotifyFrom(int pool)
{
    return true;
}

int StratumManagerDualPool::getActivePoolBalance(int pool)
{
    PThreadGuard lock(m_mutex);
    return m_split.effective(pool);
}

float StratumManagerDualPool::getActivePoolHashrate(int pool)
{
    float share;
    {
        PThreadGuard lock(m_mutex);
        share = m_split.jobShare(pool);
    }
    return SYSTEM_MODULE.getCurrentHashrate() * share;
}

void StratumManagerDualPool::loadSettings()
{
    PThreadGuard lock(m_mutex);

    uint16_t newBalance = Config::getPoolBalance();

    bool reconnect = false;
    if (m_balance != newBalance) {
        m_balance = newBalance;
        m_error_accum = 0;
        reconnect = true;
    }
    if (m_balance > 100) m_balance = 100;
    {
        uint8_t w[2] = {(uint8_t) m_balance, (uint8_t) (100 - m_balance)};
        m_split.setWeights(w, 2);
    }

    StratumManager::loadSettings(reconnect);
};

void StratumManagerDualPool::saveSettings(const JsonDocument &doc) {
    PThreadGuard lock(m_mutex);

    if (doc["poolBalance"].is<uint16_t>()) {
        Config::setPoolBalance(doc["poolBalance"].as<uint16_t>());
    }
    StratumManager::saveSettings(doc);
    Config::flush();
}

void StratumManagerDualPool::checkForBestDiff(int pool, double diff, uint32_t nbits)
{
    PThreadGuard lock(m_mutex);

    if (pool < 0 || pool >= 2) {
        return;
    }

    if ((uint64_t) diff > m_bestSessionDiff[pool]) {
        m_bestSessionDiff[pool] = (uint64_t) diff;
        suffixString(std::max(m_bestSessionDiff[0], m_bestSessionDiff[1]), m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
    }

    StratumManager::checkForBestDiff(pool, diff, nbits);
}

void StratumManagerDualPool::getManagerInfoJson(JsonObject &obj, bool verbose)
{
    PThreadGuard lock(m_mutex);

    StratumManager::getManagerInfoJson(obj, verbose);

    obj["splitFailover"] = m_split.failoverActive();
    JsonArray arr = obj["pools"].to<JsonArray>();

    for (int i = 0; i < 2; i++) {
        JsonObject pool = arr.add<JsonObject>();

        pool["connected"] = m_stratumTasks[i] ? m_stratumTasks[i]->m_isConnected : false;
        pool["poolDifficulty"] = m_poolDifficulty[i];
        pool["networkDifficulty"] = m_networkDifficulty[i];
        pool["poolDiffErr"] = m_poolDiffErr[i];
        pool["accepted"] = m_accepted[i];
        pool["rejected"] = m_rejected[i];
        pool["pingRtt"]  = m_pingTasks[i] ? m_pingTasks[i]->get_last_ping_rtt() : 0;
        pool["pingLoss"] = m_pingTasks[i] ? m_pingTasks[i]->get_recent_ping_loss() : 0;
        pool["bestDiff"] = m_bestSessionDiff[i];
        // Costean OS split
        pool["splitWeight"] = m_split.weight(i);
        pool["splitEffective"] = m_split.effective(i);
        pool["splitHealthy"] = m_split.healthy(i);
        pool["jobShare"] = m_split.jobShare(i);
        pool["creditedHashrate"] = m_split.creditedGhs(i, nowMs());
        pool["assignedHashrate"] = SYSTEM_MODULE.getCurrentHashrate() * m_split.jobShare(i);

        // dashboard-only fields — kept out of the legacy v1 /info so its
        // response stays small and shape-compatible with external clients
        if (verbose) {
            pool["active"] = true; // dual pool mode: both pools mine
            pool["verifyBlocked"] = getVerifyBlockedReason(i) ? getVerifyBlockedReason(i) : "";
            pool["activeProtocol"] = m_stratumConfig[i] ? (int)m_stratumConfig[i]->getProtocol() : 0;
            pool["encrypted"] = m_stratumConfig[i] ? (m_stratumConfig[i]->isSV2() || m_stratumConfig[i]->isTLS()) : false;
        }
    }
}
