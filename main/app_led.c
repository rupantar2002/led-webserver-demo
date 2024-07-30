#include "app_led.h"
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>

#define APP_LED_RESOLUTION_HZ 10000000U // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

#define APP_LED_GPIO_NUM GPIO_NUM_18

#define EXAMPLE_LED_NUMBERS 1

static const char *TAG = "APP_LED";

typedef struct
{
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

typedef struct
{
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

typedef struct
{
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    led_strip_encoder_config_t encodeCfg;
    rmt_transmit_config_t transmitCfg;
    uint8_t pixels[3];
} app_led_ctx_t;

/**
 * @brief Led application contex.
 */
static app_led_ctx_t gLedCtx = {0};

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *ledEncoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ledEncoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = ledEncoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (ledEncoder->state)
    {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            ledEncoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ledEncoder->reset_code,
                                                sizeof(ledEncoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            ledEncoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *ledEncoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(ledEncoder->bytes_encoder);
    rmt_del_encoder(ledEncoder->copy_encoder);
    free(ledEncoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *ledEncoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(ledEncoder->bytes_encoder);
    rmt_encoder_reset(ledEncoder->copy_encoder);
    ledEncoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *ledEncoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ledEncoder = rmt_alloc_encoder_mem(sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(ledEncoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    ledEncoder->base.encode = rmt_encode_led_strip;
    ledEncoder->base.del = rmt_del_led_strip_encoder;
    ledEncoder->base.reset = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // SK68XX transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &ledEncoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &ledEncoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
    ledEncoder->reset_code = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &ledEncoder->base;
    return ESP_OK;
err:
    if (ledEncoder)
    {
        if (ledEncoder->bytes_encoder)
        {
            rmt_del_encoder(ledEncoder->bytes_encoder);
        }
        if (ledEncoder->copy_encoder)
        {
            rmt_del_encoder(ledEncoder->copy_encoder);
        }
        free(ledEncoder);
    }
    return ret;
}

app_led_status_t app_led_Init(void)
{
    esp_err_t errCode = ESP_OK;

    rmt_tx_channel_config_t txChannelCfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = APP_LED_GPIO_NUM,
        .mem_block_symbols = 64U, // increase the block size can make the LED less flickering
        .resolution_hz = APP_LED_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };

    errCode = rmt_new_tx_channel(&txChannelCfg, &gLedCtx.channel);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "TX CHANNEL CREATION FAILED");
        return APP_LED_STATUS_ERR;
    }

    gLedCtx.encodeCfg.resolution = APP_LED_RESOLUTION_HZ;
    errCode = rmt_new_led_strip_encoder(&gLedCtx.encodeCfg, &gLedCtx.encoder);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "ENCODER CREATION FAILED");
        return APP_LED_STATUS_ERR;
    }

    errCode = rmt_enable(gLedCtx.channel);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILED TO ENABLE");
        return APP_LED_STATUS_ERR;
    }

    return APP_LED_STATUS_OK;
}

app_led_status_t app_led_SetRGB(uint8_t r, uint8_t g, uint8_t b)
{
    esp_err_t errCode = ESP_OK;

    rmt_transmit_config_t transmitCfg = {0};
    transmitCfg.loop_count = 0; // no transfer loop

    gLedCtx.pixels[0] = g;
    gLedCtx.pixels[1] = r;
    gLedCtx.pixels[2] = b;

    errCode = rmt_transmit(gLedCtx.channel, gLedCtx.encoder, gLedCtx.pixels, sizeof(gLedCtx.pixels), &transmitCfg);
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "FALED TO REST LED");
        return APP_LED_STATUS_ERR;
    }
    errCode = rmt_tx_wait_all_done(gLedCtx.channel, pdMS_TO_TICKS(100U));
    if (errCode != ESP_OK)
    {
        ESP_LOGE(TAG, "TRANSMISSION TIMEOUT");
        return APP_LED_STATUS_ERR;
    }

    return APP_LED_STATUS_OK;
}

app_led_status_t app_led_Reset(void)
{
    return app_led_SetRGB(0U, 0U, 0U);
}

void app_led_DeInit(void)
{
    if (gLedCtx.encoder)
    {
        rmt_del_encoder(gLedCtx.encoder);
    }

    if (gLedCtx.channel)
    {
        rmt_del_channel(gLedCtx.channel);
    }
}