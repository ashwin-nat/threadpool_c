#HOSTPLATFORM:=WINDOWS

ifeq ($(HOSTPLATFORM),WINDOWS)
CC:=arm-linux-gnueabihf-gcc
CLEAN:= del /q
else
CLEAN:= rm -rf
endif


CFLAGS_RELEASE:=	-O2 \
					-DNDEBUG \
					-Wall \
					-Werror \
					-lpthread
CFLAGS_DEBUG:=		-O0 \
					-ggdb3 \
					-Wall \
					-lpthread \
					-fsanitize=address \
					-fsanitize=leak

all: main
debug: main_dbg

main:
	$(CC) $(CFLAGS_RELEASE) tpool.c main.c -o tpool

main_dbg:
	$(CC) $(CFLAGS_DEBUG) tpool.c main.c -o tpool

clean:
	$(CLEAN) tpool