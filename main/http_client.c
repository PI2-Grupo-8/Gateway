#include <stdlib.h>

#include "http_client.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "server.h"

#define TAG "HTTP"
char vehicle_id[20];
int to_get_id = 0;

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
        printf("%.*s", evt->data_len, (char *)evt->data);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        ESP_LOGI(TAG, "%*s", evt->data_len, (char *)evt->data);

        if (to_get_id)
        {
            const cJSON *id = NULL;
            cJSON *json = cJSON_Parse((char *)evt->data);

            id = cJSON_GetObjectItemCaseSensitive(json, "_id");
            if (cJSON_IsString(id) && (id->valuestring != NULL))
            {
                strcpy(vehicle_id, id->valuestring);
                ESP_LOGI(TAG, "Vehicle Id: %s \n", vehicle_id);
            }

            cJSON_Delete(json);
            to_get_id = 0;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void send_data(char *data, char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", CONFIG_VEHICLE_TOKEN);

    esp_http_client_set_post_field(client, data, strlen(data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void get_vehicle_id()
{
    to_get_id = 1;
    char url[100];
    char postdata[100];
    snprintf(url, 100, "http://%s/vehicle/setIp/%s", CONFIG_VEHICLE_API_ROUTE, CONFIG_VEHICLE_CODE);
    snprintf(postdata, 100, "{\"ipAddress\": \"%s\"}", host);
    ESP_LOGI(TAG, "%s %s", url, postdata);
    send_data(postdata, url);
}

void send_sensors_data(int amount, char *type)
{
    char url[100];
    char postdata[100];
    snprintf(url, 100, "http://%s/data/%s", CONFIG_SENSORS_API_ROUTE, vehicle_id);
    snprintf(postdata, 100, "{\"type\": \"%s\",\"value\": %d}", type, amount);
    send_data(postdata, url);
}

void send_alert()
{
    char url[100];
    char postdata[100];
    snprintf(url, 100, "http://%s/alert/create", CONFIG_SENSORS_API_ROUTE);
    snprintf(postdata, 100, "{\"vehicle\": \"%s\",\"type\": \"stuck_vehicle\"}", vehicle_id);
    send_data(postdata, url);
}

void send_battery()
{
    int battery_amount = rand() % 100;
    send_sensors_data(battery_amount, "battery_data");
}

void send_fertilizer()
{
    int fertilizer_amount = rand() % 100;
    send_sensors_data(fertilizer_amount, "tank_data");
}

void send_last_distance()
{
    int last_distance = rand() % 100;
    send_sensors_data(last_distance, "distance");
}
