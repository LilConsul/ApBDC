/**
 ******************************************************************************
 * @file    web_content.h
 * @brief   Manual refresh magnetometer display with compass
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

/* HTML page with manual refresh button and compass display */
#define HTML_PAGE_TEMPLATE                                                     \
    "<!DOCTYPE html><html><head><title>STM32 Compass</title>"                 \
    "<meta charset=\"UTF-8\">"                                                 \
    "<style>"                                                                  \
    "body{font:20px Arial;margin:40px;background:#222;color:#0f0;"             \
    "text-align:center;}"                                                      \
    "h1{color:#0f0;margin-bottom:10px;}"                                       \
    ".compass-container{margin:30px auto;}"                                    \
    ".compass{width:300px;height:300px;margin:0 auto;}"                        \
    ".heading{font-size:48px;margin:20px;color:#0ff;font-weight:bold;}"        \
    ".coords{font-size:16px;margin:10px;color:#888;}"                          \
    ".btn{background:#0f0;color:#000;border:none;padding:15px 40px;"           \
    "font-size:20px;cursor:pointer;border-radius:5px;margin:20px;}"            \
    ".btn:hover{background:#0ff;}"                                             \
    ".btn:active{background:#0a0;}"                                            \
    "#status{color:#0f0;font-size:14px;margin:10px;}"                          \
    "</style>"                                                                 \
    "</head><body>"                                                            \
    "<h1>STM32 Magnetometer Compass</h1>"                                      \
    "<div id=\"status\">Click button to refresh</div>"                         \
    "<div class=\"heading\"><span id=\"heading\">%s</span>&deg;</div>"         \
    "<div class=\"compass-container\">"                                        \
    "<svg class=\"compass\" viewBox=\"0 0 300 300\">"                          \
    "<circle cx=\"150\" cy=\"150\" r=\"140\" fill=\"#111\" "                   \
    "stroke=\"#0f0\" stroke-width=\"3\"/>"                                     \
    "<text x=\"150\" y=\"30\" text-anchor=\"middle\" fill=\"#f00\" "           \
    "font-size=\"24\" font-weight=\"bold\">N</text>"                           \
    "<text x=\"150\" y=\"280\" text-anchor=\"middle\" fill=\"#0f0\" "          \
    "font-size=\"20\">S</text>"                                                \
    "<text x=\"270\" y=\"155\" text-anchor=\"middle\" fill=\"#0f0\" "          \
    "font-size=\"20\">E</text>"                                                \
    "<text x=\"30\" y=\"155\" text-anchor=\"middle\" fill=\"#0f0\" "           \
    "font-size=\"20\">W</text>"                                                \
    "<g id=\"needle\" transform=\"rotate(%s 150 150)\">"                       \
    "<polygon points=\"150,40 145,150 150,160 155,150\" fill=\"#f00\"/>"       \
    "<polygon points=\"150,160 145,150 150,260 155,150\" fill=\"#fff\"/>"      \
    "<circle cx=\"150\" cy=\"150\" r=\"8\" fill=\"#ff0\"/>"                    \
    "</g>"                                                                     \
    "</svg>"                                                                   \
    "</div>"                                                                   \
    "<div class=\"coords\">X: %s | Y: %s | Z: %s</div>"                       \
    "<div class=\"coords\">Time: %lu ms</div>"                                 \
    "<button class=\"btn\" onclick=\"location.reload()\">Refresh</button>"     \
    "</body></html>"

#ifdef __cplusplus
}
#endif

#endif /* WEB_CONTENT_H */