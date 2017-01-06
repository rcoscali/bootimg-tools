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
#include <unistd.h>
#include <getopt.h>
#include <alloca.h>
#include <errno.h>

#include <libxml/xmlreader.h>

#include "bootimg.h"
#include "cJSON.h"

#ifdef LIBXML_READER_ENABLED

#include "bootimg-priv.h"
#include "bootimg-utils.h"

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
int fflag = 0;

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
 * Local images filenames that will be aggregated
 */
xmlChar *bootImageFile = (xmlChar *)NULL;
xmlChar *ramdiskImageFile = (xmlChar *)NULL;
xmlChar *secondImageFile = (xmlChar *)NULL;
xmlChar *dtbImageFile = (xmlChar *)NULL;

/*
 * Kernel offset is 0x8000
 * then base addr is calculated with kernel_addr - offset;
 */
off_t kernel_offset = 0x00008000;
size_t base_addr = 0; 

static const char *progusage =
  "usage: %s --verbose[=<lvl>] ...] [--force] [--xml] [--json] [--name=<basename>] [--outdir=<outdir>] [--pagesize=<pgsz>] imgfile1 [imgfile2 ...]\n"
  "       %s --help                                                                                         \n"
  "                                                                                                         \n"
  "       with the following options                                                                        \n"
  "       %s --force|-f          : force files overwrite.                                                   \n"
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
  {"force",    no_argument,       0,  'f' },
  {"outdir",   required_argument, 0,  'o' },
  {"name",     required_argument, 0,  'n' },
  {"xml",      no_argument,       0,  'x' },
  {"json",     no_argument,       0,  'j' },
  {"pagesize", required_argument, 0,  'p' },
  {"help",     no_argument,       0,  'h' },
  {0,          0,                 0,   0  }
};
#define BOOTIMG_OPTSTRING "v::fo:n:xjh:"
const char *unknown_option = "????";

/*
 * Getopt external defs
 */
extern char *optarg;
extern int optind, opterr, optopt;

/*
 * errno
 */
extern error_t errno;

/*
 * Forward decl
 */
void printusage(void);
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

  if (xflag && jflag)
    {
      fprintf(stderr, "%s: error: options x and j are mutually exclusive !\n", progname);
      printusage();
      exit(1);
    }
  
  if (vflag > 3)
    {
      fprintf(stderr, "%s: optind = %d\n", progname, optind);
      fprintf(stderr, "%s: argc = %d\n", progname, argc);
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

          if (strstr(argv[optind], ".json") == (argv[optind] + strlen(argv[optind]) - 5))
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
createBootImageProcessXmlNode(bootimgXmlParsingContext_t *ctxt, xmlTextReaderPtr xmlReader)
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
                  ProcessXmlText4String(cmdLine, BOOT_ARGS_SIZE);
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
		      if (osVersionStr[0] == '0' && (osVersionStr[1] | 0x80) == 'x')
			ctxt->osVersion = strtol((const char *)osVersionStr, (char **)NULL, 16);
		      else if (osVersionStr[0] == 'b')
			ctxt->osVersion = strtol((const char *)osVersionStr, (char **)NULL, 2);
		      else if (osVersionStr[0] == '0')
			ctxt->osVersion = strtol((const char *)osVersionStr, (char **)NULL, 8);
		      else
			ctxt->osVersion = strtol((const char *)osVersionStr, (char **)NULL, 10);
		      if (errno)
			{
			  perror("invalid osVersion format");
			  fprintf(stderr,
				  "%s: error: invalid osVersion format %d:%d\n",
				  progname,
				  xmlTextReaderGetParserColumnNumber(xmlReader),
				  xmlTextReaderGetParserLineNumber(xmlReader));
			  pc = -1;
			  break;
			}
		      free((void *)osVersionStr);
                    }
                  else if (ELEMENT_OPENED(major))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(minor))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(micro))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(valueStr))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(comment))
                    {
		      /* */
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
		      if (osPatchLvlStr[0] == '0' && (osPatchLvlStr[1] | 0x80) == 'x')
			ctxt->osPatchLvl = strtol((const char *)osPatchLvlStr, (char **)NULL, 16);
		      else if (osPatchLvlStr[0] == 'b')
			ctxt->osPatchLvl = strtol((const char *)osPatchLvlStr, (char **)NULL, 2);
		      else if (osPatchLvlStr[0] == '0')
			ctxt->osPatchLvl = strtol((const char *)osPatchLvlStr, (char **)NULL, 8);
		      else
			ctxt->osPatchLvl = strtol((const char *)osPatchLvlStr, (char **)NULL, 10);
		      if (errno)
			{
			  perror("invalid osPatchLvl format");
			  fprintf(stderr,
				  "%s: error: invalid osPatchLvl format %d:%d\n",
				  progname,
				  xmlTextReaderGetParserColumnNumber(xmlReader),
				  xmlTextReaderGetParserLineNumber(xmlReader));
			  pc = -1;
			  break;
			}
		      free((void *)osPatchLvlStr);
                    }
                  else if (ELEMENT_OPENED(year))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(month))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(valueStr))
                    {
		      /* */
                    }
                  else if (ELEMENT_OPENED(comment))
                    {
		      /* */
                    }
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
      
      /* process bootImage root element */
      if (IS_ELEMENT(CMDLINE))
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
                  break;
                }
            }
          if (ELEMENT_OPENED(bootImage))
            {
              /* Create bootImage */
              if (vflag)
                fprintf(stdout, "%s: bootImage element parsed\n", progname);
            }
          ELEMENT_INCR(bootImage);
        }

      /* process cmdLine */
      if (IS_ELEMENT(BOOTIMAGE))
        ELEMENT_INCR(bootImage);
      
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

/*
 * createBootImageFromXmlMetadata
 */
void 
createBootImageFromXmlMetadata(const char *filename, const char *outdir)
{
  xmlTextReaderPtr xmlReader;
  xmlDocPtr xmlDoc;
  bootimgXmlParsingContext_t ctxt;
  boot_img_hdr *hdr = (boot_img_hdr *)NULL;

  bzero((void *)&ctxt, sizeof(bootimgXmlParsingContext_t));

  xmlReader = xmlReaderForFile(filename, NULL, 0);
  if (xmlReader == (xmlTextReaderPtr)NULL)
    fprintf(stderr, "%s: error: cannot create xml reader for '%s'!\n", progname, filename);

  else
    {
      int rc, pc;

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

      if (rc < 0)
        fprintf(stderr,
                "%s: error: Parsing Failed! Malformed XML file at %d:%d!\n",
                progname,
                xmlTextReaderGetParserColumnNumber(xmlReader),
                xmlTextReaderGetParserLineNumber(xmlReader));
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
    }
}

/*
 * createBootImageFromJsonMetadata
 */
void 
createBootImageFromJsonMetadata(const char *filename, const char *outdir)
{
  FILE *jfp = NULL;
  const char *pathname = getImageFilename(outdir, filename, BOOTIMG_JSON_FILENAME);

  jfp = fopen(pathname, "rb");
  if (jfp == NULL)
    fprintf(stderr, "%s: error: cannot open json file '%s' for reading!\n", progname, pathname);

  else
    {
      size_t json_sz;
      char *buf = (char *)NULL;

      /* Get size of json file */
      rewind(jfp);
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
              const char *bootImageFile = (const char *)NULL;
              FILE *imgfp = (FILE *)NULL;
              boot_img_hdr header, *hdr;

              /* Cleanup some resources we don't need anymore */
              fclose(jfp);                        

              /* init header struct */
              hdr = initBootImgHeader(&header);
              
              /* Ok. Parse json data in a json doc for usage. */
              cJSON *jsonDoc = cJSON_Parse(buf);

              /* Release json data buffer memory */
              free((void *)buf);

              do
                {
                  /* First get the boot.img file pathname */
                  cJSON *jsonBootImageFile = cJSON_GetObjectItem(jsonDoc, "bootImageFile");
                  const char *bootImageFile = strdup(jsonBootImageFile->valuestring);
                  if (bootImageFile == (char *)NULL)
                    {
                      fprintf(stderr,
                              "%s: error: cannot allocate memory for boot image file pathname!\n",
                              progname);
                      break;
                    }
                  cJSON_Delete(jsonBootImageFile);
                  jsonBootImageFile = (cJSON *)NULL;

                  /*
                   * TODO
                   * get all fields from json doc and set them in header struct
                   */
                  
                  /* Open image file */
                  imgfp = fopen(bootImageFile, "wb");
                  if (imgfp = (FILE *)NULL)
                    {
                      /* Failed! cleanup */
                      fprintf(stderr,
                              "%s: error: cannot open boot image file '%s' for writing!\n",
                              progname, bootImageFile);
                      break;
                    }

                  size_t wrsz = fwrite(hdr, sizeof(boot_img_hdr), 1, imgfp);
                  if (wrsz != 1)
                    {
                      fprintf(stderr,
                              "%s: error: expected %lu bytes, wrote %lu: cannot write boot image to file '%s'!\n",
                              progname, sizeof(boot_img_hdr), wrsz * sizeof(boot_img_hdr), bootImageFile);
                      break;
                    }

                  if (fflush(imgfp))
                    {
                      fprintf(stderr,
                              "%s: error: couldn't flush boot image file!\n",
                              progname);
                      break;
                    }
                }
              while (0);

              /* 
               * cleanup 
               */
              if (imgfp)
                fclose(imgfp);
              imgfp = (FILE *)NULL;

              if (bootImageFile)
                free((void *)bootImageFile);
              bootImageFile = (char *)NULL;

              if (jsonBootImageFile)
                cJSON_Delete(jsonBootImageFile);
              jsonBootImageFile = (cJSON *)NULL;
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
