/**
 ******************************************************************************
 * @file    http_client.h
 * @brief   HTTP POST request template for sending heading data to server
 * @author  ApBDC Team
 ******************************************************************************
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wifi/wifi_conf.hpp"

/**
 * HTTP POST request template.
 * Arguments: host_ip (string), content_length (int), degree_value (string)
 */
#define HTTP_POST_REQUEST_FMT                                                  \
    "POST " SERVER_ENDPOINT " HTTP/1.1\r\n"                                    \
    "Host: %s\r\n"                                                             \
    "Content-Type: application/x-www-form-urlencoded\r\n"                     \
    "Content-Length: %d\r\n"                                                   \
    "Connection: close\r\n"                                                    \
    "\r\n"                                                                     \
    "degree=%s"

#ifdef __cplusplus
}
#endif

#endif /* HTTP_CLIENT_H */
