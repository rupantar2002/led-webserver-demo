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

enum
{
    PACKET_TYPE_NULL = 0,
    PACKET_TYPE_CTRL,
    PACKET_TYPE_COLOR,
    PACKET_TYPE_MAX,
};

typedef struct
{
    uint8_t type;
    union
    {
        struct
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        } color;

        struct
        {
            bool state;
        } ctrl;

    } data;

} packet_t;

static bool gLedState = false;
static char gRespBuff[APP_SERVER_WS_PAYLOAD_SIZE + 2] = {0};
static packet_t gPacket = {0};
static TaskHandle_t gAppTask = NULL;

/**
 * @brief Initialized NVS flash, NETIF instance and default event loop.
 *
 * @return true when success and false in case of faliour.
 */
static bool SystemInit(void);

/**
 * @brief  Convert a hex character to its decimal value
 *
 * @param c input charecter
 * @return int decimal value
 */
int HexCharToDec(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    else if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    else
    {
        return -1; // Invalid character for hex
    }
}

/**
 * @brief Convert a hex string to an 8-bit integer
 *
 * @param hex string representing hex value.
 * @return uint8_t 8bit  integer value.
 */
uint8_t HexToInt(const char *hex)
{
    return (HexCharToDec(hex[0]) << 4) + HexCharToDec(hex[1]);
}

/**
 * @brief Parse JSON packets and handle them based on type
 *
 * @param json_str null terminated json string.
 */

bool ParseJson(const char *json_str, packet_t *const pkt)
{
    if (!json_str && !pkt)
    {
        return false;
    }

    char type[10];
    char state[10];
    char hex_color[8];

    // Check for "type" to determine packet structure
    if (sscanf(json_str, "{\"type\":\"%[^\"]\"", type) == 1)
    {

        if (strcmp(type, "color") == 0)
        {

            // Parse color packet
            if (sscanf(json_str, "{\"type\":\"color\",\"hex\":\"#%[^\"]\"}", hex_color) == 1)
            {

                pkt->type = PACKET_TYPE_COLOR;
                pkt->data.color.r = HexToInt(hex_color);     // First two characters
                pkt->data.color.g = HexToInt(hex_color + 2); // Second two characters
                pkt->data.color.b = HexToInt(hex_color + 4); // Third two characters

                // Print the results
                ESP_LOGI(TAG, "Packet Type : %s", type);
                ESP_LOGI(TAG, "R: %d, G: %d, B: %d", pkt->data.color.r,
                         pkt->data.color.g,
                         pkt->data.color.b);
            }
            else
            {
                ESP_LOGE(TAG, "Error parsing color packet.");
                return false;
            }
        }
        else if (strcmp(type, "ctrl") == 0)
        {

            // Parse command packet
            if (sscanf(json_str, "{\"type\":\"ctrl\",\"state\":\"%[^\"]\"}", state) == 1)
            {

                pkt->type = PACKET_TYPE_CTRL;

                if (strncmp(state, "ON", sizeof(state)) == 0)
                {
                    pkt->data.ctrl.state = true;
                }
                else if (strncmp(state, "OFF", sizeof(state)))
                {
                    pkt->data.ctrl.state = false;
                }

                // Print the results
                ESP_LOGI(TAG, "Packet Type : %s", type);
                ESP_LOGI(TAG, "State: %s\n", state);
            }
            else
            {
                ESP_LOGE(TAG, "Error parsing control packet");
                return false;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Unknown packet type");
            return false;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error reading packet type");
        return false;
    }
    return true;
}

void app_main(void)
{
    esp_netif_ip_info_t ipInfo = {0};

    app_wifi_credentials_t apCred = {
        .ssid = "MY SSID",
        .password = "12345678",
    };

    /* Retrive current task handle for notification purpose */
    gAppTask = xTaskGetCurrentTaskHandle();

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
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        (void)memset(&gPacket, '\0', sizeof(gPacket));

        if (ParseJson(gRespBuff, &gPacket))
        {
            switch (gPacket.type)
            {
            case PACKET_TYPE_CTRL:
                if (gPacket.data.ctrl.state)
                {
                    gLedState = true;
                }
                else
                {
                    gLedState = false;
                    (void)app_led_Reset();
                }

                break;
            case PACKET_TYPE_COLOR:
                if (gLedState)
                {
                    (void)app_led_SetRGB(gPacket.data.color.r,
                                         gPacket.data.color.g,
                                         gPacket.data.color.b);
                }
                break;
            default:
                break;
            }
        }
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

void app_server_SocketDataCB(const char *json, size_t len)
{
    (void)strncpy(gRespBuff, json, sizeof(gRespBuff));

    if (gAppTask)
    {
        // Notify application task about socket data.
        (void)xTaskNotify(gAppTask, 0, eNoAction);
    }
}
