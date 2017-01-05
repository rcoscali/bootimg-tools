/* bootimg-tools/bootimg-extract.c
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
//#include <unistd.h>
#include <getopt.h>
#include <alloca.h>

//#include <libxml/encoding.h>
//#include <libxml/xmlwriter.h>

#include "bootimg.h"
#include "cJSON.h"

/*
 * Options flags & values
 * - v: verbose. vflag € N+*
 * - o: outdir. oflag € [0, 1]
 * - x: xml metadata file. xflag € [0, 1]
 * - j: json metadata file. jflag € [0, 1]
 * - p: page size. pflag € [0, 1]
 * - n: basename for metadata file. nflag € [0, 1]
 */
int vflag = 0;
int oflag = 0;
int xflag = 0;
int jflag = 0;
int nflag = 0;
int pflag = 0;

/* nval: basename */
char *nval = (char *)NULL;
/* oval: outdir */
char *oval = (char *)NULL;
/* pval: pagesize */
size_t pval = 0L;

/*
 * progname & blankname are program name and space string with progname size
 * for displaying messsages and help
 */
char *progname = (char *)NULL;
char *blankname = (char *)NULL;

/*
 * Kernel offset is 0x8000
 * then base addr is calculated with kernel_addr - offset;
 */
off_t kernel_offset = 0x00008000;
size_t base_addr = 0; 

static const char *progusage =
  "usage: %s --verbose[=<lvl>] ...] [--xml] [--json] [--name=<basename>] [--outdir=<outdir>] [--pagesize=<pgsz>] imgfile1 [imgfile2 ...]\n"
  "       %s --help                                                                                         \n"
  "                                                                                                         \n"
  "       with the following options                                                                        \n"
  "       %s --verbose|-v <lvl>  : be verbose at runtime. <lvl> is added to current verbosity level.        \n"
  "       %s                       If omited, one is assumed. More verbose flags imply more verbosity.      \n"
  "       %s --outdir|-o <outdir>: Save potentially big image files in the <outdir> directory               \n"
  "       %s                       Impacted images are kernel, ramdisk, second bootloader & dtb             \n"
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
  {"verbose",  optional_argument, 0,  'v' },
  {"outdir",   required_argument, 0,  'o' },
  {"name",     required_argument, 0,  'n' },
  {"xml",      no_argument,       0,  'x' },
  {"json",     no_argument,       0,  'j' },
  {"pagesize", required_argument, 0,  'p' },
  {"help",     no_argument,       0,  'h' },
  {0,          0,                 0,   0  }
};
#define BOOTIMG_OPTSTRING "v::o:n:xjh:"
const char *unknown_option = "????";

/*
 * Getopt external defs
 */
extern char *optarg;
extern int optind, opterr, optopt;

/*
 * Forward decl
 */


/*
 * Used for cJSON allocations
 */
void *
my_malloc_fn(size_t sz)
{
  return malloc(sz);
}

/*
 * Used for cJSON allocations
 */
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
  oval = get_current_dir_name();
  
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
      
      c = getopt_long(argc, argv, BOOTIMG_OPTSTRING,
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

	case 'o':
	  {
	    const char * oldval = oval;
	    oflag = 1;
	    oval = optarg;
	    if (oval[0] != '/')
	      {
		char newval[PATH_MAX];
		if (oval[0] == '.' && oval[1] == '/')
		  oval = &oval[2];
		sprintf(newval, "%s/%s", oldval, oval);
		oval = strdup(newval);
	      }
	    free((void *)oldval);
	    if (vflag)
	      fprintf(stderr, "%s: option %s/%c (=%d) set\n",
		      progname, getLongOptionName(c), c, oflag);
	  }
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
}
