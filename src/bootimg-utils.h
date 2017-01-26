/* bootimg-tools/bootimg-utils.h
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

#ifndef __BOOTIMG_UTILS_H__
#define __BOOTIMG_UTILS_H__

#include "config.h"

#ifdef USE_LIBXML2
# include <libxml/xmlstring.h>
#endif

#include "bootimg.h"

struct boot_img_hdr *initBootImgHeader(struct boot_img_hdr *);
const char          *getLongOptionName(struct option *, char);
const char          *getImageFilename(const char *, const char *, int);
const char          *getDirname(const char *, uint8_t);
const char          *getBasename(const char *, const char *);
size_t               alignOnPage(size_t, size_t);
int                  verityVerify(FILE *, struct boot_img_hdr *);

#endif /* __BOOTIMG_UTILS_H__ */

/* Local Variables:                                                */
/* mode: C                                                         */
/* comment-column: 0                                               */
/* End:                                                            */
