PREFIX ?= /usr
CC ?= cc
BIN := sigbar
CFLAGS ?= -Wall -Wextra -O2 -pthread
LDFLAGS ?=

$(BIN): $(BIN).c config.h
	$(CC) $(BIN).c $(LDFLAGS)-o $(BIN)

config.h: config.def.h
	cp config.def.h $@

clear:
	rm -f $(BIN) config.h *.o

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
