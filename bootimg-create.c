/* bootimg-tools/bootimg-extract.c
 *
 * Copyright 2007, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "Lvoid
 createBootImageFromJsonMetadata(const char *filename, const char *outdir)icense");
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
/*
 * TODO
 * Some tests still to be done for second and dtb images. Still to test with JSON.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <alloca.h>
#include <errno.h>
#include <error.h>

#include <openssl/sha.h>
#include <libxml/xmlreader.h>

#include "bootimg.h"
#include "cJSON.h"

#ifdef LIBXML_READER_ENABLED

#include "bootimg-priv.h"
#include "bootimg-utils.h"

#define BOOTIMG_SHA_CTX	SHA1_CTX
#define BOOTIMG_SHA_Init SHA1_Init
#define BOOTIMG_SHA_Update SHA1_Update
#define BOOTIMG_SHA_Final SHA1_Final

/*
 * Options flags & values
 * - v: verbose. vflag € N+*
 * - o: outdir. oflag € [0, 1]
 * - i: display image id. iflag € [0, 1]
 * - p: page size. pflag € [0, 1]
 * - f: force overwrite. fflag € [0, 1]
 */
int vflag = 0;
int oflag = 0;
int pflag = 0;
int fflag = 0;
int iflag = 0;

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
 * Usage string
 */
static const char *progusage =
  "usage: %s --verbose[=<lvl>] ...] [--force] [--xml] [--json] [--name=<basename>] [--outdir=<outdir>] [--pagesize=<pgsz>] metadatafile1 [metadatafile2 ...]\n"
  "       %s --help                                                                                         \n"
  "                                                                                                         \n"
  "       with the following options                                                                        \n"
  "       %s --force|-f          : force files overwrite.                                                   \n"
  "       %s --verbose|-v <lvl>  : be verbose at runtime. <lvl> is added to current verbosity level.        \n"
  "       %s                       If omited, one is assumed. More verbose flags imply more verbosity.      \n"
  "       %s --outdir|-o <outdir>: Save potentially big image files in the <outdir> directory               \n"
  "       %s                       Impacted images are kernel, ramdisk, second bootloader & dtb             \n"
  "       %s --pagesize|-p <pgsz>: Image page size. If not providedn, use the one specified in the file     \n"
  "       %s --identify|-i       : display the ID field for this boot image.                                \n"
  "       %s --help|-h           : display this message.                                                    \n"
  "       %s The metadata files are either xml or json files as created by bootimg-extract command.         \n";

/*
 * Long options struct
 */
struct option long_options[] = {
  {"verbose",  optional_argument, 0,  'v' },
  {"force",    no_argument,       0,  'f' },
  {"identify", no_argument,       0,  'i' },
  {"outdir",   required_argument, 0,  'o' },
  {"pagesize", required_argument, 0,  'p' },
  {"help",     no_argument,       0,  'h' },
  {0,          0,                 0,   0  }
};
#define BOOTIMG_OPTSTRING "v::fio:h:"
const char *unknown_option = "????";

/* padding buffer */
static unsigned char padding[0x20000] = { 0, };

/*
 * Getopt external defs
 */
extern char *optarg;
extern int optind;

/*
 * Forward decl
 */
static void *loadImage(const char *, size_t *);
void *my_malloc_fn(size_t);
void my_free_fn(void *);
int writeImage(bootimgParsingContext_p);
void printusage(void);
void createBootImageFromXmlMetadata(const char *, const char *);
void createBootImageFromJsonMetadata(const char *, const char *);
int writePaddingToFd(int, size_t, size_t);
void writeStringToFile(char *, char *);
void readerErrorFunc(void *, const char *, xmlParserSeverities, xmlTextReaderLocatorPtr);
int createBootImageProcessXmlNode(bootimgParsingContext_t *, xmlTextReaderPtr);
void createBootImageFromXmlMetadata(const char *, const char *);
void createBootImageFromJsonMetadata(const char *, const char *);

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

static void *
loadImage(const char *filename, size_t *sz_p)
{
  void *data = (void *)NULL;
  size_t sz = 0;
  int fd = -1;

  do
    {
      fd = open(filename, O_RDONLY);
      if (fd < 0)
        break;

      sz = lseek(fd, 0, SEEK_END);
      if (sz < 0)
        break;

      do
        {
          if (lseek(fd, 0, SEEK_SET) != 0)
            break;

          data = (void *)malloc(sz);
          if (data == (void *)NULL)
            break;

          if (read(fd, data, sz) != sz)
            break;

          close(fd);
          fd = -1;

          if (sz)
            *sz_p = sz;
          return data;
        }
      while (0);

      if (fd != -1)
        close(fd);

      if (data != (void *)NULL)
        free(data);

      data = (void *)NULL;
    }
  while (0);

  return data;
}

/*
 * compute a digest from images and images' sizes
 * then store it in id header field
 */
void
updateIdHeaderField(bootimgParsingContext_t *ctxt, data_context_t *dctxt)
{
  /* init sha512 context */
  BOOTIMG_SHA_CTX sha;
  (void)BOOTIMG_SHA_Init(&sha);

  /* start computation with kernel image */
  (void)BOOTIMG_SHA_Update(&sha,
                      dctxt->kernel_data,
                      ctxt->hdr.kernel_size);
  /* update with the kernel_size field */
  (void)BOOTIMG_SHA_Update(&sha,
                      (const void *)&ctxt->hdr.kernel_size,
                      sizeof(ctxt->hdr.kernel_size));
  /* add the ramdisk image */
  (void)BOOTIMG_SHA_Update(&sha,
                      dctxt->ramdisk_data,
                      ctxt->hdr.ramdisk_size);
  /* and its size field */
  (void)BOOTIMG_SHA_Update(&sha,
                      (const void *)&ctxt->hdr.ramdisk_size,
                      sizeof(ctxt->hdr.ramdisk_size));
  /* second loader image is available */
  if (dctxt->second_data)
    (void)BOOTIMG_SHA_Update(&sha,
                        dctxt->second_data,
                        ctxt->hdr.second_size);
  /* and its size field */
  (void)BOOTIMG_SHA_Update(&sha,
                      (const void *)&ctxt->hdr.second_size,
                      sizeof(ctxt->hdr.second_size));
  /* then the device tree blob image */
  if (dctxt->dtb_data)
    (void)BOOTIMG_SHA_Update(&sha,
                        dctxt->dtb_data,
                        ctxt->hdr.dt_size);
  /* and its size field */
  (void)BOOTIMG_SHA_Update(&sha,
                      (const void *)&ctxt->hdr.dt_size,
                      sizeof(ctxt->hdr.dt_size));
  /* get the digest in the id field of the header */
  (void)BOOTIMG_SHA_Final((unsigned char *)&ctxt->hdr.id, &sha);
}

/*
 * Copy values obtained from parsing to header struct
 */
void
setHeaderValuesFromParsingContext(bootimgParsingContext_p ctxt)
{
  ctxt->hdr.page_size = ctxt->pageSize;
  if (vflag > 2)
    {
      fprintf(stdout, "%s: hdr.page_size = 0x%x\n", progname, ctxt->hdr.page_size);
      fprintf(stdout, "%s: ctxt->kernelOffset = 0x%lx\n", progname, ctxt->kernelOffset);
    }

  ctxt->hdr.kernel_addr  = ctxt->baseAddr + ctxt->kernelOffset;
  if (vflag > 2)
    fprintf(stdout,
            "%s: hdr.kernel_addr = baseAddr (0x%lx) + kernelOffset (0x%lx) = 0x%x\n",
            progname, ctxt->baseAddr, ctxt->kernelOffset, ctxt->hdr.kernel_addr);
  ctxt->hdr.ramdisk_addr = ctxt->baseAddr + ctxt->ramdiskOffset;
  if (vflag > 2)
    fprintf(stdout,
            "%s: hdr.ramdisk_addr = baseAddr (0x%lx) + ramdiskOffset (0x%lx) = 0x%x\n",
            progname, ctxt->baseAddr, ctxt->ramdiskOffset, ctxt->hdr.ramdisk_addr);
  ctxt->hdr.second_addr  = ctxt->baseAddr + ctxt->secondOffset;
  if (vflag > 2)
    fprintf(stdout,
            "%s: hdr.second_addr = baseAddr (0x%lx) + secondOffset (0x%lx) = 0x%x\n",
            progname, ctxt->baseAddr, ctxt->secondOffset, ctxt->hdr.second_addr);
  ctxt->hdr.tags_addr    = ctxt->baseAddr + ctxt->tagsOffset;
  if (vflag > 2)
    fprintf(stdout,
            "%s: hdr.tags_addr = baseAddr (0x%lx) + tagsOffset (0x%lx) = 0x%x\n",
            progname, ctxt->baseAddr, ctxt->tagsOffset, ctxt->hdr.tags_addr);

  if (vflag > 2)
    fprintf(stdout,
            "%s: ctxt->osVersion = 0x%x  ctxt->osPatchLvl = 0x%x\n",
            progname, ctxt->osVersion, ctxt->osPatchLvl);
  ctxt->hdr.os_version = ((ctxt->osVersion & BOOTIMG_OSVERSION_MASK) << 11) |
    (ctxt->osPatchLvl & BOOTIMG_OSPATCHLVL_MASK);
  if (vflag > 2)
    {
      fprintf(stdout,
              "%s: ctxt->osVersion & 0x1ffff = 0x%x\n",
              progname, ctxt->osVersion & BOOTIMG_OSVERSION_MASK);
      fprintf(stdout,
              "%s: (ctxt->osVersion & 0x1ffff) << 11 = 0x%x\n",
              progname, (ctxt->osVersion & BOOTIMG_OSVERSION_MASK) << 11);
      fprintf(stdout,
              "%s: ctxt->osPatchLvl & 0x7FF = 0x%x\n",
              progname, ctxt->osPatchLvl & BOOTIMG_OSPATCHLVL_MASK);
      fprintf(stdout,
              "%s: ctxt->hdr.os_version = ((ctxt->osVersion & 0x1ffff) << 11) | (ctxt->osPatchLvl&0x7ff) = 0x%x\n",
              progname, ((ctxt->osVersion & BOOTIMG_OSVERSION_MASK) << 11) | (ctxt->osPatchLvl & BOOTIMG_OSPATCHLVL_MASK));
      fprintf(stdout,
              "%s: ctxt->hdr.os_version = 0x%x\n",
              progname, ctxt->hdr.os_version);
    }

  if (strlen(ctxt->cmdLine) > BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE)
    {
      /* truncate */
      strncpy(ctxt->hdr.cmdline, ctxt->cmdLine, BOOT_ARGS_SIZE);
      strncpy(ctxt->hdr.extra_cmdline, &ctxt->cmdLine[BOOT_ARGS_SIZE], BOOT_EXTRA_ARGS_SIZE);
      fprintf(stderr, "%s: WARNING: command line arguments was truncated to %d characters!\n",
              progname, BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE);
    }
  if (BOOT_ARGS_SIZE < strlen(ctxt->cmdLine) && strlen(ctxt->cmdLine) < BOOT_EXTRA_ARGS_SIZE)
    {
      strncpy(ctxt->hdr.cmdline, ctxt->cmdLine, BOOT_ARGS_SIZE);
      strncpy(ctxt->hdr.extra_cmdline, &ctxt->cmdLine[BOOT_ARGS_SIZE], BOOTIMG_MIN(BOOT_EXTRA_ARGS_SIZE, strlen(ctxt->cmdLine) - BOOT_ARGS_SIZE));
    }
  else
    strncpy(ctxt->hdr.cmdline, ctxt->cmdLine, BOOTIMG_MIN(BOOT_ARGS_SIZE, strlen(ctxt->cmdLine)));

  // Set product name
  strncpy(ctxt->hdr.name, ctxt->boardName, BOOTIMG_MIN(strlen(ctxt->boardName), BOOT_NAME_SIZE));
}

/*
 * Load images, compute last hdr fields and write boot image
 */
int
writeImage(bootimgParsingContext_p ctxt)
{
  int rc = -1;
  data_context_t data_ctxt, *dctxt = &data_ctxt;

  do
    {
      /* init data context */
      bzero((void *)dctxt, sizeof(data_context_t));

      /* Report header data from parsing context to header struct */
      setHeaderValuesFromParsingContext(ctxt);

      /* load the kernel image */
      dctxt->kernel_data = loadImage(ctxt->kernelImageFile,
                                     (size_t *)&ctxt->hdr.kernel_size);
      if (!dctxt->kernel_data)
        {
          fprintf(stderr,
                  "%s: error: couldn't load kernel image file at '%s'\n",
                  progname,
                  ctxt->kernelImageFile);
          break;
        }

      /* load the ramdisk image */
      dctxt->ramdisk_data = loadImage(ctxt->ramdiskImageFile,
                                      (size_t *)&ctxt->hdr.ramdisk_size);
      if (!dctxt->ramdisk_data)
        {
          fprintf(stderr,
                  "%s: error: couldn't load ramdisk image file at '%s'\n",
                  progname,
                  ctxt->ramdiskImageFile);
          break;
        }

      /* load the second bootloader image if one is available */
      if (ctxt->secondImageFile)
        {
          dctxt->second_data = loadImage(ctxt->secondImageFile,
                                         (size_t *)&ctxt->hdr.second_size);
          if (!dctxt->second_data)
            {
              fprintf(stderr,
                      "%s: error: couldn't load second loader image file at '%s'\n",
                      progname,
                      ctxt->secondImageFile);
              break;
            }
        }

      /* load the device tree blob image if one is available */
      if (ctxt->dtbImageFile)
        {
          dctxt->dtb_data = loadImage(ctxt->dtbImageFile,
                                      (size_t *)&ctxt->hdr.dt_size);
          if (!dctxt->dtb_data)
            {
              fprintf(stderr,
                      "%s: error: couldn't load device tree blob image file at '%s'\n",
                      progname,
                      ctxt->dtbImageFile);
              break;
            }
        }

      /* update boot image id */
      updateIdHeaderField(ctxt, dctxt);

      /* open image file for writing */
      int fd = open(ctxt->bootImageFile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
      if (fd < 0)
        {
          perror(progname);
          fprintf(stderr,
                  "%s: error: cannot open image file '%s' for writing\n",
                  progname,
                  ctxt->bootImageFile);
          break;
        }

      /*
       * Headerval
       */
      /* write the header data */
      size_t wrsz = 0;
      if ((wrsz = write(fd, &ctxt->hdr, sizeof (ctxt->hdr))) != sizeof (ctxt->hdr))
        {
          fprintf(stderr,
                  "%s: error: expected %lu header bytes written but got only %lu!\n",
                  progname,
                  sizeof (ctxt->hdr),
                  wrsz);
          break;
        }
      /* pad to the next page boundary */
      if (writePaddingToFd(fd, ctxt->hdr.page_size, sizeof(ctxt->hdr)) == -1)
        {
          fprintf(stderr,
                  "%s: error: failed to pad boot image header!\n",
                  progname);
          break;
        }

      /*
       * Kernel image
       */
      if ((wrsz = write(fd, dctxt->kernel_data, ctxt->hdr.kernel_size)) != ctxt->hdr.kernel_size)
        {
          fprintf(stderr,
                  "%s: error: expected %lu kernel bytes written but got only %lu!\n",
                  progname,
                  sizeof (ctxt->hdr.kernel_size),
                  wrsz);
          break;
        }
      /* pad to the next page boundary */
      if (writePaddingToFd(fd, ctxt->hdr.page_size, ctxt->hdr.kernel_size) == -1)
        {
          fprintf(stderr,
                  "%s: error: failed to pad kernel image!\n",
                  progname);
          break;
        }

      /*
       * Ramdisk image
       */
      if ((wrsz = write(fd, dctxt->ramdisk_data, ctxt->hdr.ramdisk_size)) != ctxt->hdr.ramdisk_size)
        {
          fprintf(stderr,
                  "%s: error: expected %lu ramdisk bytes written but got only %lu!\n",
                  progname,
                  sizeof (ctxt->hdr.ramdisk_size),
                  wrsz);
          break;
        }
      /* pad to the next page boundary */
      if (writePaddingToFd(fd, ctxt->hdr.page_size, ctxt->hdr.ramdisk_size) == -1)
        {
          fprintf(stderr,
                  "%s: error: failed to pad ramdisk image!\n",
                  progname);
          break;
        }

      /*
       * Second boot loader image
       */
      if (dctxt->second_data)
        {
          if ((wrsz = write(fd, dctxt->second_data, ctxt->hdr.second_size)) != ctxt->hdr.second_size)
            {
              fprintf(stderr,
                      "%s: error: expected %lu second bytes written but got only %lu!\n",
                      progname,
                      sizeof (ctxt->hdr.second_size),
                      wrsz);
              break;
            }
          /* pad to the next page boundary */
          if (writePaddingToFd(fd, ctxt->hdr.page_size, ctxt->hdr.second_size) == -1)
            {
              fprintf(stderr,
                      "%s: error: failed to pad second image!\n",
                      progname);
              break;
            }
        }

      /*
       * device tree blob image
       */
      if (dctxt->dtb_data)
        {
          if ((wrsz = write(fd, dctxt->dtb_data, ctxt->hdr.dt_size)) != ctxt->hdr.dt_size)
            {
              fprintf(stderr,
                      "%s: error: expected %lu device tree blob image bytes written but got only %lu!\n",
                      progname,
                      sizeof (ctxt->hdr.dt_size),
                      wrsz);
              break;
            }
          /* pad to the next page boundary */
          if (writePaddingToFd(fd, ctxt->hdr.dt_size, ctxt->hdr.dt_size) == -1)
            {
              fprintf(stderr,
                      "%s: error: failed to pad device tree blob image!\n",
                      progname);
              break;
            }
        }

      if (iflag)
        {
          fprintf(stdout,
		  "%s: Boot Image Identification:\n%s  %02x%02x%02x%02x%02x%02x%02x%02x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		  "%02x%02x%02x%02x%02x%02x%02x%02x\t %s\n",
		  progname, blankname,
                  (ctxt->hdr.id[0]>>24)&0xFF, (ctxt->hdr.id[0]>>16)&0xFF,
		  (ctxt->hdr.id[0]>>8)&0xFF, ctxt->hdr.id[0]&0xFF,
                  (ctxt->hdr.id[1]>>24)&0xFF, (ctxt->hdr.id[1]>>16)&0xFF,
		  (ctxt->hdr.id[1]>>8)&0xFF, ctxt->hdr.id[1]&0xFF,
                  (ctxt->hdr.id[2]>>24)&0xFF, (ctxt->hdr.id[2]>>16)&0xFF,
		  (ctxt->hdr.id[2]>>8)&0xFF, ctxt->hdr.id[2]&0xFF,
                  (ctxt->hdr.id[3]>>24)&0xFF, (ctxt->hdr.id[3]>>16)&0xFF,
		  (ctxt->hdr.id[3]>>8)&0xFF, ctxt->hdr.id[3]&0xFF,
                  (ctxt->hdr.id[4]>>24)&0xFF, (ctxt->hdr.id[4]>>16)&0xFF,
		  (ctxt->hdr.id[4]>>8)&0xFF, ctxt->hdr.id[4]&0xFF,
                  (ctxt->hdr.id[5]>>24)&0xFF, (ctxt->hdr.id[5]>>16)&0xFF,
		  (ctxt->hdr.id[5]>>8)&0xFF, ctxt->hdr.id[5]&0xFF,
                  (ctxt->hdr.id[6]>>24)&0xFF, (ctxt->hdr.id[6]>>16)&0xFF,
		  (ctxt->hdr.id[6]>>8)&0xFF, ctxt->hdr.id[6]&0xFF,
                  (ctxt->hdr.id[7]>>24)&0xFF, (ctxt->hdr.id[7]>>16)&0xFF,
		  (ctxt->hdr.id[7]>>8)&0xFF, ctxt->hdr.id[7]&0xFF,
                  ctxt->bootImageFile);
        }

      /*
       * TODO
       * Manage extra_cmdline
       */
      rc = 0;
    }
  while (0);

  return rc;
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
                    progname, getLongOptionName(long_options, c), c, vflag);
          break;

        case 'f':
          fflag = 1;
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
                      progname, getLongOptionName(long_options, c), c, oflag);
          }
          break;
          
        case 'i':
          iflag = 1;
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set\n",
                    progname, getLongOptionName(long_options, c), c, iflag);
          break;
          
        case 'p':
          pflag = 1;
          pval = strtol(optarg, NULL, 10);
          if (vflag > 3)
            fprintf(stderr, "%s: option %s/%c (=%d) set with value '%lu'\n",
                    progname, getLongOptionName(long_options, c), c, pflag, pval);
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

  if (optind < argc)
    {
      if (optind != argc -1 && !fflag)
        {
          fprintf(stderr,
                  "%s: warning: You provided several image to create to the same image file ?",
                  progname);
          fprintf(stderr,
                  "%s: warning: Stop processing as the force flag was not provided",
                  progname);
        }
      
      while (optind < argc)
        {
          /* 
           * Create image from metadata file according to its type
           */
          /* Ptr arithm: check filename ends with .xml */
          if (strstr(argv[optind], ".xml") == (argv[optind] + strlen(argv[optind]) - 4))
            createBootImageFromXmlMetadata(argv[optind++], oval);

          else if (strstr(argv[optind], ".json") == (argv[optind] + strlen(argv[optind]) - 5))
            createBootImageFromJsonMetadata(argv[optind++], oval);
        }

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

/*
 * write padding in a file until page border
 */
int
writePaddingToFd(int fd, size_t pagesize, size_t itemsize)
{
  unsigned pagemask = pagesize - 1;
  ssize_t count;

  /* padding unneeded */
  if ((itemsize & pagemask) == 0)
    return 0;

  /* compute size to page border */
  count = pagesize - (itemsize & pagemask);

  /* if write fails return error */
  if (write(fd, padding, count) != count)
    return -1;
  else
    return 0;
}

/*
 * Write a C string in a file without the '\0'
 */
void
writeStringToFile(char* file, char* string)
{
  FILE* f = fopen(file, "w");
  if (f)
    {
      fwrite(string, strlen(string), 1, f);
      fwrite("\n", 1, 1, f);
      fclose(f);
    }
}

/*
 * Print usage message
 */
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

void readerErrorFunc(void *arg,
                     const char *msg,
                     xmlParserSeverities severity,
                     xmlTextReaderLocatorPtr locator)
{
  bootimgParsingContext_p ctxt = (bootimgParsingContext_p)arg;
  char severityStr[100];
  
  switch (severity)
    {
    case XML_PARSER_SEVERITY_VALIDITY_WARNING:
      memcpy((void *)severityStr, (const void *)"validity warning", sizeof("validity warning"));
      break;
    case XML_PARSER_SEVERITY_VALIDITY_ERROR:
      memcpy((void *)severityStr, (const void *)"validity error", sizeof("validity error"));
      break;
    case XML_PARSER_SEVERITY_WARNING:
      memcpy((void *)severityStr, (const void *)"warning", sizeof("warning"));
      break;
    case XML_PARSER_SEVERITY_ERROR:
      memcpy((void *)severityStr, (const void *)"error", sizeof("error"));
      break;
    default:
      memcpy((void *)severityStr, (const void *)"unknown severity", sizeof("unknown severity"));
      break;
    }
  fprintf(stderr,
          "%s: XML %s: line #%d: %s\n",
          progname,
          severityStr,
          xmlTextReaderLocatorLineNumber(locator),
          msg);
}

/*
 * createBootImageFromXmlMetadata
 */
int 
createBootImageProcessXmlNode(bootimgParsingContext_t *ctxt, xmlTextReaderPtr xmlReader)
{
  int rc = 1, pc = 1;
  xmlChar *localName = xmlTextReaderLocalName(xmlReader);

  if (vflag)
    fprintf(stdout,
            "%s: localName = '%s'\n",
            progname,
            localName);

  do
    {
      /* sanity check on element name */
      if (!localName)
        {
          fprintf(stderr, "%s: error: cannot allocate element local name\n", progname);
          break;
        }

      /* process #text nodes */
      if (!memcmp((void *)localName,
                  (const void *)BOOTIMG_XMLTYPE_TEXT_NAME,
                  (size_t)sizeof(BOOTIMG_XMLTYPE_TEXT_NAME)-1))
        {
          if (ELEMENT_OPENED(bootImage))
            {
              if (ELEMENT_OPENED(cmdLine))
                {
                  ProcessXmlText4String(cmdLine, BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE);
                }
              else if (ELEMENT_OPENED(boardName))
                {
                  ProcessXmlText4String(boardName, BOOT_NAME_SIZE);
                }
              else if (ELEMENT_OPENED(baseAddr))
                {
                  ProcessXmlText4Number(baseAddr);
                }
              else if (ELEMENT_OPENED(pageSize))
                {
                  ProcessXmlText4Number(pageSize);
                }
              else if (ELEMENT_OPENED(kernelOffset))
                {
                  ProcessXmlText4Number(kernelOffset);
                }
              else if (ELEMENT_OPENED(ramdiskOffset))
                {
                  ProcessXmlText4Number(ramdiskOffset);
                }
              else if (ELEMENT_OPENED(secondOffset))
                {
                  ProcessXmlText4Number(secondOffset);
                }
              else if (ELEMENT_OPENED(tagsOffset))
                {
                  ProcessXmlText4Number(tagsOffset);
                }
              else if (ELEMENT_OPENED(boardOsVersion))
                {
                  if (ELEMENT_OPENED(value))
                    {
                      xmlChar *osVersionStr = xmlTextReaderReadString(xmlReader);
                      if (!osVersionStr)
                        {
                          fprintf(stderr,
                                  "%s: error: cannot get osVersion text value at %d:%d\n",
                                  progname,
                                  xmlTextReaderGetParserColumnNumber(xmlReader),
                                  xmlTextReaderGetParserLineNumber(xmlReader));
                          pc = -1;
                          break;
                        }
                      errno = 0;
                      ctxt->osVersion = strtoul((const char *)osVersionStr, (char **)NULL, 0);
                      if (errno)
                        {
                          perror("invalid osVersion format");
                          fprintf(stderr,
                                  "%s: error: couldn't convert osVersion value at (%d:%d)\n",
                                  progname,
                                  xmlTextReaderGetParserColumnNumber(xmlReader),
                                  xmlTextReaderGetParserLineNumber(xmlReader));
                          pc = -1;
                        }
                      else
                        fprintf(stdout,
                                "%s: osVersion = %lu(0x%08lx)\n",
                                progname,
                                (unsigned long int)ctxt->osVersion,
                                (unsigned long int)ctxt->osVersion);
                      free((void *)osVersionStr);
                    }
                  else if (ELEMENT_OPENED(major))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(minor))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(micro))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(valueStr))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(comment))
                    {
                      /* discarded */
                    }
                }
              else if (ELEMENT_OPENED(boardOsPatchLvl))
                {
                  if (ELEMENT_OPENED(value))
                    {
                      xmlChar *osPatchLvlStr = xmlTextReaderReadString(xmlReader);
                      if (!osPatchLvlStr)
                        {
                          fprintf(stderr,
                                  "%s: error: cannot get osPatchLvl text value at %d:%d\n",
                                  progname,
                                  xmlTextReaderGetParserColumnNumber(xmlReader),
                                  xmlTextReaderGetParserLineNumber(xmlReader));
                          pc = -1;
                          break;
                        }
                      errno = 0;
                      ctxt->osPatchLvl = strtoul((const char *)osPatchLvlStr, (char **)NULL, 0);
                      if (errno)
                        {
                          perror("invalid osPatchLvl format");
                          fprintf(stderr,
                                  "%s: error: coultn't convert osPatchLvl value at (%d:%d)\n",
                                  progname,
                                  xmlTextReaderGetParserColumnNumber(xmlReader),
                                  xmlTextReaderGetParserLineNumber(xmlReader));
                          pc = -1;
                          break;
                        }
                      else
                        fprintf(stdout,
                                "%s: osPatchLvl = %lu(0x%08lx)\n",
                                progname,
                                (unsigned long int)ctxt->osPatchLvl,
                                (unsigned long int)ctxt->osPatchLvl);
                      free((void *)osPatchLvlStr);
                    }
                  else if (ELEMENT_OPENED(year))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(month))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(valueStr))
                    {
                      /* discarded */
                    }
                  else if (ELEMENT_OPENED(comment))
                    {
                      /* discarded */
                    }
                }             
              else if (ELEMENT_OPENED(kernelImageFile))
                {
                  ProcessXmlText4String(kernelImageFile, PATH_MAX);
                }
              else if (ELEMENT_OPENED(ramdiskImageFile))
                {
                  ProcessXmlText4String(ramdiskImageFile, PATH_MAX);
                }
              else if (ELEMENT_OPENED(secondImageFile))
                {
                  ProcessXmlText4String(secondImageFile, PATH_MAX);
                }
              else if (ELEMENT_OPENED(dtbImageFile))
                {
                  ProcessXmlText4String(dtbImageFile, PATH_MAX);
                }
            }
        }
      
      /* process cmdLine root element */
      if (IS_ELEMENT(BOOTIMAGE))
        {
          if (ELEMENT_CLOSED(bootImage))
            {
              if (!xmlTextReaderHasAttributes(xmlReader))
                {
                  fprintf(stderr,
                          "%s: error: bootImageFile attribute is not available on bootImage element!\n",
                          progname);
                  rc = -1;
                  break;
                }
              else
                {
                  if (vflag >= 3)
                    fprintf(stdout,
                            "%s: bootImage has %d attribute(s)\n",
                            progname,
                            xmlTextReaderAttributeCount(xmlReader));
                  ctxt->bootImageFile = xmlTextReaderGetAttribute(xmlReader, "bootImageFile");
                  if (vflag)
                    fprintf(stdout,
                            "%s: bootImageFile = '%s'\n",
                            progname,
                            ctxt->bootImageFile);
                }
            }
          else if (ELEMENT_OPENED(bootImage))
            {
              /* Create bootImage */
              if (vflag)
                fprintf(stdout, "%s: bootImage element parsed\n", progname);
            }
          ELEMENT_INCR(bootImage);
        }

      /* process bootImage */
      if (IS_ELEMENT(CMDLINE))
        ELEMENT_INCR(cmdLine);

      /* process boardName */
      if (IS_ELEMENT(BOARDNAME))
        ELEMENT_INCR(boardName);

      /* process baseAddr */
      if (IS_ELEMENT(BASEADDR))
        ELEMENT_INCR(baseAddr);

      /* process pageSize */
      if (IS_ELEMENT(PAGESIZE))
        ELEMENT_INCR(pageSize);

      /* process kernelOffset */
      if (IS_ELEMENT(KERNELOFFSET))
        ELEMENT_INCR(kernelOffset);

      /* process ramdiskOffset */
      if (IS_ELEMENT(RAMDISKOFFSET))
        ELEMENT_INCR(ramdiskOffset);

      /* process secondOffset */
      if (IS_ELEMENT(SECONDOFFSET))
        ELEMENT_INCR(secondOffset);

      /* process tagsOffset */
      if (IS_ELEMENT(TAGSOFFSET))
        ELEMENT_INCR(tagsOffset);

      /* process boardOsVersion */
      if (IS_ELEMENT(BOARDOSVERSION))
        ELEMENT_INCR(boardOsVersion);

      /* process value */
      if (IS_ELEMENT(VALUE))
        ELEMENT_INCR(value);

      /* process major */
      if (IS_ELEMENT(MAJOR))
        ELEMENT_INCR(major);

      /* process minor */
      if (IS_ELEMENT(MINOR))
        ELEMENT_INCR(minor);

      /* process micro */
      if (IS_ELEMENT(MICRO))
        ELEMENT_INCR(micro);

      /* process valueStr */
      if (IS_ELEMENT(VALUESTR))
        ELEMENT_INCR(valueStr);

      /* process comment */
      if (IS_ELEMENT(COMMENT))
        ELEMENT_INCR(comment);

      /* process boardOsPatchLvl */
      if (IS_ELEMENT(BOARDOSPATCHLVL))
        ELEMENT_INCR(boardOsPatchLvl);

      /* process year */
      if (IS_ELEMENT(YEAR))
        ELEMENT_INCR(year);

      /* process month */
      if (IS_ELEMENT(MONTH))
        ELEMENT_INCR(month);

      /* process kernelImageFile */
      if (IS_ELEMENT(KERNELIMAGEFILE))
        ELEMENT_INCR(kernelImageFile);

      /* process ramdiskImageFile */
      if (IS_ELEMENT(RAMDISKIMAGEFILE))
        ELEMENT_INCR(ramdiskImageFile);

      /* process secondImageFile */
      if (IS_ELEMENT(SECONDIMAGEFILE))
        ELEMENT_INCR(secondImageFile);

      /* process dtbImageFile */
      if (IS_ELEMENT(DTBIMAGEFILE))
        ELEMENT_INCR(dtbImageFile);
    }
  while (0);
  
  free((void *)localName);
  return rc;
}

void
releaseContextContent(bootimgParsingContext_t *ctxt)
{
  /* release ctxt */
  if (ctxt->boardName)
    free((void *)ctxt->boardName);
  ctxt->boardName = NULL;
  if (ctxt->bootImageFile)
    free((void *)ctxt->bootImageFile);
  ctxt->bootImageFile = NULL;
  if (ctxt->cmdLine)
    free((void *)ctxt->cmdLine);
  ctxt->cmdLine = NULL;
  if (ctxt->dtbImageFile)
    free((void *)ctxt->dtbImageFile);
  ctxt->dtbImageFile = NULL;
  if (ctxt->kernelImageFile)
    free((void *)ctxt->kernelImageFile);
  ctxt->kernelImageFile = NULL;
  if (ctxt->ramdiskImageFile)
    free((void *)ctxt->ramdiskImageFile);
  ctxt->ramdiskImageFile = NULL;
  if (ctxt->secondImageFile)
    free((void *)ctxt->secondImageFile);
  ctxt->secondImageFile = NULL;
  bzero((void *)ctxt, sizeof(bootimgParsingContext_t));
}

/*
 * createBootImageFromXmlMetadata
 */
void 
createBootImageFromXmlMetadata(const char *filename, const char *outdir)
{
  xmlTextReaderPtr xmlReader;
  xmlDocPtr xmlDoc;
  bootimgParsingContext_t ctxt;
  boot_img_hdr *hdr = (boot_img_hdr *)NULL;

  bzero((void *)&ctxt, sizeof(bootimgParsingContext_t));

  ctxt.pageSize = BOOTIMG_DEFAULT_PAGESIZE;
  ctxt.baseAddr = BOOTIMG_DEFAULT_BASEADDR;

  xmlReader = xmlReaderForFile(filename, NULL, 0);
  if (xmlReader == (xmlTextReaderPtr)NULL)
    fprintf(stderr, "%s: error: cannot create xml reader for '%s'!\n", progname, filename);

  else
    {
      int rc, pc;

      xmlTextReaderSetErrorHandler(xmlReader, readerErrorFunc, (void *)&ctxt);

      /* init header struct */
      hdr = initBootImgHeader(&ctxt.hdr);
      
      /* Parse document */
      do
        {
          rc = xmlTextReaderRead(xmlReader);
          if (rc == 1)
            pc = createBootImageProcessXmlNode(&ctxt, xmlReader);
        }
      while (rc == 1 && pc == 1);

      /* An error was detected by parser */
      if (rc < 0)
        fprintf(stderr,
                "%s: error: Parsing Failed! Malformed XML file at %d:%d!\n",
                progname,
                xmlTextReaderGetParserColumnNumber(xmlReader),
                xmlTextReaderGetParserLineNumber(xmlReader));

      /* An inconsistency in values was detected */
      if (pc < 0)
        fprintf(stderr,
                "%s: error: Parsing Failed! Inconsistent XML metadata at %d:%d!\n",
                progname,
                xmlTextReaderGetParserColumnNumber(xmlReader),
                xmlTextReaderGetParserLineNumber(xmlReader));
      
      /* Cleanup xml parser */
      xmlDoc = xmlTextReaderCurrentDoc(xmlReader);
      if (xmlDoc != (xmlDocPtr)NULL)
        xmlFreeDoc(xmlDoc);

      xmlFreeTextReader(xmlReader);

      /* xml file was successfully parsed: create image */
      if (rc == 0 && pc == 1)
        {
          if (writeImage(&ctxt) < 0)
            fprintf(stderr,
                    "%s: error: couldn't write image file at '%s'\n",
                    progname,
                    ctxt.bootImageFile);
          else if (vflag)
            fprintf(stdout,
                    "%s: image '%s' written!\n",
                    progname,
                    ctxt.bootImageFile);
        }

      releaseContextContent(&ctxt);
    }
}

int
processJsonDoc(cJSON *jsonDoc, bootimgParsingContext_t *ctxt)
{
  int rc = -1;

  do
    {
      cJSON *jsonItem = (cJSON *)NULL;

      /* First get the boot.img file pathname */
      ProcessJsonObjectItem4String(bootImageFile);
                      
      /* read cmdLine */
      ProcessJsonObjectItem4String(cmdLine);
                      
      /* read boardName */
      ProcessJsonObjectItem4String(boardName);
                      
      /* read baseAddr */
      ProcessJsonObjectItem4Number(baseAddr, size_t);
                      
      /* read pageSize */
      ProcessJsonObjectItem4Number(pageSize, size_t);
                      
      /* read kernelOffset */
      ProcessJsonObjectItem4Number(kernelOffset, off_t);
                      
      /* read ramdiskOffset */
      ProcessJsonObjectItem4Number(ramdiskOffset, off_t);
                      
      /* read secondOffset */
      do
        {
          ProcessJsonObjectItem4Number(secondOffset, off_t);
        }
      while (0);
                      
      /* read tagsOffset */
      ProcessJsonObjectItem4Number(tagsOffset, off_t);
                      
      /* read boardOsVersion Object */
      jsonItem = cJSON_GetObjectItem(jsonDoc,
                                     BOOTIMG_XMLELT_BOARDOSVERSION_NAME);
      if (jsonItem == (cJSON *)NULL)
        {
          fprintf(stderr,
                  "%s: error: cannot access json item for boardOsVersion !\n",
                  progname);
          break;
        }

      cJSON *jsonChildItem = cJSON_GetObjectItem(jsonItem,
                                                 BOOTIMG_XMLELT_VALUE_NAME);
      if (jsonChildItem == (cJSON *)NULL)
        {
          fprintf(stderr,
                  "%s: error: cannot access json item for boardOsVersion value !\n",
                  progname);
          break;
        }

      ctxt->osVersion = (uint32_t)jsonChildItem->valueint;

      /* read boardOsPatchLvl Object */
      jsonItem = cJSON_GetObjectItem(jsonDoc,
                                     BOOTIMG_XMLELT_BOARDOSPATCHLVL_NAME);
      if (jsonItem == (cJSON *)NULL)
        {
          fprintf(stderr,
                  "%s: error: cannot access json item for boardOsPatchLvl !\n",
                  progname);
          break;
        }

      jsonChildItem = cJSON_GetObjectItem(jsonItem,
                                          BOOTIMG_XMLELT_VALUE_NAME);
      if (jsonChildItem == (cJSON *)NULL)
        {
          fprintf(stderr,
                  "%s: error: cannot access json item for boardOsPatchLvl value !\n",
                  progname);
          break;
        }

      ctxt->osPatchLvl = (uint32_t)jsonChildItem->valueint;

      /* read kernelImageFile */
      ProcessJsonObjectItem4String(kernelImageFile);

      /* read ramdiskImageFile */
      ProcessJsonObjectItem4String(ramdiskImageFile);
                      
      /* read secondImageFile */
      do
        {
          ProcessJsonObjectItem4String(secondImageFile);
        }
      while (0);
                      
      /* read dtbImageFile */
      do
        {
          ProcessJsonObjectItem4String(dtbImageFile);
        }
      while (0);

      rc = 0;
    }
  while (0);

  return rc;
}

/*
 * createBootImageFromJsonMetadata
 */
void 
createBootImageFromJsonMetadata(const char *filename, const char *outdir)
{
  FILE *jfp = NULL;
  const char *pathname = filename;

  jfp = fopen(pathname, "rb");
  if (jfp == NULL)
    fprintf(stderr, "%s: error: cannot open json file '%s' for reading!\n", progname, pathname);

  else
    {
      size_t json_sz;
      char *buf = (char *)NULL;

      /* Get size of json file */
      if (fseek(jfp, 0, SEEK_END) == -1)
        {
          /* Failed! cleanup */
          fprintf(stderr,
                  "%s: error: cannot seek at end of file '%s'!\n",
                  progname, pathname);
          fclose(jfp);
        }
      else
        {
          json_sz = ftell(jfp);

          /* Alloc mem for storing json data */
          buf = (char *)malloc(json_sz * sizeof(char));
          if (buf == (char *)NULL)
            {
              /* Failed! cleanup */
              fprintf(stderr,
                      "%s: error: cannot allocate %ld bytes for reading '%s'!\n",
                      progname, json_sz, pathname);
              fclose(jfp);
            }

          else
            {
              /* Ok. Read data ... */
              rewind(jfp);
              size_t rdsz = fread((void *)buf, json_sz, 1, jfp);
              if (rdsz != 1)
                {
                  /* Failed! cleanup */
                  fprintf(stderr,
                          "%s: error: expected '%lu' bytes but read '%lu' bytes from '%s'!\n",
                          progname, json_sz, json_sz * rdsz, pathname);
                  free((void *)buf);
                  fclose(jfp);
                }

              else
                {
                  cJSON *jsonBootImageFile = (cJSON *)NULL;
                  FILE *imgfp = (FILE *)NULL;
                  bootimgParsingContext_t ctxt;
                  boot_img_hdr *hdr = (boot_img_hdr *)NULL;

                  /* zero parsing context */
                  bzero((void *)&ctxt, sizeof(bootimgParsingContext_t));

                  /* init some fields */
                  ctxt.pageSize = BOOTIMG_DEFAULT_PAGESIZE;
                  ctxt.baseAddr = BOOTIMG_DEFAULT_BASEADDR;
 
                  /* init header struct */
                  hdr = initBootImgHeader(&ctxt.hdr);

                  /* Cleanup some resources we don't need anymore */
                  fclose(jfp);

                  /* Ok. Parse json data in a json doc for usage. */
                  cJSON *jsonDoc = cJSON_Parse(buf);
                  if (!jsonDoc)
                    fprintf(stderr,
                            "%s: error: couldn't parse json document '%s'\n",
                            progname,
                            pathname);

                  else
                    do
                      {
                        if (processJsonDoc(jsonDoc, &ctxt))
                          {
                            fprintf(stderr,
                                    "%s: error: couldn't read data from json document '%s'\n",
                                    progname,
                                    pathname);
                            cJSON_Delete(jsonDoc);
                            break;
                          }

                        /* Delete json doc */
                        cJSON_Delete(jsonDoc);
                      
                        /* then write image file from ctxt */
                        if (writeImage(&ctxt) < 0)
                          fprintf(stderr,
                                  "%s: error: couldn't write image file at '%s'\n",
                                  progname,
                                  ctxt.bootImageFile);
                        else if (vflag)
                          fprintf(stdout,
                                  "%s: image '%s' written!\n",
                                  progname,
                                  ctxt.bootImageFile);
                      }
                    while (0);

                  /* Release json data buffer memory */
                  free((void *)buf);

                  /*
                   * cleanup
                   */
                  releaseContextContent(&ctxt);
                  
                  if (imgfp)
                    fclose(imgfp);
                  imgfp = (FILE *)NULL;
                }
            }
        }
    }
}

#else
# error Cannot build with your libxml2 library that does not have xmlReader
#endif /* LIBXML_READER_ENABLED */

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */
