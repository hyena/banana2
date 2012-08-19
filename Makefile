# Makefile for BananaClient

CC=gcc

LIBEV = ./libevent-2.0.19-stable

PROG  = server
CFLAGS=	-W -Wall -I. -pthread -g -lc -ggdb -Werror -D_GNU_SOURCE -I $(LIBEV)/include
LDFLAGS = -lc -ggdb -liconv -L $(LIBEV)/.libs -levent -levent_extra -levent_core -levent_openssl


all: $(PROG)

include Makefile.depend

C_FILES = htserver.c banana.c
# C_FILES = api.c banana.c conf.c events.c mongoose.c sessions.c util.c users.c api_admin.c api_world.c file.c worlds.c net.c genapi.c logger.c api_user.c api_file.c strutil.c api_log.c mudparser.c
# C_FILES = *.c
O_FILES = $(patsubst %.c, build/%.o, $(C_FILES))

$(PROG): $(O_FILES)
	$(CC) $(LDFLAGS) -o $(PROG) $(O_FILES) $(LDFLAGS)

build/%.o: %.c
	@mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $(patsubst %.c,build/%.o,$<)

clean:
	rm $(O_FILES) $(PROG)

API_FILES = $(filter api_%.c,$(C_FILES))

genapi.h: $(API_FILES)
	@echo "Generating genapi.h"
	@echo "/* AUTO-GENERATED FILE, DO NOT EDIT */" > genapi.h
	@echo >> genapi.h
	@echo '#ifndef __GENAPI_H_ ' >> genapi.h
	@echo '#define __GENAPI_H_ ' >> genapi.h
	@echo '#include "banana.h"' >> genapi.h
	@echo >> genapi.h
	@grep -h ^ACTION $(API_FILES) | sed 's/).*/);/' >> genapi.h
	@echo >> genapi.h
	@echo "extern ActionList allActions[];" >> genapi.h
	@echo >> genapi.h
	@echo '#endif' >> genapi.h

genapi.c: $(API_FILES)
	@echo "Generating genapi.c"
	@echo "/* AUTO-GENERATED FILE, DO NOT EDIT */" > genapi.c
	@echo >> genapi.c
	@echo '#include "banana.h"' >> genapi.c
	@echo '#include "genapi.h"' >> genapi.c
	@echo >> genapi.c
	@echo "ActionList allActions[] = {" >> genapi.c
	@grep -h ^ACTION $(API_FILES) | pcregrep -o '".*\w' | sed s/$$/\\},/ | sed 's/^/  {/' >> genapi.c
	@echo "  {NULL, NULL, 0}" >> genapi.c
	@echo "};" >> genapi.c
	@echo >> genapi.c

depend:
	touch Makefile.depend.in
	makedepend -fMakefile.depend.in -pbuild/ -w10 -- $(CFLAGS) -- $(C_FILES) 2>/dev/null
	grep -v ": /" Makefile.depend.in > Makefile.depend
	rm Makefile.depend.in Makefile.depend.in.bak

vgrun:
	valgrind ./$(PROG)

iconv:
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz
	tar xvzf libiconv-1.13.1.tar.gz
	cd libiconv-1.13.1 ; ./configure --prefix=/usr ; make ; sudo make install

run:
	./$(PROG)
