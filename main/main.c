#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include "app_led.h"


void app_main(void)
{
    app_led_Init();

    while (1)
    {
        app_led_Reset();
        app_led_SetRGB(255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));

        app_led_Reset();
        app_led_SetRGB(0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));

        app_led_Reset();
        app_led_SetRGB(0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

}