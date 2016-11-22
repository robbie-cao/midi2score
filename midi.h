#ifndef __MIDI_H__
#define __MIDI_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * http://cs.fit.edu/~ryan/cse4051/projects/midi/midi.html
 *
 * MIDI File Structure
 *
 * Chunks
 *
 * MIDI files are structured into chunks.
 * Each chunk consists of:
 *      - A 4-byte chunk type (ascii)
 *      - A 4-byte length (32 bits, msb first)
 *      - length bytes of data
 *
 * +---------+---------+--------------+
 * | Type    | Length  | Data         |
 * +---------+---------+--------------+
 * | 4 bytes | 4 bytes | length bytes |
 * +---------+---------+--------------+
 *
 * There are two types of chunks:
 *      - Header Chunks - which have a chunk type of "MThd"
 *      - Track Chunks  - which have a chunk type of "MTrk"
 *
 * A MIDI file consists of a single header chunk followed by one or more track chunks.
 *
 * Since the length-field is mandatory in the structure of chunks, it is possible to accomodate chunks
 * other than "MThd" or "MTrk" in a MIDI file, by skipping over their contents. The MIDI specification
 * requires that software be able to handle unexpected chunk-types by ignoring the entire chunk.
 *
 * +------+-------------------------------------------------+
 * |      |                 <---Chunk--->                   |
 * +------+-------+----------+------------------------------+
 * |      | type  | length   | Data                         |
 * +------+-------+----------+------------------------------+
 * | MIDI | MThd  | 6        | <format> <tracks> <division> |
 * |      +-------+----------+------------------------------+
 * | File | MTrk  | <length> | <delta_time> <event> ...     |
 * |      +-------+----------+------------------------------+
 * | :    |                     :                           |
 * |      +-------+----------+------------------------------+
 * |      | MTrk  | <length> | <delta_time> <event> ...
 * +------+-------+----------+------------------------------+
 *
 */

/**
 * Header Chunk
 * +----------+-----------------+----------------------------------+
 * | Type     | Length          | Data                             |
 * +----------+-----------------+----------------------------------+
 * | 4 bytes  | 4 bytes         |    <-- length (= 6 bytes) -->    |
 * | (ascii)  | (32-bit binary) +----------+----------+------------+
 * |          |                 | 16-bit   | 16-bit   | 16-bit     |
 * +----------+-----------------+----------+----------+------------+
 * | MThd     | <length>        | <format> | <tracks> | <division> |
 * +----------+-----------------+----------+----------+------------+
 *
 * "MThd"   - the literal string MThd, or in hexadecimal notation: 0x4d546864
 * <length> - length of the header chunk (always 6 bytes long)
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
#define MIDI_HEADER_LENGTH_OFFSET       4
#define MIDI_HEADER_FORMAT_OFFSET       8
#define MIDI_HEADER_TRACKS_OFFSET       10
#define MIDI_HEADER_DIVISION_OFFSET     12

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

enum {
    MIDI_EVENT_TYPE_EVENT,
    MIDI_EVENT_TYPE_META
};

typedef struct {
    uint32_t    delta_time;
    uint8_t     type;
    uint8_t     cmd;
    uint8_t     chan;       // Always 0 for meta events.
    uint8_t     size;       // Size of data
    uint8_t     data[];
} midi_event_t;


typedef struct midi_event_node {
    struct midi_event_node *next;
    midi_event_t            event;
} midi_event_node_t;


typedef struct {
    midi_track_hdr_t    hdr;
    uint32_t            events; // Total count of events in a track chunk
    uint8_t             num;    // No. of track
    midi_event_node_t * head;
    midi_event_node_t * cur;
} midi_track_t;

typedef struct {
    FILE *      midi_file;
    midi_hdr_t  hdr;
    uint8_t     trk_offset;     // Offset to first track

    char errmsg[512];
    int errnum;
} midi_t;

/**
 * Usage Sample:
 *
 * midi_t *midi;
 * midi_track_t *track;
 *
 * midi_open(midi_file_name, midi);
 *
 * for (int i = 0; i < midi->hdr.tracks; ++i) {
 *     track =  midi_get_track(midi, i);
 *
 *     midi_iter_track(track);
 *     midi_event_t * evnt;
 *     while (midi_track_has_next(track)) {
 *         evnt = midi_track_next(track);
 *         // Do something
 *     }
 *
 *     if (track != NULL) {
 *         midi_free_track(track);
 *     }
 * }
 *
 * midi_close(midi);
 */

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

/**
 * Helper functions
 */
// Print a textual parsed event
void midi_print_event(midi_event_t *event);

// Convert event->cmd to a string
const char *midi_get_event_str(uint8_t cmd);

const char *midi_get_errmsg(const midi_t *const);
int midi_get_errno(const midi_t *const);


#define MIDI_EVENT_NOTE_OFF                 0x08
#define MIDI_EVENT_NOTE_ON                  0x09
#define MIDI_EVENT_AFTER_TOUCH              0x0A
#define MIDI_EVENT_CONTROL_CHANGE           0x0B
#define MIDI_EVENT_PROGRAM_CHANGE           0x0C
#define MIDI_EVENT_CHANNEL_PRESSURE         0x0D
#define MIDI_EVENT_PITCH_WHEEL              0x0E

enum {
    MIDI_META_SEQUENCE_NUM          = 0x00,
    MIDI_META_TEXT_EVNT             = 0x01,
    MIDI_META_COPYRIGHT_NOTICE      = 0x02,
    MIDI_META_SEQUENCE_NAME         = 0x03,
    MIDI_META_INSTRUMENT_NAME       = 0x04,
    MIDI_META_LYRICS                = 0x05,
    MIDI_META_MARKER                = 0x06,
    MIDI_META_CUE_POINT             = 0x07,
    MIDI_META_CHANNEL_PREFIX        = 0x20,
    MIDI_META_END_TRACK             = 0x2F,
    MIDI_META_TEMPO_CHANGE          = 0x51,
    MIDI_META_SMPTE_OFFSET          = 0x54,
    MIDI_META_TIME_SIGNATURE        = 0x58,
    MIDI_META_KEY_SIGNATURE         = 0x59,
    MIDI_META_SEQUENCER_SPECIFIC    = 0x7F
};

/**
 * Control Numbers for 0xBn
 */
typedef enum {
    /* Coarse Control */
    MIDI_CTRL_BANK_SELECT           = 0x00,
    MIDI_CTRL_MODULATION_WHEEL      = 0x01,
    MIDI_CTRL_BREATH                = 0x02,
    MIDI_CTRL_FOOT_PEDAL            = 0x04,
    MIDI_CTRL_PORTAMENTO_TIME       = 0x05,
    MIDI_CTRL_DATA_ENTRY            = 0x06,
    MIDI_CTRL_VOLUME                = 0x07,
    MIDI_CTRL_BALANCE               = 0x08,
    MIDI_CTRL_PAN_POSITION          = 0x0A,
    MIDI_CTRL_EXPRESSION            = 0x0B,
    MIDI_CTRL_EFFECT_1              = 0x0C,
    MIDI_CTRL_EFFECT_2              = 0x0D,
    MIDI_CTRL_GENERAL_1             = 0x10,
    MIDI_CTRL_GENERAL_2             = 0x11,
    MIDI_CTRL_GENERAL_3             = 0x12,
    MIDI_CTRL_GENERAL_4             = 0x13,

    /* Fine Control */
    // 0x20 to 0x2D: Same Control as 0x00-0x0D but with fine params

    /* Pedal On/Off Control */
    MIDI_CTRL_HOLD_PEDAL            = 0x40,
    MIDI_CTRL_PORTAMENTO            = 0x41,
    MIDI_CTRL_SOSTENUTO_PEDAL       = 0x42,
    MIDI_CTRL_SOFT_PEDAL            = 0x43,
    MIDI_CTRL_LEGATO_PEDAL          = 0x44,
    MIDI_CTRL_HOLD_2_PEDAL          = 0x45,

    /* Sound Control */
    MIDI_CTRL_SOUND_VARIATION       = 0x46,
    MIDI_CTRL_SOUND_TIMBRE          = 0x47,
    MIDI_CTRL_SOUND_RELEASE_TIME    = 0x48,
    MIDI_CTRL_SOUND_ATTACK_TIME     = 0x49,
    MIDI_CTRL_SOUND_BRIGHTNESS      = 0x4A,
    MIDI_CTRL_SOUND_CONTROL_6       = 0x4B,
    MIDI_CTRL_SOUND_CONTROL_7       = 0x4C,
    MIDI_CTRL_SOUND_CONTROL_8       = 0x4D,
    MIDI_CTRL_SOUND_CONTROL_9       = 0x4E,
    MIDI_CTRL_SOUND_CONTROL_10      = 0x4F,

    /* Button Control */
    MIDI_CTRL_GENERAL_BUTTON_1      = 0x50,
    MIDI_CTRL_GENERAL_BUTTON_2      = 0x51,
    MIDI_CTRL_GENERAL_BUTTON_3      = 0x52,
    MIDI_CTRL_GENERAL_BUTTON_4      = 0x5e,

    /* Level Control */
    MIDI_CTRL_EFFECTS_LEVEL         = 0x5B,
    MIDI_CTRL_TREMULO_LEVEL         = 0x5C,
    MIDI_CTRL_CHORUS_LEVEL          = 0x5D,
    MIDI_CTRL_CELESTE_LEVEL         = 0x5E,
    MIDI_CTRL_PHASER_LEVEL          = 0x5F,

    MIDI_CTRL_DATA_BUTTON_INC       = 0x60,
    MIDI_CTRL_DATA_BUTTON_DEC       = 0x61,
    MIDI_CTRL_NON_REG_PARAM_FINE    = 0x62,
    MIDI_CTRL_NON_REG_PARAM_COARSE  = 0x63,
    MIDI_CTRL_REG_PARAM_FINE        = 0x64,
    MIDI_CTRL_REG_PARAM_COARSE      = 0x65,

    MIDI_CTRL_ALL_SOUND_OFF         = 0x78,
    MIDI_CTRL_ALL_CONTROLLERS_OFF   = 0x79,
    MIDI_CTRL_LOCAL_KEYBOARD        = 0x7A,
    MIDI_CTRL_ALL_NOTES_OFF         = 0x7B,
    MIDI_CTRL_OMNI_MODE_OFF         = 0x7C,
    MIDI_CTRL_OMNI_MODE_ON          = 0x7D,
    MIDI_CTRL_MONO_OPERATION        = 0x7E,
    MIDI_CTRL_POLY_OPERATION        = 0x7F,
} ControllerType;

#endif
