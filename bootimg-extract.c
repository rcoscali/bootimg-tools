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

#if defined(LIBXML_WRITER_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)

#define BOOTIMG_ENCODING "ISO-8859-1"

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
int read_padding(FILE*, unsigned, int);

/*
 * main
 */
int
main(int argc, char **argv)
{
  int c;
  int digit_optind = 0;

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
	    fprintf(stderr, "%s: option %s/%c (=%d) set with value '%s'\n",
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
      fprintf(stderr, "%s: error: No image file provided !", progname);
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

int
extractBootImageMetadata(const char *imgfile)
{
  boot_img_hdr header, *hdr = (boot_img_hdr *)NULL;
  FILE *imgfp = (FILE *)NULL;
  off_t offset = 0;
  size_t total_read = 0;
  
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

      if (hdr->os_version != 0)
	{
	}

      if (hdr->dt_size != 0)
	{
	}

      if (!pflag)
	{
	  pval = hdr->page_size;
	}

      total_read += sizeof(header);
      total_read += read_padding(fp, sizeof(header), pval);

      extractKernelImage();

      total_read += hdr->kernel_size;
      total_read += read_padding(fp, hdr->kernel_size, pagesize);

      extractRamdiskImage();

      total_read += hdr->ramdisk_size;
      total_read += read_padding(fp, hdr->ramdisk_size, pagesize);

      if (hdr->second_size)
	{
	  extractSecondBootloaderImage();

	  total_read += hdr->second_size;
	}

      total_read += read_padding(fp, hdr->second_size, pagesize);

      if (hdr->dt_size != 0)
	{
	  extractDeviceTreeImage();
	  total_read += hdr->dt_size;
	}

      if (vflag > 2)
	fprintf(stdout, "%s: total read: %ld\n", progname, total_read);
      
      fclose(fp);
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
      (void)fread(tmp, BOOT_MAGIC_SIZE, 1, fp);
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
  
  (void)fread(hdr, sizeof(boot_img_hdr), 1, fp);
  if (vflag > 1)
    {
      base = hdr->kernel_addr - 0x00008000;
      fprintf(stderr, "BOARD_KERNEL_CMDLINE %s\n", hdr->cmdline);
      fprintf(stderr, "BOARD_KERNEL_BASE %08x\n", base);
      fprintf(stderr, "BOARD_NAME %s\n", hdr->name);
      fprintf(stderr, "BOARD_PAGE_SIZE %d\n", hdr->page_size);
      fprintf(stderr, "BOARD_KERNEL_OFFSET %08x\n", hdr->kernel_addr - base);
      fprintf(stderr, "BOARD_RAMDISK_OFFSET %08x\n", hdr->ramdisk_addr - base);
      if (hdr->second_size != 0)
	fprintf(stderr, "BOARD_SECOND_OFFSET %08x\n", hdr->second_addr - base);
      fprintf(stderr, "BOARD_TAGS_OFFSET %08x\n", hdr->tags_addr - base);
    }

  return hdr;
}

int
read_padding(FILE* f, unsigned itemsize, int pagesize)
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
    
    (void)fread(buf, count, 1, f);
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
