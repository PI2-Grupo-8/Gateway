#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <esp_http_server.h>

#include "wifi.h"
#include "server.h"
#include "http_client.h"

xSemaphoreHandle conexaoWifiSemaphore;

void RealizaHTTPRequest(void * params)
{
  while(true)
  {
    if(xSemaphoreTake(conexaoWifiSemaphore, portMAX_DELAY))
    {
      ESP_LOGI("Main Task", "Realiza HTTP Request");
      http_request();
    }
  }
}


void app_main(void)
{
    //Initialize NVS
    static httpd_handle_t server = NULL;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    conexaoWifiSemaphore = xSemaphoreCreateBinary();
    wifi_start();
    /* Start the server for the first time */
    server = start_webserver();

    xTaskCreate(&RealizaHTTPRequest,  "Processa HTTP", 4096, NULL, 1, NULL);
}
