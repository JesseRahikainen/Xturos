#ifndef TUNING_H
#define TUNING_H

#include <stdint.h>

#define MIN_OCTAVE 0
#define MAX_OCTAVE 8

typedef enum {
	NOTE_C,
	NOTE_CS, NOTE_DF = NOTE_CS,
	NOTE_D,
	NOTE_DS, NOTE_EF = NOTE_DS,
	NOTE_E,
	NOTE_F,
	NOTE_FS, NOTE_GF = NOTE_FS,
	NOTE_G,
	NOTE_GS, NOTE_AF = NOTE_GS,
	NOTE_A,
	NOTE_AS, NOTE_BF = NOTE_AS,
	NOTE_B,
	NUM_NOTES
} OctaveNote;

typedef struct {
	int8_t octave;
	OctaveNote octaveNote;
} Note;

typedef enum {
	TUNING_EQUAL_TEMPERAMENT,
	TUNING_PYTHAGOREAN,
	NUM_TUNINGS
} TuningIntervals;

typedef struct {
	double freqs[(MAX_OCTAVE + 1 ) * NUM_NOTES];
} Tuning;

void tuning_Setup( Tuning* tuning, double a4Hertz, TuningIntervals tuningInterval );
void tuning_SetupStandard( Tuning* tuning );
double tuning_GetNoteHertz( Note* note, Tuning* tuning );
float tuning_NotePitchAdjustment( Note* referenceNote, Note* note, Tuning* tuning );
float tuning_HertzPitchAdjustment( double referenceNoteHertz, Note* note, Tuning* tuning );

//=============================================================
// for music stuff

typedef enum {
	SCALE_CHROMATIC,
	SCALE_MAJOR, SCALE_MAJOR_IONIAN = SCALE_MAJOR,
	SCALE_MAJOR_DORIAN,
	SCALE_MAJOR_PHYRIGIAN,
	SCALE_MAJOR_LYDIAN,
	SCALE_MAJOR_MIXOLYDIAN,
	SCALE_MAJOR_AEOLIAN,
	SCALE_MAJOR_LOCRIAN,
	SCALE_NATURAL_MINOR,
	SCALE_HARMONIC_MINOR,
	SCALE_MELODIC_MINOR_DOWN,
	SCALE_MELODIC_MINOR_UP,
	SCALE_MAJOR_PENTATONIC,
	SCALE_MINOR_PENTATONIC,
	SCALE_HUNGARIAN_MINOR,
	SCALE_AHAVA_RABA,
	SCALE_PERSIAN,
	SCALE_STEVE,
	SCALE_OCTATONIC_ONE,
	SCALE_OCTATONIC_TWO,
	SCALE_WHOLE_TONE
} Scales;

// adds a number of steps to the base note and returns the resulting not, if the steps would put it out of range it clamps
Note music_AddStepsToNote( Note* baseNote, int steps );

// returns a stretchy buffer, need to make sure it's cleaned up properly
Note* music_GetScaleSB( Note* tonic, Scales scale );

typedef enum {
	CHORD_TYPE_MAJOR_TRIAD,
	CHORD_TYPE_MINOR_TRIAD,
	CHORD_TYPE_DIMINISHED_TRIAD,
	CHORD_TYPE_AUGMENTED_TRIAD,
	CHORD_TYPE_MAJOR_SEVENTH,
	CHORD_TYPE_DOMINANT_SEVENTH,
	CHORD_TYPE_MINOR_SEVENTH,
	CHORD_TYPE_MINOR_MAJOR_SEVENTH,
	CHORD_TYPE_HALF_DIMINISHED_SEVENTH,
	CHORD_TYPE_DIMINISHED_SEVENTH,
	CHORD_TYPE_AUGMENTED_MAJOR_SEVENTH,
	CHORD_TYPE_AUGMENTED_SEVENTH
} ChordType;

Note* music_GetChordSB( Note* tonic, ChordType chordType );

#endif // inclusion guard
