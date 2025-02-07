#include <stdio.h>
#include "settings.h"
#include "log.h"

int settings_load_json(settings_t* settings_p, jvalue_ref parsed) {
	jvalue_ref value;
	if ((value = jobject_get(parsed, j_cstr_to_buffer("autostart"))) && jis_boolean(value)) jboolean_get(value, &settings_p->autostart);
	if ((value = jobject_get(parsed, j_cstr_to_buffer("width"))) && jis_number(value)) jnumber_get_i32(value, &settings_p->width);
	if ((value = jobject_get(parsed, j_cstr_to_buffer("height"))) && jis_number(value)) jnumber_get_i32(value, &settings_p->height);
	if ((value = jobject_get(parsed, j_cstr_to_buffer("framerate"))) && jis_number(value)) jnumber_get_i32(value, &settings_p->framerate);
	if ((value = jobject_get(parsed, j_cstr_to_buffer("captureVideo"))) && jis_boolean(value)) jboolean_get(value, &settings_p->capture_video);
	if ((value = jobject_get(parsed, j_cstr_to_buffer("password"))) && jis_string(value)) {
		free(settings_p->password);
        settings_p->password = (char *) jstring_get(value).m_str;
	}

	return 0;
}

int settings_save_json(settings_t* settings_p, jvalue_ref target) {
	jobject_set(target, j_cstr_to_buffer("autostart"), jboolean_create(settings_p->autostart));
	jobject_set(target, j_cstr_to_buffer("width"), jnumber_create_i32(settings_p->width));
	jobject_set(target, j_cstr_to_buffer("height"), jnumber_create_i32(settings_p->height));
	jobject_set(target, j_cstr_to_buffer("framerate"), jnumber_create_i32(settings_p->framerate));
	jobject_set(target, j_cstr_to_buffer("captureVideo"), jboolean_create(settings_p->capture_video));
	jobject_set(target, j_cstr_to_buffer("password"), jstring_create(settings_p->password));
	return 0;
}

int settings_load_file(settings_t* settings, char* source) {
	int ret = 0;

	JSchemaInfo schema;
	jvalue_ref parsed;

	FILE *f = fopen(source, "rb");
	// File read failed
	if (f == NULL) {
		return -1;
	}

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *json = malloc(fsize + 1);
	fread(json, fsize, 1, f);
	fclose(f);
	json[fsize] = 0;

	DBG("config [ %s ]", json);

	jschema_info_init (&schema, jschema_all(), NULL, NULL);
	parsed = jdom_parse(j_cstr_to_buffer(json), DOMOPT_NOOPT, &schema);

	// Parsing failed
	if (jis_null(parsed)) {
		free(json);
		j_release(&parsed);
		return -2;
	}

	ret = settings_load_json(settings, parsed);

	free(json);
	j_release(&parsed);
	return ret;
}

int settings_save_file(settings_t* settings, char* target) {
	jvalue_ref jobj = jobject_create();
	settings_save_json(settings, jobj);

	FILE* fd = fopen(target, "wb");
	if (fd == NULL) {
		j_release(&jobj);
		return 1;
	}

	fputs(jvalue_tostring_simple(jobj), fd);
	fclose(fd);

	j_release(&jobj);
	return 0;
}
