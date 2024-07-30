#ifndef __APP_WIFI_H__
#define __APP_WIFI_H__
#include <stdint.h>
#include <esp_wifi.h>
#include <esp_netif.h>

#define APP_WIFI_SSID_LEN 32U

#define APP_WIFI_PASSWORD_LEN 64U

#define APP_WIFI_DEFAULT_SSID "ESP32 WIFI"

enum
{
    APP_WIFI_STATUS_OK = 0,
    APP_WIFI_STATUS_ERR,
    APP_WIFI_STATUS_MAX,
};

typedef uint8_t app_wifi_staus_t;

enum
{
    APP_WIFI_EVENT_NULL = 0,
    APP_WIFI_EVENT_AP_STARTED,
    APP_WIFI_EVENT_AP_STOPED,
    APP_WIFI_EVENT_AP_STA_CONNECTED,
    APP_WIFI_EVENT_AP_STA_DISCONNECTED,
    APP_WIFI_EVENT_AP_STA_IP_ASSIGNED,
    APP_WIFI_EVENT_MAX,
};

typedef uint16_t app_wifi_event_t;

typedef struct
{
    app_wifi_event_t event;

} app_wifi_eventData_t;

typedef struct
{
    char ssid[APP_WIFI_SSID_LEN + 2];
    char password[APP_WIFI_PASSWORD_LEN + 2];
} app_wifi_credentials_t;

app_wifi_staus_t app_wifi_Init(void);

app_wifi_staus_t app_wifi_SetCredentials(wifi_mode_t mode, app_wifi_credentials_t *const pWifiCred);

app_wifi_staus_t app_wifi_SetIP(esp_netif_ip_info_t *pIpinfo);

app_wifi_staus_t app_wifi_Start(wifi_mode_t mode);

app_wifi_staus_t app_wifi_Stop(void);

void app_wifi_DeInit(void);

void app_wifi_EventCB(app_wifi_eventData_t const *const eventData);

#endif //__APP_WIFI_H__