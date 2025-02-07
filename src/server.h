#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <rfb/rfb.h>
#include <glib.h>
#include "settings.h"
#include "../unicapture/hyperion-webos/unicapture/unicapture.h"

typedef struct {
	unicapture_state_t unicapture;
    bool ui_backend_initialized;
    bool video_backend_initialized;
    capture_backend_t ui_backend;
    capture_backend_t video_backend;

	rfbScreenInfoPtr screen;
	int active_clients;
	settings_t* settings;
	bool running;

	guint timeout_ref;
} server_t;

int server_start(server_t* server, settings_t* settings);
int server_stop(server_t* server);
