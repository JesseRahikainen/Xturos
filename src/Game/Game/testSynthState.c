#include "testSynthState.h"

#include <SDL3/SDL.h>

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "Audio/sound.h"
#include "Audio/tuning.h"
#include "UI/text.h"
#include "UI/uiEntities.h"
#include "Input/input.h"
#include "System/jobQueue.h"
#include "IMGUI/nuklearWrapper.h"

static int baseSound = -1;
static int font = -1;
static Tuning baseTuning;
static Note baseSoundNote = { 4, NOTE_A };
static int buttonImg = -1;

static float a4Hertz = 440.0f;
static TuningIntervals tuningInterval = TUNING_EQUAL_TEMPERAMENT;

typedef struct {
	OctaveNote note;
	SDL_Keycode code;
} SynthKeyBinding;

static int8_t baseOctave = 4;

static SynthKeyBinding bindings[] = {
	{ NOTE_C , SDLK_Q },
	{ NOTE_CS, SDLK_W },
	{ NOTE_D , SDLK_E },
	{ NOTE_DS, SDLK_R },
	{ NOTE_E , SDLK_T },
	{ NOTE_F , SDLK_Y },
	{ NOTE_FS, SDLK_U },
	{ NOTE_G , SDLK_I },
	{ NOTE_GS, SDLK_O },
	{ NOTE_A , SDLK_P },
	{ NOTE_AS, SDLK_LEFTBRACKET },
	{ NOTE_B , SDLK_RIGHTBRACKET },
};

typedef struct {
	int8_t octave;
	SDL_Keycode code;
} OctaveKeyBinding;

static OctaveKeyBinding octaveBindings[] = {
	{ 0, SDLK_0 },
	{ 1, SDLK_1 },
	{ 2, SDLK_2 },
	{ 3, SDLK_3 },
	{ 4, SDLK_4 },
	{ 5, SDLK_5 },
	{ 6, SDLK_6 },
	{ 7, SDLK_7 },
	{ 8, SDLK_8 },
};

static size_t currentSample;

typedef struct {
	int soundID;
	char* soundName;
	float pitchAdj;
} SampleSlot;

static SampleSlot slots[2] = { { -1, NULL, 1.0f }, { -1, NULL, 1.0f } };

typedef struct {
	size_t slotIndex;
	char* path;
} LoadSlotData;

static void loadIntoSampleSlot( size_t slotNum, const char* file )
{
	int loaded = snd_LoadSample( file, 1, false );
	if( loaded == -1 ) {
		llog( LOG_ERROR, "Unable to load sound file: %s", file );
		return;
	}

	char* fileNameStart = (char*)file;
	char* foundBreak = SDL_strpbrk( fileNameStart, "/\\" );
	while( foundBreak != NULL ) {
		fileNameStart = foundBreak + 1;
		foundBreak = SDL_strpbrk( fileNameStart, "/\\" );
	}

	if( slots[slotNum].soundID >= 0 ) {
		snd_UnloadSample( slots[slotNum].soundID );
	}
	mem_Release( slots[slotNum].soundName );

	slots[slotNum].soundID = loaded;
	slots[slotNum].soundName = createStringCopy( fileNameStart );
}

static void loadMainThreadCallback( void* data )
{
	LoadSlotData* loadData = (LoadSlotData*)data;
	loadIntoSampleSlot( loadData->slotIndex, loadData->path );
	mem_Release( data );
}

static void fileCallback( void* userData, const char* const* fileList, int filter )
{
	int slot = *( (int*)userData );
	mem_Release( userData );

	llog( LOG_DEBUG, "Loading sample: %s", fileList[0] );

	// this can happen on a non-main thread, so queue up the load to happen on the main thread
	LoadSlotData* data = mem_Allocate( sizeof( LoadSlotData ) );
	data->path = createStringCopy( fileList[0] );
	data->slotIndex = slot;
	jq_AddMainThreadJob( loadMainThreadCallback, data );
}

static void loadSlot( int slot )
{
	const SDL_DialogFileFilter filters[] = {
		{ "Ogg Vorbis (*.ogg)", "ogg" }
	};

	int* slotIndex = mem_Allocate( sizeof( int ) );
	*slotIndex = slot;
	SDL_ShowOpenFileDialog( fileCallback, (void*)slotIndex, NULL, filters, ARRAY_SIZE( filters ), NULL, false );
}

static void adjustTuning( void )
{
	//llog( LOG_DEBUG, "Setting A 4 to %f", a4Hertz );
	tuning_Setup( &baseTuning, a4Hertz, tuningInterval );
}

static void testSynthState_Enter( void )
{
	cam_SetFlags( 0, 1 );
	gfx_SetClearColor( clr( 0.0f, 0.0118f, 0.1530f, 1.0f ) );

	a4Hertz = 440.0f;
	tuningInterval = TUNING_EQUAL_TEMPERAMENT;
	adjustTuning( );
	//tuning_Setup( &baseTuning, 440.0, TUNING_EQUAL_TEMPERAMENT );
	//tuning_Setup( &baseTuning, 440.0, TUNING_PYTHAGOREAN );

	loadIntoSampleSlot( 0, "Sounds/a4Tone.ogg" );
	loadIntoSampleSlot( 1, "Sounds/a4Tone.ogg" );
	currentSample = 0;
}

static void testSynthState_Exit( void )
{
	for( size_t i = 0; i < ARRAY_SIZE( slots ); ++i ) {
		snd_UnloadSample( slots[i].soundID );
		mem_Release( slots[i].soundName );
	}
}

static void testSynthState_ProcessEvents( SDL_Event* e )
{
	if( ( e->type == SDL_EVENT_KEY_DOWN ) && !e->key.repeat ) {
		for( size_t i = 0; i < ARRAY_SIZE( octaveBindings ); ++i ) {
			if( e->key.key == octaveBindings[i].code ) {
				baseOctave = octaveBindings[i].octave;
			}
		}

		for( size_t i = 0; i < ARRAY_SIZE( bindings ); ++i ) {
			if( e->key.key == bindings[i].code ) {
				Note playNote = { baseOctave, bindings[i].note };
				float pitchAdj = tuning_HertzPitchAdjustment( 440.0, &playNote, &baseTuning );
				snd_Play( slots[currentSample].soundID, 1.0f, pitchAdj, 0.0f, 0 );
			}
		}

		if( e->key.key == SDLK_Z ) {
			snd_Play( slots[0].soundID, 1.0f, 1.0f, 0.0f, 0 );
		}

		if( e->key.key == SDLK_X ) {
			snd_Play( slots[1].soundID, 1.0f, 1.0f, 0.0f, 0 );
		}

		if( ( e->key.key == SDLK_A ) && ( slots[0].pitchAdj > 0.0f ) ) {
			snd_Play( slots[0].soundID, 1.0f, slots[0].pitchAdj, 0.0f, 0 );
		}

		if( ( e->key.key == SDLK_S ) && ( slots[1].pitchAdj > 0.0f ) ) {
			snd_Play( slots[1].soundID, 1.0f, slots[1].pitchAdj, 0.0f, 0 );
		}
	}
}

static void testSynthState_Process( void )
{
	const char* instructions = "Load the samples you want into each slot. Choose which slot you want to be the active one. Top keyboard row (Q to ]) play "
		"notes from the current octave, starting at C with Q. Press Z to play the sample in slot 0 with no pitch change, X plays slot 1 sample. Press A to "
		"play the sample in slot 0 at the defined pitch adjustment, press S to do the same for slot 1. If notes aren't playing adjust the maximum playable "
		"sounds in sound.h, it's probably playing the tail end of a bunch of notes.";
	char slotLabelBuffer[32];
	char pitchAdjBuffer[32];

	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect windowBounds = { 0.0f, 0.0f, (float)renderWidth, (float)renderHeight };
	if( nk_begin( ctx, "Synth Test", windowBounds, 0 ) ) {
		nk_layout_row_begin( ctx, NK_DYNAMIC, 80, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			nk_label_wrap( ctx, instructions );
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 40, 3 ); {

			nk_layout_row_push( ctx, 0.3f );
			if( nk_option_label( ctx, "Equal", tuningInterval == TUNING_EQUAL_TEMPERAMENT ) ) {
				tuningInterval = TUNING_EQUAL_TEMPERAMENT;
				adjustTuning( );
			}

			nk_layout_row_push( ctx, 0.3f );
			if( nk_option_label( ctx, "Pyth", tuningInterval == TUNING_PYTHAGOREAN ) ) {
				tuningInterval = TUNING_PYTHAGOREAN;
				adjustTuning( );
			}

			float prev = a4Hertz;
			nk_layout_row_push( ctx, 0.3f );
			nk_property_float( ctx, "A 4 Hertz", 0.0f, &a4Hertz, 1000.0f, 1.0f, 1.0f );
			if( a4Hertz != prev ) adjustTuning( );

		} nk_layout_row_end( ctx );

		for( size_t i = 0; i < ARRAY_SIZE( slots ); ++i ) {
			nk_layout_row_begin( ctx, NK_STATIC, 80, 1 ); {
				nk_layout_row_push( ctx, renderWidth - 20.0f );

				SDL_snprintf( slotLabelBuffer, ARRAY_SIZE( slotLabelBuffer ), "Slot %i", (int)i );
				SDL_snprintf( pitchAdjBuffer, ARRAY_SIZE( pitchAdjBuffer ), "Pitch Adj %i", (int)i );

				if( nk_group_begin( ctx, slotLabelBuffer, NK_WINDOW_BORDER | NK_WINDOW_TITLE ) ) {
					nk_layout_row_begin( ctx, NK_STATIC, 30, 7 ); {
						nk_layout_row_push( ctx, 20.0f );
						if( nk_option_label( ctx, "", currentSample == i ) ) currentSample = i;

						nk_layout_row_push( ctx, 10.0f );
						nk_spacer( ctx );

						nk_layout_row_push( ctx, 100.0f );
						if( nk_button_label( ctx, "Load Slot" ) ) {
							loadSlot( (int)i );
						}

						nk_layout_row_push( ctx, 10.0f );
						nk_spacer( ctx );

						nk_layout_row_push( ctx, 100.0f );
						nk_label_wrap( ctx, slots[i].soundName != NULL ? slots[i].soundName : "NOTHING LOADED" );

						nk_layout_row_push( ctx, 10.0f );
						nk_spacer( ctx );

						nk_layout_row_push( ctx, 150.0f );
						nk_property_float( ctx, pitchAdjBuffer, 0.0f, &slots[i].pitchAdj, 100.0f, 0.001f, 0.01f );

					} nk_layout_row_end( ctx );

					nk_group_end( ctx );
				}
			} nk_layout_row_end( ctx );
		}
	} nk_end( ctx );
}

static void testSynthState_Draw( void )
{
}

static void testSynthState_PhysicsTick( float dt )
{
}

GameState testSynthState = { testSynthState_Enter, testSynthState_Exit, testSynthState_ProcessEvents,
	testSynthState_Process, testSynthState_Draw, testSynthState_PhysicsTick };
