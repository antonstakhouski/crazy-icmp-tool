CC ?= gcc
CFLAGS=-c -Wall -O3 -pedantic -std=c11
LDFLAGS=-pthread

SOURCEDIR = src
BUILDDIR = build

SOURCES =
CLIENT_SOURCES= client.c
SERVER_SOURCES= server.c udp_server.c tcp_server.c

OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))

EXECUTABLE ?= lin-client

BBB_ADDR="debian@192.168.0.105:~/"
RPI_ADDR="pi@192.168.0.100:~/"

BBB_PASS="temppwd"
RPI_PASS="raspberry"

BBB_CC = arm-linux-gnueabihf-gcc

ARM_CC = $(BBB_CC)
R_ADDR=$(RPI_ADDR)
R_PASS=$(RPI_PASS)

 #ARM_CC = $(BBB_CC)
 #R_ADDR=$(BBB_ADDR)
 #R_PASS=$(BBB_PASS)

.PHONY: clean lin-client arm-client

all: dir $(BUILDDIR)/$(EXECUTABLE)

dir:
	mkdir -p $(BUILDDIR)

lin-client:
	$(eval EXECUTABLE := client)
	$(eval CC := gcc)
	$(eval SOURCES += $(CLIENT_SOURCES))
	@$(MAKE) -f Makefile all EXECUTABLE=$(EXECUTABLE) CC=$(CC) SOURCES="$(SOURCES)"

arm-client:
	$(eval EXECUTABLE := client-arm)
	$(eval CC := $(BBB_CC))
	$(eval SOURCES += $(CLIENT_SOURCES))
	@$(MAKE) -f Makefile EXECUTABLE=$(EXECUTABLE) CC=$(ARM_CC) SOURCES="$(SOURCES)"
	sshpass -p $(R_PASS) scp $(BUILDDIR)/$(EXECUTABLE) $(R_ADDR)

$(BUILDDIR)/$(EXECUTABLE): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJECTS): $(BUILDDIR)/%.o : $(SOURCEDIR)/%.c
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(BUILDDIR)/*.o
