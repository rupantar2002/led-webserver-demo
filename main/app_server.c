#include "app_server.h"
#include <sys/param.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_err.h>

static const char *TAG = "APP_SERVER";

extern const char INDEX_HTML_START[] asm("_binary_index_html_start");
extern const char INDEX_HTML_END[] asm("_binary_index_html_end");

static httpd_handle_t gServer = NULL;
static httpd_uri_t gUriGet = {0};

/* Our URI handler function to be called during GET /uri request */
static esp_err_t GetUriHandler(httpd_req_t *req)
{
    esp_err_t errCode = ESP_OK;
    /* Send a index.html file as response */
    ssize_t fileLen = INDEX_HTML_END - INDEX_HTML_START;
    errCode = httpd_resp_send(req, INDEX_HTML_START, fileLen);
    return errCode;
}

void app_server_Start(void)
{
    if (gServer == NULL)
    {

        /* Generate default configuration */
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        /* Start the httpd server */
        if (httpd_start(&gServer, &config) == ESP_OK)
        {
            /* Set URI parameters */
            gUriGet.uri = "/";
            gUriGet.method = HTTP_GET;
            gUriGet.handler = GetUriHandler;
            gUriGet.user_ctx = NULL;

            /* Register URI handlers */
            httpd_register_uri_handler(gServer, &gUriGet);
            ESP_LOGI(TAG, "SERVER STARTED");
        }
        else
        {
            ESP_LOGE(TAG, "FAILED TO START SERVER");
        }
    }
}

void app_server_Stop(void)
{
    if (gServer)
    {
        (void)httpd_stop(gServer);
    }
}