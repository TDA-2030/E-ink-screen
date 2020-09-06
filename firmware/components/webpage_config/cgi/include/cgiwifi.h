#ifndef CGIWIFI_H
#define CGIWIFI_H

#include "esp_err.h"
#include "esp_http_server.h"


// int rssi2perc(int rssi);
// const char *auth2str(int auth);
// const char *opmode2str(int opmode);
esp_err_t cgi_is_success(void);

esp_err_t cgi_common_get_handler(httpd_req_t *req);
esp_err_t cgiWiFiScan(httpd_req_t *req);
// esp_err_t tplWlan(httpd_req_t *req, char *token, void **arg);
// esp_err_t cgiWiFi(httpd_req_t *req);
esp_err_t cgiWiFiConnect(httpd_req_t *req);
esp_err_t cgiWiFiSetMode(httpd_req_t *req);
esp_err_t cgiWiFiSetChannel(httpd_req_t *req);
esp_err_t cgiWiFiConnStatus(httpd_req_t *req);
esp_err_t cgiWiFiConfigSuccess(httpd_req_t *req);
// esp_err_t cgiWiFiSetSSID(httpd_req_t *req);

#endif
