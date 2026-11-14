// Costean OS API: GET /api/costean (status + config), GET /api/costean/history, PATCH /api/costean
#include "handler_costean.h"
#include <string.h>
#include "ArduinoJson.h"
#include "esp_log.h"
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "../costean/costean_core.h"

esp_err_t GET_costean(httpd_req_t *req)
{
    ConGuard g(http_server, req);
    if (is_network_allowed(req) != ESP_OK) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return ESP_FAIL; }
    PSRAMAllocator al; JsonDocument doc(&al);
    if (strstr(req->uri, "/history")) {
        costean_history_json(doc.to<JsonObject>());
    } else {
        costean_status_json(doc["status"].to<JsonObject>());
        costean_config_json(doc["config"].to<JsonObject>());
    }
    return sendJsonResponse(req, doc);
}

esp_err_t PATCH_costean(httpd_req_t *req)
{
    ConGuard g(http_server, req);
    if (is_network_allowed(req) != ESP_OK) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return ESP_FAIL; }
    if (validateOTP(req) != ESP_OK) return ESP_FAIL;
    PSRAMAllocator al; JsonDocument doc(&al);
    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) return err;
    const char *msg = costean_patch_config(doc);
    if (msg) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, msg);
    doc.clear();
    costean_config_json(doc.to<JsonObject>());
    return sendJsonResponse(req, doc);
}
