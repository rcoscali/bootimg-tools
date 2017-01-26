
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = bootimg-extract bootimg-create

bootimg_extract_SOURCES = \
	bootimg-extract.c \
	bootimg-utils.c \
	cJSON.c \
	cJSON_Utils.c

bootimg_create_SOURCES = \
	bootimg-create.c \
	bootimg-utils.c \
	cJSON.c \
	cJSON_Utils.c

noinst_HEADERS = \
	bootimg.h
	bootimg-priv.h \
	bootimg-utils.h \
	cJSON.h \
	cJSON_Utils.h


bootimg_extract_CPPFLAGS = $(XML2_CFLAGS) $(OPENSSL_CFLAGS)
bootimg_extract_CFLAGS = -std=gnu11 $(DEBUG_CFLAGS)
bootimg_extract_LDADD = $(XML2_LIBS) $(OPENSSL_LIBS) $(M_LIBS)

bootimg_create_CPPFLAGS = $(XML2_CFLAGS) $(OPENSSL_CFLAGS)
bootimg_create_CFLAGS = -std=gnu11 $(DEBUG_CFLAGS)
bootimg_create_LDADD = $(XML2_LIBS) $(OPENSSL_LIBS) $(M_LIBS)
