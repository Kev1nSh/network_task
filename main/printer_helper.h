#ifndef PRINTER_HELPER_H
#define PRINTER_HELPER_H

#include "stdio.h"

static const char *black = "\033[0;30m";
static const char *red = "\033[0;31m";
static const char *green = "\033[0;32m";
static const char *yellow = "\033[0;33m";
static const char *blue = "\033[0;34m";
static const char *magenta = "\033[0;35m";
static const char *cyan = "\033[0;36m";
static const char *white = "\033[0;37m";
static const char *reset = "\033[0m";


#define PRINTFC(color, format, ...) printf("%s" format "%s", color, ##__VA_ARGS__, reset)

#define PRINTFC_SERVER(format, ...) \
    PRINTFC(red, "Server: ");       \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_CLIENT(format, ...) \
    PRINTFC(blue, "Client: ");      \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_MAIN(format, ...)  \
    PRINTFC(yellow, "Main: ");     \
    printf(format, ##__VA_ARGS__); \
    printf("\n")

#define PRINTFC_WIFI_HANDLER(format, ...) \
    PRINTFC(magenta, "WiFi Handler: ");   \
    printf(format, ##__VA_ARGS__);        \
    printf("\n")

#define PRINTFC_MSG(format, ...)    \
    PRINTFC(yellow, "Message: ");   \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_MQTT(format, ...)   \
    PRINTFC(green, "MQTT: ");       \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_CHAT(format, ...)   \
    PRINTFC(cyan, "Chat: ");        \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")    

#define PRINTFC_SERIAL(format, ...) \
    PRINTFC(blue, "Serial: ");      \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_MYNDIGHETEN(format, ...) \
    PRINTFC(magenta, "Myndigheten: ");   \
    printf(format, ##__VA_ARGS__);       \
    printf("\n")

#define PRINTFC_TORGET(format, ...) \
    PRINTFC(blue, "Torget: ");      \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#define PRINTFC_DOWNLINK(format, ...)   \
    PRINTFC(red, "Downlink: ");         \
    printf(format, ##__VA_ARGS__);      \
    printf("\n");

#define PRINTFC_CSR(format, ...)    \
    PRINTFC(yellow, "CSR: ");       \
    printf(format, ##__VA_ARGS__);  \
    printf("\n")

#endif