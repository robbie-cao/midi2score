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

    midi_track_t * track;

    for (int i = 0; i < midi->hdr.tracks; ++i) {
        track =  midi_get_track(midi, i);

        printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n",
                track->num, track->events, track->hdr.size,
                track->hdr.magic[0], track->hdr.magic[1], track->hdr.magic[2], track->hdr.magic[3]);

        midi_iter_track(track);
        midi_event_t * evnt;
        while (midi_track_has_next(track)) {
            evnt = midi_track_next(track);

            // Do something
        }

        if (track != NULL) {
            midi_free_track(track);
        }
    }

    midi_close(midi);

    return 0;
}

