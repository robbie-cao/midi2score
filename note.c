#include "note.h"
#include "key.h"

uint8_t NumNotaiton_NoteToKeyNote(Note_t note)
{
    return 60;
}

uint8_t NumNotaiton_NoteSimpToKeyNote(NoteSimplified_t note)
{
    //calculate the note from simplified note
    const uint8_t offset[] = { 0, 2, 4, 5, 7, 9, 11 };
    uint8_t octOffset = (note.octaves & 0x2) ? (4 - note.octaves) : (- note.octaves);

    if (note.note == 0) {
        return 0;
    }

    return KEY_PIANO_60 + offset[note.note - 1] + note.sharp + octOffset * KEY_OFFSET_PER_DEGREE;
}

/* vim: set ts=4 sw=4 tw=0 list : */
