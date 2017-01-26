/* bootimg-tools/bootimg-priv.h
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

#ifndef __BOOTIMG_PRIV_H__
#define __BOOTIMG_PRIV_H__

#ifdef USE_OPENSSL
# ifndef OPENSSL_NO_SHA256
#  include <openssl/asn1.h>
#  include <openssl/asn1t.h>
#  include <openssl/x509.h>
# else
#  error No SHA256 available in this openssl ! It is mandatory ... 
# endif
#endif

#include <libxml/xmlstring.h>

/* Defaults for addresses */
#define BOOTIMG_DEFAULT_BASEADDR        0x10000000UL
#define BOOTIMG_DEFAULT_PAGESIZE        0x1000UL
#define BOOTIMG_DEFAULT_KERNEL_OFFSET   0x00008000UL
#define BOOTIMG_DEFAULT_RAMDISK_OFFSET  0x1000000UL
#define BOOTIMG_DEFAULT_SECOND_OFFSET   0xf00000UL
#define BOOTIMG_DEFAULT_TAGS_OFFSET     0x100UL

/* OS Version masks */
#define BOOTIMG_OSVERSION_MASK 0x1ffff
#define BOOTIMG_OSPATCHLVL_MASK 0x7ff

/* XML/JSON default encoding */
#define BOOTIMG_ENCODING "ISO-8859-1"

#define BOOTIMG_BOOTIMG_FILENAME 0
#define BOOTIMG_XML_FILENAME 1
#define BOOTIMG_JSON_FILENAME 2
#define BOOTIMG_KERNEL_FILENAME 3
#define BOOTIMG_RAMDISK_FILENAME 4
#define BOOTIMG_SECOND_LOADER_FILENAME 5
#define BOOTIMG_DTB_FILENAME 6

/* XML local name for #text nodes */
#define BOOTIMG_XMLTYPE_TEXT_NAME            	BAD_CAST"#text"

/* XML element local names */
#define BOOTIMG_XMLELT_BOOTIMAGE_NAME        	BAD_CAST"bootImage"
#define BOOTIMG_XMLELT_BOOTIMAGEFILE_NAME    	BAD_CAST"bootImageFile"
#define BOOTIMG_XMLELT_CMDLINE_NAME          	BAD_CAST"cmdLine"
#define BOOTIMG_XMLELT_BOARDNAME_NAME        	BAD_CAST"boardName"
#define BOOTIMG_XMLELT_BASEADDR_NAME         	BAD_CAST"baseAddr"
#define BOOTIMG_XMLELT_PAGESIZE_NAME         	BAD_CAST"pageSize"
#define BOOTIMG_XMLELT_KERNELOFFSET_NAME     	BAD_CAST"kernelOffset"
#define BOOTIMG_XMLELT_RAMDISKOFFSET_NAME    	BAD_CAST"ramdiskOffset"
#define BOOTIMG_XMLELT_SECONDOFFSET_NAME     	BAD_CAST"secondOffset"
#define BOOTIMG_XMLELT_TAGSOFFSET_NAME       	BAD_CAST"tagsOffset"
#define BOOTIMG_XMLELT_BOARDOSVERSION_NAME   	BAD_CAST"boardOsVersion"
#define BOOTIMG_XMLELT_VALUE_NAME            	BAD_CAST"value"
#define BOOTIMG_XMLELT_MAJOR_NAME            	BAD_CAST"major"
#define BOOTIMG_XMLELT_MINOR_NAME            	BAD_CAST"minor"
#define BOOTIMG_XMLELT_MICRO_NAME            	BAD_CAST"micro"
#define BOOTIMG_XMLELT_VALUESTR_NAME         	BAD_CAST"valueStr"
#define BOOTIMG_XMLELT_COMMENT_NAME          	BAD_CAST"comment"
#define BOOTIMG_XMLELT_BOARDOSPATCHLVL_NAME  	BAD_CAST"boardOsPatchLvl"
#define BOOTIMG_XMLELT_YEAR_NAME             	BAD_CAST"year"
#define BOOTIMG_XMLELT_MONTH_NAME            	BAD_CAST"month"
#define BOOTIMG_XMLELT_KERNELIMAGEFILE_NAME  	BAD_CAST"kernelImageFile"
#define BOOTIMG_XMLELT_RAMDISKIMAGEFILE_NAME 	BAD_CAST"ramdiskImageFile"
#define BOOTIMG_XMLELT_SECONDIMAGEFILE_NAME  	BAD_CAST"secondImageFile"
#define BOOTIMG_XMLELT_DTBIMAGEFILE_NAME     	BAD_CAST"dtbImageFile"

#define ELEMENT_FLAG_UNDEFINED 			0
#define ELEMENT_FLAG_OPENED 			1
#define ELEMENT_FLAG_CLOSED 			2

#define FLAG_GET_DIRNAME_NONE			0x00
#define FLAG_GET_DIRNAME_ABSOLUTE		0x01

#define BOOTIMG_MIN(x,y) ((x) < (y) ? (x) : (y))
#define BOOTIMG_MAX(x,y) ((x) > (y) ? (x) : (y))

#define MAX_COMMAND_LENGTH 1024

#define FLAG4MEMBER(x, t)                       \
  int x##Flag;                                  \
  t x;

#define FLAG4VOID(x)                            \
  int x##Flag;

#define ELEMENT_INCR(x)                         \
  {                                             \
    if (ctxt->x##Flag == ELEMENT_FLAG_CLOSED)   \
      ctxt->x##Flag = ELEMENT_FLAG_OPENED;      \
    else                                        \
      ctxt->x##Flag++;                          \
    break;                                      \
  }

#define ELEMENT_CLOSED(x)               ctxt->x##Flag == ELEMENT_FLAG_UNDEFINED || \
    ctxt->x##Flag == ELEMENT_FLAG_CLOSED

#define ELEMENT_UNDEFINED(x)            ctxt->x##Flag == ELEMENT_FLAG_UNDEFINED || \
    ctxt->x##Flag == ELEMENT_FLAG_CLOSED

#define ELEMENT_OPENED(x)               ctxt->x##Flag == ELEMENT_FLAG_OPENED

#define IS_ELEMENT(x)                                                   \
  !memcmp((void *)localName,                                            \
          (const void *)BOOTIMG_XMLELT_##x##_NAME,                      \
          (size_t)sizeof(BOOTIMG_XMLELT_##x##_NAME)-1) &&               \
  strlen(localName) == strlen(BOOTIMG_XMLELT_##x##_NAME)

#define ProcessXmlText4String(x, l)                                     \
  xmlChar *x##Str = xmlTextReaderReadString(xmlReader);                 \
  if (vflag > 2)                                                        \
    fprintf(stdout,                                                     \
            "%s: string read from xml = '%s'\n",                        \
            progname, x##Str);                                          \
  if (!x##Str)                                                          \
    {                                                                   \
      fprintf(stderr,                                                   \
              "%s: cannot get "#x" text value at %d:%d\n",              \
              progname,                                                 \
              xmlTextReaderGetParserColumnNumber(xmlReader),            \
              xmlTextReaderGetParserLineNumber(xmlReader));             \
      pc = -1;                                                          \
      break;                                                            \
    }                                                                   \
                                                                        \
  if (strlen(x##Str) > l)                                               \
    fprintf(stderr,                                                     \
            "%s: WARNING: "#x" too long (%d)! Truncated to %d bytes!\n", \
            progname,                                                   \
            (int)strlen(x##Str),                                        \
            (int)l);                                                    \
  if (!(ctxt->x = (xmlChar *)malloc(BOOTIMG_MIN(l, strlen(x##Str)))))   \
    fprintf(stderr,                                                     \
            "%s: error: cannot allocate memory for storing "#x"!\n",    \
            progname);                                                  \
  else                                                                  \
    {                                                                   \
      memcpy((void *)ctxt->x, x##Str, BOOTIMG_MIN(l, strlen(x##Str)));  \
      ctxt->x[strlen(x##Str)] = 0;                                      \
      if (vflag)                                                        \
        fprintf(stdout,                                                 \
                "%s: "#x" = '%s'\n",                                    \
                progname,                                               \
                (char *)ctxt->x);                                       \
    }                                                                   \
  free((void *)x##Str)

#define ProcessXmlText4Number(x)                                        \
  xmlChar *x##Str = xmlTextReaderReadString(xmlReader);                 \
  if (vflag > 2)                                                        \
    fprintf(stdout,                                                     \
            "%s: string read from xml = '%s'\n",                        \
            progname, x##Str);                                          \
  if (!x##Str)                                                          \
    {                                                                   \
      fprintf(stderr,                                                   \
              "%s: error: cannot get "#x" text value at %d:%d\n",       \
              progname,                                                 \
              xmlTextReaderGetParserColumnNumber(xmlReader),            \
              xmlTextReaderGetParserLineNumber(xmlReader));             \
      pc = -1;                                                          \
      break;                                                            \
    }                                                                   \
  errno = 0;                                                            \
  ctxt->x = strtoul((const char *)x##Str, (char **)NULL, 0);            \
  if (errno)                                                            \
    {                                                                   \
      perror("invalid "#x" format");                                    \
      fprintf(stderr,                                                   \
              "%s: error: invalid "#x" format %d:%d\n",                 \
              progname,                                                 \
              xmlTextReaderGetParserColumnNumber(xmlReader),            \
              xmlTextReaderGetParserLineNumber(xmlReader));             \
      pc = -1;                                                          \
    }                                                                   \
  else                                                                  \
    fprintf(stdout,                                                     \
            "%s: "#x" = %lu(0x%08lx)\n",                                \
            progname,                                                   \
            (unsigned long int)ctxt->x,                                 \
            (unsigned long int)ctxt->x);                                \
  free((void *)x##Str)

#define ProcessJsonObjectItem4String(x)                                 \
  jsonItem = cJSON_GetObjectItem(jsonDoc, #x);                          \
  if (jsonItem == (cJSON *)NULL)                                        \
    {                                                                   \
      fprintf(stderr,                                                   \
              "%s: error: cannot access json item for "#x" !\n",        \
              progname);                                                \
      break;                                                            \
    }                                                                   \
  ctxt->x = (xmlChar *)strdup(jsonItem->valuestring);                   \
  if (ctxt->x == (xmlChar *)NULL)                                       \
    {                                                                   \
      fprintf(stderr,                                                   \
              "%s: error: cannot allocate memory for "#x" !\n",         \
              progname);                                                \
      break;                                                            \
    }
    
#define ProcessJsonObjectItem4Number(x, t)                              \
  jsonItem = cJSON_GetObjectItem(jsonDoc, #x);                          \
  if (jsonItem == (cJSON *)NULL)                                        \
    {                                                                   \
      fprintf(stderr,                                                   \
              "%s: error: cannot access json item for "#x" !\n",        \
              progname);                                                \
      break;                                                            \
    }                                                                   \
  ctxt->x = (t)strtol(jsonItem->valuestring, NULL, 0);
    
typedef struct _bootimgParsingContext_st bootimgParsingContext_t;
typedef struct _bootimgParsingContext_st *bootimgParsingContext_p;

struct _bootimgParsingContext_st
{
  /* Boot image Header */
  boot_img_hdr hdr;

  /* Image file name */
  xmlChar *bootImageFile;

  /* XML parsing context */
  FLAG4VOID(bootImage);
  FLAG4MEMBER(cmdLine, xmlChar *);
  FLAG4MEMBER(boardName, xmlChar *);
  FLAG4MEMBER(baseAddr, size_t);
  FLAG4MEMBER(pageSize, size_t);
  FLAG4MEMBER(kernelOffset, off_t);
  FLAG4MEMBER(ramdiskOffset, off_t);
  FLAG4MEMBER(secondOffset, off_t);
  FLAG4MEMBER(tagsOffset, off_t);
  FLAG4VOID(boardOsVersion);
  uint32_t osVersion;
  FLAG4VOID(value);
  FLAG4VOID(major);
  FLAG4VOID(minor);
  FLAG4VOID(micro);
  FLAG4VOID(valueStr);
  FLAG4VOID(comment);
  FLAG4VOID(boardOsPatchLvl);
  uint32_t osPatchLvl;
  //FLAG4VOID(value);       Already available
  FLAG4VOID(year);
  FLAG4VOID(month);
  //FLAG4VOID(valueStr);    Already available
  //FLAG4VOID(comment);     Already available
  FLAG4MEMBER(kernelImageFile, xmlChar *);
  FLAG4MEMBER(ramdiskImageFile, xmlChar *);
  FLAG4MEMBER(secondImageFile, xmlChar *);
  FLAG4MEMBER(dtbImageFile, xmlChar *);
};

typedef struct _data_context_st data_context_t;
typedef struct _data_context_st *data_context_p;

struct _data_context_st
{
  void *kernel_data, *ramdisk_data, *second_data, *dtb_data;
};

typedef struct {
    ASN1_STRING *target;
    ASN1_INTEGER *length;
} AuthAttrs;

#ifdef __DO_IMPLEM_ASN1_AUTH_ATTRS__
ASN1_SEQUENCE(AuthAttrs) = {
    ASN1_SIMPLE(AuthAttrs, target, ASN1_PRINTABLE),
    ASN1_SIMPLE(AuthAttrs, length, ASN1_INTEGER)
} ASN1_SEQUENCE_END(AuthAttrs)

/*
 * Add implem for AuthAttrs related funcs
 */
IMPLEMENT_ASN1_FUNCTIONS(AuthAttrs);
#endif

typedef struct {
    ASN1_INTEGER *formatVersion;
    X509 *certificate;
    X509_ALGOR *algorithmIdentifier;
    AuthAttrs *authenticatedAttributes;
    ASN1_OCTET_STRING *signature;
} BootSignature;

#ifdef __DO_IMPLEM_ASN1_BOOT_SIGNATURE__
ASN1_SEQUENCE(BootSignature) = {
    ASN1_SIMPLE(BootSignature, formatVersion, ASN1_INTEGER),
    ASN1_SIMPLE(BootSignature, certificate, X509),
    ASN1_SIMPLE(BootSignature, algorithmIdentifier, X509_ALGOR),
    ASN1_SIMPLE(BootSignature, authenticatedAttributes, AuthAttrs),
    ASN1_SIMPLE(BootSignature, signature, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(BootSignature)

/*
 * Add implem for BootSignature related funcs
 */
IMPLEMENT_ASN1_FUNCTIONS(BootSignature);
#endif

#endif /* __BOOTIMG_PRIV_H__ */

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */
