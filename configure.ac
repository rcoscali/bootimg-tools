#                                               -*- Autoconf -*-
# BootImg-Tools
#
#

AC_PREREQ([2.69])
AC_INIT([bootimg-tools],[1.0],[remi.cohen-scali@nagra.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror dist-bzip2 subdir-objects])
LT_PREREQ([2.4.6])
LT_INIT([])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL()

# debug mode
use_debug=no
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug],
                              [turn on debugging build (default is no)])],
	      [use_debug=$enable_debug])
AC_MSG_CHECKING(debugging)
if test $use_debug = yes;
then
  AC_MSG_RESULT(on)
  DEBUG_CFLAGS="-g -O0"
else
  AC_MSG_RESULT(off)
  DEBUG_CFLAGS="-O2"
fi
AC_SUBST([DEBUG_CFLAGS])

# maintainer mode
AM_MAINTAINER_MODE([enable])

# Checks for libraries.

# Checks for header files.
# wanted by: src/bootimg-create.c:28
#          : src/cJSON.c:30
#          : src/bootimg-utils.c:20
#          : src/cJSON.h:31
#          : src/bootimg.h:19
#          : src/bootimg-utils.c:19
#          : src/bootimg-utils.c:22
#          : src/bootimg-utils.c:23
#          : src/bootimg-create.c:26
#          : src/bootimg-utils.c:21
AC_CHECK_HEADERS([fcntl.h float.h limits.h stddef.h stdint.h stdlib.h string.h strings.h unistd.h values.h])

# Checks for typedefs, structures, and compiler characteristics.
# wanted by: src/cJSON.c:661
AC_CHECK_HEADER_STDBOOL
# wanted by: src/bootimg-create.c:1273
#          : src/bootimg-create.c:68
#          : src/bootimg-create.c:723
#          : src/bootimg-create.c:1309
#          : src/bootimg-utils.c:156
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
# wanted by: src/bootimg-create.c:142
AC_FUNC_MALLOC
# wanted by: src/bootimg-utils.c:25
AC_FUNC_ALLOCA
# wanted by: src/bootimg-utils.c:57
#          : src/cJSON.c:361
#          : src/bootimg-create.c:589
#          : src/bootimg-extract.c:399
#          : src/cJSON.c:212
#          : src/bootimg-utils.c:167
#          : src/cJSON.c:724
#          : src/bootimg-utils.c:49
#          : src/bootimg-utils.c:67
#          : src/bootimg-utils.c:188
#          : src/bootimg-create.c:616
#          : src/bootimg-create.c:897
AC_CHECK_FUNCS([bzero floor memset mkdir pow realpath strchr strdup strrchr strstr strtol strtoul])

# Math
AC_CHECK_LIB([m],
             [floor],
	     [M_LIBS=-lm],
	     [AC_MSG_ERROR([libm is required but was not found !!])])
AC_SUBST([M_LIBS])	     

# Modules version requirements
LIBXML_2_0_REQUIRED_MIN_VERSION=2.9.0
LIBCRYPTO_REQUIRED_MIN_VERSION=1.0.2g

# XML2
PKG_CHECK_MODULES([XML2],
                  libxml-2.0 >= $LIBXML_2_0_REQUIRED_MIN_VERSION)


# CRYPTO
PKG_CHECK_MODULES([CRYPTO],
                  libcrypto >= $LIBCRYPTO_REQUIRED_MIN_VERSION)

AC_DEFINE([_GNU_SOURCE], [], [uses GNU sources for libraries])
AC_DEFINE([__USE_GNU], [], [uses GNU implems])

AC_CONFIG_FILES([Makefile src/Makefile])

# Get rid of the default -g -O2
CFLAGS=

AC_OUTPUT
