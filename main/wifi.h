#ifndef WIFI_H
#define WIFI_H

void wifi_start();
void connect_to_station(char *ssid, char *password);
int sta_is_connected(void);
void disconnect_from_wifi(void);
void send_alert(void);

#endif
