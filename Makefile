# Makefile for BananaClient

CC=gcc

LIBEV = ./libevent-2.0.21-stable

PROG  = server
# TODO: Add back in -Werror
CFLAGS=	-W -Wall -I. -Isrc -pthread -g -ggdb -D_GNU_SOURCE -I $(LIBEV)/include
LDFLAGS = -lc -ggdb -liconv -L $(LIBEV)/.libs -levent -levent_extra -levent_core -levent_openssl


all: $(PROG)

include Makefile.depend

PAGE_FILES = \
    src/page_test.c

C_FILES = \
    src/htserver.c \
    src/config.c \
    src/logger.c \
    src/em.c \
    src/middleware.c \
    src/template.c \
    src/banana.c \
    $(PAGE_FILES)

# C_FILES = *.c
O_FILES = $(patsubst src/%.c, build/%.o, $(C_FILES))

GEN_FILES = src/_middleware.h src/_page_defs.h src/_page_gen.h

$(PROG): $(O_FILES)
	$(CC) $(LDFLAGS) -o $(PROG) $(O_FILES) $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $(patsubst src/%.c,build/%.o,$<)

clean:
	rm $(O_FILES) $(PROG) $(GEN_FILES)

src/_middleware.h: $(PAGE_FILES) src/middleware.c
	grep -h MIDDLEWARE src/middleware.c $(PAGE_FILES) | perl -pi -e "s/MIDDLEWARE\((.*)\).*/MIDDLEWARE(\1);/" > src/_middleware.h

src/page.h: src/_middleware.h
	touch src/page.h

src/banana.c: src/_middleware.h
src/banana.c: src/_page_defs.h
src/banana.c: src/_page_gen.h
src/_page_defs.h: $(PAGE_FILES)
	grep -h PAGE $(PAGE_FILES) | perl -pi -e "s/PAGE\((.*)\).*/PAGE(\1);/" > src/_page_defs.h
src/_page_gen.h: $(PAGE_FILES)
	grep -h PAGE $(PAGE_FILES) | perl -pi -e "s/PAGE\((\w+), (.*)\).*/htserver_bind(htserver, \2, \1);/" > src/_page_gen.h

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
	grep -v ": /" Makefile.depend.in | grep -v "./libevent" > Makefile.depend.2
	sed s^src/^^ Makefile.depend.2 > Makefile.depend
	rm Makefile.depend.in Makefile.depend.in.bak Makefile.depend.2

vgrun:
	valgrind --leak-check=full --show-reachable=yes ./$(PROG)

iconv:
	wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.13.1.tar.gz
	tar xvzf libiconv-1.13.1.tar.gz
	cd libiconv-1.13.1 ; ./configure --prefix=/usr ; make ; sudo make install

run:
	LD_LIBRARY_PATH=$(LIBEV)/.libs ./$(PROG)
