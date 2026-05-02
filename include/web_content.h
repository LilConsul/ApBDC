/**
 ******************************************************************************
 * @file    web_content.h
 * @brief   SSE-based magnetometer streaming display
 * @author  ApBDC Team
 ******************************************************************************
 */

#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP Response Headers */
#define HTTP_HTML_HEADER                                                       \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Content-Type: text/html\r\n"                                              \
    "Connection: close\r\n"                                                    \
    "\r\n"

#define HTTP_JSON_HEADER                                                       \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Content-Type: application/json\r\n"                                       \
    "Cache-Control: no-cache\r\n"                                              \
    "Connection: close\r\n"                                                    \
    "\r\n"

/* HTML page with meta-refresh (reloads entire page every 200ms with new data)
 */
#define HTML_PAGE_TEMPLATE                                                     \
    "<!DOCTYPE html><html><head><title>STM32 Mag</title>"                      \
    "<meta charset=\"UTF-8\">"                                                 \
    "<meta http-equiv=\"refresh\" content=\"0.2\">"                            \
    "<style>body{font:20px "                                                   \
    "Arial;margin:40px;background:#222;color:#0f0;text-align:center;}"         \
    "h1{color:#0f0;}.v{font-size:40px;margin:20px;}#status{color:#0f0;font-"   \
    "size:14px;}</style>"                                                      \
    "</head><body>"                                                            \
    "<h1>STM32 Magnetometer Stream</h1>"                                       \
    "<div id=\"status\">Live Data (Auto-refresh 5Hz)</div>"                    \
    "<div class=\"v\">X: <span id=\"x\">%s</span></div>"                       \
    "<div class=\"v\">Y: <span id=\"y\">%s</span></div>"                       \
    "<div class=\"v\">Z: <span id=\"z\">%s</span></div>"                       \
    "<p id=\"time\">%lu ms</p>"                                                \
    "</body></html>"

#ifdef __cplusplus
}
#endif

#endif /* WEB_CONTENT_H */
