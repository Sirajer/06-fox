######################################################
COURSE=cs131w
ASGN=06
COMPILER=fox
EXT=fox
# CLANG=clang -g -m32 -mstackrealign -fsanitize=address
CLANG=clang -g -m32 -mstackrealign
HEAP=100000
######################################################

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
  FORMAT=aout
else
ifeq ($(UNAME), Darwin)
  FORMAT=macho
endif
endif

.PHONY: test bin build clean distclean turnin

SHELL=/bin/bash -o pipefail

test: clean
	stack test

build:
	stack build

tests/output/%.result: tests/output/%.run FORCE
	$< $(HEAP) > $@

tests/output/%.vresult: tests/output/%.run FORCE
	valgrind $< $(HEAP) > $@

tests/output/%.run: tests/output/%.o c-bits/main.c c-bits/gc.o c-bits/types.o FORCE
	$(CLANG) -o $@ c-bits/gc.o c-bits/types.o c-bits/main.c $<

tests/output/%.o: tests/output/%.s FORCE
	nasm -f $(FORMAT) -o $@ $<

tests/output/%.s: tests/input/%.$(EXT) FORCE
	stack exec -- $(COMPILER) $< > $@

c-bits/gc.o: c-bits/gc.c c-bits/gc.h
	$(CLANG) -c -o c-bits/gc.o c-bits/gc.c

c-bits/types.o: c-bits/types.c c-bits/types.h
	$(CLANG) -c -o c-bits/types.o c-bits/types.c

clean:
	rm -rf c-bits/*.o tests/output/*.o tests/output/*.s tests/output/*.dSYM tests/output/*.run tests/output/*.log tests/output/*.*result $(ASGN)-$(COMPILER).tgz

distclean: clean
	stack clean
	rm -rf .stack-work

tags:
	hasktags -x -c lib/

turnin: clean
	./files_to_submit.sh | tar --transform "s/^./$(ASGN)-$(COMPILER)/" -zcvf ../$(ASGN)-$(COMPILER).tgz -T -
	mv ../$(ASGN)-$(COMPILER).tgz .
	turnin -c $(COURSE) -p $(ASGN) ./$(ASGN)-$(COMPILER).tgz

# aliases

INPUTS  := $(patsubst tests/input/%.$(EXT),%,$(wildcard tests/input/*.$(EXT)))
ASMS    := $(patsubst %,%-s,$(INPUTS))
OBJS    := $(patsubst %,%-o,$(INPUTS))
RUNS    := $(patsubst %,%-run,$(INPUTS))
RESULTS := $(patsubst %,%-result,$(INPUTS))

$(ASMS): %-s: tests/output/%.s
	cat $<
$(OBJS): %-o: tests/output/%.o
$(RUNS): %-run: tests/output/%.run
$(RESULTS): %-result: tests/output/%.result
	cat $<

FORCE:
