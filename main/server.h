#include "esp_log.h"
#include <esp_http_server.h>

#ifndef SERVER_H
#define SERVER_H

httpd_handle_t start_webserver(void);
char host[20];

#endif
