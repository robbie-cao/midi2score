#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "midi.h"

#define DEBUG       1

const uint8_t MIDI_HEADER_MAGIC[] = { 'M', 'T', 'h', 'd' };
const uint8_t MIDI_TRACK_MAGIC[]  = { 'M', 'T', 'r', 'k' };

static inline uint16_t btol_16(const uint16_t n)
{
    return ((n >> 8) | (n << 8));
}

static inline uint32_t btol_32(const uint32_t n)
{
    return ((n >> 24) & 0xff) | ((n << 8) & 0xff0000) | ((n >> 8) & 0xff00) |  ((n << 24) & 0xff000000);
}

static bool midi_parse_hdr(midi_t *const);
static bool midi_parse_track(const midi_t *const midi, midi_track_t *);
static bool midi_parse_track_hdr(const midi_t *const, midi_track_hdr_t *);
static void midi_set_error(midi_t *, int, const char *const, ...);
static void midi_prefix_errmsg(midi_t *, const char *const, ...);

static bool midi_check_magic(const uint8_t *const, const uint8_t *const, const size_t);

static uint16_t midi_parse_division(const midi_hdr_t *const);
static inline midi_event_node_t * midi_parse_event(const midi_t *const, unsigned int * const bytes);
static inline uint32_t midi_parse_delta_time(FILE * file, unsigned int * const bytes);

/**
 * Open a midi file given by the midi_file parameter.
 *
 * On success, 0 is returned and *midi = a (midi_t *) handle to the opened midi
 * file. On error, a POSIX errno is returned and *midi is NULL;
 *
 * On error, NULL is returned
 */
int midi_open(const char *const midi_file, midi_t **midi)
{
    FILE *file = NULL;
    int status;

    *midi = NULL;

    file = fopen(midi_file, "r");
    if (file == NULL) {
        return errno;
    }

    *midi = calloc(sizeof **midi, 1);

    if (*midi == NULL) {
        fclose(file);
        return ENOMEM;
    }

    (*midi)->midi_file = file;

    if (!midi_parse_hdr(*midi)) {
        midi_close(*midi);

        return EINVAL;
    }

    (*midi)->ppq = midi_parse_division(&((*midi)->hdr));

    // Just in case there are additional bytes in the header?
    status = fseek((*midi)->midi_file, (*midi)->hdr.length - (MIDI_HEADER_SIZE - 4 - 4), SEEK_CUR);

    if (status != -1) {
        (*midi)->trk_offset = ftell((*midi)->midi_file);

        return 0;
    }

    return errno;
}

void midi_close(midi_t *midi)
{
    if (midi != NULL && midi->midi_file != NULL) {
        fclose(midi->midi_file);
    }

    free(midi);
}

/**
 * <division> specifies the meaning of the delta-times.
 * It has two formats, one for metrical time, and one for time-code-based time:
 *      +------------+----+----------------+-------------+
 *      | Bit:       | 15 | 14 ......... 8 | 7 ....... 0 |
 *      +------------+----+----------------+-------------+
 *      | <division> |  0 | ticks per quarter note       |
 *      +            +----+----------------+-------------+
 *      |            |  1 | -frames/second | ticks/frame |
 *      +------------+----+----------------+-------------+
 *
 * If bit 15 of <division> is zero, the bits 14 thru 0 represent the number of delta time "ticks" which make up a quarter-note.
 * For instance, if division is 96, then a time interval of an eighth-note between two events in the file would be 48.
 *
 * If bit 15 of <division> is a one, delta times in a file correspond to subdivisions of a second,
 * in a way consistent with SMPTE and MIDI Time Code.
 *
 * Bits 14 thru 8 contain one of the four values -24, -25, -29, or -30, corresponding to the four standard SMPTE and MIDI Time Code formats,
 * and represents the number of frames per second.
 *
 *    -24 = 24 frames per second
 *    -25 = 25 frames per second
 *    -29 = 30 frames per second, drop frame
 *    -30 = 30 frames per second, non-drop frame
 *
 * The second byte (stored positive) is the resolution within a frame: typical values may be 4 (MIDI Time Code resolution), 8, 10, 80 (bit resolution), or 100.
 * This stream allows exact specifications of time-code-based tracks, but also allows millisecond-based tracks by specifying 25 frames/sec and a resolution of 40 units per frame.
 *
 * Reference:
 * - https://en.wikipedia.org/wiki/MIDI_timecode
 * - http://www.electronics.dit.ie/staff/tscarff/Music_technology/midi/MTC.htm
 * - http://www.harfesoft.de/aixphysik/sound/midi/pages/miditmcn.html
 * - http://bradthemad.org/guitar/tempo_explanation.php
 * - https://books.google.com/books?id=EodFbML75fkC&pg=PA56
 */
static uint16_t midi_parse_division(const midi_hdr_t *const hdr)
{

    // Metrical timing
	if ((hdr->division & 0x8000) == 0) {
        return (hdr->division & 0x7FFF);
	}

    // SMPTE and MIDI Time Code
    int8_t fps = (int8_t)((hdr->division >> 8) & 0xFF);

    switch (fps) {
        case -24:
        case -25:
            return ((int16_t)(-fps) * (hdr->division & 0x7F));
        case -29:
        case -30:
            return (30 * (hdr->division & 0x7F));
        default:
            // Invalid
            break;
    }

    return 0;
}

/**
 * Retrieve a MIDI track (midi_track_t*) including the track header.
 * Suitable for iteration with midi_iter_track.
 */
midi_track_t *midi_get_track(const midi_t *const midi, uint8_t track_idx)
{
    int status;
    midi_track_t *track = NULL;

    // TODO
    status = fseek(midi->midi_file, midi->trk_offset, SEEK_SET);

    if (status == -1) {
        midi_set_error((midi_t*)midi, errno, "fseek() failed.");
        return NULL;
    }

    midi_track_hdr_t trkhdr;

    for (int i = 0; i < track_idx; ++i) {
        if (midi_parse_track_hdr(midi, &trkhdr)) {
            // Seek past the track.
            status = fseek(midi->midi_file, trkhdr.size, SEEK_CUR);

            if (status == -1) {
                midi_set_error((midi_t*)midi, errno, "fseek() failed to seek past track %d header.", track_idx);
                return NULL;
            }

        } else {
            midi_prefix_errmsg((midi_t*)midi, "Failed to parse track %d header");
            return NULL;
        }
    }

    track = malloc(sizeof *track);
    if (track != NULL) {
        track->num = track_idx;

        if (!midi_parse_track(midi, track)) {
            midi_free_track(track);
            track = NULL;
        }
    }

    return track;
}

/**
 * Free a track previously allocated with midi_get_track()
 */
void midi_free_track(midi_track_t *trk)
{
    if (trk == NULL) {
        return;
    }

    trk->cur = trk->head;
    while (trk->cur != NULL) {
        midi_event_node_t *cur = trk->cur;
        trk->cur = trk->cur->next;
        free(cur);
    }

    free(trk);
}

/**
 * Read and parse the midi header from file into a midi_hdr_t structure.
 */
static bool midi_parse_hdr(midi_t *const midi)
{
    uint8_t buf[MIDI_HEADER_SIZE] = { 0 };
    midi_hdr_t *hdr = &midi->hdr;
    size_t ret = fread(buf, MIDI_HEADER_SIZE, 1, midi->midi_file);

    if (ret != 1) {
        return false;
    }

    if (!midi_check_magic(MIDI_HEADER_MAGIC, buf, sizeof(MIDI_HEADER_MAGIC))) {
        return false;
    }

    memcpy(hdr->magic, buf + MIDI_HEADER_MAGIC_OFFSET, sizeof(hdr->magic));
    hdr->length   = btol_32(*(uint32_t*)(buf + MIDI_HEADER_LENGTH_OFFSET));
    hdr->format   = btol_16(*(uint16_t*)(buf + MIDI_HEADER_FORMAT_OFFSET));
    hdr->tracks   = btol_16(*(uint16_t*)(buf + MIDI_HEADER_TRACKS_OFFSET));
    hdr->division = btol_16(*(uint16_t*)(buf + MIDI_HEADER_DIVISION_OFFSET));

    return true;
}

static bool midi_parse_track_hdr(const midi_t *const midi, midi_track_hdr_t *hdr)
{
    uint8_t buf[MIDI_TRACK_HEADER_SIZE] = { 0 };
    size_t ret = fread(buf, MIDI_TRACK_HEADER_SIZE, 1, midi->midi_file);

    if (ret != 1) {
        midi_set_error((midi_t*)midi, errno, "fread() failed to read track header.");

        return false;
    }

    memcpy(hdr->magic, buf + MIDI_TRACK_HEADER_MAGIC_OFFSET, sizeof(hdr->magic));
    hdr->size = btol_32(*(uint32_t*)(buf + MIDI_TRACK_HEADER_SIZE_OFFSET));

    if (!midi_check_magic(MIDI_TRACK_MAGIC, hdr->magic, sizeof(MIDI_TRACK_MAGIC))) {
        midi_set_error((midi_t*)midi, errno, "track has bad magic.");
        return false;
    }

    return true;
}

static bool midi_parse_track(const midi_t *const midi, midi_track_t *trk)
{
    midi_event_node_t *node;
    bool parsed = midi_parse_track_hdr(midi, &trk->hdr);

    if (!parsed) {
        return false;
    }

    unsigned int bytes = 0;
    trk->events = 0;

    node = midi_parse_event(midi, &bytes);

    if (node == NULL) {
        return false;
    }

    trk->head = node;
    trk->cur = trk->head;
    trk->events += 1;

    node = trk->head;

    while (bytes < trk->hdr.size) {
        node->next = midi_parse_event(midi, &bytes);
        if (node->next != NULL) {
            node = node->next;
            trk->events++;
        } else {
            // Something wrong
            return false;
        }
    }

    node->next = NULL;

    return true;
}

static inline midi_event_node_t *midi_parse_event(const midi_t *const midi, unsigned int *const bytes)
{
    /**
     * Per midi format, sometimes events may not contain  a command byte
     * And in this case, the "running command" from the last command byte is used.
     */
    static uint8_t running_cmd = 0;

#if DEBUG
    printf("Event: ");
#endif
    uint32_t delta_time = midi_parse_delta_time(midi->midi_file, bytes);

    midi_event_node_t * node;

    uint8_t cmdchan = 0;

    *bytes += fread(&cmdchan, 1, 1, midi->midi_file);
#if DEBUG
    printf(" %02x", cmdchan);
#endif

    // 0xFF = meta event.
    // TODO: split this out into a function for meta / event
    if (cmdchan == 0xFF)  {
        uint8_t cmd ;
        uint8_t size ;

        // xx, nn, dd = command, length, data...
        // Skip the command.
        *bytes += fread(&cmd, 1, 1, midi->midi_file);
        *bytes += fread(&size, 1, 1, midi->midi_file);
#if DEBUG
        printf(" %02x %02x", cmd, size);
#endif

        node = malloc((sizeof *node) + size);
        if (node == NULL) {
            midi_set_error((midi_t*)midi, EINVAL, "malloc() failed");
            return NULL;
        }

        fread(node->event.data, size, 1, midi->midi_file);
        *bytes += size;
#if DEBUG
        for (int i = 0; i < size; i++) {
            printf(" %02x", node->event.data[i]);
        }
#endif

        node->event.size = size;
        node->event.cmd = cmd;
        node->event.delta_time = delta_time;
        node->event.chan = 0;
        node->event.type = MIDI_EVENT_TYPE_META;

    } else {

        uint8_t cmd = (cmdchan >> 4) & 0x0F;
        uint8_t chan = cmdchan & 0x0F;
        uint8_t args[2];
        int argc = 0;
        int argn = 2;

        if (!(cmd & 0x08)) {
            cmd = running_cmd;
            args[argc++] = cmdchan;
        } else {
            running_cmd = cmd;
        }

        if (!(cmd & 0x08)) {
            midi_set_error((midi_t*)midi, EINVAL, "Invalid command, but none running :(.");
            return NULL;
        }

        /**
         * Only 1 data bytes follow after these two event
         */
        if (cmd == MIDI_EVENT_PROGRAM_CHANGE || cmd == MIDI_EVENT_CHANNEL_PRESSURE) {
            argn--;
        }

        node = malloc(sizeof(*node) + argn);
        if (node == NULL) {
            midi_set_error((midi_t*)midi, ENOMEM, "malloc() failed");
            return NULL;
        }

        for ( ; argc < argn; ++argc ) {
            *bytes += fread(&args[argc], 1, 1, midi->midi_file);
#if DEBUG
            printf(" %02x", args[argc]);
#endif
        }

        for (int i = 0; i < argn; ++i) {
            node->event.data[i] = args[i];
        }

        node->event.cmd = cmd;
        node->event.delta_time = delta_time;
        node->event.size = (uint8_t)argc;
        node->event.chan = chan;
        node->event.type = MIDI_EVENT_TYPE_EVENT;
    }

    node->next = NULL;
#if DEBUG
    printf("\n");
    midi_print_event(&node->event);
#endif

    return node;
}

static inline uint32_t midi_parse_delta_time(FILE *file, unsigned int *const bytes)
{
    uint8_t tmp[4] = { 0 };
    uint32_t delta_time = 0;
    int read = 0;
    int more;

    do {
        fread(&tmp[read], 1, 1, file);
#if DEBUG
        printf(" %02x", tmp[read]);
#endif
        more = tmp[read] & 0x80;
        tmp[read] &= 0x7F;
        delta_time = (delta_time << 7) | tmp[read];
        read++;
    } while (more && read < 4);

    *bytes += read;

    return delta_time;
}

void midi_print_event(midi_event_t * event)
{
    if (!event) {
        return ;
    }

    printf("delta_time: %5d, type: %d, cmd: 0x%02x, chan: %2d, size: %2d, data:",
            event->delta_time,
            event->type,
            event->cmd,
            event->chan,
            event->size
            );
    for (int i = 0; i < event->size; i++) {
        printf(" %02x", event->data[i]);
    }
    printf("\n");
}

static const char *event_str[] =
{
    /* 0x08 */ "NoteOff",
    /* 0x09 */ "NoteOn",
    /* 0x0A */ "AfterTouch",
    /* 0x0B */ "ControlChange",
    /* 0x0C */ "ProgramChange",
    /* 0x0D */ "ChannelPressure",
    /* 0x0E */ "PitchWheel",
    /* 0x0F */ "Meta",
};

const char *midi_get_event_str(uint8_t cmd)
{
    if (!(cmd & 0x08)) {
        return "Invalid cmd";
    }

    return event_str[cmd & 0x07];
}


void midi_iter_track(midi_track_t *trk)
{
    trk->cur = trk->head;
}

bool midi_track_has_next(midi_track_t *trk)
{
    return trk->cur != NULL;
}

midi_event_t *midi_track_next(midi_track_t *trk)
{
    midi_event_node_t *cur = trk->cur;
    trk->cur = trk->cur->next;

    return &cur->event;
}

static void midi_set_error(midi_t *const midi, const int midi_errno, const char *const errmsg, ...)
{
    va_list ap;
    va_start(ap, errmsg);
    vsnprintf(midi->errmsg, sizeof(midi->errmsg), errmsg, ap);
    midi->errnum = midi_errno;
    va_end(ap);
}

static void midi_prefix_errmsg(midi_t *const midi, const char *const errmsg, ...)
{
    size_t size = sizeof(midi->errmsg);
    char old_errmsg[size];
    va_list ap;
    int bytes;


    strncpy(old_errmsg, midi->errmsg, size - 1);
    old_errmsg[size - 1] = '\0';
    va_start(ap, errmsg);
    bytes = vsnprintf(midi->errmsg, size, errmsg, ap);
    va_end(ap);

    if ((size_t)bytes < size) {
        snprintf(&midi->errmsg[bytes], size - bytes, ": %s", old_errmsg);
    }
}

const char *midi_get_errmsg(const midi_t *const midi)
{
    return midi->errmsg;
}

int midi_get_errno(const midi_t *const midi)
{
    return midi->errnum;
}

static bool midi_check_magic(const uint8_t *const expected, const uint8_t *const check, const size_t magic_size)
{
    return (memcmp(check, expected, magic_size) == 0);
}
