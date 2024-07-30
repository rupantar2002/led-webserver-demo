#include "app_wifi.h"
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>


static const char *TAG = "APP_WIFI";

static wifi_config_t gWifiCfg = {0};

static esp_netif_t *pgNetifAP = NULL;

static char gWifiSsidBuff[APP_WIFI_SSID_LEN + 2] = {0};

static char gWifiPasswordBuff[APP_WIFI_PASSWORD_LEN + 2] = {0};

static app_wifi_eventData_t gEventData = {0};

/**
 * @brief Configure wifi AP.
 *
 * @return app_wifi_staus_t
 */
static app_wifi_staus_t ConfigureWifiAP(void)
{
    esp_err_t errCode = ESP_OK;

    if (pgNetifAP == NULL)
    {
        pgNetifAP = esp_netif_create_default_wifi_ap();
        if (pgNetifAP == NULL)
        {
            ESP_LOGE(TAG, "NETIF AP CREATION FAILED");
            return APP_WIFI_STATUS_ERR;
        }
    }

    // Coinfigure AP
    (void)memset(&gWifiCfg, '\0', sizeof(gWifiCfg)); // clear old configuration

    // Copy ssid
    if (strlen(gWifiSsidBuff) == 0) // NO ssid given using default one
    {
        ESP_LOGW(TAG, "NO SSID FOUND USING DEFAULT SSID");
        (void)strncpy((char *)gWifiCfg.ap.ssid, APP_WIFI_DEFAULT_SSID, sizeof(gWifiCfg.ap.ssid));
    }
    else
    {
        (void)strncpy((char *)gWifiCfg.ap.ssid, gWifiSsidBuff, sizeof(gWifiCfg.ap.ssid));
    }

    // Copy password
    if (strlen(gWifiPasswordBuff) == 0)
    {
        ESP_LOGW(TAG, "NO PASSWORD FOUND USING OPEN NETWORK");
        gWifiCfg.ap.authmode = WIFI_AUTH_OPEN; // make open network
    }
    else
    {
        (void)strncpy((char *)gWifiCfg.ap.password, gWifiPasswordBuff, sizeof(gWifiCfg.ap.password));
        gWifiCfg.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK; // set authentication mode
    }

    gWifiCfg.ap.ssid_len = 0;             // set to null terminated string
    gWifiCfg.ap.channel = 0;              // set channel to 0
    gWifiCfg.ap.max_connection = 1U;      // allows only 1 connection at a time
    gWifiCfg.ap.pmf_cfg.required = false; // No PMF

    errCode = esp_wifi_set_config(WIFI_IF_AP, &gWifiCfg);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI AP CONFIGURATION FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    return APP_WIFI_STATUS_OK;
}

static void WifiEventHandler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_WIFI_READY)
    {
        ESP_LOGI(TAG, "WIFI EVENT => WIFI_EVENT_WIFI_READY ");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "WIFI EVENT => WIFI_EVENT_AP_START ");

        gEventData.event = APP_WIFI_EVENT_AP_STARTED;
        app_wifi_EventCB(&gEventData);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP)
    {
        ESP_LOGI(TAG, "WIFI EVENT => WIFI_EVENT_AP_STOP ");

        gEventData.event = APP_WIFI_EVENT_AP_STOPED;
        app_wifi_EventCB(&gEventData);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        ESP_LOGI(TAG, "WIFI EVENT => WIFI_EVENT_AP_STACONNECTED ");

        gEventData.event = APP_WIFI_EVENT_AP_STA_CONNECTED;
        app_wifi_EventCB(&gEventData);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "WIFI EVENT => WIFI_EVENT_AP_STADISCONNECTED ");

        gEventData.event = APP_WIFI_EVENT_AP_STA_DISCONNECTED;
        app_wifi_EventCB(&gEventData);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
        ESP_LOGI(TAG, "WIFI EVENT => IP_EVENT_AP_STAIPASSIGNED ");

        gEventData.event = APP_WIFI_EVENT_AP_STA_IP_ASSIGNED;
        app_wifi_EventCB(&gEventData);
    }
    else
    {
        // do nothing
    }
}

app_wifi_staus_t app_wifi_Init(void)
{
    esp_err_t errCode = ESP_OK;
    wifi_init_config_t wifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();

    // Init wifi
    errCode = esp_wifi_init(&wifiInitCfg);

    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI INITIALIZATION FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    // Register Event handler
    errCode = esp_event_handler_register(WIFI_EVENT,
                                         ESP_EVENT_ANY_ID,
                                         &WifiEventHandler,
                                         NULL);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI EVENT REGISTRATION FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    errCode = esp_event_handler_register(IP_EVENT,
                                         ESP_EVENT_ANY_ID,
                                         &WifiEventHandler,
                                         NULL);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "IP EVENT REGISTRATION FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    return APP_WIFI_STATUS_OK;
}

app_wifi_staus_t app_wifi_SetCredentials(wifi_mode_t mode, app_wifi_credentials_t *const pWifiCred)
{
    if (mode <= WIFI_MODE_NULL && mode >= WIFI_MODE_MAX && pWifiCred == NULL)
    {
        ESP_LOGE(TAG, "INVALID INPUT");
        return APP_WIFI_STATUS_ERR;
    }

    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP)
    {
        (void)strncpy((char *)gWifiSsidBuff, pWifiCred->ssid, sizeof(gWifiSsidBuff));
        (void)strncpy((char *)gWifiPasswordBuff, pWifiCred->password, sizeof(gWifiPasswordBuff));
    }

    return APP_WIFI_STATUS_OK;
}

app_wifi_staus_t app_wifi_SetIP(esp_netif_ip_info_t *pIpinfo)
{
    if (pIpinfo == NULL)
    {
        ESP_LOGE(TAG, "INVALID INPUT");
        return APP_WIFI_STATUS_ERR;
    }

    if (pgNetifAP)
    {
        if (esp_netif_set_ip_info(pgNetifAP, pIpinfo) != ESP_OK)
        {
            return APP_WIFI_STATUS_ERR;
        }
    }

    return APP_WIFI_STATUS_OK;
}

app_wifi_staus_t app_wifi_Start(wifi_mode_t mode)
{
    esp_err_t errCode = ESP_OK;

    if (mode <= WIFI_MODE_NULL && mode >= WIFI_MODE_MAX)
    {
        ESP_LOGE(TAG, "INVALID MODE SELLECTION");
        return APP_WIFI_STATUS_ERR;
    }

    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP)
    {
        errCode = ConfigureWifiAP();
    }

    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI AP CONFIGURATIOn FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    errCode = esp_wifi_set_mode(mode);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI MODE SETUP FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    errCode = esp_wifi_start();
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "WIFI STARTUP FAILED");
        return APP_WIFI_STATUS_ERR;
    }

    return APP_WIFI_STATUS_OK;
}

app_wifi_staus_t app_wifi_Stop(void)
{
    esp_err_t errCode = ESP_OK;

    errCode = esp_wifi_stop();
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILED TO STOP WIFI");
        return APP_WIFI_STATUS_ERR;
    }

    return APP_WIFI_STATUS_OK;
}

void app_wifi_DeInit(void)
{
    esp_netif_destroy(pgNetifAP);
    (void)esp_wifi_deinit();
}

__attribute__((__weak__)) void app_wifi_EventCB(app_wifi_eventData_t const *const eventData) {}
