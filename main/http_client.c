#include "http_client.h"

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "server.h"

#define TAG "HTTP"

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

void http_request()
{
    char url[100];
    char postdata[100];
    snprintf(url, 100, "http://%s/vehicle/setIp/%s", CONFIG_VEHICLE_API_ROUTE, CONFIG_VEHICLE_CODE);
    snprintf(postdata, 100, "{\"ipAddress\": \"%s\"}", host);
    ESP_LOGI(TAG, "%s %s", url, postdata);
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    //esp_err_t err = esp_http_client_perform(client);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", CONFIG_VEHICLE_TOKEN);
    //esp_http_client_set_url(client, url);

    esp_http_client_set_post_field(client, postdata, strlen(postdata));
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
