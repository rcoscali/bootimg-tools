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
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
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
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include <libgen.h>

#include "bootimg.h"

// trigger implem for asn1 funcs
#define __DO_IMPLEM_ASN1_AUTH_ATTRS__
#define __DO_IMPLEM_ASN1_BOOT_SIGNATURE__
#include "bootimg-priv.h"
#undef __DO_IMPLEM_ASN1_AUTH_ATTRS__
#undef __DO_IMPLEM_ASN1_BOOT_SIGNATURE__

#ifdef USE_OPENSSL
# ifndef OPENSSL_NO_SHA256
#  include <openssl/sha.h>
#  include <openssl/crypto.h>
#  include <openssl/err.h>
#  include <openssl/evp.h>
#  include <openssl/rsa.h>
#  include <openssl/x509.h>
# else
#  error No SHA256 available in this openssl ! It is mandatory ... 
# endif
#endif

#define VERITY_FORMAT_VERSION  	1
#define SHA_BUF_SIZE_K		128

/* External decls */
extern int vflag;
extern char *progname;

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

/*
 * Align on page boundary
 */
uint64_t
alignOnPage(uint64_t size, uint64_t page_size)
{
  return ((size + page_size -1) & ~(page_size - 1));
}

/* 
 * Compute signature block offset in image file
 */
off64_t
computeSignatureBlockOffset(struct boot_img_hdr *hdr)
{
  uint64_t imgSz = hdr->page_size +			/* Header */
    alignOnPage(hdr->kernel_size, hdr->page_size) +	/* Kernel img */
    alignOnPage(hdr->ramdisk_size, hdr->page_size) +	/* Ramdisk img */
    alignOnPage(hdr->second_size,  hdr->page_size) + 	/* Second boot_loader img */
    alignOnPage(hdr->dt_size, hdr->page_size);		/* DTB img */
  return (off64_t)alignOnPage(imgSz, hdr->page_size);		/* Next page */
}

/*
 * Read signature block from image file
 */
int
readSignatureBlock(int imgfd, off64_t block_offset, BootSignature **bs)
{
  int ret = -1;
  BIO *in = NULL;

  do
    {
      if (!bs || lseek64(imgfd, block_offset, SEEK_SET) == -1) break;
      if ((in = BIO_new_fd(imgfd, BIO_NOCLOSE)) == NULL)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      if ((*bs = ASN1_item_d2i_bio(ASN1_ITEM_rptr(BootSignature), in, bs)) == NULL)
	{
	  ERR_print_errors_fp(stderr);
	  BIO_free(in);
	  break;
	}
      ret = 0;
      BIO_free(in);
    }
  while (0);

  return ret;
}

/*
 * Verify signature block consistency
 */
int
checkSignatureBlockConsistency(uint64_t imglen, const BootSignature *bs)
{
  int ret = -1;
  BIGNUM wanted_value;
  BIGNUM read_value;
  
  BN_init(&wanted_value);
  BN_init(&read_value);
  
  do
    {
      if (!bs) break;
      if (!BN_set_word(&wanted_value, VERITY_FORMAT_VERSION))
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      ASN1_INTEGER_to_BN(bs->formatVersion, &read_value);

      if (BN_cmp(&wanted_value, &read_value))
	{
	  fprintf(stderr,
		  "%s: error: mismatch in signature format version\n",
		  progname);
	  fprintf(stderr, "%s: support (", progname);
	  BN_print_fp(stderr, &wanted_value);
	  fprintf(stderr, ") but got (");
	  BN_print_fp(stderr, &read_value);
	  fprintf(stderr, ") !\n");
	  break;
	}

      BN_clear(&wanted_value);
      BN_clear(&read_value);

      imglen = htobe64(imglen); /* put image len in correct order for ASN1 */
      BN_bin2bn((const unsigned char *) &imglen, sizeof(imglen), &wanted_value);

      ASN1_INTEGER_to_BN(bs->authenticatedAttributes->length, &read_value);

      if (BN_cmp(&wanted_value, &read_value))
	{
	  fprintf(stderr,
		  "%s: error: mismatch between image length and signature authenticated attributes\n",
		  progname);
	  fprintf(stderr, "%s: expected (", progname);
	  BN_print_fp(stderr, &wanted_value);
	  fprintf(stderr, ") but got (");
	  BN_print_fp(stderr, &read_value);
	  fprintf(stderr, ") !\n");
	  break;
	}

      ret = 0; /* success */
    }
  while (0);

  BN_free(&wanted_value);
  BN_free(&read_value);
  
  return ret;
}

/*
 * Compute a SHA256 digest on image and auth attrs
 */
int
computeImageDigest(int imgfd, uint64_t imglen,
		   const AuthAttrs *aa,
		   unsigned char *digest)
{
  int ret = -1;
  EVP_MD_CTX *ctx = NULL;
  struct stat statbuf;
  unsigned char *buffer;
  unsigned char *attrs, *tmp;
  uint64_t imgsz, totsz = 0L, bufsz, rdsz = 0L;

  do
    {
      if (!aa || !digest) break;
      if (fstat(imgfd, &statbuf) == -1)
	{
	  perror("getting stat on image file");
	  break;
	}
      imgsz = statbuf.st_size;
      bufsz = BOOTIMG_MIN(statbuf.st_size, SHA_BUF_SIZE_K*1024);
      buffer = malloc(bufsz);
      assert(buffer);
      if (lseek64(imgfd, 0, SEEK_SET)) break;
      if (!(ctx = EVP_MD_CTX_create()))
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      /* Init digest */
      EVP_DigestInit(ctx, EVP_sha256());

      /* Loop on image for digest comptation */
      do
	{
	  rdsz = BOOTIMG_MIN(bufsz, imgsz - totsz);
	  if ((rdsz = read(imgfd, buffer, rdsz)) == -1)
	    {
	      perror("reading image for digest computing");
	      break;
	    }
	  EVP_DigestUpdate(ctx, buffer, rdsz);
	  totsz += rdsz;
	}
      while (totsz < imgsz);
      /* Could not read until end */
      if (totsz < imgsz) break;

      /* Get Auth Attrs size ... */
      if ((rdsz = i2d_AuthAttrs((AuthAttrs *)aa, NULL)) < 0)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      /* ..., Alloc, ... */
      if ((attrs = OPENSSL_malloc(rdsz)) == NULL)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      /* ..., then get */
      tmp = attrs;
      if ((rdsz = i2d_AuthAttrs((AuthAttrs *)aa, &tmp)) < 0)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}

      /* Display */
      if (vflag)
	{
	  BIGNUM value;
	  BN_init(&value);
	  
	  fprintf(stdout, "%s: Image auth attrs: \n", progname);
	  fprintf(stdout, "   target = " );
	  ASN1_STRING_print_ex_fp(stdout, aa->target, ASN1_STRFLGS_UTF8_CONVERT|ASN1_STRFLGS_SHOW_TYPE);
	  fprintf(stdout, "\n" );
	  ASN1_INTEGER_to_BN(aa->length, &value);
	  fprintf(stdout, "   length = 0x");
	  BN_print_fp(stdout, &value);
	  fprintf(stdout, "\n" );
	  
	  BN_free(&value);
	}

      /* Finally add Auth Attrs and close digest computation */
      EVP_DigestUpdate(ctx, attrs, rdsz);
      EVP_DigestFinal(ctx, digest, NULL);

      if (vflag)
	{
	  fprintf(stdout, "%s: Image Verity digest (SHA256)\n\t", progname);
	  for (int nx = 0; nx < SHA256_DIGEST_LENGTH; nx++)
	    fprintf(stdout, "%02x", digest[nx]);
	  fprintf(stdout, "\n");
	}

      ret = 0;
    }
  while (0);

  return ret;
}

/*
 * Check signature validity vs image
 */
int
checkSignatureBlockValidity(int imgfd, uint64_t imglen, const BootSignature *bs)
{
  int ret = -1;
  EVP_PKEY *pubkey = NULL;
  RSA *rsa = NULL;
  unsigned char digest[SHA256_DIGEST_LENGTH];

  do
    {
      if (!bs) break;
      if (computeImageDigest(imgfd, imglen, bs->authenticatedAttributes, digest) == -1) break;
      if ((pubkey = X509_get_pubkey(bs->certificate)) == NULL)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      if ((rsa = EVP_PKEY_get1_RSA(pubkey)) == NULL)
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}
      if (!RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH, bs->signature->data, bs->signature->length, rsa))
	{
	  ERR_print_errors_fp(stderr);
	  break;
	}

      if (vflag)
	{
	  BIGNUM value;

	  BN_init(&value);

	  fprintf(stdout, "%s: Image signature block: \n", progname);
	  ASN1_INTEGER_to_BN(bs->formatVersion, &value);
	  fprintf(stdout, "   formatVersion = 0x");
	  BN_print_fp(stdout, &value);    
	  fprintf(stdout, "\n");
	  fprintf(stdout, "   certificate = ");
	  X509_print_fp(stdout, bs->certificate);
	  fprintf(stdout, "\n");
	  fprintf(stdout, "   signature = ");
	  ASN1_STRING_print_ex_fp(stdout,
				  bs->signature,
				  ASN1_STRFLGS_UTF8_CONVERT|\
				  ASN1_STRFLGS_SHOW_TYPE|\
				  ASN1_STRFLGS_DUMP_ALL|\
				  ASN1_STRFLGS_DUMP_DER);
	  fprintf(stdout, "\n");
	}

      fprintf(stdout, "%s: Image signature is VALID\n", progname);
      
      ret = 0;
    }
  while (0);

  if (ret == -1)
    fprintf(stdout, "%s: Image signature is INVALID\n", progname);
  
  if (pubkey) EVP_PKEY_free(pubkey);
  if (rsa) RSA_free(rsa);
  
  return ret;
}

/*
 * Verify Verity signature of the image
 */
int
verityVerify(FILE* imgfp, struct boot_img_hdr *hdr)
{
  int ret = -1;
  BootSignature *bs = NULL;
  int imgfd = -1;
  off64_t offset = 0L;
  uint64_t imglen = 0L;
  
  do
    {
      if (fseek(imgfp, 0L, SEEK_END) == -1) break;
      if ((imglen = ftell(imgfp)) == -1) break;
      rewind(imgfp);
      if ((offset = computeSignatureBlockOffset(hdr)) == -1) break;
      if (readSignatureBlock(fileno(imgfp), offset, &bs)) break;
      if (checkSignatureBlockConsistency(imglen, bs)) break;
      if (checkSignatureBlockValidity(fileno(imgfp), imglen, bs)) break;

      ret = 0;
    }
  while (0);

  return ret;
}

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */
