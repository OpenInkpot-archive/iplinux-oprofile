/**
 * @file op_mangle.h
 * Mangling of sample file names
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_MANGLE_H
#define OP_MANGLE_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * op_mangle_filename - mangle a file filename
 * @param image_name  a path name to the image file
 * @param app_name  a path name for the owner image
 * of this image or %NULL if no owner exist. FIXME: code don't reflect this
 * comment
 * @param event_name  the event name to encode
 * @param count  counter count to encode
 * @param unit_mask unit mask to encode
 * @param tgid  thread group to encode or pid_t(-1) to use all as encoding
 * @param tid  the pid to encode or pid_t(-1) to use all as encoding
 * @param cpu  cpu numbr to encode or -1 to use all as encoding
 * @param is_kernel  non-zero if filename belong to a kernel or module image
 *
 * See also PP:3 for the encoding scheme
 *
 * Returns a char* pointer to the mangled string. Caller
 * is responsible for freeing this string.
 *
 */
char * op_mangle_filename(char const * filename, char const * app_name,
			  char const * event_name, int count,
			  unsigned int unit_mask, pid_t tgid, pid_t tid,
			  int cpu, int is_kernel);

#ifdef __cplusplus
}
#endif

#endif /* OP_MANGLE_H */
