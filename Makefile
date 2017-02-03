SHELL		= /bin/sh
UNAME := $(shell uname)
TARGET		= libuthread.so

OFILES		= uthread.o \
			  uthread_ctx.o uthread_queue.o uthread_mtx.o \
			  uthread_cond.o uthread_sched.o uthread_idle.o \
			  interpose.o

HANDIN		= snarf.tar

EXECS		= test.o # other tests can go here
ifeq ($(UNAME), Darwin)
	CC	= clang
else
	CC = gcc
endif

CFLAGS		= -g -Wall -fPIC

ifeq ($(UNAME), Darwin)
	CFLAGS += -D_XOPEN_SOURCE -Wno-deprecated-declarations
endif

IFLAGS		=
LFLAGS		= -L.
ifeq ($(UNAME), Linux)
	LFLAGS +=  -Wl,--rpath,.
endif

LIBS		= -ldl

.PHONY: all cscope clean handin


all: cscope $(TARGET) $(EXECS)
	@for exec in $(EXECS); do \
		$(CC) $(CFLAGS)  $(LFLAGS) -o `basename $$exec .o` $$exec -luthread; \
	done \

cscope:
	@find . -name "*.[chS]" > cscope.files
	cscope -k -b -q -v

$(TARGET): $(OFILES)
	$(CC) -g -shared $(LFLAGS) -o $(TARGET) $(OFILES) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) *.o *.dSYM
	rm -f cscope.files cscope.out cscope.in.out cscope.po.out
	for exec in $(EXECS) ; do \
		if [ -f `basename $$exec .o` ] ; then \
			rm `basename $$exec .o` ; \
		fi \
	done

# Deprecated handin target, based on antiquated handin system
# handin: clean
# 	@rm -f $(HANDIN)
# 	tar cf $(HANDIN) *
# 	/cs/bin/handin -c cs167 uthreads $(HANDIN)
# 	@rm -f $(HANDIN)

# This is magic for implementing make nyi - basically, the grep command prepends
# the file and line number, so a line looks like
#
# foo.c:3:		NOT_YET_IMPLEMENTED("PROJECT: bar")
#
# The sed command finds the relevant parts and separates them to be printed by awk
SED_REGEX := 's/^\(.*:.*:\).*"\(.*\):\(.*\)".*/\1 \2 \3/'

PROJ_FILTER := $(foreach def,$(COMPILE_CONFIG_BOOLS), \
	$(if $(findstring 0,$($(def))),grep -v $(def) |,))

FILTER := grep -v define | $(PROJ_FILTER) \
	sed -e $(SED_REGEX) | awk '{printf("%30s %30s() %10s\n", $$1, $$3, $$2)}'

nyi:
	@find . -name "*.c" | xargs grep -n NOT_YET_IMPLEMENTED | $(FILTER)
