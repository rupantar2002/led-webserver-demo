#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>
#include "app_led.h"
#include "app_wifi.h"
#include "app_server.h"

static const char *TAG = "MAIN";

/**
 * @brief Initialized NVS flash, NETIF instance and default event loop.
 *
 * @return true when success and false in case of faliour.
 */
static bool SystemInit(void);

void app_main(void)
{
    esp_netif_ip_info_t ipInfo = {0};

    app_wifi_credentials_t apCred = {
        .ssid = "MY SSID",
        .password = "12345678",
    };

    ipInfo.ip.addr = ESP_IP4TOADDR(10, 10, 10, 10);
    ipInfo.gw.addr = ESP_IP4TOADDR(10, 10, 10, 1);
    ipInfo.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);

    (void)SystemInit();

    (void)app_wifi_Init();

    (void)app_wifi_SetIP(&ipInfo);

    (void)app_wifi_SetCredentials(WIFI_MODE_AP, &apCred);

    (void)app_wifi_Start(WIFI_MODE_AP);

    (void)app_led_Init();

    while (1)
    {
        (void)app_led_Reset();
        (void)app_led_SetRGB(20, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));

        (void)app_led_Reset();
        (void)app_led_SetRGB(0, 20, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));

        (void)app_led_Reset();
        (void)app_led_SetRGB(0, 0, 20);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static bool SystemInit(void)
{
    esp_err_t errCode = ESP_OK;

    // Initialize NVS
    errCode = nvs_flash_init();

    if (errCode == ESP_ERR_NVS_NO_FREE_PAGES ||
        errCode == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        errCode = nvs_flash_init();
    }

    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS INITIALIZATION FAILED");
        return false;
    }

    ESP_LOGI(TAG, "NVS FLASH INITIALIZED");

    // Init Netif
    errCode = esp_netif_init();

    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "NETIF INITIALIZATION FAILED");
        return false;
    }

    ESP_LOGI(TAG, "NETIF INITIALIZED");

    // Init Event Loop
    errCode = esp_event_loop_create_default();

    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "EVENT LOOP CREATION FAILED");
        return false;
    }

    ESP_LOGI(TAG, "DEFAULT EVENT LOOP CREATED");

    return true;
}

void app_wifi_EventCB(app_wifi_eventData_t const *const eventData)
{
    switch (eventData->event)
    {
    case APP_WIFI_EVENT_AP_STARTED:
        ESP_LOGI(TAG, "APP_WIFI_EVENT_AP_STARTED");
        app_server_Start();
        break;
    case APP_WIFI_EVENT_AP_STOPED:
        ESP_LOGI(TAG, "APP_WIFI_EVENT_AP_STOPED");
        app_server_Stop();
        break;
    default:
        break;
    }
}
