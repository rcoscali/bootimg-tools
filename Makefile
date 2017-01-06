PROGRAMS := bootimg-extract bootimg-create
BOOTIMG_EXTRACT_SRCS := bootimg-extract.c bootimg-utils.c cJSON.c
BOOTIMG_EXTRACT_OBJS := $(BOOTIMG_EXTRACT_SRCS:%.c=%.o)
BOOTIMG_CREATE_SRCS := bootimg-create.c bootimg-utils.c cJSON.c
BOOTIMG_CREATE_OBJS := $(BOOTIMG_CREATE_SRCS:%.c=%.o)
BOOTIMG_LDLIBS := xml2 crypto
BOOTIMG_LDFLAGS := 

DEFINES := -D_GNU_SOURCE -D__USE_GNU
INCLUDES := -I/usr/include/libxml2

CC := gcc
CFLAGS := -std=gnu11
ifeq ($(DEBUG),0)
CFLAGS += -O2
else
CFLAGS += -g -O0
endif

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

.PHONY: all clean


all: $(PROGRAMS)


clean:
	$(RM) *.o $(PROGRAMS) *~


bootimg-extract: $(BOOTIMG_EXTRACT_OBJS)
	$(CC) $(CFLAGS) $(BOOTIMG_EXTRACT_OBJS) -o $@ $(BOOTIMG_LDFLAGS) $(foreach lib,$(BOOTIMG_LDLIBS),-l$(lib)) -lm


bootimg-create: $(BOOTIMG_CREATE_OBJS)
	$(CC) $(CFLAGS) $(BOOTIMG_CREATE_OBJS) -o $@ $(BOOTIMG_LDFLAGS) $(foreach lib,$(BOOTIMG_LDLIBS),-l$(lib)) -lm

## Depends (not generated -- pls maintain)
bootimg-create.c: bootimg-utils.c bootimg.h bootimg-utils.h bootimg-priv.h

bootimg-extract.c: bootimg-utils.c bootimg.h bootimg-utils.h bootimg-priv.h

bootimg-utils.c: bootimg-utils.h bootimg.h bootimg-priv.h

cJSON.c: cJSON.h
