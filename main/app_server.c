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
static httpd_uri_t gUriWs = {0};
static char gPayloadBuff[APP_SERVER_WS_PAYLOAD_SIZE + 2] = {0};

/* Our URI handler function to be called during GET /uri request */
static esp_err_t GetUriHandler(httpd_req_t *req)
{
    esp_err_t errCode = ESP_OK;
    /* Send a index.html file as response */
    ssize_t fileLen = INDEX_HTML_END - INDEX_HTML_START;
    errCode = httpd_resp_send(req, INDEX_HTML_START, fileLen);
    return errCode;
}

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char *data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK)
    {
        free(resp_arg);
    }
    return ret;
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t EchoHandler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t wsPkt;

    uint8_t *buf = NULL;
    memset(&wsPkt, 0, sizeof(httpd_ws_frame_t));
    wsPkt.type = HTTPD_WS_TYPE_TEXT;

    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &wsPkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILED TO GET FRAME LENGTH");
        return ret;
    }
    ESP_LOGI(TAG, "FRAME LENGTH = %d ", wsPkt.len);

    if (wsPkt.len > 0 && wsPkt.len < APP_SERVER_WS_PAYLOAD_SIZE)
    {

        // clear payload buffer
        (void)memset(gPayloadBuff, '\0', sizeof(gPayloadBuff));

        // ws_pkt.payload = buf;
        wsPkt.payload = (uint8_t *)gPayloadBuff;

        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &wsPkt, wsPkt.len);

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "FAILED TO PARSE WS FRAME");
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message:\"%s\"", wsPkt.payload);

        // Generate callback
        app_server_SocketDataCB((const char *)wsPkt.payload, wsPkt.len);
    }
    else
    {
        ESP_LOGW(TAG, "PACKET LENGTH TOO LONG");
    }

    return ret;
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

            gUriWs.uri = "/ws";
            gUriWs.method = HTTP_GET;
            gUriWs.handler = EchoHandler;
            gUriWs.user_ctx = NULL;
            gUriWs.is_websocket = true;

            /* Register URI handlers */
            httpd_register_uri_handler(gServer, &gUriGet);
            httpd_register_uri_handler(gServer, &gUriWs);

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

__attribute__((__weak__)) void app_server_SocketDataCB(const char *json, size_t len) {}
