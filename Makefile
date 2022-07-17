CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -I./inc
CREATOR = rngg
RCP = gemgrab.rcp
RSC = .
RSCS = gemgrab.h gemgrab.bmp gemgrabsmall.bmp
SRCS = gemgrab.c MathLib.c
OBJS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))
TARGET = GemGrabber
APPNAME = 'Gem Grab'

all: $(TARGET).prc

$(TARGET).prc: $(TARGET) bin.stamp
	build-prc $(TARGET).prc $(APPNAME) $(CREATOR) $(TARGET) *.bin

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

bin.stamp: $(RCP) $(RSCS)
	pilrc -I $(RSC) $(RCP)

clean:
	-rm -f *.[oa] $(TARGET) *.bin *.stamp
