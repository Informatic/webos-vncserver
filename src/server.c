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
#include "capture.h"
#include "uinput.h"

unsigned int screenwidth = 1920;
unsigned int screenheight = 1080;
const unsigned int bpp = 4;

unsigned int nativewidth = 1920;
unsigned int nativeheight = 1080;

#define FBSIZE (screenwidth * screenheight * bpp)

int running = 1;
int activeClients = 0;

int capture_backend_load(capture_backend_t* backend, const char* name) {
	void* handle;

	handle = dlopen(name, RTLD_NOW);

	if (handle == NULL) {
		DBG("Failed to load %s: %s", name, dlerror());
		return -1;
	}

	backend->name = name;
	backend->init = dlsym(handle, "capture_init");
	backend->execute = dlsym(handle, "capture_execute");
	backend->destroy = dlsym(handle, "capture_destroy");

	return 0;
}

int capture_backend_init(capture_backend_t* backend, uint32_t width, uint32_t height) {
	int ret;

	if ((ret = capture_backend_load(backend, "libcapture_halgal.so")) != 0) {
		DBG("%s load failed: %d", "libcapture_halgal", ret);
	} else {
		if ((ret = backend->init(width, height)) != 0) {
			DBG("%s init failed: %d", "libcapture_halgal", ret);
		} else {
			return 0;
		}
	}

	if ((ret = capture_backend_load(backend, "libcapture_gm.so")) != 0) {
		DBG("%s load failed: %d", "libcapture_gm", ret);
	} else {
		if ((ret = backend->init(width, height)) != 0) {
			DBG("%s init failed: %d", "libcapture_gm", ret);
		} else {
			return 0;
		}
	}

	WARN("No eligible capture backends found");

	return -1;
}

void intHandler(int dummy) {
	running = 0;
}

static void server_client_gone(rfbClientPtr cl) {
	server_t* server = (server_t*) cl->screen->screenData;

	INFO("%s [%d]: Client disconnected", cl->host, server->active_clients);

	server->active_clients -= 1;
}

static enum rfbNewClientAction server_client_incoming(rfbClientPtr cl) {
	server_t* server = (server_t*) cl->screen->screenData;
	server->active_clients += 1;

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

	if ((ret = capture_backend_init(&server->capture, settings->width, settings->height)) != 0) {
		ERR("capture_init() failed: %d", ret);
		return -2;
	}

	INFO("Using capture backend: %s", server->capture.name);

	rfbLogEnable(0);

	rfbScreenInfoPtr screen = rfbGetScreen(NULL, NULL, settings->width, settings->height, 8, 3, bpp);
	assert(screen != NULL);

	server->screen = screen;
	screen->screenData = (void*) server;

	screen->newClientHook = server_client_incoming;

	assert(initialize_uinput() >= 0);

	// signal(SIGINT, intHandler);

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

	return 0;
}

int server_update(server_t* server) {
	int ret;
	if (server->active_clients > 0) {
		int fbsize = server->screen->width * server->screen->height * bpp;
		if ((ret = server->capture.execute(server->screen->frameBuffer, fbsize)) != 0) {
			ERR("capture execute failed: %08x", ret);
			return -5;
		}

		rfbMarkRectAsModified(server->screen, 0, 0, server->screen->width, server->screen->height);
	}

	return 0;
}


int server_stop(server_t* server) {
	server->capture.destroy();
	rfbShutdownServer(server->screen, TRUE);
	free(server->screen->frameBuffer);
	rfbScreenCleanup(server->screen);
	shutdown_uinput();

	return 0;
}
