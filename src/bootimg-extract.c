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

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# ifdef HAVE_STDDEF_H
#  include <stddef.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <getopt.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include <errno.h>

#ifdef USE_LIBXML2
# include <libxml/xmlversion.h>
# if defined(LIBXML_WRITER_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)
#  include <libxml/encoding.h>
#  include <libxml/xmlwriter.h>
# else
#  error Cannot build with your libxml2 that does not have xmlWriter and having output disabled
# endif /* defined(LIBXML_WRITER_ENABLED) && defined(LIBXML_OUTPUT_ENABLED) */
#endif

#include "cJSON.h"

#ifdef USE_OPENSSL
# ifndef OPENSSL_NO_SHA256
#  include <openssl/sha.h>
#  include <openssl/err.h>
# else
#  error No SHA256 available in this openssl ! It is mandatory ... 
# endif
#endif

#include "bootimg.h"
#include "bootimg-priv.h"
#include "bootimg-utils.h"

#define BUF_LENGTH 1024

#define BOARD_OS_VERSION_COMMENT                                        \
  "This is the version of the board Operating System. It is ususally "  \
  "the Android version with major, minor, micro format. Values are computed " \
  "as: major = (os_version >> 14) & 0x7f, minor = (os_version >> 7) & 0x7f, " \
  "micro = os_version & 0x7f"

#define BOARD_OS_PATCH_LEVEL_COMMENT                                    \
  "This is the version of the board Operating System patch Level (Month & Year). " \
  "Values are computed as: year = (os_patch_level >> 4) + 2000, month = " \
  "os_patch_level & 0xf"

/*
 * TODO
 * Some comments for others fields of image are missing
 */

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
#ifdef USE_LIBXML2
int xflag = 0;
#endif
int jflag = 0;
int nflag = 0;
int iflag = 0;
int pflag = 0;
int Fflag = 0;
int Vflag = 0;
int dflag = 0;
int rrflag = 0;
int brrflag = 0;
int errflag = 0;
int frrflag = 0;
int prrflag = 0;

/* nval: basename */
char *nval = (char *)NULL;
/* oval: outdir */
char *oval = (char *)NULL;
/* pval: pagesize */
size_t pval = 0L;
/* Fval: ramdisk FS dir */
char *Fval = (char *)NULL;

/* rewrite rules */
char *basename_rr = (char *)NULL;
char *extension_rr = (char *)NULL;
char *filename_rr = (char *)NULL;
char *pathname_rr = (char *)NULL;

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
  "usage: %s [options] imgfile1 [imgfile2 ... imgfileN]\n"
  "       %s --help\n";
static const char *proghelp =
  "\n"
  "       basic user options:\n"
  "       %s -h --help                     display this message.\n"
  "       %s -v --verbose[=<lvl>]          be verbose at runtime. <lvl> is\n"
  "       %s                               added to current verbosity level.\n"
  "       %s                               If omited, one is assumed. More\n"
  "       %s                               verbose flags imply more verbosity.\n"
  "       %s -i --identity                 Dump the image id.\n"
  "       %s -p --pagesize=<pgsz>          Image page size. If not provided,\n"
  "       %s                               use the one specified in the image\n"
  "       %s                               file.\n"
  "\n"
  "       options for controling metadata files generation:\n"
  "       %s -o --outdir=<outdir>          Save potentially big image files\n"
  "       %s                               in the <outdir> directory.\n"
  "       %s                               Impacted images are kernel,\n"
  "       %s                               ramdisk, second bootloader & dtb\n"
  "       %s                               ones.\n"
#ifdef USE_LIBXML2
  "       %s -x --xml                      generate an xml metadata file.\n"
  "       %s                               This option is exclusive with json.\n"
  "       %s                               Only the last one will be taken into\n"
  "       %s                               account.\n"
#endif
  "       %s -j --json                     generate a json metadata file.\n"
  "       %s                               This option is exclusive with xml.\n"
  "       %s                               Only the last one will be taken into\n"
  "       %s                               account.\n"
  "       %s --image-basename-rewrite-cmd=<sed-cmd>\n"
  "       %s --image-ext-rewrite-cmd=<sed-cmd>\n"
  "       %s --image-filename-rewrite-cmd=<sed-cmd>\n"
  "       %s --image-pathname-rewrite-cmd=<sed-cmd>\n"
  "       %s                               These commands allows to provide\n"
  "       %s                               a sed command string that will be\n"
  "       %s                               on resp. basename, extension,\n"
  "       %s                               filename and pathname.\n"
  "       %s -F --fs=[<fsdir>]             Extract the filesystem cpio archive.\n"
  "       %s                               in <fsdir>.\n"
#ifdef USE_OPENSSL
  "       %s -V --verity                   Verify VERITY signature block if one\n"
  "       %s                               is available.\n"
#endif
  "       %s -d --dummy                    Dummy run: display id and verity but\n"
  "       %s                               do not extract/create anything\n"
  "       %s -n --name=<basename>          provide a basename template for the\n"
  "       %s                               metadata file.\n";

/*
 * Long options
 */
struct option long_options[] = {
  {"verbose",                    optional_argument, 0,      'v' },
  {"outdir",                     required_argument, 0,      'o' },
  {"name",                       required_argument, 0,      'n' },
#ifdef USE_LIBXML2
  {"xml",                        no_argument,       0,      'x' },
#endif
  {"json",                       no_argument,       0,      'j' },
  {"identity",                   no_argument,       0,      'i' },
  {"fs",                         optional_argument, 0,      'F' },
  {"pagesize",                   required_argument, 0,      'p' },
  {"help",                       no_argument,       0,      'h' },
  {"image-basename-rewrite-cmd", required_argument, &rrflag, 0  },
  {"image-ext-rewrite-cmd",      required_argument, &rrflag, 1  },
  {"image-filename-rewrite-cmd", required_argument, &rrflag, 2  },
  {"image-pathname-rewrite-cmd", required_argument, &rrflag, 3  },
#ifdef USE_LIBXML2
  {"verity",                     no_argument,       0,      'V' },
#endif
  {"dummy",                      no_argument,       0,      'd' },
  {0,                            0,                 0,       0  }
};
#ifdef USE_LIBXML2
# ifdef USE_OPENSSL
#  define BOOTIMG_OPTSTRING "v::o:n:xjiF::p:hVd"
# else
#  define BOOTIMG_OPTSTRING "v::o:n:xjiF::p:hd"
# endif
#else
# ifdef USE_OPENSSL
#  define BOOTIMG_OPTSTRING "v::o:n:jiF::p:hVd"
# else
#  define BOOTIMG_OPTSTRING "v::o:n:jiF::p:hd"
# endif
#endif
const char *unknown_option = "????";

/*
 * Getopt external defs
 */
extern char *optarg;
extern int optind;

/*
 * Forward decls
 */
int           extractBootImageMetadata(const char *, const char *);
void          printusage(int);
boot_img_hdr *findBootMagic(FILE *, boot_img_hdr *, off_t *);
int           readPadding(FILE*, unsigned, int);
size_t        extractKernelImage(FILE *, boot_img_hdr *, const char *, const char *);
size_t        extractRamdiskImage(FILE *, boot_img_hdr *, const char *, const char *);
size_t        extractSecondBootloaderImage(FILE *, boot_img_hdr *, const char *, const char *);
size_t        extractDeviceTreeImage(FILE *, boot_img_hdr *, const char *, const char *);
void          extractRamdiskFiles(const char *, const char *);

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
  
#ifdef USE_LIBXML2
  /*
   * This initialize the libxml2 library and check potential ABI
   * mismatches between the version it was compiled for and the 
   * actual shared library used.
   */
  LIBXML_TEST_VERSION;
#endif
  
#ifdef USE_OPENSSL
  ERR_load_crypto_strings();
#endif
  
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
        case 0:
          switch (rrflag)
            {
            case 0:
              basename_rr = optarg;
              brrflag = 0;
              if (vflag > 3)
                fprintf(stdout, "%s: option %s set to '%s'\n", progname, getLongOptionName(long_options, rrflag), basename_rr);
              break;

            case 1:
              extension_rr = optarg;
              errflag = 0;
              if (vflag > 3)
                fprintf(stdout, "%s: option %s set to '%s'\n", progname, getLongOptionName(long_options, rrflag), extension_rr);
              break;

            case 2:
              filename_rr = optarg;
              frrflag = 0;
              if (vflag > 3)
                fprintf(stdout, "%s: option %s set to '%s'\n", progname, getLongOptionName(long_options, rrflag), filename_rr);
              break;

            case 3:
              pathname_rr = optarg;
              prrflag = 0;
              if (vflag > 3)
                fprintf(stdout, "%s: option %s set to '%s'\n", progname, getLongOptionName(long_options, rrflag), pathname_rr);
              break;
            }
          rrflag = 0;
          break;

        case 'v':
          if (optarg)
            vflag += strtol(optarg, NULL, 10);
          else
            vflag++;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c set to %d\n",
                    progname, getLongOptionName(long_options, c), c, vflag);
          break;

        case 'o':
          {
            const char * oldval = oval;
            oflag = 1;
            oval = optarg;
            if (oval[0] != '/')
              {
                char *newval = (char *)malloc(PATH_MAX+1);
                assert(newval);
                if (oval[0] == '.' && oval[1] == '/')
                  oval = &oval[2];
                snprintf(newval,
                         PATH_MAX,
                         "%s%s%s",
                         oldval,
                         (oldval[strlen(oldval)-1]=='/'?"":"/"),
                         oval);
                oval = newval;
              }
            free((void *)oldval);
            if (vflag)
              fprintf(stderr, "%s: option %s/%c (=%d) set to '%s'\n",
                      progname, getLongOptionName(long_options, c), c, oflag, oval);
          }
          break;

#ifdef USE_LIBXML2
        case 'x':
          xflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set\n",
                    progname, getLongOptionName(long_options, c), c, xflag);
          break;
#endif
          
        case 'j':
          jflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set\n",
                    progname, getLongOptionName(long_options, c), c, jflag);
          break;
          
        case 'n':
          nflag = 1;
          nval = optarg;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set with value '%s'\n",
                    progname, getLongOptionName(long_options, c), c, nflag, nval);
          break;

        case 'i':
          iflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c set\n",
                    progname, getLongOptionName(long_options, c), c);
          break;
          
        case 'V':
          Vflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c set\n",
                    progname, getLongOptionName(long_options, c), c);
          break;
          
        case 'd':
          dflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c set\n",
                    progname, getLongOptionName(long_options, c), c);
          break;
          
        case 'p':
          pflag = 1;
          pval = strtol(optarg, NULL, 10);
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set with value '%lu'\n",
                    progname, getLongOptionName(long_options, c), c, pflag, pval);
          break;

        case 'F':
          Fflag = 1;
          if (optarg)
            {
              Fval = strdup(optarg);
              assert(Fval);
            }
          
          else if (argv[optind] && argv[optind][0] != '-')
            {
              Fval = strdup(argv[optind]);
              assert(Fval);
              optind++;
            }
          
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set with value '%s'\n",
                    progname, getLongOptionName(long_options, c), c, Fflag, Fval);
          break;

        case 'h':
          printusage(1);
          exit(1);

        case '?':
          printusage(0);
          break;
          
        default:
          fprintf(stderr, "%s: getopt returned character code 0%o ??\n", progname, c);
          printusage(0);
        }
    }

  brrflag = (basename_rr != NULL);
  errflag = (extension_rr != NULL);
  frrflag = (filename_rr != NULL);
  prrflag = (pathname_rr != NULL);
  rrflag = brrflag || errflag || frrflag || prrflag;

#ifdef USE_LIBXML2
  /* XML is default metadata format */
  if (!xflag && !jflag)
    xflag = 1;
#endif

  if (optind < argc)
    {
      while (optind < argc)
	{
          if (!oflag)
            {
              oval = (char *)getDirname(argv[optind], FLAG_GET_DIRNAME_ABSOLUTE);
              if (!oval)
                {
                  fprintf (stderr, "%s: error: cannot allocate memory for output directory name!\n", progname);
                  exit (1);
                }
            }
	  
          struct stat statbuf;
          if (stat(oval, &statbuf) == -1 && errno == ENOENT)
            {
              if (mkdir(oval, 0755) == -1)
                {
                  perror(progname);
                  fprintf(stderr, "%s: error: Cannot create output directory '%s'!\n", progname, oval);
                  exit(1);
                }
            }
          if (Fval)
            {
              if (Fval[0] != '/')
                {
                  char *Fval_tmp = (char *)malloc(PATH_MAX +1);

                  assert(Fval_tmp);

                  snprintf(Fval_tmp, PATH_MAX, "%s/%s", oval, Fval);
                  free((void *)Fval);
                  Fval = Fval_tmp;
                }
              if (stat(Fval, &statbuf) == -1 && errno == ENOENT)
                {
                  if (mkdir(Fval, 0755) == -1)
                    {
                      perror(progname);
                      fprintf(stderr, "%s: error: Cannot create ramdisk output directory '%s'!\n", progname, Fval);
                      exit(1);
                    }
                }
            }

          if (extractBootImageMetadata(argv[optind++], oval) && vflag)
            fprintf(stdout, "%s: image data successfully extracted from '%s'\n", progname, argv[optind-1]);
          else if (vflag)
            fprintf(stderr, "%s: error: image data extraction failure for '%s'\n", progname, argv[optind-1]);

          if (!oflag)
            free(oval);
        }

#ifdef USE_LIBXML2
      /*
       * Cleanup function for the XML library.
       */
      xmlCleanupParser();
#endif
      exit(0);
    }
  else
    {
      fprintf(stderr, "%s: error: No image file provided !\n", progname);
      printusage(0);
      exit (1);
    }

  return(0);
}

/*
 * Print usage message
 */
void
printusage(int withhelp)
{
  char line[256];
  char *tok = (char *)NULL;
  char twolines = 2;
  char *str;

  if (withhelp)
  {
	  str = (char *)malloc(strlen(progusage) +strlen(proghelp) +1);
	  assert(str);
	  memcpy(str, progusage, strlen(progusage));
	  memcpy(str +strlen(progusage), proghelp, strlen(proghelp) +1);
  }
  else
  {
	  str = (char *)malloc(strlen(progusage) +1);
	  assert(str);
	  memcpy(str, progusage, strlen(progusage) +1);
  }
  
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

/*
 * Extract the kernel image in a file
 */
size_t
extractKernelImage(FILE *fp, boot_img_hdr *hdr, const char *outdir, const char *basename)
{
  size_t readsz = 0;
  const char *filename = getImageFilename(basename, outdir, BOOTIMG_KERNEL_FILENAME);
  FILE *k = fopen(filename, "w");
  
  if (k)
    {
      byte* kernel = (byte*)malloc(hdr->kernel_size);
      if (kernel)
        {
          readsz = fread(kernel, hdr->kernel_size, 1, fp);
          if (readsz != 1)
            fprintf(stderr, "%s: error: expected %d bytes read but got %ld !\n",
                    progname, hdr->kernel_size, readsz * hdr->kernel_size);
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

  return(readsz * hdr->kernel_size);
}

/*
 * Extract the ramdisk image in a file
 */
size_t
extractRamdiskImage(FILE *fp, boot_img_hdr *hdr, const char *outdir, const char *basename)
{
  size_t readsz = 0;
  const char *filename = getImageFilename(basename, outdir, BOOTIMG_RAMDISK_FILENAME);
  FILE *r = fopen(filename, "wb");

  if (r)
    {
      byte* ramdisk = (byte*)malloc(hdr->ramdisk_size);
      if (ramdisk)
        {
          readsz = fread(ramdisk, hdr->ramdisk_size, 1, fp);
          if (readsz != 1)
            fprintf(stderr, "%s: error: expected %d bytes read but got %ld !\n",
                    progname, hdr->ramdisk_size, readsz * hdr->ramdisk_size);
          
          fwrite(ramdisk, hdr->ramdisk_size, 1, r);
          fclose(r);

          if (Fflag)
            {
              const char fsdir[PATH_MAX+1];

              if (Fval && Fval[0] != '/')
                sprintf((char *)fsdir, "%s/%s", outdir, Fval);
              else if (!Fval)
                memcpy((void *)fsdir, (void *)filename, strlen(filename)+1);
              else
            	memcpy((void *)fsdir, (void *)Fval, strlen(Fval)+1);
              if (rindex(fsdir, '.'))
                *(rindex(fsdir, '.')) = '\0';
              extractRamdiskFiles(fsdir, filename);
            }
          
          free((void *)filename);
        }
    }
  else
    fprintf(stderr,
            "%s: error: cannot open ramdisk image file '%s' for writing !\n",
            progname, filename);
  
  return(readsz * hdr->ramdisk_size);
}

/*
 * Extract the 2nd bootloader image in a file
 */
size_t
extractSecondBootloaderImage(FILE *fp, boot_img_hdr *hdr, const char *outdir, const char *basename)
{
  size_t readsz = 0;
  const char *filename = getImageFilename(basename, outdir, BOOTIMG_SECOND_LOADER_FILENAME);
  FILE *s = fopen(filename, "wb");

  if (s)
    {
      byte* second = (byte*)malloc(hdr->second_size);
      if (second)
        {
          readsz = fread(second, hdr->second_size, 1, fp);
          if (readsz != 1)
            fprintf(stderr, "%s: error: expected %d bytes read but got %ld !\n",
                    progname, hdr->second_size, readsz * hdr->second_size);
          
          fwrite(second, hdr->second_size, 1, s);
          fclose(s);
          free((void *)filename);
        }
    }
  else
    fprintf(stderr,
            "%s: error: cannot open second bootloader image file '%s' for writing !\n",
            progname, filename);
  
  return(readsz * hdr->second_size);
}

/*
 * Extract the DTB image in a file
 */
size_t
extractDeviceTreeImage(FILE *fp, boot_img_hdr *hdr, const char *outdir, const char *basename)
{
  size_t readsz = 0;
  const char *filename = getImageFilename(basename, outdir, BOOTIMG_DTB_FILENAME);
  FILE *d = fopen(filename, "wb");

  if (d)
    {
      byte* dtb = (byte*)malloc(hdr->dt_size);
      if (dtb)
        {
          readsz = fread(dtb, hdr->dt_size, 1, fp);
          if (readsz != 1)
            fprintf(stderr, "%s: error: expected %d bytes read but got %ld !\n",
                    progname, hdr->dt_size, readsz * hdr->dt_size);
          
          fwrite(dtb, hdr->dt_size, 1, d);
          fclose(d);
          free((void *)filename);
        }
    }
  else
    fprintf(stderr,
            "%s: error: cannot open device tree blob image file '%s' for writing !\n",
            progname, filename);
  
  return(readsz * hdr->dt_size);
}

/*
 * Apply the rewrite rules
 */
char *
rewrite(char *str, const char *rule)
{
  char command[MAX_COMMAND_LENGTH+1];
  FILE *fp = (FILE *)NULL;
  char *rewrote = (char *)NULL;

  bzero((void *)command, MAX_COMMAND_LENGTH+1);
  snprintf(command, MAX_COMMAND_LENGTH, "echo -n '%s' | sed -e '%s'", str, rule);
  if (vflag > 3)
    fprintf(stdout,
            "%s: command = '%s'\n",
            progname, command);
  if ((fp = popen(command, "r")) != NULL)
    {
      if (!(rewrote = (char *)malloc(BUF_LENGTH)))
        fprintf(stderr,
                "%s: error: cannot allocate buffer for rewriting !\n",
                progname);

      else
        {
          if (fgets(rewrote, BUF_LENGTH, fp) == NULL)
            {
              fprintf(stderr,
                      "%s: error: cannot read from pipe for rewrite !\n",
                      progname);
            }
          else
            {
              if (vflag)
                fprintf(stdout,
                        "%s: '%s' rewrote in '%s'\n",
                        progname, str, rewrote);
            }
        }
      pclose(fp);
    }
  
  return(rewrote);
}

/*
 * Apply rewrite rules on the filename of a file
 */
char *
rewriteFilename(const char *pathname)
{
  char *extension = (rindex(pathname, '.') ? strdup((char *)(rindex(pathname, '.')+1)) : strdup(""));
  char *pathname_2free = strdup(pathname);
  char *pathname_copy = pathname_2free;
  const char *dir_name = getDirname(pathname_copy, FLAG_GET_DIRNAME_ABSOLUTE);
  const char *base_name = getBasename(pathname, extension);
  char *file_name = (char *)NULL;
  char *path_name = (char *)NULL;

  assert(extension);
  assert(pathname_2free);

  if (!dir_name || !base_name)
    {
      fprintf(stderr, "%s: error: cannot allocate dirname/basename!\n", progname);

      if (dir_name)
        free((void *)dir_name);
      if (base_name)
        free((void *)base_name);
      return (char *)NULL;
    }

  if (rindex(base_name, '.'))
    *(rindex(base_name, '.')) = '\0';

  if (vflag > 2)
    fprintf(stdout,
            "%s: basename = <%s>\n",
            progname, base_name);

  if (brrflag)
    {
      base_name = rewrite((char *)base_name, basename_rr);
      if (!base_name)
        {
          fprintf(stderr, "%s: error: cannot rewrite basename!\n", progname);          
          free((void *)dir_name);
          return (char *)NULL;
        }
    }

  if (vflag > 2)
    fprintf(stdout,
            "%s: extension = <%s>\n",
            progname, extension);

  if (extension && errflag)
    {
      extension = rewrite(extension, extension_rr);
      if (!extension)
        {
          fprintf(stderr, "%s: error: cannot rewrite extension!\n", progname);
          
          free((void *)dir_name);
          free((void *)base_name);
          
          return (char *)NULL;
        }
    }

  int file_name_len = strlen(base_name) + (extension ? strlen(extension) + 2 : 0);
  file_name = (char *)malloc(file_name_len);
  if (!file_name)
    {
      fprintf(stderr, "%s: error: cannot allocate memory for filename!\n", progname);
      
      free((void *)base_name);
      free((void *)dir_name);
      if (extension)
        free((void *)extension);
      
      return (char *)NULL;
    }  
  snprintf(file_name, file_name_len,
           "%s%s%s",
           base_name, extension ? "." : "", extension ? extension : "");

  if (vflag > 2)
    fprintf(stdout,
            "%s: filename = <%s>\n",
            progname, file_name);
  
  free((void *)base_name);
  if (extension)
    free((void *)extension);

  if (frrflag)
    {
      file_name = rewrite(file_name, filename_rr);
      if (!file_name)
        {
          fprintf(stderr, "%s: error: cannot rewrite filename!\n", progname);
          free((void *)dir_name);
          return (char *)NULL;
        }
    }
  
  int path_name_len = strlen(dir_name) + strlen(file_name) + 2;
  path_name = (char *)malloc(path_name_len);
  if (!path_name)
    {
      fprintf(stderr, "%s: error: cannot allocate memory for pathname!\n", progname);
      free((void *)dir_name);
      return (char *)NULL;
    }
  snprintf(path_name, path_name_len,
           "%s/%s",
           dir_name, file_name);  
  free((void *)dir_name);
  if (vflag > 2)
    fprintf(stdout,
            "%s: pathname = <%s>\n",
            progname, path_name);
  if (prrflag)
    {
      path_name = rewrite(path_name, pathname_rr);
      if (!path_name)
        {
          fprintf(stderr, "%s: error: cannot rewrite pathname!\n", progname);          
          return (char *)NULL;
        }
    }  
  free((void *)pathname_2free);
  return (char *)path_name;
}

/*
 * Process an image file and extract metadata & images 
 */
int
extractBootImageMetadata(const char *imgfile, const char *outdir)
{
  int rc = 0;
  boot_img_hdr header, *hdr = (boot_img_hdr *)NULL;
  FILE *imgfp = (FILE *)NULL,
#ifdef USE_LIBXML2
       *xfp = (FILE *)NULL,
#endif
       *jfp = (FILE *)NULL;
  off_t offset = 0;
  size_t total_read = 0;
  const char *imgfilename;
  const char *baseName = (const char *)(nval ? nval : imgfile);
#ifdef USE_LIBXML2
  const char *xml_filename = (const char *)NULL, *json_filename = (const char *)NULL;
  xmlTextWriterPtr xmlWriter;
  xmlDocPtr xmlDoc;
#endif
  cJSON *jsonDoc;
  char tmp[PATH_MAX+1];
  
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
        fprintf(stdout,
                "%s: Magic found at offset %ld in file '%s'\n",
                progname, offset, imgfile);
      
      if (iflag)
        {
          fprintf(stdout,
                  "%s: Boot Image Identification:\n"
                  "%s  \t%02x%02x%02x%02x%02x%02x%02x%02x"
                  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                  "%02x%02x%02x%02x%02x%02x%02x%02x\t %s\n",
                  progname, blankname, 
                  (hdr->id[0]>>24)&0xFF, (hdr->id[0]>>16)&0xFF, (hdr->id[0]>>8)&0xFF, hdr->id[0]&0xFF,
                  (hdr->id[1]>>24)&0xFF, (hdr->id[1]>>16)&0xFF, (hdr->id[1]>>8)&0xFF, hdr->id[1]&0xFF,
                  (hdr->id[2]>>24)&0xFF, (hdr->id[2]>>16)&0xFF, (hdr->id[2]>>8)&0xFF, hdr->id[2]&0xFF,
                  (hdr->id[3]>>24)&0xFF, (hdr->id[3]>>16)&0xFF, (hdr->id[3]>>8)&0xFF, hdr->id[3]&0xFF,
                  (hdr->id[4]>>24)&0xFF, (hdr->id[4]>>16)&0xFF, (hdr->id[4]>>8)&0xFF, hdr->id[4]&0xFF,
                  (hdr->id[5]>>24)&0xFF, (hdr->id[5]>>16)&0xFF, (hdr->id[5]>>8)&0xFF, hdr->id[5]&0xFF,
                  (hdr->id[6]>>24)&0xFF, (hdr->id[6]>>16)&0xFF, (hdr->id[6]>>8)&0xFF, hdr->id[6]&0xFF,
                  (hdr->id[7]>>24)&0xFF, (hdr->id[7]>>16)&0xFF, (hdr->id[7]>>8)&0xFF, hdr->id[7]&0xFF,
                  imgfile);
        }
      
      if (Vflag)
	verityVerify(imgfp, hdr);

      base_addr = hdr->kernel_addr - kernel_offset;

      const char *tmpfname = getImageFilename(baseName, outdir, BOOTIMG_BOOTIMG_FILENAME);
      if (!(tmpfname = rewriteFilename(tmpfname)))
        fprintf(stderr,
                "%s: error: cannot create the xml writer !\n",
                progname);

      /* Ok let's go ... */
      else
        {
          fprintf(stdout,
                  "%s: boot image file name = '%s'!\n",
                  progname, tmpfname);
          
#ifdef USE_LIBXML2
          /* Create & start XML metadata file */
          if (xflag)
            do
              {
                int rc;
                
                xml_filename = getImageFilename(baseName, outdir, BOOTIMG_XML_FILENAME);
                xml_filename = rewriteFilename(xml_filename);
                if (!xml_filename)
                  break;
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
                rc = xmlTextWriterStartElement(xmlWriter, BOOTIMG_XMLELT_BOOTIMAGE_NAME);
                if (rc < 0)
                  {
                    fprintf(stderr, "%s: error: cannot start the bootImage root element !\n", progname);
                    xmlFreeTextWriter(xmlWriter);
                    free((void *)xml_filename);
                    xml_filename = (const char *)NULL;
                    break;
                  }
                
                if (tmpfname)
                  xmlTextWriterWriteAttribute(xmlWriter, BOOTIMG_XMLELT_BOOTIMAGEFILE_NAME, tmpfname);
              }
            while (0);
#endif
	  
          /* Create and start JSON metadata file */
          if (jflag)
            do
              {
                json_filename = getImageFilename(baseName, outdir, BOOTIMG_JSON_FILENAME);
                json_filename = rewriteFilename(json_filename);
                if (!json_filename)
                  break;
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
                if (tmpfname)
                  cJSON_AddItemToObject(jsonDoc, "bootImageFile", cJSON_CreateString(tmpfname));

                free((void *)tmpfname);
              }
            while(0);

          /* Process OS version value */
          if (hdr->os_version != 0)
            {
              int os_version, os_patch_level;
              int major, minor, micro;
              int year, month;
              char boardOsVersionStr[100], boardOsPatchLvlStr[100];
              
              os_version = hdr->os_version >> 11;
              os_patch_level = hdr->os_version&0x7ff;
              
              major = (os_version >> 14)&0x7f;
              minor = (os_version >> 7)&0x7f;
              micro = os_version&0x7f;
              if (vflag)
                fprintf(stdout, "%s: OS_VERSION %d.%d.%d\n", progname, major, minor, micro);
              
              year = (os_patch_level >> 4) + 2000;
              month = os_patch_level&0xf;
              if (vflag)
                fprintf(stdout, "%s: OS_PATCH_LEVEL %d-%02d\n", progname, year, month);
              
              sprintf(boardOsVersionStr, "%d.%d.%d", major, minor, micro);
              sprintf(boardOsPatchLvlStr, "%d-%02d", year, month);
              
#ifdef USE_LIBXML2
              /* If xml is requested */
              if (xflag)
                do
                  {
                    /* cmdLine */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_CMDLINE_NAME,
                                                        "%s", hdr->cmdline) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for cmdLine\n", progname);
                    
                    /* boardName */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_BOARDNAME_NAME,
                                                        "%s", hdr->name) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for boardName\n", progname);
                    
                    /* baseAddr */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_BASEADDR_NAME,
                                                        "0x%08lx", base_addr) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for baseAddr\n", progname);
                    
                    /* pageSize */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_PAGESIZE_NAME,
                                                        "%d", hdr->page_size) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for pageSize\n", progname);
                    
                    /* kernelOffset */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_KERNELOFFSET_NAME,
                                                        "0x%08lx", hdr->kernel_addr - base_addr) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for kernelOffset\n", progname);
                    
                    /* ramdiskOffset */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_RAMDISKOFFSET_NAME,
                                                        "0x%08lx", hdr->ramdisk_addr - base_addr) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for ramdiskOffset\n", progname);
                    
                    /* secondOffset */
                    if (hdr->second_size != 0 &&
                        xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_SECONDOFFSET_NAME,
                                                        "0x%08lx", hdr->second_addr - base_addr) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for secondOffset\n", progname);
                    
                    /* tagsOffset */
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_TAGSOFFSET_NAME,
                                                        "0x%08lx", hdr->tags_addr - base_addr) < 0)
                      fprintf(stderr, "%s: error: cannot create xml element for tagsOffset\n", progname);
                    
                    /* Add boardOsVersion element */
                    if (xmlTextWriterStartElement(xmlWriter, BOOTIMG_XMLELT_BOARDOSVERSION_NAME) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_VALUE_NAME,
                                                        "0x%x", os_version) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion value\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_MAJOR_NAME,
                                                        "%d", major) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion major\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_MINOR_NAME,
                                                        "%d", minor) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion minor\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_MICRO_NAME,
                                                        "%d", micro) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion micro\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_VALUESTR_NAME,
                                                        "%s", boardOsVersionStr) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsVersion valueStr\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_COMMENT_NAME,
                                                        "%s", BOARD_OS_VERSION_COMMENT) < 0)
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
                    if (xmlTextWriterStartElement(xmlWriter, BOOTIMG_XMLELT_BOARDOSPATCHLVL_NAME) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_VALUE_NAME,
                                                        "0x%x", os_patch_level) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl value\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_YEAR_NAME,
                                                        "%d", year) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl year\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_MONTH_NAME,
                                                        "%d", month) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl month\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_VALUESTR_NAME,
                                                        "%s", boardOsPatchLvlStr) < 0)
                      {
                        fprintf(stderr, "%s: error: cannot create xml element for boardOsPatchLvl valueStr\n", progname);
                        break;
                      }
                    if (xmlTextWriterWriteFormatElement(xmlWriter,
                                                        BOOTIMG_XMLELT_COMMENT_NAME,
                                                        "%s", BOARD_OS_PATCH_LEVEL_COMMENT) < 0)
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
#endif
            
              if (jflag)
                do
                  {
                    cJSON *boardOsVersion, *boardOsPatchLvl;
                    
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_CMDLINE_NAME, cJSON_CreateString(hdr->cmdline));
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_BOARDNAME_NAME, cJSON_CreateString(hdr->name));
                    sprintf(tmp, "0x%08lx", base_addr);
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_BASEADDR_NAME, cJSON_CreateString(tmp));
                    sprintf(tmp, "%d", hdr->page_size);
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_PAGESIZE_NAME, cJSON_CreateString(tmp));
                    sprintf(tmp, "0x%08lx", hdr->kernel_addr - base_addr);
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_KERNELOFFSET_NAME, cJSON_CreateString(tmp));
                    sprintf(tmp, "0x%08lx", hdr->ramdisk_addr - base_addr);
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_RAMDISKOFFSET_NAME, cJSON_CreateString(tmp));
                    if (hdr->second_size != 0)
                      {
                        sprintf(tmp, "0x%08lx", hdr->second_addr - base_addr);
                        cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_SECONDOFFSET_NAME, cJSON_CreateString(tmp));
                      }
                    sprintf(tmp, "0x%08lx", hdr->tags_addr - base_addr);
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_TAGSOFFSET_NAME, cJSON_CreateString(tmp));
                    
                    boardOsVersion = cJSON_CreateObject();
                    if (!boardOsVersion)
                      {
                        fprintf(stderr, "%s: error: cannot create json object for boardOsVersion", progname);
                        break;
                      }
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_VALUE_NAME,
                                          cJSON_CreateNumber(os_version));
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_MAJOR_NAME,
                                          cJSON_CreateNumber(major));
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_MINOR_NAME,
                                          cJSON_CreateNumber(minor));
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_MICRO_NAME,
                                          cJSON_CreateNumber(micro));
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_VALUESTR_NAME,
                                          cJSON_CreateString(boardOsVersionStr));
                    cJSON_AddItemToObject(boardOsVersion,
                                          BOOTIMG_XMLELT_COMMENT_NAME,
                                          cJSON_CreateString(BOARD_OS_VERSION_COMMENT));
                    
                    cJSON_AddItemToObject(jsonDoc,
                                          BOOTIMG_XMLELT_BOARDOSVERSION_NAME,
                                          boardOsVersion);
                    
                    boardOsPatchLvl = cJSON_CreateObject();
                    if (!boardOsPatchLvl)
                      {
                        fprintf(stderr, "%s: error: cannot create json object for boardOsPatchLvl\n", progname);
                        cJSON_Delete(boardOsPatchLvl);
                        break;
                      }
                    cJSON_AddItemToObject(boardOsPatchLvl,
                                          BOOTIMG_XMLELT_VALUE_NAME,
                                          cJSON_CreateNumber(os_patch_level));
                    cJSON_AddItemToObject(boardOsPatchLvl,
                                          BOOTIMG_XMLELT_YEAR_NAME,
                                          cJSON_CreateNumber(year));
                    cJSON_AddItemToObject(boardOsPatchLvl,
                                          BOOTIMG_XMLELT_MONTH_NAME,
                                          cJSON_CreateNumber(month));
                    cJSON_AddItemToObject(boardOsPatchLvl,
                                          BOOTIMG_XMLELT_VALUESTR_NAME,
                                          cJSON_CreateString(boardOsPatchLvlStr));
                    cJSON_AddItemToObject(boardOsPatchLvl,
                                          BOOTIMG_XMLELT_COMMENT_NAME,
                                          cJSON_CreateString(BOARD_OS_PATCH_LEVEL_COMMENT));
                    
                    cJSON_AddItemToObject(jsonDoc,
                                          BOOTIMG_XMLELT_BOARDOSPATCHLVL_NAME,
                                          boardOsPatchLvl);
                  }
                while(0);
            }         
          
          if (hdr->dt_size != 0)
            {
              fprintf(stdout,
                      "%s: BOARD_DT_SIZE %d\n",
                      progname, hdr->dt_size);
            }
          
          if (!pflag)
            {
              if (vflag)
                fprintf(stdout,
                        "%s: page size (%u) from image used\n",
                        progname, hdr->page_size);
              pval = hdr->page_size;
            }
          
          total_read += sizeof(header);
          total_read += readPadding(imgfp, sizeof(header), pval);
          
          size_t kernel_sz = extractKernelImage(imgfp, hdr, outdir, baseName);
          if (vflag && kernel_sz)
            fprintf(stdout,
                    "%s: %lu bytes kernel image extracted!\n",
                    progname, kernel_sz);
          
          total_read += hdr->kernel_size;
          total_read += readPadding(imgfp, hdr->kernel_size, pval);
          
          const char *tmpfname = getImageFilename(baseName, outdir, BOOTIMG_KERNEL_FILENAME);
          if ((tmpfname = rewriteFilename(tmpfname)))
            {
#ifdef USE_LIBXML2
              if (xflag)
                xmlTextWriterWriteFormatElement(xmlWriter, BOOTIMG_XMLELT_KERNELIMAGEFILE_NAME, "%s", tmpfname);
#endif
              if (jflag)
                cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_KERNELIMAGEFILE_NAME, cJSON_CreateString(tmpfname));
              free((void *)tmpfname);
            }
          
          size_t ramdisk_sz = extractRamdiskImage(imgfp, hdr, outdir, baseName);
          if (vflag && ramdisk_sz)
            fprintf(stdout,
                    "%s: %lu bytes ramdisk image extracted!\n",
                    progname, ramdisk_sz);
          
          tmpfname = getImageFilename(baseName, outdir, BOOTIMG_RAMDISK_FILENAME);
#ifdef USE_LIBXML2
          if (xflag)
            xmlTextWriterWriteFormatElement(xmlWriter, BOOTIMG_XMLELT_RAMDISKIMAGEFILE_NAME, "%s", tmpfname);
#endif
          if (jflag)
            cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_RAMDISKIMAGEFILE_NAME, cJSON_CreateString(tmpfname));
          free((void *)tmpfname);
          
          total_read += hdr->ramdisk_size;
          total_read += readPadding(imgfp, hdr->ramdisk_size, pval);
          
          if (hdr->second_size)
            {
              size_t second_sz = extractSecondBootloaderImage(imgfp, hdr, outdir, baseName);
              if (vflag && second_sz)
                fprintf(stdout,
                        "%s: %lu bytes second bootloader image extracted!\n",
                        progname, second_sz);
              
              tmpfname = getImageFilename(baseName, outdir, BOOTIMG_SECOND_LOADER_FILENAME);
#ifdef USE_LIBXML2
              if (xflag)
                xmlTextWriterWriteFormatElement(xmlWriter, "secondBootloaderImageFile", "%s", tmpfname);
#endif
              if (jflag)
                cJSON_AddItemToObject(jsonDoc, "secondBootloaderImageFile", cJSON_CreateString(tmpfname));
              free((void *)tmpfname);
              
              total_read += hdr->second_size;
            }
          
          total_read += readPadding(imgfp, hdr->second_size, pval);
          
          if (hdr->dt_size != 0)
            {
              size_t dtb_sz = extractDeviceTreeImage(imgfp, hdr, outdir, baseName);
              if (vflag && dtb_sz)
                fprintf(stdout,
                        "%s: %lu bytes device tree blob image extracted!\n",
                        progname, dtb_sz);
              
              tmpfname = getImageFilename(baseName, outdir, BOOTIMG_DTB_FILENAME);
              if ((tmpfname = rewriteFilename(tmpfname)))
                {
#ifdef USE_LIBXML2
                  if (xflag)
                    xmlTextWriterWriteFormatElement(xmlWriter, BOOTIMG_XMLELT_DTBIMAGEFILE_NAME, "%s", tmpfname);
#endif
                  if (jflag)
                    cJSON_AddItemToObject(jsonDoc, BOOTIMG_XMLELT_DTBIMAGEFILE_NAME, cJSON_CreateString(tmpfname));
                  free((void *)tmpfname);
                }
              
              total_read += hdr->dt_size;
            }
          
          if (vflag > 2)
            fprintf(stdout,
                    "%s: total read: %ld\n",
                    progname, total_read);
          
#ifdef USE_LIBXML2
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
#endif
          
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
          
          rc = 1;
        }

      fclose(imgfp);
    }
  else
    fprintf(stderr,
            "%s: error: Magic not found in file '%s'\n",
            progname, imgfile);
      
  return rc;
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
    fprintf(stderr,
            "%s: error: expected %lu bytes read, but got %lu\n",
            progname, sizeof(boot_img_hdr), rdsz * sizeof(boot_img_hdr));
  if (vflag > 1)
    {
      base = hdr->kernel_addr - 0x00008000;
      fprintf(stderr, "%s: KERNEL_CMDLINE %s\n", progname, hdr->cmdline);
      fprintf(stderr, "%s: KERNEL_BASE %08lx\n", progname, base);
      fprintf(stderr, "%s: NAME %s\n", progname, hdr->name);
      fprintf(stderr, "%s: PAGE_SIZE %d\n", progname, hdr->page_size);
      fprintf(stderr, "%s: KERNEL_OFFSET %08lx\n", progname, hdr->kernel_addr - base);
      fprintf(stderr, "%s: RAMDISK_OFFSET %08lx\n", progname, hdr->ramdisk_addr - base);
      if (hdr->second_size != 0)
        fprintf(stderr, "%s: SECOND_OFFSET %08lx\n", progname, hdr->second_addr - base);
      fprintf(stderr, "%s: TAGS_OFFSET %08lx\n", progname, hdr->tags_addr - base);
    }

  return hdr;
}

int
readPadding(FILE* f, unsigned itemsize, int pagesize)
{
  byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
  unsigned pagemask = pagesize - 1;
  unsigned count;

  assert(buf);  
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

void
extractRamdiskFiles(const char *fsdir, const char *ramdisk_image)
{
  do
    {
      char cpio_command[MAX_COMMAND_LENGTH+1];
      FILE *cpio_fp;
      char size_str[MAX_COMMAND_LENGTH+1];
      struct stat statbuf;
      
      const char *cwd = get_current_dir_name();
      snprintf(cpio_command,
	       MAX_COMMAND_LENGTH,
	       "bash -c \"(rm -rf %s >/dev/null 2>&1; mkdir -p %s >/dev/null 2>&1; cd %s >/dev/null 2>&1; zcat %s | cpio -i 2>&1)\"",
	       fsdir, fsdir, fsdir, ramdisk_image);
      if (vflag >= 3)
    	fprintf(stdout, "%s: pipe cpio command = '%s'\n", progname, cpio_command);
      if ((cpio_fp = popen(cpio_command, "r")) != NULL)
        {
          if (fgets(size_str, sizeof(size_str), cpio_fp) == NULL)
            fprintf(stderr,
                    "%s: error: cannot read from pipe for cpio command!\n",
                    progname);

          else
            if (vflag)
              fprintf(stdout,
                      "%s: ramdisk extracted: %s\n",
                      progname, size_str);
          pclose(cpio_fp);
        }
      free((void *)cwd);
    }
  while (0);
}

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */

