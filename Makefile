FFMPEG_LIBS=     libavcodec                         \
                 libavformat                        \

INC=-I$(HOME)/ffmpeg_sources/ffmpeg

TARGET = main
CC = gcc
CFLAGS = -g -Wall $(INC)

LDFLAGS := $(shell pkg-config --libs $(FFMPEG_LIBS))
LDFLAGS += -lvdpau -lX11 -lva-x11 -lva-drm

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@ $(LDFLAGS)

clean:
	-rm -f *.o
	-rm -f $(TARGET)
