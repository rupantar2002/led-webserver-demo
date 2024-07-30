#ifndef __APPP_LED_H__
#define __APPP_LED_H__

#include <stdint.h>

enum
{
    APP_LED_STATUS_OK = 0,
    APP_LED_STATUS_ERR,
    APP_LED_STATUS_MAX,
};

typedef uint8_t app_led_status_t;

app_led_status_t app_led_Init(void);

app_led_status_t app_led_SetRGB(uint8_t r, uint8_t g, uint8_t b);

app_led_status_t app_led_Reset(void);

void app_led_DeInit(void);

#endif //__APPP_LED_H__