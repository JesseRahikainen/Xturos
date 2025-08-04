#include "tuning.h"

#include <stdlib.h>

#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"

// https://musiccrashcourses.com/lessons/tuning_systems.html

#define FULL_OCTAVE_CENTS 1200.0

static const double EQUAL_TEMPERAMENT[] = { 0.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0, 1100.0 };
static const double PYTHAGOREAN_TUNING[] = { 0.0, 90.225, 203.910, 294.135, 407.820, 588.270, 611.730, 701.955, 792.180, 905.865, 996.090, 1109.775 };

static size_t getIndex( int8_t octave, OctaveNote note )
{
	ASSERT_AND_IF_NOT( octave >= MIN_OCTAVE ) return 0;
	ASSERT_AND_IF_NOT( octave <= MAX_OCTAVE ) return 0;

	return ( octave * NUM_NOTES ) + note;
}

static Note fromIndex( size_t index )
{
	const size_t MAX_INDEX = MAX_OCTAVE * NUM_NOTES + ( NUM_NOTES - 1 );
	ASSERT_AND_IF_NOT( index <= MAX_INDEX ) {
		Note result = { MAX_OCTAVE, NOTE_B };
		return result;
	}

	ASSERT_AND_IF_NOT( index >= 0 ) {
		Note result = { MIN_OCTAVE, NOTE_C };
		return result;
	}

	Note result = { (int8_t)( index / NUM_NOTES ), (OctaveNote)( index % NUM_NOTES ) };
	return result;
}

static size_t getNoteIndex( Note* note )
{
	return getIndex( note->octave, note->octaveNote );
}

static double generateNoteHertz( double a4Hertz, int8_t octave, OctaveNote note, const double intervalCents[NUM_NOTES] )
{
	// size in cents from f1 to f2: cents = 1200 * log2(f2/f1)
	// f1 from f2 given difference in cents: f1 = f2 * 2^(cents/1200)
	// generate the difference based on the octave
	double cents = ( octave - 4 ) * FULL_OCTAVE_CENTS; // gives base value for the given octave
	cents += intervalCents[note] - intervalCents[NOTE_A]; // gives the note within the octave, the octave is base C, while the tuning tone is A
	double result = a4Hertz * pow( 2.0, cents / FULL_OCTAVE_CENTS );
	return result;
}

static void setup( Tuning* tuning, double a4Hertz, const double cents[NUM_NOTES] )
{
	SDL_assert( tuning != NULL );

	for( int8_t octave = MIN_OCTAVE; octave <= MAX_OCTAVE; ++octave ) {
		for( OctaveNote note = NOTE_C; note < NUM_NOTES; ++note ) {
			size_t index = getIndex( octave, note );
			tuning->freqs[index] = generateNoteHertz( a4Hertz, octave, note, cents );
		}
	}
}

void tuning_Setup( Tuning* tuning, double a4Hertz, TuningIntervals tuningInterval )
{
	const double* cents = NULL;
	switch( tuningInterval ) {
	case TUNING_EQUAL_TEMPERAMENT:
		cents = EQUAL_TEMPERAMENT;
		break;
	case TUNING_PYTHAGOREAN:
		cents = PYTHAGOREAN_TUNING;
		break;
	default:
		SDL_assert( false && "Invalid tuningInterval." );
		cents = EQUAL_TEMPERAMENT;
		break;
	}
	setup( tuning, a4Hertz, cents );
}

void tuning_SetupStandard( Tuning* tuning )
{
	SDL_assert( tuning != NULL );
	tuning_Setup( tuning, 440.0, TUNING_EQUAL_TEMPERAMENT );
}

double tuning_GetNoteHertz( Note* note, Tuning* tuning )
{
	ASSERT_AND_IF_NOT( tuning != NULL ) return 0.0;
	ASSERT_AND_IF_NOT( note != NULL ) return 0.0;
	ASSERT_AND_IF_NOT( note->octave >= MIN_OCTAVE && note->octave <= MAX_OCTAVE ) return 0.0;

	return tuning->freqs[getNoteIndex( note )];
}

float tuning_NotePitchAdjustment( Note* referenceNote, Note* note, Tuning* tuning )
{
	SDL_assert( referenceNote != NULL );
	SDL_assert( note != NULL );
	SDL_assert( tuning != NULL );

	double referenceHertz = tuning_GetNoteHertz( referenceNote, tuning );
	double hertz = tuning_GetNoteHertz( note, tuning );
	return (float)( hertz / referenceHertz );
}

float tuning_HertzPitchAdjustment( double referenceNoteHertz, Note* note, Tuning* tuning )
{
	SDL_assert( note != NULL );
	SDL_assert( tuning != NULL );

	double hertz = tuning_GetNoteHertz( note, tuning );
	return (float)( hertz / referenceNoteHertz );
}

//=============================================================
// for music stuff

const static int CHROMATIC_SCALE[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
const static int MAJOR_SCALE[] = { 0, 2, 4, 5, 7, 9, 11 };
const static int MAJOR_DORIAN_SCALE[] = { 0, 2, 3, 5, 7, 9, 10 };
const static int MAJOR_PHYRGIAN_SCALE[] = { 0, 1, 3, 5, 7, 8, 10 };
const static int MAJOR_LYDIAN_SCALE[] = { 0, 2, 4, 6, 7, 9, 11 };
const static int MAJOR_MIXOLYDIAN_SCALE[] = { 0, 2, 4, 5, 7, 9, 10 };
const static int MAJOR_AEOILAN_SCALE[] = { 0, 2, 3, 5, 7, 8, 10 };
const static int MAJOR_LOCRIAN_SCALE[] = { 0, 1, 3, 5, 6, 8, 10 };
const static int NATURAL_MINOR_SCALE[] = { 0, 2, 3, 4, 7, 8, 10 };
const static int HARMONIC_MINOR_SCALE[] = { 0, 2, 3, 5, 7, 8, 11 };
const static int MELODIC_MINOR_DOWN_SCALE[] = { 0, 2, 4, 5, 7, 9, 10 };
const static int MELODIC_MINOR_UP_SCALE[] = { 0, 2, 3, 5, 7, 9, 11 };
const static int MAJOR_PENTATONIC_SCALE[] = { 0, 2, 4, 7, 9 };
const static int MINOR_PENTATONIC_SCALE[] = { 0, 3, 5, 7, 10 };
const static int HUNGARIAN_MINOR_SCALE[] = { 0, 2, 3, 6, 7, 8, 11 };
const static int AHAVA_RABA_SCALE[] = { 0, 1, 4, 5, 7, 8, 10 };
const static int PERSIAN_SCALE[] = { 0, 1, 4, 5, 6, 8, 11 };
const static int STEVE_SCALE[] = { 0, 1, 3, 4, 5, 7, 8, 10 };
const static int OCTATONIC_ONE_SCALE[] = { 0, 1, 3, 4, 6, 7, 9, 10 };
const static int OCTATONIC_TWO_SCALE[] = { 0, 2, 3, 5, 6, 8, 9, 11 };
const static int WHOLE_TONE_SCALE[] = { 0, 2, 4, 6, 8, 10 };

// adds a number of steps to the base note and returns the resulting not, if the steps would put it out of range it clamps
Note music_AddStepsToNote( Note* baseNote, int steps )
{
	size_t index = getIndex( baseNote->octave, baseNote->octaveNote );
	index += steps;
	Note result = fromIndex( index );
	return result;
}

static void getScaleSteps( Scales scale, const int** outScaleSteps, size_t* outScaleStepsSize )
{
	SDL_assert( outScaleSteps != NULL );
	SDL_assert( outScaleStepsSize != NULL );

	switch( scale ) {
	case SCALE_CHROMATIC: ( *outScaleSteps ) = CHROMATIC_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( CHROMATIC_SCALE ); break;
	case SCALE_MAJOR: ( *outScaleSteps ) = MAJOR_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_SCALE ); break;
	case SCALE_MAJOR_DORIAN: ( *outScaleSteps ) = MAJOR_DORIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_DORIAN_SCALE ); break;
	case SCALE_MAJOR_PHYRIGIAN: ( *outScaleSteps ) = MAJOR_PHYRGIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_PHYRGIAN_SCALE ); break;
	case SCALE_MAJOR_LYDIAN: ( *outScaleSteps ) = MAJOR_LYDIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_LYDIAN_SCALE ); break;
	case SCALE_MAJOR_MIXOLYDIAN: ( *outScaleSteps ) = MAJOR_MIXOLYDIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_MIXOLYDIAN_SCALE ); break;
	case SCALE_MAJOR_AEOLIAN: ( *outScaleSteps ) = MAJOR_AEOILAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_AEOILAN_SCALE ); break;
	case SCALE_MAJOR_LOCRIAN: ( *outScaleSteps ) = MAJOR_LOCRIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_LOCRIAN_SCALE ); break;
	case SCALE_NATURAL_MINOR: ( *outScaleSteps ) = NATURAL_MINOR_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( NATURAL_MINOR_SCALE ); break;
	case SCALE_HARMONIC_MINOR: ( *outScaleSteps ) = HARMONIC_MINOR_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( HARMONIC_MINOR_SCALE ); break;
	case SCALE_MELODIC_MINOR_DOWN: ( *outScaleSteps ) = MELODIC_MINOR_DOWN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MELODIC_MINOR_DOWN_SCALE ); break;
	case SCALE_MELODIC_MINOR_UP: ( *outScaleSteps ) = MELODIC_MINOR_UP_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MELODIC_MINOR_UP_SCALE ); break;
	case SCALE_MAJOR_PENTATONIC: ( *outScaleSteps ) = MAJOR_PENTATONIC_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MAJOR_PENTATONIC_SCALE ); break;
	case SCALE_MINOR_PENTATONIC: ( *outScaleSteps ) = MINOR_PENTATONIC_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( MINOR_PENTATONIC_SCALE ); break;
	case SCALE_HUNGARIAN_MINOR: ( *outScaleSteps ) = HUNGARIAN_MINOR_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( HUNGARIAN_MINOR_SCALE ); break;
	case SCALE_AHAVA_RABA: ( *outScaleSteps ) = AHAVA_RABA_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( AHAVA_RABA_SCALE ); break;
	case SCALE_PERSIAN: ( *outScaleSteps ) = PERSIAN_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( PERSIAN_SCALE ); break;
	case SCALE_STEVE: ( *outScaleSteps ) = STEVE_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( STEVE_SCALE ); break;
	case SCALE_OCTATONIC_ONE: ( *outScaleSteps ) = OCTATONIC_ONE_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( OCTATONIC_ONE_SCALE ); break;
	case SCALE_OCTATONIC_TWO: ( *outScaleSteps ) = OCTATONIC_TWO_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( OCTATONIC_TWO_SCALE ); break;
	case SCALE_WHOLE_TONE: ( *outScaleSteps ) = WHOLE_TONE_SCALE; ( *outScaleStepsSize ) = ARRAY_SIZE( WHOLE_TONE_SCALE ); break;
	default:
		SDL_assert( false && "Not a valid scale. Using chromatic." );
		( *outScaleSteps ) = CHROMATIC_SCALE;
		( *outScaleStepsSize ) = ARRAY_SIZE( CHROMATIC_SCALE );
		break;
	}
}

static Note* getStepNotes( Note* tonic, int* steps, size_t numSteps )
{
	Note* result = NULL;

	for( size_t i = 0; i < numSteps; ++i ) {
		Note note = music_AddStepsToNote( tonic, steps[i] );
		sb_Push( result, note );
	}

	return result;
}

// returns a stretchy buffer, need to make sure it's cleaned up properly
Note* music_GetScaleSB( Note* tonic, Scales scale )
{
	int* scaleSteps;
	size_t numScaleSteps;

	getScaleSteps( scale, &scaleSteps, &numScaleSteps );
	return getStepNotes( tonic, scaleSteps, numScaleSteps );
}

const static int MAJOR_TRIAD_CHORD_STEPS[] = { 0, 4, 7 };
const static int MINOR_TRIAD_CHORD_STEPS[] = { 0, 3, 7 };
const static int DIMINISHED_TRIAD_CHORD_STEPS[] = { 0, 3, 6 };
const static int AUGMENTED_TRIAD_CHORD_STEPS[] = { 0, 4, 8 };
const static int MAJOR_SEVENTH_CHORD_STEPS[] = { 0, 4, 7, 11 };
const static int DOMINANT_SEVENTH_CHORD_STEPS[] = { 0, 4, 7, 10 };
const static int MINOR_SEVENTH_CHORD_STEPS[] = { 0, 3, 7, 10 };
const static int MINOR_MAJOR_SEVENTH_CHORD_STEPS[] = { 0, 3, 7, 11 };
const static int HALF_DIMINISHED_SEVENTH_CHORD_STEPS[] = { 0, 3, 6, 10 };
const static int DIMINISHED_SEVENTH_CHORD_STEPS[] = { 0, 3, 6, 9 };
const static int AUGMENTED_MAJOR_SEVENTH_CHORD_STEPS[] = { 0, 4, 8, 12 };
const static int AUGMENTED_SEVENTH_CHORD_STEPS[] = { 0, 4, 8, 11 };

static void getChordSteps( ChordType type, const int** outChordSteps, size_t* outChordStepsSize )
{
	SDL_assert( outChordSteps != NULL );
	SDL_assert( outChordStepsSize != NULL );

	switch( type ) {
	case CHORD_TYPE_MAJOR_TRIAD: (*outChordSteps) = MAJOR_TRIAD_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( MAJOR_TRIAD_CHORD_STEPS ); break;
	case CHORD_TYPE_MINOR_TRIAD: ( *outChordSteps ) = MINOR_TRIAD_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( MINOR_TRIAD_CHORD_STEPS ); break;
	case CHORD_TYPE_DIMINISHED_TRIAD: ( *outChordSteps ) = DIMINISHED_TRIAD_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( DIMINISHED_TRIAD_CHORD_STEPS ); break;
	case CHORD_TYPE_AUGMENTED_TRIAD: ( *outChordSteps ) = AUGMENTED_TRIAD_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( AUGMENTED_TRIAD_CHORD_STEPS ); break;
	case CHORD_TYPE_MAJOR_SEVENTH: ( *outChordSteps ) = MAJOR_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( MAJOR_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_DOMINANT_SEVENTH: ( *outChordSteps ) = DOMINANT_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( DOMINANT_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_MINOR_SEVENTH: ( *outChordSteps ) = MINOR_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( MINOR_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_MINOR_MAJOR_SEVENTH: ( *outChordSteps ) = MINOR_MAJOR_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( MINOR_MAJOR_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_HALF_DIMINISHED_SEVENTH: ( *outChordSteps ) = HALF_DIMINISHED_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( HALF_DIMINISHED_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_DIMINISHED_SEVENTH: ( *outChordSteps ) = DIMINISHED_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( DIMINISHED_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_AUGMENTED_MAJOR_SEVENTH: ( *outChordSteps ) = AUGMENTED_MAJOR_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( AUGMENTED_MAJOR_SEVENTH_CHORD_STEPS ); break;
	case CHORD_TYPE_AUGMENTED_SEVENTH: ( *outChordSteps ) = AUGMENTED_SEVENTH_CHORD_STEPS; ( *outChordStepsSize ) = ARRAY_SIZE( AUGMENTED_SEVENTH_CHORD_STEPS ); break;
	default:
		SDL_assert( false && "Not a valid chord. Using major triad." );
		( *outChordSteps ) = MAJOR_TRIAD_CHORD_STEPS;
		( *outChordStepsSize ) = ARRAY_SIZE( MAJOR_TRIAD_CHORD_STEPS );
		break;
	}
}

Note* music_GetChordSB( Note* tonic, ChordType chordType )
{
	int* steps;
	size_t numSteps;
	getChordSteps( chordType, &steps, &numSteps );
	return getStepNotes( tonic, steps, numSteps );
}