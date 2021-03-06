/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**
**
**  Licensed Materials - Property of IBM
**
**  IBM Linear Tape File System Single Drive Edition Version 2.2.0.2 for Linux and Mac OS X
**
**  Copyright IBM Corp. 2010, 2014
**
**  This file is part of the IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X
**  (formally known as IBM Linear Tape File System)
**
**  The IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X is free software;
**  you can redistribute it and/or modify it under the terms of the GNU Lesser
**  General Public License as published by the Free Software Foundation,
**  version 2.1 of the License.
**
**  The IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X is distributed in the
**  hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
**  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**  See the GNU Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
**  or download the license from <http://www.gnu.org/licenses/>.
**
**
**  ZZ_Copyright_END
**
*************************************************************************************
**
** COMPONENT NAME:  IBM Linear Tape File System
**
** FILE NAME:       arch/filename_handling.c
**
** DESCRIPTION:     Implements platform-specific filename handling functions.
**
** AUTHOR:          Takashi Ashida
**                  IBM Yamato, Japan
**                  ashida@jp.ibm.com
**
*************************************************************************************
**
** Copyright (C) 2012 OSR Open Systems Resources, Inc.
** 
************************************************************************************* 
*/

#include "libltfs/arch/filename_handling.h"
#include "libltfs/fs.h"
#include "libltfs/pathname.h"

#ifdef HP_mingw_BUILD
#include "arch/win/win_util.h"
#endif

#if defined(mingw_PLATFORM)
bool _replace_invalid_chars(char * file_name, bool * dosdev);
char * _generate_target_file_name(const char *prefix, const char *extension, int suffix, bool dosdev);
int _utf8_strlen(const char *s);
int _utf8_strncpy(char *t, const char *s, int n);
#endif

/**
 *  Update platfrom_safe_name member in dentry
 * @param dentry dentry to update
 * @param handle_invalid_char Replace invalid chars in the name
 * if TRUE. otherwise the name is skipped withour updating
 * platform_safe_name field.
 */
void update_platform_safe_name(struct dentry* dentry, bool handle_invalid_char, struct ltfs_index *idx)
{
#if defined(mingw_PLATFORM)
	bool dosdev = false;
	int suffix = 0;
	char source_file_name[LTFS_FILENAME_MAX*4+1];
	char *source_file_name_prefix, *source_file_name_extension;
	char *target_file_name;
	struct dentry *d;
	int ret;

	dentry->platform_safe_name = NULL;
	strcpy(source_file_name, dentry->name);

	if (_replace_invalid_chars(source_file_name, &dosdev)) {
		if (! handle_invalid_char)
			return;
		suffix++;
	}

	/* Split source file name to prefix and extension */
	if (! dosdev) {
		source_file_name_prefix = source_file_name;
		source_file_name_extension = strrchr(source_file_name, '.');

		/* If '.' is at the beginning of file name, then all of file name is
		   recognized as prefix, not extension. */
		if (source_file_name_extension == source_file_name_prefix)
			source_file_name_extension = NULL;
	} else {
		source_file_name_prefix = source_file_name;
		source_file_name_extension = strchr(source_file_name, '.');
	}

	if (source_file_name_extension) {
		*source_file_name_extension = 0x00;
		source_file_name_extension++;
	}

	while (1) {
		target_file_name = _generate_target_file_name(source_file_name_prefix, source_file_name_extension, suffix, dosdev);

		if (! target_file_name)
			break;
		else {
			if (dentry->parent) {
				ret = fs_directory_lookup(dentry->parent, target_file_name, &d);
				if (ret < 0) {
					free(target_file_name);
					break;
				}
			}
			if (! dentry->parent || ! d ) {
				dentry->platform_safe_name = target_file_name;
				break;
			} else {
				if (d) {
					d->numhandles--;
				}
				suffix++;
				free(target_file_name);
			}
		}
	}
#else
	dentry->platform_safe_name = strdup(dentry->name);
#endif
}

/**
 *  Perform platform dependent name matching
 * @param name1 A file name to be matched.
 * @param name2 A file name to be matched.
 * @param result Outputs matching result
 */
int ltfs_compare_names(const char *name1, const char *name2, int *result)
{
#if defined(mingw_PLATFORM)
	return pathname_caseless_match(name1, name2, result);
#else
	*result = strcmp(name1, name2);
	return 0;
#endif
}

#if defined(mingw_PLATFORM)
/**
 *  Replace invalid chars for a file name with '_'. Returns TRUE
 *  if the file name is changed in this function or the file
 *  name includes reserved dos device name.
 * @param file_name file_name to be checked
 */
bool _replace_invalid_chars(char * file_name, bool * dosdev)
{
	bool to_be_changed = false;
	static char invalid_chars[] = "\\:*?\"<>|" ;
	static char *dosdev_list[] = {	"CON", "PRN", "AUX", "CLOCK$", "NUL", "COM0",
									"COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
									"COM7", "COM8", "COM9", "LPT0", "LPT1", "LPT2",
									"LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8",
									"LPT9", NULL };
	int i;

	*dosdev = false;
	for (i = 0; dosdev_list[i]; i++) {
		if ( strcasestr(file_name, dosdev_list[i]) == file_name &&
			 (file_name[strlen(dosdev_list[i])] == '\0' ||file_name[strlen(dosdev_list[i])] == '.') ) {
			*dosdev = true;
			to_be_changed = true;
		}
	}

	for (i = 0; ; i++) {
		if (file_name[i]) {
            /* OSR
             *
             * Add parantheses to avoid compiler stylistic warning
             */
#ifdef HP_mingw_BUILD
            if ((file_name[i] >= 0x01 && file_name[i] <= 0x1F) ||
                strchr(invalid_chars, file_name[i])) {
#else
            if (file_name[i] >= 0x01 && file_name[i] <= 0x1F ||
                strchr(invalid_chars, file_name[i])) {
#endif
				file_name[i] = '_';
				to_be_changed = true;
			}
		}
		else
			break;
	}

	return to_be_changed;
}

/**
 *  Generate target file name candidate from source file prefix,
 *  extension, and suffix.
 * @param file_name file_name to be checked
 */
char * _generate_target_file_name(const char *prefix, const char *extension, int suffix, bool dosdev)
{
	char *target;
	char suffix_string[LTFS_FILENAME_MAX*4+1];
	char trimmed_name[LTFS_FILENAME_MAX*4+1];
	int prefix_length, extension_length, suffix_length, ret;

	ret = -1;
	target = NULL;

	if (suffix) {
		sprintf( suffix_string, "~%d", suffix );

		prefix_length    = prefix    ? _utf8_strlen(prefix)    : 0;
		extension_length = extension ? _utf8_strlen(extension) : 0;
		suffix_length    = _utf8_strlen(suffix_string);

		if (prefix_length + extension_length + suffix_length > LTFS_FILENAME_MAX) {
			/* Need to trim source file name to add suffix */
			if (! dosdev && prefix_length > suffix_length) {
				/* Prefix is to be trimmed. */
				_utf8_strncpy(trimmed_name, prefix, prefix_length - suffix_length);
				if (extension)
					ret = asprintf(&target, "%s%s.%s", trimmed_name, suffix_string, extension);
				else
					ret = asprintf(&target, "%s%s", trimmed_name, suffix_string);

			} else if (extension_length > suffix_length) {
				/* Extension is to be trimmed. */
				_utf8_strncpy(trimmed_name, extension, extension_length - suffix_length);
				ret = asprintf(&target, "%s%s.%s", prefix, suffix_string, trimmed_name);
			} else {
				/* Unable to generate target file name. NULL is to be returned. */
			}
		} else {
			if (extension)
				ret = asprintf(&target, "%s%s.%s", prefix, suffix_string, extension);
			else
				ret = asprintf(&target, "%s%s", prefix, suffix_string);
		}
	} else {
		if (extension)
			ret = asprintf(&target, "%s.%s", prefix, extension);
		else {
			target = strdup(prefix);
			ret = target ? (int) strlen(target) : -1;
		}
	}

	return ret > 0 ? target : NULL;
}

/**
 *  Return the length of specified UTF-8 string.
 * @param s string to be counted.
 */
int _utf8_strlen(const char *s)
{
	int ret = 0;

	CHECK_ARG_NULL(s, -LTFS_NULL_ARG);

	while (*s) {
		if (! (*s & 0x80) || (*s & 0xC0) == 0xC0)
			++ret;
		++s;
	}
	return ret;
}

/**
 *  Copy UTF-8 string.
 * @param t Target buffer to store the string.
 * @param s Source string to be copied.
 * @param n Maximum length to be copied.
 */
int _utf8_strncpy(char *t, const char *s, int n)
{
	int ret = 0;

	CHECK_ARG_NULL(t, -LTFS_NULL_ARG);
	CHECK_ARG_NULL(s, -LTFS_NULL_ARG);

	while (*s) {
		if (! (*s & 0x80) || (*s & 0xC0) == 0xC0) {
			++ret;
			if (ret > n) {
				*t = 0x00;
				break;
			}
		}
		*t = *s;
		++t;
		++s;
	}
	return ret;
}

#endif
