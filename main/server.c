#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <esp_http_server.h>
#include "wifi.h"

static const char *TAG = "SERVER";
const char *loading_page = "<head>"
                           "<meta charset=\"UTF-8\">"
                           "<meta name=\"viewport\" content=\"width=device-width initial-scale=1.0\">"
                           "<meta http-equiv=\"refresh\" content=\"30\">"
                           "<title>Strongberry</title>"
                           "</head>"
                           "<body>"
                           "<h2>Carregando...</h2>"
                           "</body>"
                           "<style>"
                           "body {display: flex;align-items: center;justify-content: center;flex-direction: column;}"
                           "</style>";
const char *error_page = "<head>"
                         "<meta charset=\"UTF-8\">"
                         "<meta name=\"viewport\" content=\"width=device-width initial-scale=1.0\">"
                         "<meta http-equiv=\"refresh\" content=\"10;url=/\">"
                         "<title>Strongberry</title>"
                         "</head>"
                         "<body>"
                         "<h2>Erro de conexão</h2>"
                         "</body>"
                         "<style>"
                         "body {display: flex;align-items: center;justify-content: center;flex-direction: column;}"
                         "</style>";
const char *connected_page = "<head>"
                             "<meta charset=\"UTF-8\">"
                             "<meta name=\"viewport\" content=\"width=device-width initial-scale=1.0\">"
                             "<title>Strongberry</title>"
                             "</head>"
                             "<body>"
                             "<h2>Wifi Conectado</h2>"
                             "<p><a href=\"/disconnect\">Desconectar da rede</a></p>"
                             "</body>"
                             "<style>"
                             "body {display: flex;align-items: center;justify-content: center;flex-direction: column;}"
                             "</style>";
const char *disconnect_page = "<head>"
                              "<meta charset=\"UTF-8\">"
                              "<meta name=\"viewport\" content=\"width=device-width initial-scale=1.0\">"
                              "<meta http-equiv=\"refresh\" content=\"5;url=/\">"
                              "<title>Strongberry</title>"
                              "</head>"
                              "<body>"
                              "<h2>Desconectando....</h2>"
                              "</body>"
                              "<style>"
                              "body {display: flex;align-items: center;justify-content: center;flex-direction: column;}"
                              "</style>";
const char *login_page = "<head>"
                         "<meta charset=\"UTF-8\">"
                         "<meta name=\"viewport\" content=\"width=device-width initial-scale=1.0\">"
                         "<title>Strongberry</title>"
                         "</head>"
                         "<body>"
                         "<h2>Login Wifi</h2>"
                         "<form action=\"/save_data\">"
                         "<label for=\"fssid\">Nome da rede wifi:</label><br>"
                         "<input type=\"text\" id=\"fssid\" name=\"fssid\" value=\"\"><br>"
                         "<label for=\"fpassword\">Senha da rede:</label><br>"
                         "<input type=\"password\" id=\"fpassword\" name=\"fpassword\" value=\"\"><br><br>"
                         "<input type=\"submit\" value=\"Submit\">"
                         "</form>"
                         "</body>"
                         "<style>"
                         "body {display: flex;align-items: center;justify-content: center;flex-direction: column;}"
                         "</style>";
static esp_err_t get_form_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;

    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    httpd_resp_send(req, login_page, HTTPD_RESP_USE_STRLEN);

    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_form_handler,
};

static esp_err_t get_store_data(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    char ssid[32];
    char password[32];

    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);

            if (httpd_query_key_value(buf, "fssid", ssid, sizeof(ssid)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => fssid=%s", ssid);
            }
            if (httpd_query_key_value(buf, "fpassword", password, sizeof(password)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => fpassword=%s", password);
            }
        }

        if (!sta_is_connected())
        {
            connect_to_station(ssid, password);
        }
        free(buf);
    }

    ESP_LOGI(TAG, "%d", sta_is_connected());
    httpd_resp_send(req, sta_is_connected() == 1 ? connected_page : sta_is_connected() == -1 ? error_page
                                                                                             : loading_page,
                    HTTPD_RESP_USE_STRLEN);

    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t store_data = {
    .uri = "/save_data",
    .method = HTTP_GET,
    .handler = get_store_data,
};

static esp_err_t disconnect_wifi(httpd_req_t *req)
{
    disconnect_from_wifi();

    httpd_resp_send(req, disconnect_page, HTTPD_RESP_USE_STRLEN);

    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t disconnect_route = {
    .uri = "/disconnect",
    .method = HTTP_GET,
    .handler = disconnect_wifi,
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &store_data);
        httpd_register_uri_handler(server, &disconnect_route);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, NULL);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
