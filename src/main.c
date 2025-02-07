#include <stdio.h>
#include "server.h"
#include "service.h"
#include "log.h"

GMainLoop* loop;

void int_handler(int dummy) {
	INFO("SIGINT detected, stopping...");
	g_main_loop_quit(loop);
}

int main (int argc, char** argv) {
	int ret;

	log_init();

	if (getenv("VNCSERVER_DEBUG")) {
		INFO("Enabling verbose logging");
		log_set_level(Debug);
	}

	server_t server = {0};
	server.active_clients = 0;
	server.running = false;

    settings_t settings = {0};
	loop = g_main_loop_new(NULL, FALSE);

	settings.framerate = 30;
	settings.width = 1920/2;
	settings.height = 1080/2;
	settings.password = strdup("password");

	INFO("Config load result: %d", settings_load_file(&settings, SETTINGS_PERSISTENCE_PATH));

	INFO("Starting up service...");

	if ((ret = service_init(loop, &server, &settings)) != 0) {
		WARN("service_init() failed: %d", ret);
	}

	if (!getenv("LS_SERVICE_NAMES")) {
		INFO("Running via CLI - starting anyway");
		server_start(&server, &settings);
		signal(SIGINT, int_handler);
	}

	// INFO("Adding timeout...");
	// g_timeout_add(1000 / 30, server_frame_handler, (gpointer) &server);

	INFO("Starting main loop...");

	g_main_loop_run(loop);

	INFO("loop finished!");
    g_main_loop_unref(loop);

	if (server.running) {
		server_stop(&server);
	}

	return 0;
}
