#ifndef __APP_SERVER_H__
#define __APP_SERVER_H__
#include <stdint.h>
#include <stddef.h>

#define APP_SERVER_WS_PAYLOAD_SIZE 256U

enum
{
    APP_SERVER_EVENT_NULL = 0,
    APP_SERVER_EVENT_DATA,
    APP_SERVER_EVENT_MAX,
};

typedef uint16_t app_server_event_t;

void app_server_Start(void);

void app_server_Stop(void);

void app_server_SocketDataCB(const char *json, size_t len);

#endif //__APP_SERVER_H__