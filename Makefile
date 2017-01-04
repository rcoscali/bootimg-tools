PROGRAMS := bootimg-extract bootimg-create
BOOTIMG_EXTRACT_SRCS := bootimg-extract.c cJSON.c
BOOTIMG_EXTRACT_OBJS := $(BOOTIMG_EXTRACT_SRCS:%.c=%.o)
BOOTIMG_CREATE_SRCS := bootimg-create.c cJSON.c
BOOTIMG_CREATE_OBJS := $(BOOTIMG_CREATE_SRCS:%.c=%.o)
BOOTIMG_LDLIBS := xml2
BOOTIMG_LDFLAGS := 

INCLUDES := -I/usr/include/libxml2

CC := gcc
ifneq ($(DEBUG),)
CFLAGS := -g -O0
else
CFLAGS := -O2
endif

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: all clean

all: $(PROGRAMS)

clean:
	$(RM) *.o $(PROGRAMS) *~

bootimg-extract: $(BOOTIMG_EXTRACT_OBJS)
	$(CC) $(CFLAGS) $(BOOTIMG_EXTRACT_OBJS) -o $@ $(BOOTIMG_LDFLAGS) $(foreach lib,$(BOOTIMG_LDLIBS),-l$(lib)) -lm

bootimg-create: $(BOOTIMG_CREATE_OBJS)
	echo $(CC) $(CFLAGS) $(BOOTIMG_CREATE_OBJS) -o $@ $(BOOTIMG_LDFLAGS) $(foreach lib,$(BOOTIMG_LDLIBS),-l$(lib)) -lm
