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

#include <stdio.h>
#include <values.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>

#include "bootimg.h"
#include "bootimg-priv.h"

/* Verbosity flag (from command line options) */
extern int vflag;
extern char *progname;
extern struct option *long_options;
extern const char *unknown_option;

/*
 * Retrieve long option name from short name
 */
const char *
getLongOptionName(char option)
{
  struct option *opt;

  opt = long_options;
  while (opt)
    {
      if (opt->val == option)
	return opt->name;
      opt++;
    }
  return unknown_option;
}

/*
 * Return an image file name
 */
const char *
getImageFilename(const char *basename, const char *outdir, int kind)
{
  char pathname[PATH_MAX];

  bzero((void *)pathname, PATH_MAX);
  
  switch (kind)
    {
      /* strip extension for boot image files */
    case BOOTIMG_BOOTIMG_FILENAME:
      *(strchrnul(basename, '.')) = 0;
      break;
    default:
      /* else change extension .<ext> in _<ext> for all other images */
      if (strrchr(basename, '.'))
	*(strrchr(basename, '.')) = '_';
    }
  
  switch (kind)
    {
    case BOOTIMG_BOOTIMG_FILENAME:
      sprintf(pathname, "%s/%s.img", outdir, basename);
      if (vflag > 2)
	fprintf(stdout, "%s: Boot Image filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_XML_FILENAME:
      sprintf(pathname, "%s.xml", basename);
      if (vflag > 2)
	fprintf(stdout, "%s: XML Metadata filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_JSON_FILENAME:
      sprintf(pathname, "%s.json", basename);
      if (vflag > 2)
	fprintf(stdout, "%s: JSON Metadata filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_KERNEL_FILENAME:
      sprintf(pathname, "%s/%s.img", outdir, basename);
      if (vflag > 2)
	fprintf(stdout, "%s: KERNEL filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_RAMDISK_FILENAME:
      sprintf(pathname, "%s/%s.cpio.gz", outdir, basename);
      if (vflag > 2)
	fprintf(stdout, "%s: RAMDISK filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_SECOND_LOADER_FILENAME:
      sprintf(pathname, "%s/%s-2ndldr.img", outdir, basename);
      if (vflag > 2)
	fprintf(stdout, "%s: 2nd BOOTLOADER filename = '%s'\n", progname, pathname);
      break;
    case BOOTIMG_DTB_FILENAME:
      sprintf(pathname, "%s/%s.dtb", outdir, basename);
      if (vflag > 2)
	fprintf(stdout, "%s: DTB filename = '%s'\n", progname, pathname);
      break;
    default:
      sprintf(pathname, "%s-unknown.dat", basename);
      fprintf(stderr, "%s: error: Unknown filename = '%s'\n", progname, pathname);
      break;
    }
  
  if (vflag == 2)
    fprintf(stdout, "%s: creating file with name '%s'\n", progname, pathname);
    
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
