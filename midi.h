#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Header Chunk
 * +-----------------+-------------------------+----------------------------------+
 * | Chunk Type      | Length                  | Data                             |
 * +-----------------+-------------------------+----------------------------------+
 * | 4 bytes (ascii) | 4 bytes (32-bit binary) |    <-- length (= 6 bytes) -->    |
 * +                 |                         +----------+----------+------------+
 * |                 |                         | 16-bit   | 16-bit   | 16-bit     |
 * +-----------------+-------------------------+----------+----------+------------+
 * | MThd            | <length>                | <format> | <tracks> | <division> |
 * +-----------------+-------------------------+----------+----------+------------+
 *
 * "MThd"   - the literal string MThd, or in hexadecimal notation: 0x4d546864.
 * <length> - length of the header chunk (always 6 bytes long).
 * <format> - the midi file format
 *      0 = single track file format
 *      1 = multiple track file format
 *      2 = multiple song file format (i.e., a series of type 0 files)
 * <tracks> - number of track chunks that follow the header chunk
 * <division> - unit of time for delta timing.
 *      +------------+----+----------------+-------------+
 *      | Bit:       | 15 | 14 ......... 8 | 7 ....... 0 |
 *      +------------+----+----------------+-------------+
 *      | <division> |  0 | ticks per quarter note       |
 *      +------------+----+----------------+-------------+
 *      |            |  1 | -frames/second | ticks/frame |
 *      +------------+----+----------------+-------------+
 *
 */
#define MIDI_HEADER_SIZE                14
#define MIDI_HEADER_MAGIC_OFFSET        0
#define MIDI_HEADER_HSIZE_OFFSET        4
#define MIDI_HEADER_FORMAT_OFFSET       8
#define MIDI_HEADER_TRACKS_OFFSET       10
#define MIDI_HEADER_DD_OFFSET           12

typedef struct {
    uint8_t     magic[4];
    uint32_t    length;
    uint16_t    format;
    uint16_t    tracks;
    int16_t     division;
} midi_hdr_t;

/**
 *
 * Track Chunk
 * +---------+-----------------+--------------------------+
 * | Type    | Length          | Data                     |
 * +---------+-----------------+--------------------------+
 * | 4 bytes | 4 bytes         | <-- length bytes -->     |
 * | (ascii) | (32-bit binary) | (binary data)            |
 * +---------+-----------------+--------------------------+
 * | MTrk    | <length>        | <delta_time> <event> ... |
 * +---------+-----------------+--------------------------+
 *
 * <delta_time> - is the number of 'ticks' from the previous event, and is represented as a variable length quantity
 * <event> - is one of:
 *      <midi_event>
 *      <sysex_event>
 *      <meta_event>
 */
#define MIDI_TRACK_HEADER_SIZE          8
#define MIDI_TRACK_HEADER_MAGIC_OFFSET  0
#define MIDI_TRACK_HEADER_SIZE_OFFSET   4

typedef struct {
    uint8_t     magic[4];
    uint32_t    size;
} midi_track_hdr_t;

typedef struct {
    uint32_t    td;

    enum {
        MIDI_TYPE_EVENT,
        MIDI_TYPE_META
    } type;

    uint8_t cmd;

    // Always 0 for meta events.
    uint8_t chan;

    // Size of data
    uint8_t size;
    uint8_t data[];
} midi_event_t;


typedef struct midi_event_node_s {
    struct midi_event_node_s *next;
    midi_event_t event;
} midi_event_node_t;


typedef struct {
    midi_track_hdr_t hdr;
    uint32_t events;
    uint8_t num;
    midi_event_node_t *head;
    midi_event_node_t *cur;
} midi_track_t;

typedef struct {
    midi_hdr_t hdr;

    char errmsg[512];
    int errnum;

    // Offset to first track.
    uint8_t trk_offset;

    FILE *midi_file;
} midi_t;


int midi_open(const char *const midi_file, midi_t **);
void midi_close(midi_t *midi);
midi_track_t *midi_get_track(const midi_t *const midi, uint8_t n);
void midi_free_track(midi_track_t *trk);

/**
 * Track iteration
 */

void midi_iter_track(midi_track_t *trk);
bool midi_track_has_next(midi_track_t *trk);
midi_event_t *midi_track_next(midi_track_t *trk);

// Print a textual meta argument
void midi_printmeta(midi_event_t *meta);

// Convert event->cmd to a string
char *midi_get_eventstr(uint8_t cmd);

const char *midi_get_errstr(const midi_t *const);
int midi_get_errno(const midi_t *const);


#define MIDI_EVENT_NOTE_OFF                 0x08
#define MIDI_EVENT_NOTE_ON                  0x09
#define MIDI_EVENT_KEY_AFTER_TOUCH          0x0A
#define MIDI_EVENT_CONTROL_CHANGE           0x0B
#define MIDI_EVENT_PATH_CHANGE              0x0C
#define MIDI_EVENT_CHANNEL_AFTER_TOUCH      0x0D
#define MIDI_EVENT_PITCH_WHEEL_CHANGE       0x0E

