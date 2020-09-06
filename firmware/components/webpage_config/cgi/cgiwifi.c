#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"

#include "../cgi/include/cgiwifi.h"
#include "../adapt/include/esp32_wifi.h"

static const char *TAG = "cgiWifi";

// --------------------------------------------------------------------------
// defines
// --------------------------------------------------------------------------

#define GOOD_IP2STR(ip) ((ip) >> 0) & 0xff, ((ip) >> 8) & 0xff, ((ip) >> 16) & 0xff, ((ip) >> 24) & 0xff

#define CONNTRY_IDLE 0
#define CONNTRY_WORKING 1
#define CONNTRY_SUCCESS 2
#define CONNTRY_FAIL 3

#define SCAN_IDLE 0
#define SCAN_RUNING 1
#define SCAN_DONE 2
#define SCAN_INVALID 3

// Scan result
typedef struct {
    char scanInProgress; // if 1, don't access the underlying stuff from the webpage.
    wifi_ap_record_t *apData;
    int noAps;
} ScanResultData;

// Static scan status storage.
static ScanResultData g_cgiWifiAps = {0};

// Connection result var
static int g_connTryStatus = CONNTRY_IDLE;

// Temp store for new ap info.
static wifi_config_t g_wifi_config_cgi = {0};

static TaskHandle_t g_staConnectHandle;

// --------------------------------------------------------------------------
// taken from MightyPork/libesphttpd
// --------------------------------------------------------------------------

static int rssi2perc(int rssi)
{
    int r;

    if (rssi > 200) {
        r = 100;
    } else if (rssi < 100) {
        r = 0;
    } else {
        r = 100 - 2 * (200 - rssi); // approx.
    }

    if (r > 100) {
        r = 100;
    }

    if (r < 0) {
        r = 0;
    }

    return r;
}

// static const char *auth2str(int auth)
// {
//     switch (auth) {
//         case WIFI_AUTH_OPEN:
//             return "Open";

//         case WIFI_AUTH_WEP:
//             return "WEP";

//         case WIFI_AUTH_WPA_PSK:
//             return "WPA";

//         case WIFI_AUTH_WPA2_PSK:
//             return "WPA2";

//         case WIFI_AUTH_WPA_WPA2_PSK:
//             return "WPA/WPA2";

//         default:
//             return "Unknown";
//     }
// }

// static const char *opmode2str(int opmode)
// {
//     switch (opmode) {
//         case WIFI_MODE_NULL:
//             return "Disabled";

//         case WIFI_MODE_STA:
//             return "Client";

//         case WIFI_MODE_AP:
//             return "AP only";

//         case WIFI_MODE_APSTA:
//             return "Client+AP";

//         default:
//             return "Unknown";
//     }
// }

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)
/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";

    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }

    return httpd_resp_set_type(req, type);
}
#undef CHECK_FILE_EXTENSION

// Stupid li'l helper function that returns the value of a hex char.
static int httpdHexVal(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return 0;
}
// Decode a percent-encoded value.
// Takes the valLen bytes stored in val, and converts it into at most retLen bytes that
// are stored in the ret buffer. Returns the actual amount of bytes used in ret. Also
// zero-terminates the ret buffer.

static bool httpdUrlDecode(const char *val, int valLen, char *ret, int retLen, int *bytesWritten)
{
    int s = 0; // index of theread position in val
    int d = 0; // index of the write position in 'ret'
    int esced = 0, escVal = 0;

    // d loops for ( retLen - 1 ) to ensure there is space for the null terminator
    while (s < valLen && d < (retLen - 1)) {
        if (esced == 1) {
            escVal = httpdHexVal(val[s]) << 4;
            esced = 2;
        } else if (esced == 2) {
            escVal += httpdHexVal(val[s]);
            ret[d++] = escVal;
            esced = 0;
        } else if (val[s] == '%') {
            esced = 1;
        } else if (val[s] == '+') {
            ret[d++] = ' ';
        } else {
            ret[d++] = val[s];
        }

        s++;
    }

    ret[d++] = 0;
    *bytesWritten = d;

    // if s == valLen then we processed all of the input bytes
    return (s == valLen) ? true : false;
}

// Find a specific arg in a string of get- or post-data.
// Line is the string of post/get-data, arg is the name of the value to find. The
// zero-terminated result is written in buff, with at most buffLen bytes used. The
// function returns the length of the result, or -1 if the value wasn't found. The
// returned string will be urldecoded already.

static int httpdFindArg(const char *line, const char *arg, char *buff, int buffLen)
{
    const char *p, *e;

    if (line == NULL) {
        return -1;
    }

    const int arglen = strlen(arg);
    p = line;

    while (p != NULL && *p != '\n' && *p != '\r' && *p != 0) {
        // ESP_LOGD( TAG, "findArg: %s", p );
        // check if line starts with "arg="
        if (strncmp(p, arg, arglen) == 0 && p[arglen] == '=') {
            p += arglen + 1; // move p to start of value
            e = strstr(p, "&");

            if (e == NULL) {
                e = p + strlen(p);
            }

            // ESP_LOGD( TAG, "findArg: val %s len %d", p, ( e-p ) );
            int bytesWritten;

            if (!httpdUrlDecode(p, (e - p), buff, buffLen, &bytesWritten)) {
                // TODO: proper error return through this code path
                ESP_LOGE(TAG, "out of space storing url");
            }

            return bytesWritten;
        }

        // line doesn't start with "arg=", so look for '&'
        p = strstr(p, "&");

        if (p != NULL) {
            p += 1;
        }
    }

    // ESP_LOGD( TAG, "Finding %s in %s: Not found", arg, line );
    return -1; // not found
}

static esp_err_t get_req_data(httpd_req_t *req, char **out_buf)
{
    char *buf = malloc(req->content_len + 1);
    size_t off = 0;
    esp_err_t ret;

    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory of %d bytes!", req->content_len + 1);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (off < req->content_len) {
        /* Read data received in the request */
        ret = httpd_req_recv(req, buf + off, req->content_len - off);

        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }

            free(buf);
            return ESP_FAIL;
        }

        off += ret;
    }

    buf[off] = '\0';
    *out_buf = buf;
    ESP_LOGD(TAG, "receive request content:%s", buf);
    return ESP_OK;
}

static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    extern const unsigned char index_script_start[] asm("_binary_wifi_html_start");
    extern const unsigned char index_script_end[] asm("_binary_wifi_html_end");
    const size_t index_script_size = (index_script_end - index_script_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_script_start, index_script_size);
    return ESP_OK;
}

esp_err_t cgi_common_get_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG, "Common GET uri:\"%s\"", req->uri);

    if (req->uri[strlen(req->uri) - 1] == '/') {
        return index_html_get_handler(req);
    } else if (strcmp(req->uri, "/index.html") == 0) {
        return index_html_get_handler(req);
    } else if (strcmp(req->uri, "/wifi.tpl") == 0) {
        return index_html_get_handler(req);
    } else if (strcmp(req->uri, "/favicon.ico") == 0) {
        extern const unsigned char favicon_ico_start[] asm("_binary_cfg_favicon_ico_start");
        extern const unsigned char favicon_ico_end[] asm("_binary_cfg_favicon_ico_end");
        const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
        set_content_type_from_file(req, req->uri);
        return httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    } else if (strcmp(req->uri, "/style.css") == 0) {
        extern const unsigned char style_css_start[] asm("_binary_style_css_start");
        extern const unsigned char style_css_end[] asm("_binary_style_css_end");
        const size_t style_css_size = (style_css_end - style_css_start);
        set_content_type_from_file(req, req->uri);
        return httpd_resp_send(req, (const char *)style_css_start, style_css_size);
    } else if (strcmp(req->uri, "/140medley.min.js") == 0) {
        extern const unsigned char file_140medley_start[] asm("_binary_medley_min_js_start");
        extern const unsigned char file_140medley_end[] asm("_binary_medley_min_js_end");
        const size_t file_140medley_size = (file_140medley_end - file_140medley_start);
        set_content_type_from_file(req, req->uri);
        return httpd_resp_send(req, (const char *)file_140medley_start, file_140medley_size);
    } else if (strcmp(req->uri, "/wifi.png") == 0) {
        extern const unsigned char wifi_png_start[] asm("_binary_wifi_png_start");
        extern const unsigned char wifi_png_end[] asm("_binary_wifi_png_end");
        const size_t wifi_png_size = (wifi_png_end - wifi_png_start);
        set_content_type_from_file(req, req->uri);
        return httpd_resp_send(req, (const char *)wifi_png_start, wifi_png_size);
    } else if (strcmp(req->uri, "/connecting.html") == 0) {
        extern const unsigned char connecting_start[] asm("_binary_connecting_html_start");
        extern const unsigned char connecting_end[] asm("_binary_connecting_html_end");
        const size_t connecting_size = (connecting_end - connecting_start);
        set_content_type_from_file(req, req->uri);
        return httpd_resp_send(req, (const char *)connecting_start, connecting_size);
    } else {
        ESP_LOGE(TAG, "Unsupport URI \"%s\"", req->uri);
    }

    return ESP_OK;
}

esp_err_t cgiWiFiScan(httpd_req_t *req)
{
    int len;
    char buff[256];

    ESP_LOGI(TAG, "cgiWiFiScan%s state %d  ...", g_cgiWifiAps.scanInProgress ? " in propgress ..." : "", g_cgiWifiAps.scanInProgress);

    if (g_cgiWifiAps.scanInProgress == SCAN_IDLE) {
        // idle state -> start scaning
        wifi_mode_t mode;
        wifiGetCurrentMode(&mode);

        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            // Start a new scan.
            ESP_LOGI(TAG, "Start a new scan ...");
            wifiStartScan();
            g_cgiWifiAps.scanInProgress = SCAN_RUNING;
        } else {
            ESP_LOGE(TAG, "Cannot start a new scan because not in a station mode");
            return ESP_OK;
        }
    }

    if (g_cgiWifiAps.scanInProgress == SCAN_RUNING) {
        // scan is runing, wait for done

        if (xEventGroupGetBits(g_wifi_event_group) & WIFI_SCAN_DONE) {
            // wifi scan done
            ESP_LOGI(TAG, "wifi scan done");

            // Clear prev ap data if needed.
            if (g_cgiWifiAps.apData != NULL) {
                free(g_cgiWifiAps.apData);
            }

            g_cgiWifiAps.noAps = wifiScanDone(&g_cgiWifiAps.apData); // allocates memory

            if (g_cgiWifiAps.apData == NULL) {
                ESP_LOGI(TAG, "Cannot allocate memory for scan result");
                g_cgiWifiAps.noAps = 0;
            }

            g_cgiWifiAps.scanInProgress = SCAN_DONE;
        }

        // We're still scanning. Tell Javascript code that.
        ESP_LOGI(TAG, "We're still scanning");
        len = sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
        httpd_resp_send(req, buff, len);
        return ESP_OK;
    } else if (g_cgiWifiAps.scanInProgress == SCAN_DONE) {
        int pos = 0;
        ESP_LOGI(TAG, "We have a scan result");
        while (1) {
            if (0 == pos) {
                // update web page
                httpd_resp_set_type(req, "application/json");
                // We have a scan result. Send json header
                len = sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n");
                httpd_resp_send_chunk(req, buff, len);
            }

            if (pos < g_cgiWifiAps.noAps) {
                // Fill in json code for an access point
                // ESP_LOGD( TAG, "Fill in json code for an access point" );
                int rssi = g_cgiWifiAps.apData[pos].rssi;

                len = sprintf(buff, "{\"essid\": \"%s\", \"bssid\": \"" MACSTR "\", \"rssi\": \"%d\", \"rssi_perc\": \"%d\", \"enc\": \"%d\", \"channel\": \"%d\"}%s\r\n",
                              g_cgiWifiAps.apData[pos].ssid,
                              MAC2STR(g_cgiWifiAps.apData[pos].bssid),
                              rssi,
                              rssi2perc(rssi),
                              g_cgiWifiAps.apData[pos].authmode,
                              g_cgiWifiAps.apData[pos].primary,
                              (pos == g_cgiWifiAps.noAps - 1) ? " " : ","); // <-terminator

                httpd_resp_send_chunk(req, buff, len);
                ESP_LOGI(TAG, "send scan result: %s", buff);
            } else {
                // terminate datas of previous scan and start a new scan
                len = sprintf(buff, "]\n}\n}\n"); // terminate the whole object

                httpd_resp_send_chunk(req, buff, len);
                /* Send empty chunk to signal HTTP response completion */
                httpd_resp_sendstr_chunk(req, NULL);
                ESP_LOGD(TAG, "%s", buff);

                // Clear ap data
                if (g_cgiWifiAps.apData != NULL) {
                    free(g_cgiWifiAps.apData);
                    g_cgiWifiAps.apData = NULL;
                }

                g_cgiWifiAps.scanInProgress = SCAN_IDLE;
                break;
            }

            pos++;
        }
    }

    return ESP_OK;
}

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.
// Redirect to the given URL.
static void httpdRedirect(httpd_req_t *req, const char *newUrl)
{
    httpd_resp_set_status(req, "302 Redirect");
    httpd_resp_set_hdr(req, "Location", newUrl);
    httpd_resp_send(req, NULL, 0); // Response body can be empty
}

// Station connect to AP.
static void staWiFiDoConnect(void *arg)
{
    /**
     *  Delay for processing uri "/connecting.html" 
     */
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "staWiFiDoConnect: to AP \"%s\" pw \"%s\"", g_wifi_config_cgi.sta.ssid, g_wifi_config_cgi.sta.password);

    ESP_LOGD(TAG, "Wifi disconnect ...");
    wifiDisconnect(); // waits for disconnected

    wifi_mode_t mode;
    wifiGetCurrentMode(&mode);

    // keep AP mode, when set
    if (mode & WIFI_MODE_AP) {
        ESP_LOGD(TAG, "Wifi set mode to APSTA");
        mode = WIFI_MODE_APSTA;
    } else {
        ESP_LOGD(TAG, "Wifi set mode to STA");
        mode = WIFI_MODE_STA;
    }

    wifiSetConfig(WIFI_IF_STA, &g_wifi_config_cgi);

    ESP_LOGD(TAG, "Connect....");

    if (wifiConnect()) {
        g_connTryStatus = CONNTRY_WORKING;
    } else {
        g_connTryStatus = CONNTRY_FAIL;
    }

    ESP_LOGD(TAG, "Delete task staWiFiDoConnect");
    vTaskDelete(NULL);
}

esp_err_t cgiWiFiConnect(httpd_req_t *req)
{
    ESP_LOGI(TAG, "cgiWiFiConnect ... ");
    char essid[64] = {0};
    char password[64] = {0};

    // stop scanning
    if (g_cgiWifiAps.scanInProgress != SCAN_IDLE) {
        ESP_LOGI(TAG, "cgiWiFiConnect: stop scan ");
        wifiStopScan();
        g_cgiWifiAps.scanInProgress = SCAN_IDLE;
        g_cgiWifiAps.noAps = 0;
    }

    char *buf;
    get_req_data(req, &buf);
    int ssilen = httpdFindArg(buf, "essid", essid, sizeof(essid));
    int passlen = httpdFindArg(buf, "password", password, sizeof(password));

    if (ssilen == -1 || passlen == -1) {
        ESP_LOGE(TAG, "Not rx needed args!");
        httpdRedirect(req, "/wifi.tpl");
    } else {
        // get the current wifi configuration
        // check if we are in station mode
        if (ESP_OK == wifiGetConfig(WIFI_IF_STA, &g_wifi_config_cgi)) {
            strncpy((char *)g_wifi_config_cgi.sta.ssid, essid, 32);
            strncpy((char *)g_wifi_config_cgi.sta.password, password, 64);
            ESP_LOGI(TAG, "Try to connect to AP \"%s\" pw \"%s\"", essid, password);

            // redirect & start connecting a little bit later
            // Schedule disconnect/connect in call back function
            g_connTryStatus = CONNTRY_IDLE;
            xTaskCreate(staWiFiDoConnect, "staDoConnect", 2048, NULL, 6, &g_staConnectHandle);
            httpdRedirect(req, "/connecting.html"); // -> "connstatus.cgi" -> cgiWiFiConnStatus()
        }
    }

    free(buf);
    return ESP_OK;
}

// called from web page /wifi/connecting.html --> GET connstatus.cgi -->
// json parameter
//      "status"  : "idle" | "success" | "working" | "fail"
//      "ip"
esp_err_t cgiWiFiConnStatus(httpd_req_t *req)
{
    ESP_LOGI(TAG, "cgiWiFiConnStatus: %d - %s", g_connTryStatus,
             g_connTryStatus == 0 ? "Idle" : g_connTryStatus == 1 ? "Working" : g_connTryStatus == 2 ? "Success" : "Fail");
    char buff[128];
    int len;

    httpd_resp_set_type(req, "application/json");

    if (g_connTryStatus == CONNTRY_IDLE) {
        len = sprintf(buff, "{ \"status\": \"idle\" }");
    } else if (g_connTryStatus == CONNTRY_WORKING || g_connTryStatus == CONNTRY_SUCCESS) {
        EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);

        if (bits & WIFI_STA_CONNECTED) {
            tcpip_adapter_ip_info_t ipInfo;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
            len = sprintf(buff, "{\"status\": \"success\", \"ip\": \"" IPSTR "\"}", GOOD_IP2STR(ipInfo.ip.addr));
        } else {
            len = sprintf(buff, "{ \"status\": \"working\" }");
        }
    } else {
        len = sprintf(buff, "{ \"status\": \"fail\" }");
    }

    ESP_LOGI(TAG, "Upload WiFi connect status: %s", buff);
    httpd_resp_send(req, buff, len);
    return ESP_OK;
}

esp_err_t cgi_is_success(void)
{
    if (g_connTryStatus == CONNTRY_SUCCESS) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t cgiWiFiConfigSuccess(httpd_req_t *req)
{
    char config_status[32] = {0};
    char *buf;
    get_req_data(req, &buf);

    int status_len = httpdFindArg(buf, "configstatus", config_status, sizeof(config_status));

    if (status_len == -1) {
        ESP_LOGE(TAG, "Not rx needed args!");
    } else {

        // if receiced wifi config success status from client, set WiFi operating mode as only STA mode.
        if (strncmp(config_status, "success", strlen("success")) == 0) {
            ESP_LOGI(TAG, "WiFi config success, set WiFi operating mode as only STA mode.");
            wifiSetNewMode(WIFI_MODE_STA);
            g_connTryStatus = CONNTRY_SUCCESS;
        } else {
            ESP_LOGE(TAG, "WiFi config fail!");
        }
    }

    free(buf);

    return ESP_OK;
}
