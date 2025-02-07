#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

#include "log.h"
#include "server.h"
#include "settings.h"
#include "unicapture.h"
#include "uinput.h"

unsigned int screenwidth = 1920;
unsigned int screenheight = 1080;
const unsigned int bpp = 4;

unsigned int nativewidth = 1920;
unsigned int nativeheight = 1080;

static int server_frame_handler(void* data, int width, int height, uint8_t* rgb_data) {
	server_t* server = (server_t*) data;

    if (server->active_clients > 0) {
        memcpy(server->screen->frameBuffer, rgb_data, width * height * 4);
        rfbMarkRectAsModified(server->screen, 0, 0, width, height);
    }

    return 0;
}

int server_init_backends(server_t * server, settings_t* settings) {

    cap_backend_config_t config = {0};
    config.resolution_width = settings->width;
    config.resolution_height = settings->height;
    config.fps = settings->framerate;

    const char* const ui_backends[] = { "libgm_backend.so", "libhalgal_backend.so", NULL };
    const char* const video_backends[] = { "libvtcapture_backend.so", "libdile_vt_backend.so", NULL };
    char backend_name[FILENAME_MAX] = { 0 };

    if (!server->ui_backend_initialized) {
        server->unicapture.ui_capture = NULL;
        INFO("Autodetecting UI backend...");
        if (unicapture_try_backends(&config, &server->ui_backend, ui_backends) == 0) {
            server->unicapture.ui_capture = &server->ui_backend;
            server->ui_backend_initialized = true;
        } else {
            ERR("Failed to initialize UI capture backend");
            return -1;
        }
    }

    if (!server->video_backend_initialized) {
        server->unicapture.video_capture = NULL;

        if (!settings->capture_video) {
            INFO("Video capture disabled");
        } else {
            INFO("Autodetecting video backend...");
            if (unicapture_try_backends(&config, &server->video_backend, video_backends) == 0) {
                server->unicapture.video_capture = &server->video_backend;
                server->video_backend_initialized = true;
            } else {
                ERR("Failed to initialize video capture backend");
                return -1;
            }
        }
    }
    return 0;
}

void server_destroy_backends(server_t* server) {
    if (server->ui_backend_initialized && server->ui_backend.cleanup) {
        DBG("Cleaning up UI backend...");
        DBG("Result: %d", server->ui_backend.cleanup(server->ui_backend.state));
        server->ui_backend_initialized = false;
    }

    if (server->video_backend_initialized && server->video_backend.cleanup) {
        DBG("Cleaning up video backend...");
        DBG("Result: %d", server->video_backend.cleanup(server->video_backend.state));
        server->video_backend_initialized = false;
    }
}

static void server_client_gone(rfbClientPtr cl) {
	server_t* server = (server_t*) cl->screen->screenData;

	INFO("%s [%d]: Client disconnected", cl->host, server->active_clients);

	server->active_clients -= 1;
    if (server->active_clients == 0) {
        unicapture_stop(&server->unicapture);
    }
}

static enum rfbNewClientAction server_client_incoming(rfbClientPtr cl) {
	server_t* server = (server_t*) cl->screen->screenData;
	server->active_clients += 1;
    unicapture_start(&server->unicapture);

	cl->clientGoneHook = &server_client_gone;

	INFO("%s [%d]: New client connected", cl->host, server->active_clients);

	return RFB_CLIENT_ACCEPT;
}

static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	uinput_key_command(down, key);
}

static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl) {
	// fprintf(stderr, "%03d x %03d: %08x\n", x, y, buttonMask);
	ptr_abs(x * 1920 / cl->screen->width, y * 1080 / cl->screen->height, buttonMask);
}

int server_start(server_t* server, settings_t* settings) {
	int ret;

    unicapture_init(&server->unicapture);
    server->unicapture.vsync = true;
    server->unicapture.fps = settings->framerate;
    server->unicapture.target_format = PIXFMT_ARGB;
    server->unicapture.callback = server_frame_handler;
    server->unicapture.callback_data = server;

	if ((ret = server_init_backends(server, settings)) != 0) {
		ERR("capture_init() failed: %d", ret);
		return -2;
	}

	INFO("Using capture backend: ui=%s, video=%s", server->ui_backend.name, server->video_backend.name);

	rfbLogEnable(0);

	rfbScreenInfoPtr screen = rfbGetScreen(NULL, NULL, settings->width, settings->height, 8, 3, bpp);

	if (screen == NULL) {
		ERR("rfbGetScreen() initialization failed");
		return -3;
	}

	server->active_clients = 0;
	server->settings = settings;
	server->screen = screen;
	screen->screenData = (void*) server;

	screen->newClientHook = server_client_incoming;

	if ((ret = initialize_uinput()) != 0) {
		ERR("uinput initialization failed: %d", ret);
		return -4;
	}

	// switch red and blue channels
	int tmp = screen->serverFormat.redShift;
	screen->serverFormat.redShift = screen->serverFormat.blueShift;
	screen->serverFormat.blueShift = tmp;

	screen->kbdAddEvent = keyevent;
	screen->ptrAddEvent = ptrevent;

	int fbsize = screen->width * screen->height * bpp;
	screen->frameBuffer = malloc (fbsize);

	if (settings->password && strlen(settings->password)) {
		char** passwords = calloc(2, sizeof(char*));
		passwords[0] = settings->password;
		screen->authPasswdData = (void*)passwords;
		screen->passwordCheck = rfbCheckPasswordByList;
	}

	rfbInitServer(screen);

	// Run event loop in background thread
	rfbRunEventLoop(screen, -1, TRUE);

	INFO("VNC server running on %d", screen->port);

	server->running = true;

	return 0;
}

int server_stop(server_t* server) {
	INFO("Shutting down...");
	g_source_remove(server->timeout_ref);
	server->running = false;
    unicapture_stop(&server->unicapture);
    server_destroy_backends(server);
	rfbShutdownServer(server->screen, TRUE);
	free(server->screen->frameBuffer);
	rfbScreenCleanup(server->screen);
	shutdown_uinput();

	return 0;
}
