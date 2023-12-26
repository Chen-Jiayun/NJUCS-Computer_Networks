#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

// #define LOG_DEBUG

enum log_level { DEBUG = 0, INFO, WARNING, ERROR };

static enum log_level this_log_level = DEBUG;

static const char *log_level_str[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

#ifdef LOG_DEBUG
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "[%s:%u] %s: " fmt  "\n", __FILE__, __LINE__, \
				level_str, ##__VA_ARGS__);
#else
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "%s: " fmt "\n", level_str, ##__VA_ARGS__);
#endif

#define log(level, fmt, ...) \
	do { \
		if (level < this_log_level) \
			break; \
		log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

#define print_mac(macAddress)     fprintf(stderr, " %02x:%02x:%02x:%02x:%02x:%02x\n", \
           macAddress[0], macAddress[1], macAddress[2],\
           macAddress[3], macAddress[4], macAddress[5])


 #define ESC "\033["
 #define RED ESC "01;31m"
 #define GREEN ESC "01;32m"
 #define YELLOW ESC "01;33m"
 #define BLUE ESC "01;34m"
 #define MAGENTA ESC "01;35m"
 #define CYAN ESC "01;36m"
 #define WHITE ESC "01;37m"
 #define CLR ESC "0m"

#endif
