PKGCONF = pkg-config
SERVER = server

SRCS_SERVER-y := server.c


CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS += $(shell $(PKGCONF) --libs libdpdk)

$(SERVER): $(SRCS_SERVER-y) Makefile
	$(CC) $(CFLAGS) $(SRCS_SERVER-y) -o $@ $(LDFLAGS)