#ifndef WIFI_CONF_HPP
#define WIFI_CONF_HPP

// Network credentials
#define WIFI_SSID "CapOf"
#define WIFI_PASSWORD ""

// Target HTTP server (Python server receiving heading data)
// Update SERVER_IP_BYTES to match the machine running server/server.py
#define SERVER_IP_BYTES \
    { 192, 168, 0, 53 }
#define SERVER_PORT 5000
#define SERVER_ENDPOINT "/data"

// HTTP client buffer size
#define HTTP_BUFFER_SIZE 512

// Timeouts (milliseconds)
#define WIFI_CONNECT_TIMEOUT 10000
#define HTTP_SEND_TIMEOUT 5000

// How often to POST data to the server (milliseconds)
#define HTTP_SEND_INTERVAL_MS 500

#endif /* WIFI_CONF_HPP */
