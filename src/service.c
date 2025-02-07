#include <stdint.h>
#include <pbnjson.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "service.h"
#include "log.h"

#define SERVICE_NAME "org.webosbrew.vncserver.service"

#define AUTOSTART_SYMLINK_DIR "/var/lib/webosbrew/init.d"
#define AUTOSTART_SYMLINK_PATH AUTOSTART_SYMLINK_DIR "/webos-vncserver"
#define AUTOSTART_SCRIPT_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.vncserver.service/autostart.sh"

server_t* server_p;
settings_t* settings_p;
GMainLoop* loop_p;

bool method_start(LSHandle *sh, LSMessage *message, void *data) {
	int ret;
	LSError lserror;
	LSErrorInit(&lserror);

	jvalue_ref jobj = jobject_create();
	if (jis_null(jobj)) {
		j_release(&jobj);
		return false;
	}

	if (server_p->running) {
		jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
		jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Running already"));
	} else {
		if ((ret = server_start(server_p, settings_p)) == 0) {
			INFO("Server started, replying...");
			server_p->running = true;
			jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
			jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Started"));
		} else {
			jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(FALSE));
			jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Startup failed"));
			jobject_set(jobj, j_cstr_to_buffer("errorCode"), jnumber_create_i32(ret));
		}
	}

	LSMessageReply(sh, message, jvalue_tostring_simple(jobj), &lserror);
	j_release(&jobj);

	return true;
}

bool method_stop(LSHandle *sh, LSMessage *message, void *data) {
	LSError lserror;
	LSErrorInit(&lserror);

	jvalue_ref jobj = jobject_create();
	if (jis_null(jobj)) {
		j_release(&jobj);
		return false;
	}

	if (server_p->running) {
		server_stop(server_p);
		server_p->running = false;
		jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
		jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Stopped"));
	} else {
		jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
		jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Already stopped"));
	}

	LSMessageReply(sh, message, jvalue_tostring_simple(jobj), &lserror);
	j_release(&jobj);

	return true;
}

bool method_configure(LSHandle *sh, LSMessage *message, void *data) {
	int ret;
	LSError lserror;
	JSchemaInfo schema;
	jvalue_ref parsed;

	LSErrorInit(&lserror);

	jschema_info_init (&schema, jschema_all(), NULL, NULL);
	parsed = jdom_parse(j_cstr_to_buffer(LSMessageGetPayload(message)), DOMOPT_NOOPT, &schema);

	if (jis_null(parsed)) {
		j_release(&parsed);
		return false;
	}

	bool was_running = server_p->running;

	if (was_running) {
		server_stop(server_p);
	}

	settings_load_json(settings_p, parsed);

	INFO("settings: %dx%d - password: %s", settings_p->width, settings_p->height, settings_p->password);

	jvalue_ref jobj = jobject_create();
	if (jis_null(jobj)) {
		j_release(&jobj);
		return false;
	}

	if (settings_p->autostart) {
		// While we generally are guaranteed to have /var/lib/webosbrew, it's
		// not the case with /var/lib/webosbrew/init.d...
		mkdir(AUTOSTART_SYMLINK_DIR, 0755);

		if (symlink(AUTOSTART_SCRIPT_PATH, AUTOSTART_SYMLINK_PATH) != 0 && errno != EEXIST) {
			WARN("Autostart script creation failed: %s", strerror(errno));
		}
	} else {
		if (unlink(AUTOSTART_SYMLINK_PATH) != 0 && errno != ENOENT) {
			WARN("Autostart script removal failed: %s", strerror(errno));
		}
	}

	if (was_running) {
		if ((ret = server_start(server_p, settings_p)) == 0) {
			INFO("Server started, replying...");
			jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
			jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Restarted"));
		} else {
			jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(FALSE));
			jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Startup failed"));
			jobject_set(jobj, j_cstr_to_buffer("errorCode"), jnumber_create_i32(ret));
		}
	} else {
		jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));
		jobject_set(jobj, j_cstr_to_buffer("message"), jstring_create("Reconfigured"));
	}

	settings_save_file(settings_p, SETTINGS_PERSISTENCE_PATH);

	LSMessageReply(sh, message, jvalue_tostring_simple(jobj), &lserror);
	j_release(&jobj);

	return true;
}

bool method_status(LSHandle *sh, LSMessage *message, void *data) {
	LSError lserror;
	LSErrorInit(&lserror);

	jvalue_ref jobj = jobject_create();
	if (jis_null(jobj)) {
		j_release(&jobj);
		return false;
	}

	jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(getuid() == 0));
	jobject_set(jobj, j_cstr_to_buffer("running"), jboolean_create(server_p->running));
	jobject_set(jobj, j_cstr_to_buffer("activeClients"), jnumber_create_i32(server_p->active_clients));

	jvalue_ref settings_obj = jobject_create();
	settings_save_json(settings_p, settings_obj);
	jobject_set(jobj, j_cstr_to_buffer("settings"), settings_obj);

	LSMessageReply(sh, message, jvalue_tostring_simple(jobj), &lserror);
	j_release(&jobj);

	return true;
}

bool method_quit(LSHandle *sh, LSMessage *message, void *data) {
	LSError lserror;
	LSErrorInit(&lserror);

	jvalue_ref jobj = jobject_create();
	if (jis_null(jobj)) {
		j_release(&jobj);
		return false;
	}

	jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(TRUE));

	LSMessageReply(sh, message, jvalue_tostring_simple(jobj), &lserror);
	j_release(&jobj);

	g_main_loop_quit(loop_p);

	return true;
}

LSMethod service_methods[] = {
	{"start", method_start},
	{"stop", method_stop},
	{"configure", method_configure},
	{"status", method_status},
	{"quit", method_quit},
	{0, 0},
};

int service_init(GMainLoop* loop, server_t* server, settings_t* settings) {
	static LSError lserror;
	static LSHandle *handle = NULL;
	bool ret = FALSE;

	LSErrorInit(&lserror);

	ret = LSRegister(SERVICE_NAME, &handle, &lserror);
	if (ret == FALSE) {
		WARN("Unable to register service: %s", lserror.message);
		LSErrorFree(&lserror);
		return -1;
	}

	LSRegisterCategory(handle, "/", service_methods, NULL, NULL, &lserror);

	LSGmainAttach(handle, loop, &lserror);

	server_p = server;
	settings_p = settings;
	loop_p = loop;

	return 0;
}
