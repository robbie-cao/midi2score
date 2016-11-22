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

/* vim: set ts=4 sw=4 tw=0 list : */
