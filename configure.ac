AC_INIT([bootimg-tools],[1.0],[remi.cohen-scali@nagra.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror dist-bzip2])
AC_PROG_CC

CFLAGS="-D_GNU_SOURCE -D__USE_GNU"

# wanted by: src/bootimg-utils.c:57
AC_CHECK_FUNCS([bzero])
# wanted by: src/cJSON.c:361
AC_CHECK_FUNCS([floor])
# wanted by: src/bootimg-create.c:589
AC_CHECK_FUNCS([memset]) 
# wanted by: src/bootimg-extract.c:399
AC_CHECK_FUNCS([mkdir])
# wanted by: src/cJSON.c:212
AC_CHECK_FUNCS([pow])
# wanted by: src/bootimg-utils.c:167
AC_CHECK_FUNCS([realpath])
# wanted by: src/cJSON.c:724
AC_CHECK_FUNCS([strchr])
# wanted by: src/bootimg-utils.c:49
AC_CHECK_FUNCS([strdup])
# wanted by: src/bootimg-utils.c:67
AC_CHECK_FUNCS([strrchr])
# wanted by: src/bootimg-utils.c:188
AC_CHECK_FUNCS([strstr])
# wanted by: src/bootimg-create.c:616
AC_CHECK_FUNCS([strtol])
# wanted by: src/bootimg-create.c:897
AC_CHECK_FUNCS([strtoul])
# wanted by: src/bootimg-create.c:28
AC_CHECK_HEADERS([fcntl.h])
# wanted by: src/cJSON.c:30
AC_CHECK_HEADERS([float.h])
# wanted by: src/bootimg-utils.c:20
AC_CHECK_HEADERS([limits.h])
# wanted by: src/cJSON.h:31
AC_CHECK_HEADERS([stddef.h])
# wanted by: src/bootimg.h:19
AC_CHECK_HEADERS([stdint.h])
# wanted by: src/bootimg-utils.c:19
AC_CHECK_HEADERS([stdlib.h])
# wanted by: src/bootimg-utils.c:22
AC_CHECK_HEADERS([string.h])
# wanted by: src/bootimg-utils.c:23
AC_CHECK_HEADERS([strings.h])
# wanted by: src/bootimg-create.c:26
AC_CHECK_HEADERS([unistd.h])
# wanted by: src/bootimg-utils.c:21
AC_CHECK_HEADERS([values.h])
# wanted by: src/cJSON.c:661
AC_CHECK_HEADER_STDBOOL
# wanted by: src/bootimg-utils.c:25
AC_FUNC_ALLOCA
# wanted by: src/bootimg-create.c:142
AC_FUNC_MALLOC
# wanted by: src/bootimg-create.c:1273
AC_TYPE_OFF_T
# wanted by: src/bootimg-create.c:68
AC_TYPE_SIZE_T
# wanted by: src/bootimg-create.c:723
AC_TYPE_SSIZE_T
# wanted by: src/bootimg-create.c:1309
AC_TYPE_UINT32_T
# wanted by: src/bootimg-utils.c:156
AC_TYPE_UINT8_T

# Math
AC_CHECK_LIB([m],
             [floor],
	     [M_LIBS=-lm],
	     [AC_MSG_ERROR([libm is required but was not found !!])])
AC_SUBST([M_LIBS])	     
# XML2	
AC_CHECK_LIB([xml2],
             [xmlNewTextWriterDoc],
	     [XML2_LIBS=`pkg-config --libs libxml-2.0`; XML2_CFLAGS=`pkg-config --cflags libxml-2.0`],
	     [AC_MSG_ERROR([libxml-2.0 is required but was not found !!])])
AC_SUBST([XML2_LIBS])

# CRYPTO
AC_CHECK_LIB([crypto],
             [SHA_Init],
	     [CRYPTO_LIBS=`pkg-config --libs libcrypto`],
	     [AC_MSG_ERROR([libcrypto is required but was not found !!])])
AC_SUBST([CRYPTO_LIBS]) 

LIBS="$LIBS $XML2_LIBS $CRYPTO_LIBS $M_LIBS"
CFLAGS="$CFLAGS $XML2_CFLAGS"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
