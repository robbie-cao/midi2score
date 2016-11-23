#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "note.h"
#include "key.h"
#include "midi.h"

/**
 * MIDI File:
 *
 *      +----------------+
 *      | header         |   -> ppq (pulse(ticks) per quarternote
 *      |                |
 *      +----------------+
 *      | track 0        |   -> tempo
 *      |                |   -> time signature
 *      |                |   -> key signature
 *      |                |
 *      +----------------+
 *      | track 1        |   -> note 1 with note / sharp / octaves / length
 *      |                |   -> note 2 with note / sharp / octaves / length
 *      |                |   -> :
 *      |                |   -> :
 *      |                |
 *      |                |
 *      |                |
 *      |                |
 *      |                |
 *      |                |
 *      +----------------+
 *
 * Score File:
 *
 * Byte 0           1           2           3           4
 *      +-----------+-----------+-----------+-----------+
 *    0 |     M     |     S     |     S     |     C     |
 *      +-----------+-----------+-----------+-----------+
 *    4 | Clef      | Key Sign  | Time Sign | Reserved  |
 *      +-----------+-----------+-----------+-----------+
 *    8 | Size(MSB) | Size(LSB) | Reserved  | Reserved  |
 *      +-----------+-----------+-----------+-----------+
 *   12 | Note 1    | Note 1    | Note 2    | Note 3    |
 *      +-----------+-----------+-----------+-----------+
 *      | ....      | ....      | ....      | ....      |
 *      | ....      | ....      | ....      | ....      |
 *      | ....      | ....      | ....      | ....      |
 *      | ....      | ....      |           |           |
 *      |           |           |           |           |
 *      |           |           |           |           |
 *      +-----------+-----------+-----------+-----------+
 */

#define SCORE_OFFSET_MAGIC          0
#define SCORE_OFFSET_SIGNATURE      4
#define SCORE_OFFSET_SIZE           8
#define SCORE_OFFSET_DATA           12

static uint16_t ppq = 960;
static uint32_t tempo = 500000;     // 0x07A120


static uint8_t score[512];

static Clef_t            clef = { 0, 0, 0};
static KeySignature_t    ks   = { 0, 0 };
static TimeSignature_t   ts   = { 4, 2 };   // 4 / 4

#define FRACTION_TOLERANCE          0.40

static uint8_t midi_delta_time_to_length(uint32_t delta_time, uint32_t base)
{
    uint8_t len = 0;
    float fraction = 1.0;

    if (!delta_time || !base) {
        return NOTE_LENGTH_QUARTER;
    }

    fraction = (float)delta_time / base;

    // Simple algorithm for length calculation.
    //
    // Can be extended to support dot note.
    // Eg, for a quarternote with dot, the fraction will be 1.5
    // TODO
    //
    if (fraction >= 4.0 - FRACTION_TOLERANCE) {
        len = NOTE_LENGTH_WHOLE;
    } else if (fraction >= 2.0 - FRACTION_TOLERANCE / 2) {
        len = NOTE_LENGTH_HALF;
    } else if (fraction >= 1.0 - FRACTION_TOLERANCE / 4) {
        len = NOTE_LENGTH_QUARTER;
    } else if (fraction >= 0.5 - FRACTION_TOLERANCE / 8) {
        len = NOTE_LENGTH_EIGHTH;
    } else {
        len = NOTE_LENGTH_16TH;
    }

    return len;
}

int midi_to_score(char * midi_file)
{
    midi_t *midi;
    midi_track_t *track;
    midi_event_t *event;
    uint8_t trk_no = 0;
    uint32_t delta_time = 0;
    uint32_t count = 0;
    uint32_t position = 0;
    int status;

    status = midi_open(midi_file, &midi);

    if (status) {
        fprintf(stderr, "Failed open midi file: %s\n", strerror(status));
        return 1;
    }

    ppq = midi->ppq;

    /**
     * Currently only support midi file which contain 1 or 2 tracks.
     * If there are more than 2 tracks, only the first track will be handled.
     *
     * midi->hdr.tracks
     *   0          : Invalid (Should not happen)
     *   1          : Only 1 track. No tempo, key signature and time signature setting.
     *   2 (or more): Multiple tracks. Tempo, key signature and time signature setting in track 0.
     */
    if (midi->hdr.tracks == 0) {
        fprintf(stderr, "Invalid midi file: %s\n", strerror(status));
        goto cleanup;
    }

    trk_no = 0;
    if (midi->hdr.tracks >= 2) {
        /**
         * Retrieve tempo, key signature and time signature setting in track 0
         */
        track =  midi_get_track(midi, trk_no);
        trk_no += 1;
        midi_iter_track(track);
        while (midi_track_has_next(track)) {
            event = midi_track_next(track);

            if (event->type == MIDI_EVENT_TYPE_META)
                switch (event->cmd) {
                    case MIDI_META_SEQUENCE_NUM:
                    case MIDI_META_TEXT_EVNT:
                    case MIDI_META_COPYRIGHT_NOTICE:
                    case MIDI_META_SEQUENCE_NAME:
                    case MIDI_META_INSTRUMENT_NAME:
                    case MIDI_META_LYRICS:
                    case MIDI_META_MARKER:
                    case MIDI_META_CUE_POINT:
                    case MIDI_META_CHANNEL_PREFIX:
                    case MIDI_META_END_TRACK:
                        break;
                    case MIDI_META_TEMPO_CHANGE:
                        // Tempo (in microseconds per MIDI quarter-note)
                        // FF 51 03 tttttt
                        tempo = event->data[0] << 16 | event->data[1] << 8 | event->data[2];
                        printf("Tempo: %d us per quarternote\n", tempo);
                        break;
                    case MIDI_META_SMPTE_OFFSET:
                        // TODO
                        break;
                    case MIDI_META_TIME_SIGNATURE:
                        // Time Signature
                        // FF 58 04 nn dd cc bb
                        ts.upper = event->data[0];
                        ts.lower = event->data[1];
                        printf("Time Signature: %d/%d\n", event->data[0], 1 << event->data[1]);
                        break;
                    case MIDI_META_KEY_SIGNATURE:
                        // Key Signature
                        // FF 59 02 sf mi
                        // sf = -7 : 7 flats
                        // sf = -1 : 1 flat
                        // sf =  0 : key of C
                        // sf =  1 : 1 sharp
                        // sf =  7 : 7 sharps
                        // mi =  0 : major key
                        // mi =  1 : minor key
                        ks.signature = event->data[0];
                        ks.scale = event->data[1];
                        break;
                    default:
                        break;
            }
        }
        midi_free_track(track);
    }

    // Magic
    score[0] = 'M';
    score[1] = 'S';
    score[2] = 'S';
    score[3] = 'C';
    // Header
    score[4] = *(uint8_t *)&clef;
    score[5] = *(uint8_t *)&ks;
    score[6] = *(uint8_t *)&ts;
    score[7] = 0;

    position = SCORE_OFFSET_DATA;

    // MIDI event -> score notes
    //
    // Assumption:
    // - 1 channel
    // - Note On -> Note Off -> Note On -> Note Off -> ...
    track =  midi_get_track(midi, trk_no);
    trk_no += 1;
    midi_iter_track(track);
    while (midi_track_has_next(track)) {
        NoteSimplified_t note;

        event = midi_track_next(track);

        if (event->type != MIDI_EVENT_TYPE_EVENT) {
            // Ignore META event
            continue;
        }

        switch (event->cmd) {
            case MIDI_EVENT_NOTE_OFF:
                delta_time += event->delta_time;
                note = NumNotaiton_KeyToNoteSimp(event->data[0], midi_delta_time_to_length(delta_time, ppq));
                count += 1;
                score[position] = *(uint8_t *)&note;
                position += 1;
                printf("Note - note: %d, sharp: %d, length: %d, octaves: %d\n", note.note, note.sharp, note.length, note.octaves);
                break;
            case MIDI_EVENT_NOTE_ON:
                delta_time = event->delta_time;
                break;
            case MIDI_EVENT_AFTER_TOUCH:
            case MIDI_EVENT_CONTROL_CHANGE:
            case MIDI_EVENT_PROGRAM_CHANGE:
            case MIDI_EVENT_CHANNEL_PRESSURE:
            case MIDI_EVENT_PITCH_WHEEL:
            default:
                break;
        }
    }
    midi_free_track(track);

    // Parse the remained tracks
    for (; trk_no < midi->hdr.tracks; trk_no++) {
        track =  midi_get_track(midi, trk_no);

        midi_iter_track(track);
        while (midi_track_has_next(track)) {
            event = midi_track_next(track);

            // Do something
        }

        if (track != NULL) {
            midi_free_track(track);
        }
    }

    printf("Total count of notes: %d\n", count);
    score[8] = (count >> 8) & 0xFF;
    score[9] = count & 0xFF;
    score[10] = 0;
    score[11] = 0;

    // Write score to file
    char file_name[128];
    FILE *fp = NULL;

    snprintf(file_name, sizeof(file_name), "%s.ssc", midi_file);
    fp = fopen(file_name, "wb");
    if (fp) {
        fwrite(score, sizeof(score), 1, fp);
        fclose(fp);
    }

cleanup:
    midi_close(midi);

    return 0;
}

int main(int argc, char**argv)
{

    char * midi_file;
    if (argc != 2 || strlen(argv[1]) < 1) {
        fprintf(stderr, "Usage: %s filename.mid\n\n", argv[0]);
        return 1;
    }

    midi_file = argv[1];

    return midi_to_score(midi_file);
}

/* vim: set ts=4 sw=4 tw=0 list : */
