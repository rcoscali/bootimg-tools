/* bootimg-tools/bootimg-utils.c
 *
 * Copyright 2007, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#include <stdlib.h>
# endif
# ifdef HAVE_STDDEF_H
#include <stddef.h>
# endif
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <getopt.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <libgen.h>

#include "bootimg.h"
#include "bootimg-priv.h"

/*
 * Retrieve long option name from short name
 */
const char *
getLongOptionName(struct option *opt, char option)
{
  do { if (opt->val == option) return opt->name; opt++; } while (opt->name);
  extern const char *unknown_option;
  return unknown_option;
}

/*
 * Return an image file name
 */
const char *
getImageFilename(const char *basenm, const char *outdir, int kind)
{
  /* Verbosity flag (from command line options) */
  char *bname = strdup(basenm);
  extern int vflag;
  extern char *progname;
  char *pathname;
  int basenameIsAbsolute = 0;

  /* Allocate & Zero memory */
  pathname = alloca(PATH_MAX+1);
  bzero((void *)pathname, PATH_MAX+1);  
  /* First handle extension */
  switch (kind)
    {
      /* strip extension for boot image files */
    case BOOTIMG_BOOTIMG_FILENAME:
      *(strchrnul(bname, '.')) = 0;
      break;
    default:
      /* else change extension .<ext> in _<ext> for all other images */
      if (strrchr(bname, '.'))
        *(strrchr(bname, '.')) = '_';
    }
  if (bname[0] == '/')
    basenameIsAbsolute = 1;
  
  /* Then create file pathname according to its type */
  switch (kind)
    {
    case BOOTIMG_BOOTIMG_FILENAME:
      sprintf(pathname,
              basenameIsAbsolute ? "%s.img" : "%s/%s.img",
              basenameIsAbsolute ? bname : outdir,
              bname);
      if (vflag > 2)
        fprintf(stdout, "%s: Boot Image filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_XML_FILENAME:
      sprintf(pathname, "%s.xml", bname);
      if (vflag > 2)
        fprintf(stdout, "%s: XML Metadata filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_JSON_FILENAME:
      sprintf(pathname, "%s.json", bname);
      if (vflag > 2)
        fprintf(stdout, "%s: JSON Metadata filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_KERNEL_FILENAME:
      sprintf(pathname,
              basenameIsAbsolute ? "%s.img" : "%s/%s.zImage",
              basenameIsAbsolute ? bname : outdir,
              bname);
      if (vflag > 2)
        fprintf(stdout, "%s: KERNEL filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_RAMDISK_FILENAME:
      sprintf(pathname,
              basenameIsAbsolute ? "%s.cpio.gz" : "%s/%s.cpio.gz",
              basenameIsAbsolute ? bname : outdir,
              bname);
      if (vflag > 2)
        fprintf(stdout, "%s: RAMDISK filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_SECOND_LOADER_FILENAME:
      sprintf(pathname,
              basenameIsAbsolute ? "%s-2ndldr.img" : "%s/%s-2ndldr.img",
              basenameIsAbsolute ? bname : outdir,
              bname);
      if (vflag > 2)
        fprintf(stdout, "%s: 2nd BOOTLOADER filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_DTB_FILENAME:
      sprintf(pathname,
              basenameIsAbsolute ? "%s.dtb" : "%s/%s.dtb",
              basenameIsAbsolute ? bname : outdir,
              bname);
      if (vflag > 2)
        fprintf(stdout, "%s: DTB filename = '%s'\n", progname, pathname);
      break;
    default:
      sprintf(pathname, "%s-unknown.dat", bname);
      fprintf(stderr, "%s: error: Unknown filename = '%s'\n", progname, pathname);
      break;
    }
  
  if (vflag == 2)
    fprintf(stdout, "%s: using file with name '%s'\n", progname, pathname);

  return strdup(pathname);
}

/*
 * initBootImgHeader - initialize the image header struct
 */
boot_img_hdr *
initBootImgHeader(boot_img_hdr *hdr)
{
  /* First zero all bytes */
  bzero(hdr, sizeof(boot_img_hdr));

  /* Init MAGIC value to "ANDROID!" */
  memcpy((void *)hdr->magic, (const void *)BOOT_MAGIC, BOOT_MAGIC_SIZE);
  
  return hdr;
}

/*
 *
 */
const char *getDirname(const char *pathname, uint8_t flags)
{
  char buffer[PATH_MAX+1];
  char *dir_name_ptr;

  bzero((void *)buffer, PATH_MAX+1);
  memcpy((void *)buffer, pathname, BOOTIMG_MIN(PATH_MAX, strlen(pathname)));
  dir_name_ptr = dirname((char *)pathname);
  if (dir_name_ptr[0] != '/' && (flags & FLAG_GET_DIRNAME_ABSOLUTE) != 0)
    {
      char rootpath[PATH_MAX+1];
      char *cwd = realpath(".", rootpath);
      sprintf(buffer, "%s/%s", cwd, dir_name_ptr);
      return realpath(dir_name_ptr, NULL);
    }
  else  
    return strdup(dir_name_ptr);
}

/*
 *
 */
const char *getBasename(const char *pathname, const char *ext)
{
  char buffer[PATH_MAX+1];
  unsigned short pathname_len = 0, ext_len = 0;
  char *base_name_ptr = (char *)NULL, *ext_ptr = (char *)NULL;

  bzero((void *)buffer, PATH_MAX+1);
  pathname_len = strlen(pathname);
  ext_len = strlen(ext)+1; /* +1 4 dot */
  memcpy((void *)buffer, pathname, BOOTIMG_MAX(PATH_MAX, strlen(pathname)));
  ext_ptr = strstr(buffer, ext) -1;
  if (ext_ptr == buffer + (pathname_len - ext_len))
    *ext_ptr = '\0';
  base_name_ptr = rindex(buffer, '/') ? rindex(buffer, '/')+1 : buffer;
  return strdup(base_name_ptr);
}


/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */
