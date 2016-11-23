#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"

static int midi_dump(char * midi_file);

int main(int argc, char**argv)
{

    char * midi_file;
    if (argc != 2 || strlen(argv[1]) < 1) {
        fprintf(stderr, "Usage: %s filename.mid\n\n", argv[0]);
        return 1;
    }

    midi_file = argv[1];

    return midi_dump(midi_file);
}

static int midi_dump(char * midi_file)
{
    midi_t *midi;
    int status;

    status = midi_open(midi_file, &midi);

    if (status) {
        fprintf(stderr, "Failed open midi file: %s\n", strerror(status));
        return 1;
    }

    midi_print_info(midi);

    midi_close(midi);

    return 0;
}

