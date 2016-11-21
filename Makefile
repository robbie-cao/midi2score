all: target
FORCE: ;
.PHONY: FORCE

program= dan midi-dump

target: $(program)

VPATH = .
IPATH = .

CFLAGS  = -O2 --std=c99 -Wall -Wextra
CFLAGS += ${patsubst %,-I%,${subst :, ,${IPATH}}}

CC = gcc

LDFLAGS =

%.o: %.c
	@$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

dan: midi.o dan.o
	$(CC) $(LDFLAGS) $^ -o $@

midi-dump: midi.o midi-dump.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(program)

