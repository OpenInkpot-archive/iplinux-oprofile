/**
 * @file op_mangle.c
 * Mangling of sample file names
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_mangle.h"

#include <string.h>
#include <stdio.h>

#include "op_libiberty.h"

#include "op_sample_file.h"
#include "op_config.h"

char * op_mangle_filename(char const * image_name, char const * app_name,
			  char const * event_name, int count,
			  unsigned int unit_mask, pid_t tgid, pid_t tid,
			  int cpu, int is_kernel)
{
	char * mangled;
	size_t len;
	int shared_lib = app_name && strcmp(app_name, image_name);

	len = strlen(OP_SAMPLES_CURRENT_DIR) + strlen(image_name) + 1 + strlen(event_name) + 1;
	if (shared_lib) {
		len += strlen(app_name) + 1;
	}

	/* provision for tgid, tid, unit_mask, cpu and three {root}, {dep} or
	 * {kern} marker */
	len += 128;	/* FIXME: too ugly */

	mangled = xmalloc(len);

	strcpy(mangled, OP_SAMPLES_CURRENT_DIR);

	if (is_kernel && !strchr(image_name, '/')) {
		strcat(mangled, "{kern}" "/");
	} else {
		strcat(mangled, "{root}" "/");
	}

	strcat(mangled, image_name);
	strcat(mangled, "/");

	if (shared_lib) {
		if (is_kernel && !strchr(image_name, '/'))
			strcat(mangled, "{kern}" "/");
		else
			strcat(mangled, "{root}" "/");
		strcat(mangled, app_name);
		strcat(mangled, "/");
	}

	strcat(mangled, event_name);
	sprintf(mangled + strlen(mangled), ".%d.%d.", count, unit_mask);

	if (tgid == (pid_t)-1) {
		sprintf(mangled + strlen(mangled), "%s.", "all");
	} else {
		sprintf(mangled + strlen(mangled), "%d.", tgid);
	}

	if (tid == (pid_t)-1) {
		sprintf(mangled + strlen(mangled), "%s.", "all");
	} else {
		sprintf(mangled + strlen(mangled), "%d.", tid);
	}

	if (cpu == -1) {
		sprintf(mangled + strlen(mangled), "%s", "all");
	} else {
		sprintf(mangled + strlen(mangled), "%d", cpu);
	}

	return mangled;
}
