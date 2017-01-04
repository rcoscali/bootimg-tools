/* bootimg-tools/bootimg.h
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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <alloca.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "bootimg.h"
#include "cJSON.h"

#if defined(LIBXML_WRITER_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)

#define BOOTIMG_ENCODING "ISO-8859-1"

#define BOOTIMG_XML_FILENAME 1
#define BOOTIMG_JSON_FILENAME 2
#define BOOTIMG_KERNEL_FILENAME 3
#define BOOTIMG_RAMDISK_FILENAME 4
#define BOOTIMG_SECOND_LOADER_FILENAME 5
#define BOOTIMG_DTB_FILENAME 6

#define BOARD_OS_VERSION_COMMENT \
  "This is the version of the board Operating System. It is ususally " \
  "the Android version with major, minor, micro format. Values are computed " \
  "as: major = (os_version >> 14) & 0x7f, minor = (os_version >> 7) & 0x7f, " \
  "micro = os_version & 0x7f"

#define BOARD_OS_PATCH_LEVEL_COMMENT \
  "This is the version of the board Operating System patch Level (Month & Year). " \
  "Values are computed as: year = (os_patch_level >> 4) + 2000, month = " \
  "os_patch_level & 0xf"

/*
 * Options flags & values
 * - v: verbose. vflag € N+*
 * - x: xml metadata file. xflag € [0, 1]
 * - j: json metadata file. jflag € [0, 1]
 * - n: basename for metadata file. nflag € [0, 1]
 */
int vflag = 0;
int xflag = 0;
int jflag = 0;
int nflag = 0;
int pflag = 0;
char *nval = (char *)NULL;
size_t pval = 0L;
char *progname = (char *)NULL;
char *blankname = (char *)NULL;

static const char *progusage =
  "usage: %s [--verbose[=<lvl>] ...] [--xml] [--json] [--name=<basename>] [--pagesize=<pgsz>] imgfile1 [imgfile2 ...]\n"
  "       %s --help                                                                                         \n"
  "                                                                                                         \n"
  "       with the following options                                                                        \n"
  "       %s --verbose|-v <lvl>  : be verbose at runtime. <lvl> is added to current verbosity level.        \n"
  "       %s                       If omited, one is assumed. More verbose flags imply more verbosity.      \n"
  "       %s --xml|-x            : generate an xml metadata file.                                           \n"
  "       %s                       This option is exclusive with json.                                      \n"
  "       %s                       Only the last one will be taken into account.                            \n"
  "       %s --json|-j           : generate a json metadata file.                                           \n"
  "       %s                       This option is exclusive with xml.                                       \n"
  "       %s                       Only the last one will be taken into account.                            \n"
  "       %s --pagesize|-p <pgsz>: Image page size. If not providedn, use the one specified in the file     \n"
  "       %s --name|-n <basename>: provide a basename template for the metadata file.                       \n"
  "       %s --help|-h           : display this message.                                                    \n";

/*
 * Long options
 */
struct option long_options[] = {
  {"name",     required_argument, 0,  'n' },
  {"xml",      no_argument,       0,  'x' },
  {"json",     no_argument,       0,  'j' },
  {"verbose",  optional_argument, 0,  'v' },
  {"pagesize", required_argument, 0,  'p' },
  {"help",     no_argument,       0,  'h' },
  {0,          0,                 0,   0  }
};

const char *unknown_option = "????";

/*
 * Getopt external defs
 */
extern char *optarg;
extern int optind, opterr, optopt;

/*
 * Forward decl
 */
int extractBootImageMetadata(const char *);
void printusage(void);
const char *getLongOptionName(char);
boot_img_hdr *findBootMagic(FILE *, boot_img_hdr *, off_t *);
int readPadding(FILE*, unsigned, int);
const char *getImageFilename(const char *, int);
size_t extractKernelImage(FILE *, boot_img_hdr *, const char *);
size_t extractRamdiskImage(FILE *, boot_img_hdr *, const char *);
size_t extractSecondBootloaderImage(FILE *, boot_img_hdr *, const char *);
size_t extractDeviceTreeImage(FILE *, boot_img_hdr *, const char *);

void *
my_malloc_fn(size_t sz)
{
  return malloc(sz);
}

void
my_free_fn(void *ptr)
{
  free(ptr);
}

/*
 * main
 */
int
main(int argc, char **argv)
{
  int c;
  int digit_optind = 0;
  cJSON_Hooks hooks;

  hooks.malloc_fn = my_malloc_fn;
  hooks.free_fn = my_free_fn;
  
  cJSON_InitHooks(&hooks);
  
  progname = (rindex(argv[0], '/') ? rindex(argv[0], '/')+1 : argv[0]);
  blankname = (char *)alloca(strlen(progname) +1);
  blankname[strlen(progname) +1] = 0;
  memset((void *)blankname, (int)' ', (size_t)strlen(progname));
  
  /*
   * This initialize the libxml2 library and check potential ABI
   * mismatches between the version it was compiled for and the 
   * actual shared library used.
   */
  LIBXML_TEST_VERSION;

  /*
   * Process options
   */
  while (1)
    {
      int this_option_optind = optind ? optind : 1;
      int option_index = 0;
      
      c = getopt_long(argc, argv, "n:xjv::h:",
		      long_options, &option_index);
      if (c == -1)
	break;

      switch (c)
	{
	case 'v':
	  if (optarg)
	    vflag += strtol(optarg, NULL, 10);
	  else
	    vflag++;
	  if (vflag > 3)
	    fprintf(stderr, "%s: option %s/%c set to %d\n",
		    progname, getLongOptionName(c), c, vflag);
	  break;

	case 'x':
	  xflag = 1;
	  if (vflag > 3)
	    fprintf(stderr, "%s: option %s/%c (=%d) set\n",
		    progname, getLongOptionName(c), c, xflag);
	  break;
	  
	case 'j':
	  jflag = 1;
	  if (vflag > 3)
	    fprintf(stderr, "%s: option %s/%c (=%d) set\n",
		    progname, getLongOptionName(c), c, jflag);
	  break;
	  
	case 'n':
	  nflag = 1;
	  nval = optarg;
	  if (vflag > 3)
	    fprintf(stderr, "%s: option %s/%c (=%d) set with value '%s'\n",
		    progname, getLongOptionName(c), c, nflag, nval);
	  break;

	case 'p':
	  pflag = 1;
	  pval = strtol(optarg, NULL, 10);
	  if (vflag > 3)
	    fprintf(stderr, "%s: option %s/%c (=%d) set with value '%lu'\n",
		    progname, getLongOptionName(c), c, pflag, pval);
	  break;

	case 'h':
	  printusage();
	  exit(1);

	case '?':
	  break;
	  
	default:
	  fprintf(stderr, "%s: getopt returned character code 0%o ??\n", progname, c);
	}
    }

  if (vflag > 3)
    {
      fprintf(stderr, "%s: optind = %d\n", progname, optind);
      fprintf(stderr, "%s: argc = %d\n", progname, argc);
    }

  if (optind < argc)
    {
      while (optind < argc)
	/* 
	 *
	 */
	extractBootImageMetadata(argv[optind++]);

      /*
       * Cleanup function for the XML library.
       */
      xmlCleanupParser();
      exit(0);
    }
  else
    {
      fprintf(stderr, "%s: error: No image file provided !\n", progname);
      printusage();
      exit (1);
    }

  return(0);
}

void
printusage(void)
{
  char line[256];
  char *tok = (char *)NULL;
  char twolines = 2;

  char *str = (char *)malloc(strlen(progusage) +1);
  memcpy(str, progusage, strlen(progusage) +1);
  
  while ((tok = strtok((char *)str, "\n")) != (char *)NULL)
    {
      if (twolines)
	{
	  sprintf(line, tok, progname);
	  twolines--;
	}
      else
	sprintf(line, tok, blankname);

      str = (char *)NULL;
      fprintf(stdout, "%s\n", line);
    }

  free((void *)str);
}

const char *
getImageFilename(const char *basename, int kind)
{
  char pathname[PATH_MAX];

  bzero((void *)pathname, PATH_MAX);
  switch (kind)
    {
    case BOOTIMG_XML_FILENAME:
      sprintf(pathname, "%s.xml", basename);
      break;
    case BOOTIMG_JSON_FILENAME:
      sprintf(pathname, "%s.json", basename);
      break;
    case BOOTIMG_KERNEL_FILENAME:
      sprintf(pathname, "%s.img", basename);
      break;
    case BOOTIMG_RAMDISK_FILENAME:
      sprintf(pathname, "%s.cpio.gz", basename);
      break;
    case BOOTIMG_SECOND_LOADER_FILENAME:
      sprintf(pathname, "%s-2ndldr.img", basename);
      break;
    case BOOTIMG_DTB_FILENAME:
      sprintf(pathname, "%s.dtb", basename);
      break;
    default:
      sprintf(pathname, "%s-unknown.dat", basename);
      break;
    }

  return strdup(pathname);
}

size_t
extractKernelImage(FILE *fp, boot_img_hdr *hdr, const char *basename)
{
  size_t readsz = 0;
  const char *filename = getImageFilename(basename, BOOTIMG_KERNEL_FILENAME);
  FILE *k = fopen(filename, "wb");
  
  if (k)
    {
      byte* kernel = (byte*)malloc(hdr->kernel_size);
      if (kernel)
	{
	  readsz = fread(kernel, hdr->kernel_size, 1, fp);
	  if (readsz != 1)
	    fprintf(stderr, "%s: error: expected %d bytes read but got %ld !\n", progname, hdr->kernel_size, readsz * hdr->kernel_size);
	  fwrite(kernel, hdr->kernel_size, 1, k);
	  free(kernel);
	}
      fclose(k);
      free((void *)filename);
    }
  else
    fprintf(stderr,
	    "%s: error: cannot open kernel image file '%s' for writing !\n",
	    progname, filename);

  return(readsz);
}

size_t
extractRamdiskImage(FILE *fp, boot_img_hdr *hdr, const char *basename)
{
  size_t readsz = 0;
  return(readsz);
}

size_t
extractSecondBootloaderImage(FILE *fp, boot_img_hdr *hdr, const char *basename)
{
  size_t readsz = 0;
  return(readsz);
}

size_t
extractDeviceTreeImage(FILE *fp, boot_img_hdr *hdr, const char *basename)
{
  size_t readsz = 0;
  return(readsz);
}

int
extractBootImageMetadata(const char *imgfile)
{
  boot_img_hdr header, *hdr = (boot_img_hdr *)NULL;
  FILE *imgfp = (FILE *)NULL, *xfp = (FILE *)NULL, *jfp = (FILE *)NULL;
  off_t offset = 0;
  size_t total_read = 0;
  const char *basename = (const char *)(nval ? nval : imgfile);
  const char *xml_filename = (const char *)NULL, *json_filename = (const char *)NULL;
  xmlTextWriterPtr xmlWriter;
  xmlDocPtr xmlDoc;
  cJSON *jsonDoc;
  
  if (vflag)
    fprintf(stderr, "Image filename option: '%s'\n", imgfile);
  
  if ((imgfp = fopen(imgfile, "rb")) == (FILE *)NULL)
    {
      fprintf(stderr, "%s: error: cannot open image file at '%s'\n", progname, imgfile);
      return 1;
    }
  
  if ((hdr = findBootMagic(imgfp, &header, &offset)) != (boot_img_hdr *)NULL)
    {
      total_read = offset;
      
      if (vflag)
	fprintf(stdout, "%s: Magic found at offset %ld in file '%s'\n", progname, offset, imgfile);
      
      if (xflag)
	do
	  {
	    int rc;
	    
	    xml_filename = getImageFilename(basename, BOOTIMG_XML_FILENAME);
	    xmlWriter = xmlNewTextWriterDoc(&xmlDoc, 0);
	    if (xmlWriter == NULL)
	      {
		fprintf(stderr, "%s: error: cannot create the xml writer !\n", progname);
		xml_filename = (const char *)NULL;
		break;
	      }
	    rc = xmlTextWriterSetIndent(xmlWriter, 4);
	    if (rc < 0)
	      fprintf(stderr, "%s: error: cannot set indentation level !\n", progname);
	    rc = xmlTextWriterStartDocument(xmlWriter, NULL, BOOTIMG_ENCODING, NULL);
	    if (rc < 0)
	      {
		fprintf(stderr, "%s: error: cannot start the xml document !\n", progname);
		xmlFreeTextWriter(xmlWriter);
		free((void *)xml_filename);
		xml_filename = (const char *)NULL;
		break;
	      }
	    rc = xmlTextWriterStartElement(xmlWriter, BAD_CAST "bootImage");
	    if (rc < 0)
	      {
		fprintf(stderr, "%s: error: cannot start the bootImage root element !\n", progname);
		xmlFreeTextWriter(xmlWriter);
		free((void *)xml_filename);
		xml_filename = (const char *)NULL;
		break;
	      }
	  }
	while (0);
      
      if (jflag)
	do
	  {
	    json_filename = getImageFilename(basename, BOOTIMG_JSON_FILENAME);
	    jfp = fopen(json_filename, "wb");
	    if (!jfp)
	      {
		fprintf(stdout, "%s: error: cannot open json file '%s' for writing !\n", progname, json_filename);
		break;
	      }
	    jsonDoc = cJSON_CreateObject();
	    if (!jsonDoc)
	      {
		fprintf(stdout, "%s: error: cannot create json object !\n", progname);
		fclose(jfp);
		jfp = (FILE *)NULL;
		break;		
	      }
	  }
	while(0);
      
      if (!jfp && !xml_filename)
	{
	  fprintf(stderr, "%s: error: cannot save metadata %s%s%s%s!\n",
		  progname,
		  xflag && jflag ? "neither " : "",
		  xflag ? "in xml" : "",
		  xflag && jflag ? " nor ": "",
		  jflag ? "in json" : "");
	  goto exit_on_error;
	}

      /* Process OS version value */
      if (hdr->os_version != 0)
	{
	  int os_version, os_patch_level;
	  int major, minor, micro;
	  int year, month;
	  char boardOsVersionStr[100], boardOsPatchLvlStr[100];
	  
	  os_version = header.os_version >> 11;
	  os_patch_level = header.os_version&0x7ff;
	  
	  major = (os_version >> 14)&0x7f;
 	  minor = (os_version >> 7)&0x7f;
	  micro = os_version&0x7f;
	  if (vflag)
	    fprintf(stdout, "BOARD_OS_VERSION %d.%d.%d\n", major, minor, micro);
	  
	  year = (os_patch_level >> 4) + 2000;
	  month = os_patch_level&0xf;
	  if (vflag)
	    fprintf(stdout, "BOARD_OS_PATCH_LEVEL %d-%02d\n", year, month);
	  	  
	  sprintf(boardOsVersionStr, "%d.%d.%d", major, minor, micro);
	  sprintf(boardOsPatchLvlStr, "%d-%02d", year, month);
	  
	  if (xflag)
	    do
	      {
		/* Add boardOsVersion element */
		if (xmlTextWriterStartElement(xmlWriter, "boardOsVersion") < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "value", "%d", os_version) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion value\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "major", "%d", major) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion major\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "minor", "%d", minor) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion minor\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "micro", "%d", micro) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion micro\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "valueStr", "%s", boardOsVersionStr) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion valueStr\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "comment", "%s", BOARD_OS_VERSION_COMMENT) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion comment\n", progname);
		    break;
		  }
		if (xmlTextWriterEndElement(xmlWriter) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot close xml element for boardOsVersion\n", progname);
		    break;
		  }
		
		/* Add boardOsPatchLvl element */
		if (xmlTextWriterStartElement(xmlWriter, "boardOsPatchLvl") < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "value", "%d", os_patch_level) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl value\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "year", "%d", year) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl year\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "month", "%d", month) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl month\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "valueStr", "%s", boardOsPatchLvlStr) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl valueStr\n", progname);
		    break;
		  }
		if (xmlTextWriterWriteFormatElement(xmlWriter, "comment", "%s", BOARD_OS_PATCH_LEVEL_COMMENT) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl comment\n", progname);
		    break;
		  }
		if (xmlTextWriterEndElement(xmlWriter) < 0)
		  {
		    fprintf(stderr, "%s: error: cannot close xml element for boardOsPatchLvl\n", progname);
		    break;
		  }
	      }
	    while (0);
	  
	  if (jflag)
	    do
	      {
		cJSON *boardOsVersion, *boardOsPatchLvl;
		
		boardOsVersion = cJSON_CreateObject();
		if (!boardOsVersion)
		  {
		    fprintf(stderr, "%s: error: cannot create json object for boardOsVersion", progname);
		    break;
		  }
		cJSON_AddItemToObject(boardOsVersion, "value", cJSON_CreateNumber(os_version));
		cJSON_AddItemToObject(boardOsVersion, "major", cJSON_CreateNumber(major));
		cJSON_AddItemToObject(boardOsVersion, "minor", cJSON_CreateNumber(minor));
		cJSON_AddItemToObject(boardOsVersion, "micro", cJSON_CreateNumber(micro));
		cJSON_AddItemToObject(boardOsVersion, "valueStr", cJSON_CreateString(boardOsVersionStr));
		cJSON_AddItemToObject(boardOsVersion, "comment", cJSON_CreateString(BOARD_OS_VERSION_COMMENT));
		
		boardOsPatchLvl = cJSON_CreateObject();
		if (!boardOsPatchLvl)
		  {
		    fprintf(stderr, "%s: error: cannot create json object for boardOsPatchLvl\n", progname);
		    cJSON_Delete(boardOsVersion);
		    break;
		  }
		cJSON_AddItemToObject(boardOsVersion, "value", cJSON_CreateNumber(os_patch_level));
		cJSON_AddItemToObject(boardOsVersion, "year", cJSON_CreateNumber(year));
		cJSON_AddItemToObject(boardOsVersion, "month", cJSON_CreateNumber(month));
		cJSON_AddItemToObject(boardOsVersion, "valueStr", cJSON_CreateString(boardOsPatchLvlStr));
		cJSON_AddItemToObject(boardOsVersion, "comment", cJSON_CreateString(BOARD_OS_PATCH_LEVEL_COMMENT));
		
		cJSON_AddItemToObject(jsonDoc, "boardOsVersion", boardOsVersion);
		cJSON_AddItemToObject(jsonDoc, "boardOsPatchLvl", boardOsPatchLvl);
	      }
	    while(0);
	}	  

      if (hdr->dt_size != 0)
	{
	}
      
      if (!pflag)
	{
	  pval = hdr->page_size;
	}
	  
      total_read += sizeof(header);
      total_read += readPadding(imgfp, sizeof(header), pval);
	  
      extractKernelImage(imgfp, hdr, basename);

      total_read += hdr->kernel_size;
      total_read += readPadding(imgfp, hdr->kernel_size, pval);

      extractRamdiskImage(imgfp, hdr, basename);

      total_read += hdr->ramdisk_size;
      total_read += readPadding(imgfp, hdr->ramdisk_size, pval);

      if (hdr->second_size)
	{
	  extractSecondBootloaderImage(imgfp, hdr, basename);

	  total_read += hdr->second_size;
	}

      total_read += readPadding(imgfp, hdr->second_size, pval);

      if (hdr->dt_size != 0)
	{
	  extractDeviceTreeImage(imgfp, hdr, basename);
	  total_read += hdr->dt_size;
	}

      if (vflag > 2)
	fprintf(stdout, "%s: total read: %ld\n", progname, total_read);

      if (xflag)
	do
	  {
	    (void)xmlTextWriterEndDocument(xmlWriter);
	    (void)xmlTextWriterFlush(xmlWriter);
	    xmlFreeTextWriter(xmlWriter);
	    xmlSaveFileEnc(xml_filename, xmlDoc, BOOTIMG_ENCODING);
	    xmlFreeDoc(xmlDoc);
	    free((void *)xml_filename);
	  }
	while (0);

      if (jflag)
	do
	  {
	    char *buf = cJSON_Print(jsonDoc);
	    (void)fwrite((void *)buf, strlen(buf), 1, jfp);
	    free((void *)buf);
	    free((void *)json_filename);
	    fclose(jfp);
	  }
	while (0);

    exit_on_error:
      fclose(imgfp);
    }
  else
    {
      fprintf(stderr, "%s: error: Magic not found in file '%s'\n", progname, imgfile);
    }
      
  return 0;
}
  
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

boot_img_hdr *
findBootMagic(FILE *fp, boot_img_hdr *hdr, off_t *off)
{
  size_t total_read = 0;
  size_t base = 0;
  off_t i;
  off_t seeklimit = 4096;
  char tmp[PATH_MAX];
  
  if (vflag > 3)
    fprintf(stderr, "%s: Reading header...\n", progname);

  for (i = 0; i <= seeklimit; i++)
    {
      fseek(fp, i, SEEK_SET);
      size_t rdsz = fread(tmp, BOOT_MAGIC_SIZE, 1, fp);
      if (vflag)
	fprintf(stdout, "%s: %lu bytes read for header\n", progname, rdsz * BOOT_MAGIC_SIZE);
      if (rdsz != 1)
	fprintf(stderr, "%s: error: expected %d bytes read, but got %lu\n", progname, BOOT_MAGIC_SIZE, rdsz* BOOT_MAGIC_SIZE);
      if (memcmp(tmp, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0)
	break;
    }
  total_read = (size_t)i;
  if (i > seeklimit)
    {
      fprintf(stderr, "%s: error: Android boot magic not found.\n", progname);
      return (boot_img_hdr *)NULL;
    }
  fseek(fp, i, SEEK_SET);
  
  if (vflag && i > 0)
    {
      fprintf(stderr, "Android magic found at offset: %ld\n", i);
    }
  
  size_t rdsz = fread(hdr, sizeof(boot_img_hdr), 1, fp);
  if (rdsz != 1)
    fprintf(stderr, "%s: error: expected %lu bytes read, but got %lu\n", progname, sizeof(boot_img_hdr), rdsz * sizeof(boot_img_hdr));
  if (vflag > 1)
    {
      base = hdr->kernel_addr - 0x00008000;
      fprintf(stderr, "BOARD_KERNEL_CMDLINE %s\n", hdr->cmdline);
      fprintf(stderr, "BOARD_KERNEL_BASE %08lx\n", base);
      fprintf(stderr, "BOARD_NAME %s\n", hdr->name);
      fprintf(stderr, "BOARD_PAGE_SIZE %d\n", hdr->page_size);
      fprintf(stderr, "BOARD_KERNEL_OFFSET %08lx\n", hdr->kernel_addr - base);
      fprintf(stderr, "BOARD_RAMDISK_OFFSET %08lx\n", hdr->ramdisk_addr - base);
      if (hdr->second_size != 0)
	fprintf(stderr, "BOARD_SECOND_OFFSET %08lx\n", hdr->second_addr - base);
      fprintf(stderr, "BOARD_TAGS_OFFSET %08lx\n", hdr->tags_addr - base);
    }

  return hdr;
}

int
readPadding(FILE* f, unsigned itemsize, int pagesize)
{
  byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
  unsigned pagemask = pagesize - 1;
  unsigned count;
    
  if((itemsize & pagemask) == 0)
    {
      free(buf);
      return 0;
    }
  
    count = pagesize - (itemsize & pagemask);
    
    size_t rdsz = fread(buf, count, 1, f);
    if (rdsz != 1)
      fprintf(stderr, "%s: error: expected %u bytes but got %lu\n", progname, count, count * rdsz);
    free((void *)buf);

    return count;
}

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* compile-command: "cc bootimg-extract.c -I/usr/include/libxml2 \ */
/*                   -Dmumble=blaah -o bootimg-extract -llibxml2"  */
/* End:                                                            */

#endif /* defined(LIBXML_WRITER_ENABLED) && defined(LIBXML_OUTPUT_ENABLED) */
