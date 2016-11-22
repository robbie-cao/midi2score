#include <string.h>

#include "note.h"
#include "key.h"

uint8_t NumNotaiton_NoteToKeyNote(Note_t note)
{
    const uint8_t offset[] = { 0, 2, 4, 5, 7, 9, 11 };
    uint8_t octOffset = 0;

    if (note.note == 0) {
        return 0;
    }

    /**
     * Octaves map:
     * +---------+-----+-----+-----+-----+-----+-----+-----+-----+
     * | octaves | -3  | -2  | -1  | 0   | 1   | 2   | 3   | 4   |
     * +---------+-----+-----+-----+-----+-----+-----+-----+-----+
     * | bit 2-0 | 111 | 110 | 001 | 000 | 011 | 010 | 101 | 100 |
     * +---------+-----+-----+-----+-----+-----+-----+-----+-----+
     *
     * Algorithm:
     *
     * if (note.oct2) {
     *     // b2 == 1
     *     if (note.octaves & 0x2) {
     *         // b1 == 1
     *         octOffset = note.octaves - 4;
     *     } else {
     *         // b1 == 0
     *         octOffset = 4 - note.octaves;
     *     }
     * } else {
     *     // b2 == 0
     *     if (note.octaves & 0x2) {
     *         // b1 == 1
     *         octOffset = 4 - note.octaves;
     *     } else {
     *         // b1 == 0
     *         octOffset = 0 - note.octaves;
     *     }
     * }
     */
    if (note.oct2) {
        octOffset = (note.octaves & 0x2) ? (note.octaves - 4) : (4 - note.octaves);
    } else {
        octOffset = (note.octaves & 0x2) ? (4 - note.octaves) : (- note.octaves);
    }

    return KEY_PIANO_60 + offset[note.note - 1] + note.sharp + octOffset * KEY_OFFSET_PER_DEGREE;
}

uint8_t NumNotaiton_NoteSimpToKeyNote(NoteSimplified_t note)
{
    // Calculate the note from simplified note
    const uint8_t offset[] = { 0, 2, 4, 5, 7, 9, 11 };
    uint8_t octOffset = (note.octaves & 0x2) ? (4 - note.octaves) : (- note.octaves);

    if (note.note == 0) {
        return 0;
    }

    return KEY_PIANO_60 + offset[note.note - 1] + note.sharp + octOffset * KEY_OFFSET_PER_DEGREE;
}

/**
 * Note with dot in an octave
 */
struct NoteSharp {
    uint8_t note;
    uint8_t sharp;
} noteSharpMap[] = {
    { 1, 0 },   /* C  */
    { 1, 1 },   /* C# */
    { 2, 0 },   /* D  */
    { 2, 1 },   /* D# */
    { 3, 0 },   /* E  */
    { 4, 0 },   /* F  */
    { 4, 1 },   /* F# */
    { 5, 0 },   /* G  */
    { 5, 1 },   /* G# */
    { 6, 0 },   /* A  */
    { 6, 1 },   /* A# */
    { 7, 0 },   /* B  */
};

Note_t NumNotaiton_KeyToNote(uint8_t key, uint8_t length, uint8_t dot)
{
    // Key range: 24 ~ 108
    Note_t note;
    uint8_t offset = key % 12;
    int8_t octave = (key - KEY_PIANO_24) / 12;

    struct {
        uint8_t oct2;
        uint8_t octaves;
    } octaveMap[] = {
        { 1, 3 }, /* Octave: -3, Key:  24 -  35 */
        { 1, 2 }, /* Octave: -2, Key:  36 -  47 */
        { 0, 1 }, /* Octave: -1, Key:  48 -  59 */
        { 0, 0 }, /* Octave:  0, Key:  60 -  71 */
        { 0, 3 }, /* Octave:  1, Key:  72 -  83 */
        { 0, 2 }, /* Octave:  2, Key:  84 -  95 */
        { 1, 1 }, /* Octave:  3, Key:  96 - 107 */
        { 1, 0 }, /* Octave:  4, Key: 108 - 119 */
    };

    if (key < KEY(24) || key >= KEY(108)) {
        memset(&note, 0, sizeof(note));
        return note;
    }

    note.note = noteSharpMap[offset].note;
    note.sharp = noteSharpMap[offset].sharp;
    note.dot = !!dot;
    note.length = (length & 0x3);
    note.len2 = !!(length & 0x4);
    note.octaves = octaveMap[octave].octaves;
    note.oct2 = octaveMap[octave].oct2;

    return note;
}

NoteSimplified_t NumNotaiton_KeyToNoteSimp(uint8_t key, uint8_t length)
{
    // Key range: 48 ~ 96
    NoteSimplified_t  note;
    uint8_t offset = key % 12;
    int8_t octave = (key - KEY_PIANO_48) / 12;

    uint8_t octaves[] = {
        1, /* Octave: -1, Key:  48 -  59 */
        0, /* Octave:  0, Key:  60 -  71 */
        3, /* Octave:  1, Key:  72 -  83 */
        2, /* Octave:  2, Key:  84 -  95 */
    };

    if (key < KEY(48) || key >= KEY(96)) {
        memset(&note, 0, sizeof(note));
        return note;
    }

    note.note = noteSharpMap[offset].note;
    note.sharp = noteSharpMap[offset].sharp;
    note.length = (length & 0x3);
    note.octaves = octaves[octave];

    return note;
}

/* vim: set ts=4 sw=4 tw=0 list : */
