#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "tcpip_adapter.h"
#include "include/esp32_wifi.h"

static const char *TAG = "esp32_wifi";

// WPS_TYPE_PBC: Start esp32 and it will enter wps PBC mode. Then push the
//               button of wps on router down. The esp32 will connected to
//               the router.
// WPS_TYPE_PIN: Start esp32, You'll see PIN code which is a eight-digit
//               number showing on COM. Enter the PIN code in router and then
//               the esp32 will connected to router.

// set wps mode via "make menuconfig"
#if CONFIG_WPS_TYPE_PBC
#define WPS_TEST_MODE WPS_TYPE_PBC
#elif CONFIG_WPS_TYPE_PIN
#define WPS_TEST_MODE WPS_TYPE_PIN
#else
#define WPS_TEST_MODE WPS_TYPE_DISABLE
#endif // CONFIG_EXAMPLE_WPS_TYPE_PBC

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

#ifndef GOOD_IP2STR
#define GOOD_IP2STR(ip) ((ip) >> 0) & 0xff, ((ip) >> 8) & 0xff, ((ip) >> 16) & 0xff, ((ip) >> 24) & 0xff
#endif

#define MAX_AP_NUM 48

// --------------------------------------------------------------------------
// global variables
// --------------------------------------------------------------------------

EventGroupHandle_t g_wifi_event_group;
const int WIFI_INITIALIZED = BIT0;
const int WIFI_STA_CONNECTED = BIT1; // ESP32 is currently connected
const int WIFI_AP_STACONNECTED = BIT2;

const int WIFI_STA_STARTED = BIT4;
const int WIFI_AP_STARTED = BIT5;     // Set automatically once the SoftAP is started
const int WIFI_STA_DISCONNECT = BIT6; // This bit is set automatically as soon as a connection was lost

const int WIFI_SCAN_DONE = BIT7; //

#define WIFI_ALL_EVENTS (0 | WIFI_INITIALIZED | WIFI_STA_CONNECTED | WIFI_AP_STACONNECTED | WIFI_STA_STARTED | WIFI_AP_STARTED | WIFI_STA_DISCONNECT | WIFI_SCAN_DONE)

static esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);

static const char *auth2str(int auth)
{
    switch (auth)
    {
    case WIFI_AUTH_OPEN:
        return "Open";

    case WIFI_AUTH_WEP:
        return "WEP";

    case WIFI_AUTH_WPA_PSK:
        return "WPA";

    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";

    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2";

    default:
        return "Unknown";
    }
}

static const char *mode2str(int mode)
{
    switch (mode)
    {
    case WIFI_MODE_NULL:
        return "Disabled";

    case WIFI_MODE_STA:
        return "Station";

    case WIFI_MODE_AP:
        return "SoftAP";

    case WIFI_MODE_APSTA:
        return "Station + SoftAP";

    default:
        return "Unknown";
    }
}

// see also C:\Espressif\esp-idf\components\esp32\event_default_handlers.c

// +----------------------------------+-------------------------------------+--------------------+
// | Event                            | Structure                           | Field              |
// +----------------------------------+-------------------------------------+--------------------+
// | SYSTEM_EVENT_STA_CONNECTED       | system_event_sta_connected_t        | connected          |
// | SYSTEM_EVENT_STA_DISCONNECTED    | system_event_sta_disconnected_t     | disconnected       |
// | SYSTEM_EVENT_SCAN_DONE           | system_event_sta_scan_done_t        | scan_done          |
// | SYSTEM_EVENT_STA_AUTHMODE_CHANGE | system_event_sta_authmode_change_t  | auth_change        |
// | SYSTEM_EVENT_STA_GOT_IP          | system_event_sta_got_ip_t           | got_ip             |
// | SYSTEM_EVENT_STA_WPS_ER_PIN      | system_event_sta_wps_er_pin_t       | sta_er_pin         |
// | SYSTEM_EVENT_STA_WPS_ER_FAILED   | system_event_sta_wps_fail_reason_t  | sta_er_fail_reason |
// | SYSTEM_EVENT_AP_STACONNECTED     | system_event_ap_staconnected_t      | sta_connected      |
// | SYSTEM_EVENT_AP_STADISCONNECTED  | system_event_ao_stadisconnected_t   | sta_disconnected   |
// | SYSTEM_EVENT_AP_PROBEREQRECVED   | system_event_ap_probe_req_rx_t      | ap_probereqrecved  |
// | SYSTEM_EVENT_GOT_IP6             | system_event_got_ip6_t              | got_ip6            |
// +----------------------------------+-------------------------------------+--------------------+

static esp_err_t wifiEventHandlerCb(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        // case SYSTEM_EVENT_WIFI_READY:             // ESP32 WiFi ready

    case SYSTEM_EVENT_SCAN_DONE:
    { // ESP32 finish scanning AP
        system_event_sta_scan_done_t *scan_done = &event->event_info.scan_done;
        ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE, status:%d, number:%d scan_id:%d", scan_done->status, scan_done->number, scan_done->scan_id);

        xEventGroupSetBits(g_wifi_event_group, WIFI_SCAN_DONE);
        break;
    }

    case SYSTEM_EVENT_STA_START:
    { // ESP32 station start
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");

        const char *hostName;
        ESP_ERROR_CHECK(tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &hostName));
        ESP_LOGD(TAG, "STA hostname: \"%s\"", hostName);

        xEventGroupSetBits(g_wifi_event_group, WIFI_STA_STARTED);
        break;
    }

    case SYSTEM_EVENT_STA_CONNECTED:
    { // ESP32 station connected to AP
        system_event_sta_connected_t *connected = &event->event_info.connected;
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED, ssid:\"%s\", ssid_len:%d, bssid:" MACSTR ", channel:%d, authmode:%d",
                 connected->ssid, connected->ssid_len, MAC2STR(connected->bssid), connected->channel, connected->authmode);

        break;
    }

    case SYSTEM_EVENT_STA_DISCONNECTED:
    { // ESP32 station disconnected from AP
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        // This is a workaround as ESP32 WiFi libs don't currently auto-reassociate.
        ESP_ERROR_CHECK(esp_wifi_connect());

        xEventGroupSetBits(g_wifi_event_group, WIFI_STA_DISCONNECT);
        xEventGroupClearBits(g_wifi_event_group, WIFI_STA_CONNECTED);
        break;
    }

    case SYSTEM_EVENT_STA_GOT_IP:
    { // ESP32 station got IP from connected AP
        system_event_sta_got_ip_t *got_ip = &event->event_info.got_ip;

        // cannot use ip4addr_ntoa() function, because it return for all parameter the a pointer to the same buffer
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP, ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
                 IP2STR(&got_ip->ip_info.ip),
                 IP2STR(&got_ip->ip_info.netmask),
                 IP2STR(&got_ip->ip_info.gw));

        xEventGroupSetBits(g_wifi_event_group, WIFI_STA_CONNECTED);
        break;
    }

    case SYSTEM_EVENT_STA_STOP:
    { // ESP32 station stop
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");

        xEventGroupClearBits(g_wifi_event_group, WIFI_STA_STARTED);
        break;
    }

        // case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:    // the auth mode of AP connected by ESP32 station changed

    case SYSTEM_EVENT_STA_LOST_IP:
    { // ESP32 station lost IP and the IP is reset to 0
        system_event_sta_got_ip_t *got_ip = &event->event_info.got_ip;

        // cannot use ip4addr_ntoa() function, because it return for all parameter the a pointer to the same buffer
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP, ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
                 IP2STR(&got_ip->ip_info.ip),
                 IP2STR(&got_ip->ip_info.netmask),
                 IP2STR(&got_ip->ip_info.gw));

        xEventGroupClearBits(g_wifi_event_group, WIFI_STA_CONNECTED);
        break;
    }

    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
        // point: the function esp_wifi_wps_start() only get ssid & password
        // so call the function esp_wifi_connect() here
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;

    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;

    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;

    case SYSTEM_EVENT_STA_WPS_ER_PIN:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
        // show the PIN code here
        ESP_LOGI(TAG, "WPS_PIN = " PINSTR, PIN2STR(event->event_info.sta_er_pin.pin_code));
        break;

    case SYSTEM_EVENT_AP_START:
    { // ESP32 soft-AP start
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START");

        const char *hostName;
        ESP_ERROR_CHECK(tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_AP, &hostName));
        ESP_LOGD(TAG, "AP hostname:  \"%s\"", hostName);

        xEventGroupSetBits(g_wifi_event_group, WIFI_AP_STARTED);
        break;
    }

    case SYSTEM_EVENT_AP_STACONNECTED:
    { // a station connected to ESP32 soft-AP
        system_event_ap_staconnected_t *staconnected = &event->event_info.sta_connected;
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED, mac:" MACSTR ", aid:%d",
                 MAC2STR(staconnected->mac), staconnected->aid);

        xEventGroupSetBits(g_wifi_event_group, WIFI_AP_STACONNECTED);
        break;
    }

    case SYSTEM_EVENT_AP_STADISCONNECTED:
    { // a station disconnected from ESP32 soft-AP
        system_event_ap_staconnected_t *staconnected = &event->event_info.sta_connected;
        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED, mac:" MACSTR ", aid:%d",
                 MAC2STR(staconnected->mac), staconnected->aid);

        xEventGroupClearBits(g_wifi_event_group, WIFI_AP_STACONNECTED);
        break;
    }

        // case SYSTEM_EVENT_AP_STOP:                // ESP32 soft-AP stop
        // case SYSTEM_EVENT_AP_PROBEREQRECVED:      // Receive probe request packet in soft-AP interface
        // case SYSTEM_EVENT_AP_STA_GOT_IP6:         // ESP32 station or ap interface v6IP addr is preferred

        // case SYSTEM_EVENT_ETH_START:              // ESP32 ethernet start
        // case SYSTEM_EVENT_ETH_STOP:               // ESP32 ethernet stop
        // case SYSTEM_EVENT_ETH_CONNECTED:          // ESP32 ethernet phy link up
        // case SYSTEM_EVENT_ETH_DISCONNECTED:       // ESP32 ethernet phy link down
        // case SYSTEM_EVENT_ETH_GOT_IP:             // ESP32 ethernet got IP from connected AP

    default:
        break;
    }

    return ESP_OK;
}

// in order to avoid a false positive on the front end app we need to quickly flush the ip json
// There'se a risk the front end sees an IP or a password error when in fact
// it's a remnant from a previous connection

bool wifiConnect(void)
{
    ESP_LOGD(TAG, "wifiConnect");

    EventBits_t evBits;
    bool rc = false;

    // first thing: if the esp32 is already connected to a access point: disconnect
    evBits = xEventGroupGetBits(g_wifi_event_group);

    if (evBits & WIFI_STA_CONNECTED)
    {
        ESP_LOGD(TAG, "Disconnect ...");
        xEventGroupClearBits(g_wifi_event_group, WIFI_STA_DISCONNECT);
        // get ip addr for msg after disconnected
        tcpip_adapter_ip_info_t ipInfo;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
        ESP_ERROR_CHECK(esp_wifi_disconnect());

        // wait until wifi disconnects. From experiments, it seems to take about 150ms to disconnect
        xEventGroupWaitBits(g_wifi_event_group, WIFI_STA_DISCONNECT, 0, 0, portMAX_DELAY);
        ESP_LOGD(TAG, "Disconnected " IPSTR, GOOD_IP2STR(ipInfo.ip.addr));
    }

    // set the new config and connect - reset the disconnect bit first as it is later tested
    xEventGroupClearBits(g_wifi_event_group, WIFI_STA_DISCONNECT);
    ESP_LOGI(TAG, "Connect ...");
    ESP_ERROR_CHECK(esp_wifi_connect());
    rc = true;

    // // 2 scenarios here: connection is successful and SYSTEM_EVENT_STA_GOT_IP will be posted
    // // or it's a failure and we get a SYSTEM_EVENT_STA_DISCONNECTED with a reason code.
    // // Note that the reason code is not exploited. For all intent and purposes a failure is a failure.

    // evBits = xEventGroupWaitBits(g_wifi_event_group, WIFI_STA_CONNECTED | WIFI_STA_DISCONNECT, 0, 0, portMAX_DELAY);

    // if (evBits & WIFI_STA_CONNECTED) {
    //     tcpip_adapter_ip_info_t ipInfo;
    //     tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
    //     ESP_LOGD(TAG, "Connected "IPSTR, GOOD_IP2STR(ipInfo.ip.addr));
    //     rc = true;
    // }

    // if (evBits & WIFI_STA_DISCONNECT) {
    //     ESP_LOGD(TAG, "Not/Dis Connected");
    //     rc = false;
    // }

    return rc;
}

void wifiDisconnect(void)
{
    ESP_LOGI(TAG, "wifiDisconnect");

    EventBits_t evBits;

    // get ip addr for msg after disconnected
    tcpip_adapter_ip_info_t ipInfo;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

    //disconnect only if it was connected to begin with!
    evBits = xEventGroupGetBits(g_wifi_event_group);

    if (evBits & WIFI_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Disconnect ...");
        xEventGroupClearBits(g_wifi_event_group, WIFI_STA_DISCONNECT);
        ESP_ERROR_CHECK(esp_wifi_disconnect());

        // wait until wifi disconnects. From experiments, it seems to take about 150ms to disconnect
        xEventGroupWaitBits(g_wifi_event_group, WIFI_STA_DISCONNECT, 0, 0, portMAX_DELAY);
    }
    else
    {
        ESP_LOGI(TAG, "Device is already disconnected");
    }

    xEventGroupClearBits(g_wifi_event_group, WIFI_STA_DISCONNECT);
    ESP_LOGI(TAG, "Disconnected " IPSTR, GOOD_IP2STR(ipInfo.ip.addr));
}

void wifiStartScan(void)
{
    ESP_LOGD(TAG, "wifiStartScan");

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    if (mode & WIFI_MODE_STA)
    {
        wifi_config_t wifi_config = {0};

        if (ESP_OK == esp_wifi_get_config(WIFI_IF_STA, &wifi_config))
        {
            wifi_config.sta.bssid_set = 0; // no need to check MAC address of AP
            wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
            wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }

        // configure and run the scan process in nonblocking mode
        wifi_scan_config_t scan_config = {
            .ssid = 0,
            .bssid = 0,
            .channel = 0,
            .show_hidden = false, // don't show hidden access points
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active =
                {
                    .min = 0,
                    .max = 0}};

        ESP_LOGD(TAG, "Start Scanning ...");
        esp_wifi_disconnect();
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
    }
    else
    {
        ESP_LOGE(TAG, "Cannot start a new scan because not in a station mode");
    }
}

void wifiStopScan(void)
{
    esp_wifi_scan_stop();
    xEventGroupClearBits(g_wifi_event_group, WIFI_SCAN_DONE);
}

// Allocate memory for access point data
// Copy access point data to the ap_record struct
// print the list
// clear WIFI_SCAN_DONE bit

uint16_t wifiScanDone(wifi_ap_record_t **ap_records)
{
    ESP_LOGD(TAG, "wifiScanDone");

    // get the list of APs found in the last scan
    uint16_t ap_num = 0; // amount of access points found.

    esp_wifi_scan_get_ap_num(&ap_num);

    // safe guard against overflow
    if (ap_num > MAX_AP_NUM)
    {
        ap_num = MAX_AP_NUM;
        ESP_LOGD(TAG, "Scan done: limit found APs to %d", ap_num);
    }

    // Allocate memory for access point data
    *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);

    if (*ap_records == NULL)
    {
        ESP_LOGE(TAG, "Out of memory allocating apData\r\n");
    }
    else
    {
        ESP_LOGI(TAG, "Scan done: found %d APs", ap_num);

        // Copy access point data to the ap_record struct
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, *ap_records));

        // print the list
        ESP_LOGD(TAG, "Found %d access points:", ap_num);
        ESP_LOGD(TAG, "");
        ESP_LOGD(TAG, "---------------------------------+---------+------+-------------");
        ESP_LOGD(TAG, "               SSID              | Channel | RSSI |   Auth Mode ");
        ESP_LOGD(TAG, "---------------------------------+---------+------+-------------");

        for (int i = 0; i < ap_num; i++)
        {
            ESP_LOGD(TAG, "%32s | %7d | %4d | %12s",
                     (char *)(*ap_records)[i].ssid,
                     (*ap_records)[i].primary,
                     (*ap_records)[i].rssi,
                     auth2str((*ap_records)[i].authmode));
        }

        ESP_LOGD(TAG, "---------------------------------+---------+------+-------------");
    }

    xEventGroupClearBits(g_wifi_event_group, WIFI_SCAN_DONE);
    return ap_num;
}

static esp_err_t is_configured(bool *configured)
{
    *configured = false;

    /* Get WiFi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (strlen((const char *)wifi_cfg.sta.ssid))
    {
        *configured = true;
        ESP_LOGD(TAG, "Found ssid %s", (const char *)wifi_cfg.sta.ssid);
        ESP_LOGD(TAG, "Found password %s", (const char *)wifi_cfg.sta.password);
    }
    return ESP_OK;
}

esp_err_t wifiIinitialize(char *ap_ssid, char *ap_pwd, bool *configured)
{
    ESP_LOGD(TAG, "wifi initialize ...");
    *configured = 0;

    // initialize the tcp stack
    tcpip_adapter_init();

    // initialize the wifi event handler
    g_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifiEventHandlerCb, NULL));

    // initialize wifi stack
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    bool cfg;
    is_configured(&cfg);
    if (1 == cfg)
    {
        xEventGroupSetBits(g_wifi_event_group, WIFI_INITIALIZED);
        ESP_LOGD(TAG, "wifi_initialized");
        ESP_ERROR_CHECK(esp_wifi_start());
        wifiConnect();
        *configured = 1;
    }
    else
    {
        // softap setting
        wifi_config_t ap_config;
        esp_wifi_get_config(ESP_IF_WIFI_AP, &ap_config);
        ap_config.ap.channel = 6;

        if ((ap_ssid != NULL) && (ap_pwd != NULL))
        {
            ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            memcpy(ap_config.ap.ssid, ap_ssid, strlen(ap_ssid));
            memcpy(ap_config.ap.password, ap_pwd, strlen(ap_pwd));
        }
        else if ((ap_ssid == NULL) && (ap_pwd != NULL))
        {
            ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            memcpy(ap_config.ap.password, ap_pwd, strlen(ap_pwd));
        }
        else if ((ap_ssid != NULL) && (ap_pwd == NULL))
        {
            ap_config.ap.authmode = WIFI_AUTH_OPEN;
            memcpy(ap_config.ap.ssid, ap_ssid, strlen(ap_ssid));
            ap_config.ap.ssid_len = strlen(ap_ssid);
        }

        ESP_LOGI(TAG, "Softap SSID: %s PASSWORD: %s", ap_config.ap.ssid, ap_config.ap.password);
        esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);

        xEventGroupSetBits(g_wifi_event_group, WIFI_INITIALIZED);
        ESP_LOGD(TAG, "wifi_initialized");

        // start as softap mode
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    return ESP_OK;
}

esp_err_t wifiConnected(uint32_t ticks_to_wait)
{
    tcpip_adapter_ip_info_t sta_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &sta_info);
    portTickType ticks_start = xTaskGetTickCount();

    while (sta_info.ip.addr == 0)
    {
        vTaskDelay(500 / portTICK_RATE_MS);
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &sta_info);
        portTickType tick_cur = xTaskGetTickCount();

        if (ticks_to_wait > (tick_cur - ticks_start))
        {
            ticks_to_wait -= (tick_cur - ticks_start);
        }
        else
        {
            return ESP_FAIL;
        }

        ticks_start = tick_cur;
    }

    return ESP_OK;
}

// show stautus for debug purposes

void wifiShowStatus(void)
{
    ESP_LOGD(TAG, "--------------------------------------------------------------------------\r\n");

    int bits = xEventGroupWaitBits(g_wifi_event_group, WIFI_ALL_EVENTS, 0, 0, 0);
    ESP_LOGD(TAG, "Wifi event bits: 0x%02x", bits);

    if (bits & WIFI_INITIALIZED)
    {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        ESP_LOGD(TAG, "Wifi Mode: %s\r\n", mode2str(mode));

        if (mode & WIFI_MODE_AP)
        {
            tcpip_adapter_ip_info_t ap_info;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info);

            ESP_LOGD(TAG, "Access Point TCP adapter configured with ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR "\r\n",
                     IP2STR(&ap_info.ip),
                     IP2STR(&ap_info.netmask),
                     IP2STR(&ap_info.gw));

            wifi_config_t ap_config = {0};

            if (ESP_OK != esp_wifi_get_config(WIFI_IF_AP, &ap_config))
            {
                ESP_LOGD(TAG, "Cannot get configuration for access point");
            }
        }

        if (mode & WIFI_MODE_STA)
        {
            tcpip_adapter_ip_info_t sta_info;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &sta_info);

            ESP_LOGD(TAG, "Station TCP adapter configured with ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR "\r\n",
                     IP2STR(&sta_info.ip),
                     IP2STR(&sta_info.netmask),
                     IP2STR(&sta_info.gw));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Wifi stack not working");
    }

    ESP_LOGD(TAG, "--------------------------------------------------------------------------\r\n");
}

// WPS_TYPE_PBC: Start esp32 and it will enter wps PBC mode. Then push the
//               button of wps on router down. The esp32 will connected to
//               the router.
// WPS_TYPE_PIN: Start esp32, You'll see PIN code which is a eight-digit
//               number showing on COM. Enter the PIN code in router and then
//               the esp32 will connected to router.

/*init wifi as sta and start wps*/
void wifiStartWps(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "*** esp wifi start");

    ESP_LOGI(TAG, "start wps...");

    ESP_ERROR_CHECK(esp_wifi_wps_enable(&wps_config));
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

void wifiSetNewMode(wifi_mode_t mode)
{
    esp_wifi_set_mode(mode);
}

bool wifiGetCurrentMode(wifi_mode_t *mode)
{
    return esp_wifi_get_mode(mode);
}

esp_err_t wifiSetConfig(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_wifi_set_config(interface, conf);
}

esp_err_t wifiGetConfig(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_wifi_get_config(interface, conf);
}

esp_err_t wifiSetChannel(uint8_t primary, wifi_second_chan_t second)
{
    return esp_wifi_set_channel(primary, second);
}
