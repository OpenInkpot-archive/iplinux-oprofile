/* $Id$ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/* for hton */ 
#include <netinet/in.h> 
#include <stdlib.h> 
 
#include "opd_util.h"

static int remove_component_p(const char *);

/**
 * opd_malloc - allocate memory
 * @size: size in bytes
 *
 * Allocate memory of size @size bytes. Failure
 * to malloc() is fatal.
 *
 * The memory may be freed with standard C library
 * free().
 *
 * Returns a void pointer to the memory allocated.
 */ 
void *opd_malloc(size_t size)
{
	void *p;

#ifdef OPD_DEBUG 
	if (size == 0) {
		fprintf(stderr,"oprofiled:opd_malloc: 0 bytes requested\n");
		abort();
	}
#endif

	p = malloc(size);

	if (!p) {
		fprintf(stderr,"oprofiled:opd_malloc: alloc %d bytes failed.\n",size);
		exit(1);
	}	

	return p; 
} 
 
/**
 * opd_malloc0 - allocate and clear memory
 * @size: size in bytes
 *
 * Allocate memory of size @size bytes. Failure
 * to malloc() is fatal. The buffer is filled
 * with zeroes.
 *
 * The memory may be freed with standard C library
 * free().
 *
 * Returns a void pointer to the memory allocated.
 */ 
void *opd_malloc0(size_t size)
{
	void *p;

	p = opd_malloc(size);
	memset(p, 0, size); 
	return p; 
}
 
/**
 * opd_free - free previously allocated memory
 * @p: pointer to memory
 *
 * Free previously allocated memory. @p may be
 * %NULL, in which case nothing happens.
 */ 
void opd_free(void *p)
{
	if (p)
		free(p);
}
 
/**
 * opd_realloc - resize allocated memory
 * @buf: current memory
 * @size: size in bytes of new request
 *
 * Re-allocates a buffer with a different
 * size. Requesting re-allocation of the
 * same size is a fatal error.
 *
 * @buf may be %NULL. Failure to allocate is a 
 * fatal error.
 * 
 * Returns a void pointer to the new memory
 * allocated. 
 */ 
void *opd_realloc(void *buf, size_t size)
{
	void *p;

#ifdef OPD_DEBUG 
	if (!size) {
		fprintf(stderr,"oprofiled:opd_realloc: 0 extra bytes requested\n");
		abort(); 
	}

#endif 
 
	if (!buf)
		return opd_malloc(size);
 
	p = realloc(buf, size);

	if (!p) {
		fprintf(stderr,"oprofiled:opd_realloc: realloc %d bytes failed.\n", size);
		exit(1);
	} 

	return p; 
}

/**
 * opd_strdup - duplicate a string through opd_malloc
 * @str: string pointer
 *
 * Allocate memory of size strlen(@str) + 1 bytes then copy the @str
 * to new allocated storage. Failure to malloc() is fatal.
 *
 * The memory may be freed with standard C library
 * free().
 *
 * Returns a char* pointer to the duplicated string.
 */
char *opd_strdup(const char *str) 
{
	size_t sz = strlen(str) + 1;

	char *temp = (char *)opd_malloc(sz);
	memcpy(temp, str, sz);

	return temp;
}

/**
 * opd_mangle_filename - mangle a file filename
 * @smpdir: base directory name
 * @image_name: a path name to the image file
 * @extra_size: an extra size to allocate
 *
 * allocate memory of size strlen(@image_name) + 
 * strlen(@smpdir) + 2 + 32 then concat
 * @smpdir and the mangled name of @filename
 * the 32 bytes added are assumed to concat
 * something like "-%d"
 *
 * Returns a char* pointer to the mangled string.
 *
 */
char* opd_mangle_filename(const char *smpdir, const char* image_name)
{
	char *mangled;
	char *c;
	const char *c2;

	mangled = (char *)opd_malloc(strlen(smpdir) + 2 + strlen(image_name) + 32);
	strcpy(mangled, smpdir);
	strcat(mangled, "/");
	c = mangled + strlen(smpdir) + 1;
	c2 = image_name;
	do {
		if (*c2 == '/')
			*c++ = OPD_MANGLE_CHAR;
		else
			*c++ = *c2;
	} while (*++c2);
	*c = '\0';

	return mangled;
}


/* remove_component_p() and op_simlify_pathname() comes from the gcc
 preprocessor */

/**
 * remove_component_p - check if it is safe to remove the final component
 * of a path.
 * @path: string pointer
 *
 * Returns 1 if it is safe to remove the component
 * 0 otherwise
 */ 
static int remove_component_p (const char *path)
{
	struct stat s;
	int result;

	result = lstat (path, &s);

	/* There's no guarantee that errno will be unchanged, even on
	   success.  Cygwin's lstat(), for example, will often set errno to
	   ENOSYS.  In case of success, reset errno to zero.  */
	if (result == 0)
		errno = 0;

	return result == 0 && S_ISDIR (s.st_mode);
}

/**
 * opd_simplify_path_name - simplify a path name in place
 * @path: string pointer to the path.
 *
 *  Simplify a path name in place, deleting redundant components.  This
 *  reduces OS overhead and guarantees that equivalent paths compare
 *  the same (modulo symlinks).
 *
 *  Transforms made:
 *  foo/bar/../quux	foo/quux
 *  foo/./bar		foo/bar
 *  foo//bar		foo/bar
 *  /../quux		/quux
 *  //quux		//quux  (POSIX allows leading // as a namespace escape)
 *
 *  Guarantees no trailing slashes.  All transforms reduce the length
 *  of the string.  Returns @path.  errno is 0 if no error occurred;
 *  nonzero if an error occurred when using stat().
 */
char *opd_simplify_pathname (char *path)
{
	char *from, *to;
	char *base, *orig_base;
	int absolute = 0;

	errno = 0;
	/* Don't overflow the empty path by putting a '.' in it below.  */
	if (*path == '\0')
		return path;

	from = to = path;
    
	/* Remove redundant leading /s.  */
	if (*from == '/') {
		absolute = 1;
		to++;
		from++;
		if (*from == '/') {
			if (*++from == '/')
				/* 3 or more initial /s are equivalent to 1 /.  */
				while (*++from == '/');
			else
				/* On some hosts // differs from /; Posix allows this.  */
				to++;
		}
	}

	base = orig_base = to;
	for (;;) {
		int move_base = 0;

		while (*from == '/')
			from++;

		if (*from == '\0')
			break;

		if (*from == '.') {
			if (from[1] == '\0')
				break;
			if (from[1] == '/') {
				from += 2;
				continue;
			}
			else if (from[1] == '.' && (from[2] == '/' || from[2] == '\0')) {
				/* Don't simplify if there was no previous component.  */
				if (absolute && orig_base == to) {
					from += 2;
					continue;
				}
				/* Don't simplify if the previous component 
				 * was "../", or if an error has already
				 * occurred with (l)stat.  */
				if (base != to && errno == 0) {
					/* We don't back up if it's a symlink.  */
					*to = '\0';
					if (remove_component_p (path)) {
						while (to > base && *to != '/')
							to--;
						from += 2;
						continue;
					}
				}
				move_base = 1;
			}
		}

		/* Add the component separator.  */
		if (to > orig_base)
			*to++ = '/';

		/* Copy this component until the trailing null or '/'.  */
		while (*from != '\0' && *from != '/')
			*to++ = *from++;

		if (move_base)
			base = to;
	}
    
	/* Change the empty string to "." so that it is not treated as stdin.
	   Null terminate.  */
	if (to == path)
		*to++ = '.';
	*to = '\0';

	return path;
}

/**
 * opd_relative_to_absolute_path - translate relative path to absolute path.
 * @path: path name
 * @base_dir: optionnal base directory, if %NULL getcwd() is used
 * to get the base directory.
 *
 * prepend @base_dir or the result of getcwd if the path is not absolute.
 * The returned string is dynamic allocated, caller must free it. if 
 * base_dir == NULL this function use getcwd to translate the path.
 *
 * Returns the translated path.
 */
char *opd_relative_to_absolute_path(const char *path, const char *base_dir) {

	char dir[PATH_MAX + 1];
	char *temp_path = NULL;

	if (path && path[0] != '/') {
		if (base_dir == NULL) {
			if (getcwd(dir, PATH_MAX) != NULL) {
				base_dir = dir;
			}

		}

		if (base_dir != NULL) {
			temp_path = (char *)opd_malloc(strlen(path) + strlen(base_dir) + 2);
			strcpy(temp_path, base_dir);
			strcat(temp_path, "/");
			strcat(temp_path, path);
		}
	}

	/* absolute path or (base_dir == NULL && getcwd have lose) : 
         * always return a value */
	if (temp_path == NULL) {
		temp_path = opd_strdup(path);
	}

	return opd_simplify_pathname(temp_path);
}

 
/**
 * opd_do_open_file - open a file
 * @name: file name
 * @mode: mode string
 * @fatal: is failure fatal or not
 *
 * Open a file name.
 * Returns file handle or %NULL on failure.
 */
FILE *opd_do_open_file(const char *name, const char *mode, int fatal)
{
	FILE *fp;

	fp = fopen(name, mode);

	if (!fp) {
		if (fatal) { 
			fprintf(stderr,"oprofiled:opd_do_open_file: %s: %s", name, strerror(errno)); 
			exit(1);
		} 
	}

	return fp;	
} 

/**
 * opd_close_file - close a file
 * @fp: file pointer
 *
 * Closes a file pointer. A non-fatal
 * error message is produced if the
 * close fails.
 */ 
void opd_close_file(FILE *fp)
{
	if (fclose(fp)) {
		perror("oprofiled:opd_close_file: ");
	}
}

/**
 * opd_read_link - read contents of symbolic link
 * @name: file name of symbolic link
 *
 * Read a symbolic link and return a pointer to
 * the file name pointed to by the link. %NULL
 * is returned on failure.
 *
 * The link contents are stored in a static buffer
 * so should not be freed (or used in a thread context).
 */ 
char *opd_read_link(const char *name)
{
	static char linkbuf[FILENAME_MAX]="";
	int c;

	c = readlink(name, linkbuf, FILENAME_MAX);

	if (c == -1)
		return NULL; 

	if (c == FILENAME_MAX)
		linkbuf[FILENAME_MAX-1] = '\0';
	else 
		linkbuf[c] = '\0'; 

	return opd_strdup(linkbuf);
} 
 
/**
 * opd_do_read_file - read a file
 * @fp: file pointer
 * @buf: buffer
 * @size: size in bytes to read
 * @fatal: is failure fatal or not
 *
 * Read from a file. It is considered an error 
 * if anything less than @size bytes is read.
 */
void opd_do_read_file(FILE *fp, void *buf, size_t size, int fatal)
{
	size_t count;

	count = fread(buf, size, 1, fp);

	if (fatal && count != 1) {
		if (feof(fp))
			fprintf(stderr,"oprofiled:opd_read_file: read less than expected %d bytes\n", size);
		else
			fprintf(stderr,"oprofiled:opd_read_file: error reading\n");
		exit(1);
	}
}
 
/**
 * opd_do_read_u8 - read a byte from a file
 * @fp: file pointer
 *
 * Read an unsigned byte from a file.
 * 0 is returned if the read fails in any way.
 */ 
u8 opd_read_u8(FILE *fp)
{
	u8 val = 0;
	opd_do_read_file(fp, &val, sizeof(u8), 0);

	return val; 
}
 
/**
 * opd_do_read_u16_he - read two bytes from a file
 * @fp: file pointer
 *
 * Read an unsigned two-byte value from a file.
 * 0 is returned if the read fails in any way.
 *
 * No byte-swapping is done. 
 */ 
u16 opd_read_u16_he(FILE *fp)
{
	u16 val = 0;
	opd_do_read_file(fp, &val, sizeof(u16), 0);
	return val;
}
 
/**
 * opd_do_read_u32_he - read four bytes from a file
 * @fp: file pointer
 *
 * Read an unsigned four-byte value from a file.
 * 0 is returned if the read fails in any way.
 *
 * No byte-swapping is done. 
 */ 
u32 opd_read_u32_he(FILE *fp)
{
	u32 val = 0;
	opd_do_read_file(fp, &val, sizeof(u32), 0);
 
	return val;
}
 
/**
 * opd_write_file - write to a file
 * @fp: file pointer
 * @buf: buffer
 * @size: nr. of bytes to write
 *
 * Write @size bytes of buffer @buf to a file.
 * Failure is fatal.
 */ 
void opd_write_file(FILE *fp, const void *buf, size_t size)
{
	size_t written;

	written = fwrite(buf, size, 1, fp);

	if (written != 1) {
		fprintf(stderr,"oprofiled:opd_write_file: wrote less than expected: %d bytes.\n", size);
		exit(1);
	}
}
 
/**
 * opd_write_u8 - write a byte to a file
 * @fp: file pointer
 * @val: value to write
 *
 * Write the unsigned byte value @val to a file.
 * Failure is fatal.
 */ 
void opd_write_u8(FILE *fp, u8 val)
{
	opd_write_file(fp, &val, sizeof(val));
}
 
/**
 * opd_write_u16_he - write two bytes to a file
 * @fp: file pointer
 * @val: value to write
 *
 * Write an unsigned two-byte value @val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done. 
 */
void opd_write_u16_he(FILE *fp, u16 val)
{
	opd_write_file(fp, &val, sizeof(val));
}
 
/**
 * opd_write_u23_he - write four bytes to a file
 * @fp: file pointer
 * @val: value to write
 *
 * Write an unsigned four-byte value @val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done. 
 */ 
void opd_write_u32_he(FILE *fp, u32 val)
{
	opd_write_file(fp, &val, sizeof(val));
}
 
/**
 * opd_read_int_from_file - parse an ASCII value from a file into an integer
 * @filename: name of file to parse integer value from
 *
 * Reads an ASCII integer from the given file. All errors are fatal.
 * The value read in is returned.
 */
u32 opd_read_int_from_file(const char *filename) 
{

	FILE *fp;
	u32 value;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "opd_read_int_from_file: Failed to open %s, reason %s\n", filename, strerror(errno));
		exit(1);
	}

	if (fscanf(fp, "%u", &value) != 1) {
		fclose(fp);
		fprintf(stderr, "opd_read_int_from_file: Failed to convert contents of file %s to integer\n", filename);
		exit(1);
	}

	fclose(fp);

	return value;
}

/**
 * opd_get_fsize - get size of file
 * @file: file name
 * @fatal: exit on error
 *
 * Returns the size of the named file in bytes.
 * Failure is fatal if @fatal is %TRUE.
 */ 
off_t opd_get_fsize(const char *file, int fatal)
{
	struct stat st;

	if (stat(file, &st)) {
		if (!fatal) 
			return 0;
		 
		fprintf(stderr,"opd_get_fsize: stat failed\n");
		exit(1);
	}

	/* PHE FIXME caller can not make any difference between failure and
	 * zero file size when fatal != 0 */
	return st.st_size;
}

/**
 * opd_get_mtime - get mtime of file
 * @file: file name
 *
 * Returns the mtime of the given file or 0 on failure
 */
time_t opd_get_mtime(const char * file)
{
	struct stat st;

	if (stat(file, &st))
		return 0;

	return st.st_mtime;
}


/**
 * opd_get_time - get current date and time
 *
 * Returns a string representing the current date
 * and time, or %NULL on error.
 *
 * The string is statically allocated and should not be freed.
 */ 
char *opd_get_time(void)
{
	time_t t = time(NULL);

	if (t == -1)
		return NULL;

	return ctime(&t);
}
 
/**
 * opd_get_line - read an ASCII line from a file
 * @fp: file pointer 
 *
 * Get a line of ASCII text from a file. The file is read
 * up to the first \0 or \n. A trailing \n is deleted.
 * 
 * Empty lines are not handled.
 *
 * Returns the dynamically-allocated string containing
 * that line. At the end of a file a string "" will
 * be returned.
 *
 * In *both* cases, the string must be free()d at some
 * point.
 */ 
char *opd_get_line(FILE *fp)
{
	char *buf;
	char *cp;
	int c;
	/* average allocation is about 31, so 64 should be good */
	size_t max = 64;

	buf = (char *)opd_malloc(max);
	cp = buf; 

	do {
		switch (c = fgetc(fp)) { 
			case EOF:
			case '\n':
			case '\0':
				*cp = '\0';
				return buf;
				break;

			default:
				*cp = (char)c;
				cp++;
				if (((size_t)(cp - buf)) == max) {
					buf = (char *)opd_realloc(buf, max + 64);
					cp = buf+max;
					max += 64;
				}
				break;
		}
	} while (1);
}

/**
 * opd_open_device - open a special char device for reading
 * @name: file name of device file
 * @fatal: fatal or not
 *
 * Open the special file @name. Returns the file descriptor
 * for the file or -1 on error.
 */ 
fd_t opd_open_device(const char *name, int fatal)
{
	fd_t fd;
 
	fd = open(name, O_RDONLY);
	if (fatal && fd == -1) {
		fprintf(stderr,"oprofiled:opd_open_device: %s: %s\n", name, strerror(errno)); 
		exit(1);
	}

	return fd;
}

/**
 * opd_close_device - close a special char device
 * @devfd: file descriptor of device
 *
 * Close a special file. Failure is fatal.
 */ 
void opd_close_device(fd_t devfd)
{
	if (close(devfd)) {
		perror("oprofiled:opd_close_device: ");
		exit(1);
	}	
} 
 
/**
 * opd_read_device - read from a special char device
 * @devfd: file descriptor of device
 * @buf: buffer
 * @size: size of buffer
 * @seek: seek to the start or not 
 *
 * Read @size bytes from a device into buffer @buf.
 * A seek to the start of the device file is done first
 * if @seek is non-zero, then a read is requested in one 
 * go of @size bytes.
 *
 * The driver returning %EINTR is handled to allow signals.
 * Any other error return is fatal.
 *
 * It is the caller's responsibility to do further opd_read_device()
 * calls if the number of bytes read is not what is requested
 * (where this is applicable).
 *
 * The number of bytes read is returned.
 */ 
size_t opd_read_device(fd_t devfd, void *buf, size_t size, int seek)
{
	ssize_t count;
 
	do {
		if (seek) 
			lseek(devfd,0,SEEK_SET);
 
		count = read(devfd, buf, size);

		if (count<0 && errno != EINTR) {
			perror("oprofiled:opd_read_device: ");
			exit(1);
		}
 
	} while (count < 0);
	return count;
}

 
/**
 * opd_poptGetContext - wrapper for popt
 *
 * Use this instead of poptGetContext to cope with
 * different popt versions. 
 */
poptContext opd_poptGetContext(const char * name,
		int argc, const char ** argv,
		const struct poptOption * options, int flags)
{
#ifdef CONST_POPT
	return poptGetContext(name, argc, argv, options, flags); 
#else
	return poptGetContext((char *)name, argc, (char **)argv, options, flags); 
#endif
}
