#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define ESP_AP_WIFI_SSID CONFIG_ESP_AP_WIFI_SSID
#define ESP_AP_WIFI_PASS CONFIG_ESP_AP_WIFI_PASSWORD
#define ESP_AP_WIFI_CHANNEL CONFIG_ESP_AP_WIFI_CHANNEL
#define MAX_AP_STA_CONN CONFIG_ESP_AP_MAX_STA_CONN

#define ESP_ST_WIFI_SSID CONFIG_ESP_ST_WIFI_SSID
#define ESP_ST_WIFI_PASS CONFIG_ESP_ST_WIFI_PASSWORD
#define ESP_ST_MAXIMUM_RETRY CONFIG_ESP_ST_MAXIMUM_RETRY

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAGST = "wifi station";
static const char *TAGAP = "wifi softAP";

static int s_retry_num = 0;
static int is_connected = 0;

extern xSemaphoreHandle conexaoWifiSemaphore;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAGAP, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAGAP, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        //esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < ESP_ST_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAGST, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAGST, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAGST, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xSemaphoreGive(conexaoWifiSemaphore);
    }
}

void wifi_start(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_AP_WIFI_SSID,
            .ssid_len = strlen(ESP_AP_WIFI_SSID),
            .channel = ESP_AP_WIFI_CHANNEL,
            .password = ESP_AP_WIFI_PASS,
            .max_connection = MAX_AP_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(ESP_AP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAGAP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_AP_WIFI_SSID, ESP_AP_WIFI_PASS, ESP_AP_WIFI_CHANNEL);
}

void connect_to_station(char *ssid, char *password)
{

    ESP_LOGW(TAGST, "SSID:%s, password:%s",
             ssid, password);

    wifi_config_t sta_config = {};
    strcpy((char *)sta_config.sta.ssid, ssid);
    strcpy((char *)sta_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    //ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAGST, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAGST, "connected to ap SSID:%s password:%s",
                 sta_config.sta.ssid, password);
        is_connected = 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAGST, "Failed to connect to SSID:%s, password:%s",
                 sta_config.sta.ssid, password);
        is_connected = -1;
    }
    else
    {
        ESP_LOGE(TAGST, "UNEXPECTED EVENT");
    }

    vEventGroupDelete(s_wifi_event_group);
}

int sta_is_connected(void)
{
    return is_connected;
}

void disconnect_from_wifi(void)
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
}