/*
 * slog.c
 *
 *  Created on: Jun 9, 2018
 *      Author: pchero
 */

#include <syslog.h>
#include <stdbool.h>
#include <jansson.h>


#define PROGNAME "auido_recognizer"
#define DEF_LOGLEVEL 3


bool g_log_initiated = false;


bool slog_init_handler(void)
{
  // set max loglevel
  setlogmask(LOG_UPTO(DEF_LOGLEVEL));

  // set log
  openlog(PROGNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
  printf("Complete init_log.\n");

  return true;
}

bool slog_update_log_level(int level)
{
  setlogmask(LOG_UPTO(level));

  return true;
}
