# midi2score

Translate midi file to score.

## File Format

```
MIDI File:

     +----------------+
     | header         |   -> ppq (pulse(ticks) per quarternote
     |                |
     +----------------+
     | track 0        |   -> tempo
     |                |   -> time signature
     |                |   -> key signature
     |                |
     +----------------+
     | track 1        |   -> note 1 with note / sharp / octaves / length
     |                |   -> note 2 with note / sharp / octaves / length
     |                |   -> :
     |                |   -> :
     |                |
     |                |
     |                |
     |                |
     |                |
     |                |
     +----------------+

Score File:

Byte 0           1           2           3           4
     +-----------+-----------+-----------+-----------+
   0 |     M     |     S     |     S     |     C     |
     +-----------+-----------+-----------+-----------+
   4 | Clef      | Key Sign  | Time Sign | Reserved  |
     +-----------+-----------+-----------+-----------+
   8 | Size(MSB) | Size(LSB) | Reserved  | Reserved  |
     +-----------+-----------+-----------+-----------+
  12 | Note 1    | Note 1    | Note 2    | Note 3    |
     +-----------+-----------+-----------+-----------+
     | ....      | ....      | ....      | ....      |
     | ....      | ....      | ....      | ....      |
     | ....      | ....      | ....      | ....      |
     | ....      | ....      |           |           |
     |           |           |           |           |
     |           |           |           |           |
     +-----------+-----------+-----------+-----------+
```

## API Usage

```
Usage Sample:

void foo()
{
    midi_t *midi;
    midi_track_t *track;

    midi_open(midi_file_name, midi);

    for (int i = 0; i < midi->hdr.tracks; ++i) {
        track =  midi_get_track(midi, i);

        midi_iter_track(track);
        midi_event_t * event;
        while (midi_track_has_next(track)) {
            event = midi_track_next(track);
            // Do something
        }

        if (track != NULL) {
            midi_free_track(track);
        }
    }

    midi_close(midi);
}
```

## Reference

- https://www.midi.org
- https://www.midi.org/specifications
- http://oktopus.hu/uploaded/Tudastar/MIDI%201.0%20Detailed%20Specification.pdf
- http://www.ccarh.org/courses/253/handout/smf/
- http://cs.fit.edu/~ryan/cse4051/projects/midi/midi.html
- https://www.cs.cmu.edu/~music/cmsip/readings/Standard-MIDI-file-format-updated.pdf
- https://en.wikipedia.org/wiki/MIDI_1.0
- https://en.wikipedia.org/wiki/MIDI
- https://github.com/abique/midi-parser
- https://github.com/topnotcher/midi
