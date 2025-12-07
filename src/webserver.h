// Webserver for ESP-NOW radio control configuration
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "settings.h"
#include "common.h"

// Start webserver (must be called from a task context)
void webserver_start(device_settings_t *settings);

// Stop webserver
void webserver_stop(void);

// Check if webserver is running
bool webserver_is_running(void);

#endif // WEBSERVER_H
