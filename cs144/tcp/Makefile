

# Debug malloc support (http://dmalloc.com).  Comment out if you don't
# have dmalloc, but it is highly recommended.
#
#DMALLOC_CFLAGS = -I/usr/local/include -DDMALLOC=1
#DMALLOC_LIBS = -L/usr/local/lib -ldmalloc
#
# On Stanford machines, you need these paths for dmalloc:
#
#DMALLOC_CFLAGS = -I/afs/ir/class/cs144/dmalloc -DDMALLOC=1
#DMALLOC_LIBS = -L/afs/ir/class/cs144/dmalloc -ldmalloc

LIBRT = `test -f /usr/lib/librt.a && printf -- -lrt -lm`

CC = gcc
CFLAGS = -g -Wall -Werror $(DMALLOC_CFLAGS)
LIBS = $(DMALLOC_LIBS) -lrt -lm

all: uc reliable

.c.o:
	$(CC) $(CFLAGS) -c $<

uc: uc.o
	$(CC) $(CFLAGS) -pthread -o $@ uc.o $(LIBS)

rlib.o reliable.o: rlib.h

reliable: reliable.o rlib.o
	$(CC) $(CFLAGS) -o $@ reliable.o rlib.o $(LIBS) $(LIBRT)

.PHONY: tester reference
tester reference:
	cd tester-src && $(MAKE) Examples/reliable/$@
	cp tester-src/Examples/reliable/$@ .
	strip $@

TAR = reliable.tar.gz

SUBMIT = reliable/Makefile reliable/*.[ch] reliable/README

.PHONY: submit
submit: clean
	ln -s . reliable
	tar -czf $(TAR) $(SUBMIT)
	rm -f reliable
	@echo '************************************************************'
	@echo '                                                            '
	@echo '  Please submit file $(TAR) to the following URL:  '
	@echo '                                                            '
	@echo '   https://www.stanford.edu/class/cs144/cgi-bin/submit/     '
	@echo '                                                            '
	@echo '************************************************************'

.PHONY: dist
dist: clean
	cd tester-src && $(MAKE) clean
	$(MAKE)
	mv reliable reference
	$(MAKE) clean
	mv reliable.c reliable.c.soln
	./stripsol reliable.c.soln > reliable.c
	ln -s . reliable
	tar -czf $(TAR) \
		reliable/reliable.c \
		reliable/Makefile reliable/uc.c reliable/rlib.[ch] \
		reliable/reference
	rm -f reference
	rm -r reliable
	mv reliable.c.soln reliable.c

.PHONY: clean
clean:
	@find . \( -name '*~' -o -name '*.o' -o -name '*.hi' \) \
		-print0 > .clean~
	@xargs -0 echo rm -f -- < .clean~
	@xargs -0 rm -f -- < .clean~
	rm -f uc reliable $(TAR)

.PHONY: clobber
clobber: clean
	cd tester-src && $(MAKE) clean
	rm -f tester reference reliable.c-dist
