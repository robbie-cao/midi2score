all: target
FORCE: ;
.PHONY: FORCE

program= dan midi-dump midi2score

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

midi2score: midi2score.o midi.o note.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o $(program)

