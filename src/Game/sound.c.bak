#include "sound.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>
#include <stdlib.h>
#include <math.h>

#include "Others\stb_vorbis_sdl.c"
#include "System\platformLog.h"
#include "Math\mathUtil.h"
#include "System\memory.h"
#include "Utils\stretchyBuffer.h"
#include "Utils\helpers.h"
#include "Utils\cfgFile.h"
#include "System\jobQueue.h"

#define MAX_SAMPLES 256
#define MAX_PLAYING_SOUNDS 32
#define MAX_STREAMING_SOUNDS 8
#define STREAMING_BUFFER_SAMPLES 4096

// TODO: Get pitch shifting working with stereo sounds.
typedef struct {
	int sample;
	float volume;
	float pitch;
	float pos;
	float pan; // only counts if there is one channel
	unsigned int group;
} Sound;

typedef struct {
	int numChannels;
	float* data;
	int numSamples;
	bool loops;
} Sample;

// TODO: Get pitch working with streaming sounds, was running into problems with clicking when doing streaming sounds with pitch
//  related to the copying of data, basically where it tests to see if we would go past the end, if we use >= instead of > we
//  get clicking, probably a stupid little error but have spent more time than planned on this already and don't forsee the need
//  to use this in the immediate future
typedef struct {
	bool playing;

	stb_vorbis* access;

	float volume;
	float pan;
	bool loops;
	unsigned int group;

	Uint8 channels;

	bool readDone;
	SDL_AudioStream* sdlStream; // does all the conversion automatically
} StreamingSound;

static Uint8 workingSilence = 0;
static SDL_AudioDeviceID devID = 0;

#define WORKING_CHANNELS 2
#define WORKING_FORMAT AUDIO_F32
// if the WORKING_RATE isn't 48000 some browsers use too small of an audio buffer for the mixer callback which causes popping
// TODO: Figure out why and fix it, or make sure it's something entirely on their end and therefore nothing i have control over
#if defined( __EMSCRIPTEN__ )
  static const int WORKING_RATE = 48000;
#else
  static const int WORKING_RATE = 44100;
#endif
static Uint16 AUDIO_SAMPLES = 1024;
static int workingBufferSize;

static Sample sineWave;

static float* workingBuffer = NULL;

static Sample samples[MAX_SAMPLES];
static Sound playingSounds[MAX_PLAYING_SOUNDS];
static IDSet playingIDSet; // we want to be able to change the currently playing sounds, this will help

static StreamingSound streamingSounds[MAX_STREAMING_SOUNDS];

static void* soundCfgFile;

typedef struct {
	float volume;
} SoundGroup;
static SoundGroup* sbSoundGroups;

static float masterVolume = 1.0f;

static float testTimePassed = 0.0f;

#define STREAM_READ_BUFFER_SIZE ( STREAMING_BUFFER_SAMPLES * WORKING_CHANNELS * sizeof( short ) )
static short streamReadBuffer[STREAM_READ_BUFFER_SIZE];
static float* sbStreamWorkingBuffer = NULL;

// stereo LRLRLR order
void mixerCallback( void* userdata, Uint8* streamData, int len )
{
	memset( streamData, len, workingSilence );
	memset( workingBuffer, 0, workingBufferSize );
	if( workingBuffer == NULL ) {
		return;
	}

	int numSamples = ( len / WORKING_CHANNELS ) / ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 );

#if 0
	int soundFreq = 440; // wave length
	float step = 1.0f / WORKING_RATE;
	// for testing audio output
	for( int s = 0; s < numSamples; ++s ) {
		int streamIdx = ( s * WORKING_CHANNELS );
		testTimePassed += 1.0f / WORKING_RATE;
		float v = sinf( testTimePassed * soundFreq * M_TWO_PI_F );
		workingBuffer[streamIdx] = v * 0.1f;
		workingBuffer[streamIdx+1] = v * 0.1f;
	}
#else
	// advance each playing sound
	for( EntityID id = idSet_GetFirstValidID( &playingIDSet ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &playingIDSet, id ) ) {
		int i = idSet_GetIndex( id );
		Sound* snd = &( playingSounds[i] );
		Sample* sample = &( samples[snd->sample] );
		
		// for right now lets assume the pitch will stay the same, when we get it working it'll just involve
		//  changing the speed at which we move through the array
		bool soundDone = false;
		float volume = snd->volume * sbSoundGroups[snd->group].volume * masterVolume;
		for( int s = 0; ( s < numSamples ) && !soundDone; ++s ) {

			int streamIdx = ( s * WORKING_CHANNELS );

			// we're assuming stereo output here
			if( sample->numChannels == 1 ) {
				float data = sample->data[(int)snd->pos] * volume;
/* left */		workingBuffer[streamIdx] += data * inverseLerp( 1.0f, 0.0f, snd->pan );
/* right */		workingBuffer[streamIdx+1] += data * inverseLerp( -1.0f, 0.0f, snd->pan );
				snd->pos += snd->pitch;
			} else {
				// if the sample is stereo then we ignore panning
				//  NOTE: Pitch change doesn't work with stereo samples yet
				workingBuffer[streamIdx] += sample->data[(int)snd->pos] * volume;
				workingBuffer[streamIdx+1] += sample->data[(int)snd->pos+1] * volume;
				snd->pos += 2.0f;
			}

			if( snd->pos >= sample->numSamples ) {
				if( sample->loops ) {
					snd->pos -= (float)sample->numSamples;
				} else {
					soundDone = true;
				}
			}
		}

		if( soundDone ) {
			idSet_ReleaseID( &playingIDSet, id ); // this doesn't invalidate the id for the loop
		}
	}

	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		if( !streamingSounds[i].playing ) continue;

		StreamingSound* stream = &( streamingSounds[i] );
		bool soundDone = false;
		float volume = stream->volume * sbSoundGroups[stream->group].volume * masterVolume;

		// if the next buffer fill would be past what we have loaded then load some more
		//  note: the SDL_AudioStreamAvailable return value is in bytes
		while( ( SDL_AudioStreamAvailable( stream->sdlStream ) < len ) && !( stream->readDone ) ) {
			int read = 0;
			int request = STREAM_READ_BUFFER_SIZE / sizeof( short );
			size_t test = ARRAY_SIZE( streamReadBuffer );
			// returns the number of samples stored per channel
			int samplesPerChannel = stb_vorbis_get_samples_short_interleaved(
				stream->access, stream->access->channels, streamReadBuffer, request );
			read = samplesPerChannel * sizeof( short );

			SDL_AudioStreamPut( stream->sdlStream, streamReadBuffer, samplesPerChannel * stream->channels * sizeof( short ) );

			// reached the end of the file, are we looping?
			if( read != request ) {
				if( stream->loops ) {
					stb_vorbis_seek_start( stream->access );
				} else {
					stream->readDone = true;
					SDL_AudioStreamFlush( stream->sdlStream );
				}
			}
		}

		// read data from the audio stream until there is no more left or the end of the buffer to fill
		int bytesToStream = numSamples * stream->channels * sizeof( sbStreamWorkingBuffer[0] );
		sb_Reserve( sbStreamWorkingBuffer, (size_t)bytesToStream );
		int gotten = SDL_AudioStreamGet( stream->sdlStream, sbStreamWorkingBuffer, bytesToStream );
		if( gotten < 0 ) {
			llog( LOG_ERROR, "Error reading from sdlStream: %s", SDL_GetError( ) );
			stream->playing = false;
			continue;
		} else if( gotten == 0 ) {
			// end of stream
			stream->playing = false;
		}

		int samplesGotten = gotten / ( stream->channels * sizeof( sbStreamWorkingBuffer[0] ) );
		for( int s = 0; s < samplesGotten; ++s ) {
			// then just mix those samples
			int streamIdx = ( s * WORKING_CHANNELS );
			int workingIdx = ( s * stream->channels );
			
			// we're assuming stereo output here
			if( stream->access->channels == 1 ) {
				float data = sbStreamWorkingBuffer[workingIdx] * volume;
				workingBuffer[streamIdx] += data * inverseLerp( 1.0f, 0.0f, stream->pan );		// left
				workingBuffer[streamIdx+1] += data * inverseLerp( -1.0f, 0.0f, stream->pan );   // right
			} else {
				// if the sample is stereo then we ignore panning
				workingBuffer[streamIdx] += sbStreamWorkingBuffer[workingIdx] * volume;
				workingBuffer[streamIdx+1] += sbStreamWorkingBuffer[workingIdx + 1] * volume;
			}
		}
	}
#endif

	memcpy( streamData, workingBuffer, len );
}

int snd_LoadSample( const char* fileName, Uint8 desiredChannels, bool loops )
{
	assert( ( desiredChannels >= 1 ) && ( desiredChannels <= 2 ) );

	int newIdx = -1;
	for( int i = 0; ( i < ARRAY_SIZE( samples ) ) && ( newIdx < 0 ); ++i ) {
		if( samples[i].data == NULL ) {
			newIdx = i;
		}
	}

	if( newIdx < 0 ) {
		llog( LOG_ERROR, "Unable to find free space for sample." );
		return -1;
	}

	// read the entire file into memory and decode it
	int channels;
	int rate;
	short* data = NULL;
	int numSamples = stb_vorbis_decode_filename( fileName, &channels, &rate, &data );

	if( numSamples <= 0 ) {
		newIdx = -1;
		llog( LOG_ERROR, "No samples in sound file or file doesn't exist." );
		goto clean_up;
	}

	// convert it
	SDL_AudioCVT loadConverter;
	if( SDL_BuildAudioCVT( &loadConverter,
		AUDIO_S16, (Uint8)channels, rate,
		WORKING_FORMAT, desiredChannels, WORKING_RATE ) < 0 ) {
		llog( LOG_ERROR, "Unable to create converter for sound." );
		newIdx = -1;
		goto clean_up;
	}

	loadConverter.len = numSamples * channels * sizeof( data[0] );
	size_t totLen = loadConverter.len * loadConverter.len_mult;
	if( loadConverter.len_mult > 1 ) {
		data = mem_Resize( data, loadConverter.len * loadConverter.len_mult ); // need to make sure there's enough room
		if( data == NULL ) {
			llog( LOG_ERROR, "Unable to allocate more memory for converting." );
			newIdx = -1;
			goto clean_up;
		}
	}
	loadConverter.buf = (Uint8*)data;

	if( SDL_ConvertAudio( &loadConverter ) < 0 ) {
		llog( LOG_ERROR, "Unable to convert sound: %s", SDL_GetError( ) );
		newIdx = -1;
		goto clean_up;
	}

	// store it
	samples[newIdx].data = mem_Allocate( loadConverter.len_cvt );

	memcpy( samples[newIdx].data, loadConverter.buf, loadConverter.len_cvt );

	samples[newIdx].numChannels = desiredChannels;
	samples[newIdx].numSamples = loadConverter.len_cvt / ( desiredChannels * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 ) );
	samples[newIdx].loops = loops;

clean_up:
	// clean up the working data
	mem_Release( data );
	return newIdx;
}

typedef struct {
	const char* fileName;
	Uint8 desiredChannels;
	bool loops;
	int* outID;
	SDL_AudioCVT loadConverter;
} ThreadedSoundLoadData;

static void cleanUpThreadedSoundLoadData( ThreadedSoundLoadData* data )
{
	mem_Release( data->loadConverter.buf );
	data->loadConverter.buf = NULL;

	mem_Release( data );
}

static void bindSampleJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into loadSampleJob!" );
		return;
	}

	ThreadedSoundLoadData* loadData = (ThreadedSoundLoadData*)data;

	int newIdx = -1;
	for( int i = 0; ( i < ARRAY_SIZE( samples ) ) && ( newIdx < 0 ); ++i ) {
		if( samples[i].data == NULL ) {
			newIdx = i;
		}
	}

	if( newIdx < 0 ) {
		llog( LOG_ERROR, "Unable to find free space for sample." );
		goto clean_up;
	}

	// store it
	samples[newIdx].data = mem_Allocate( loadData->loadConverter.len_cvt );

	memcpy( samples[newIdx].data, loadData->loadConverter.buf, loadData->loadConverter.len_cvt );

	samples[newIdx].numChannels = loadData->desiredChannels;
	samples[newIdx].numSamples = loadData->loadConverter.len_cvt / ( loadData->desiredChannels * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 ) );
	samples[newIdx].loops = loadData->loops;

	(*(loadData->outID)) = newIdx;

clean_up:
	cleanUpThreadedSoundLoadData( loadData );
}

static void loadSampleJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into loadSampleJob!" );
		return;
	}

	ThreadedSoundLoadData* loadData = (ThreadedSoundLoadData*)data;

	// read the entire file into memory and decode it
	int channels;
	int rate;
	short* buffer;
	int numSamples = stb_vorbis_decode_filename( loadData->fileName, &channels, &rate, &buffer );

	if( numSamples < 0 ) {
		llog( LOG_ERROR, "Error decoding sound sample %s", loadData->fileName );
		goto error;
	}

	// convert it
	if( SDL_BuildAudioCVT( &( loadData->loadConverter ),
		AUDIO_S16, (Uint8)channels, rate,
		WORKING_FORMAT, loadData->desiredChannels, WORKING_RATE ) < 0 ) {
		llog( LOG_ERROR, "Unable to create converter for sound." );
		goto error;
	}

	loadData->loadConverter.len = numSamples * channels * sizeof( buffer[0] );
	size_t totLen = loadData->loadConverter.len * loadData->loadConverter.len_mult;
	if( loadData->loadConverter.len_mult > 1 ) {
		buffer = mem_Resize( buffer, loadData->loadConverter.len * loadData->loadConverter.len_mult ); // need to make sure there's enough room
		if( buffer == NULL ) {
			llog( LOG_ERROR, "Unable to allocate more memory for converting." );
			goto error;
		}
	}
	loadData->loadConverter.buf = (Uint8*)buffer;

	SDL_ConvertAudio( &( loadData->loadConverter ) );

	jq_AddMainThreadJob( bindSampleJob, (void*)loadData );

	return;

error:
	cleanUpThreadedSoundLoadData( loadData );
}

void snd_ThreadedLoadSample( const char* fileName, Uint8 desiredChannels, bool loops, int* outID )
{
	assert( ( desiredChannels >= 1 ) && ( desiredChannels <= 2 ) );
	assert( outID != NULL );

	(*outID) = -1;

	ThreadedSoundLoadData* loadData = mem_Allocate( sizeof( ThreadedSoundLoadData ) );
	if( loadData == NULL ) {
		llog( LOG_ERROR, "Unable to allocated data struct for threaded loading of sound sample" );
		return;
	}

	loadData->fileName = fileName;
	loadData->desiredChannels = desiredChannels;
	loadData->loops = loops;
	loadData->outID = outID;
	loadData->loadConverter.buf = NULL;

	jq_AddJob( loadSampleJob, (void*)loadData );
}

/* Sets up the SDL mixer. Returns 0 on success. */
int snd_Init( unsigned int numGroups )
{
	assert( numGroups > 0 );

	// clear out the samples storage
	SDL_memset( samples, 0, ARRAY_SIZE( samples ) * sizeof( samples[0] ) );
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		streamingSounds[i].access = NULL;
		streamingSounds[i].sdlStream = NULL;
		streamingSounds[i].playing = false;
	}

	SDL_AudioSpec desired;
	SDL_memset( &desired, 0, sizeof( desired ) );
	desired.freq = WORKING_RATE;
	desired.format = WORKING_FORMAT;
	desired.channels = WORKING_CHANNELS;
	desired.samples = AUDIO_SAMPLES;
	desired.callback = mixerCallback;
	desired.userdata = NULL;

	if( idSet_Init( &playingIDSet, MAX_PLAYING_SOUNDS ) != 0 ) {
		llog( LOG_CRITICAL, "Failed to create playing sounds id set."  );
		return -1;
	}

	// sending 0 will cause SDL to convert everything automatically
	devID = SDL_OpenAudioDevice( NULL, 0, &desired, NULL, 0 );

	if( devID == 0 ) {
		llog( LOG_CRITICAL, "Failed to open audio device: %s", SDL_GetError( ) );
		return -1;
	}
	SDL_PauseAudioDevice( devID, 0 );

	workingSilence = desired.silence;

	workingBufferSize = desired.samples * desired.channels * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 );

	SDL_LockAudioDevice( devID );
	workingBuffer = mem_Allocate( workingBufferSize );
	SDL_UnlockAudioDevice( devID );
	if( workingBuffer == NULL ) {
		llog( LOG_CRITICAL, "Failed to create audio working buffer." );
		return -1;
	}

	sb_Add( sbSoundGroups, numGroups );
	for( size_t i = 0; i < sb_Count( sbSoundGroups ); ++i ) {
		sbSoundGroups[i].volume = 1.0f;
	}

	// load the master volume
	soundCfgFile = cfg_OpenFile( "snd.cfg" );
	if( soundCfgFile != NULL ) {
		int vol;
		cfg_GetInt( soundCfgFile, "vol", 100, &vol );
		masterVolume = (float)vol / 100.0f;
	} else {
		masterVolume = 1.0f;
	}

	return 0;
}

void snd_CleanUp( )
{
	snd_StopStreamingAllBut( -1 );
	if( workingBuffer != NULL ) {
		SDL_LockAudioDevice( devID ); {
			sb_Release( sbStreamWorkingBuffer );
			mem_Release( workingBuffer );
			workingBuffer = NULL;
		} SDL_UnlockAudioDevice( devID );
	}

	if( devID == 0 ) return;
	SDL_CloseAudioDevice( devID );
}

void snd_SetFocus( bool hasFocus )
{
	SDL_LockAudioDevice( devID ); {
		SDL_PauseAudioDevice( devID, hasFocus ? 0 : 1 );
	} SDL_UnlockAudioDevice( devID );
}

float snd_GetMasterVolume( void )
{
	return masterVolume;
}

void snd_SetMasterVolume( float volume )
{
	SDL_LockAudioDevice( devID ); {
		masterVolume = volume;
	} SDL_UnlockAudioDevice( devID );

	// just save this out every single time
	if( soundCfgFile != NULL ) {
		cfg_SetInt( soundCfgFile, "vol", (int)volume );
		cfg_SaveFile( soundCfgFile );
	}
}

float snd_GetVolume( unsigned int group )
{
	assert( group < sb_Count( sbSoundGroups ) );

	return sbSoundGroups[group].volume;
}

void snd_SetVolume( float volume, unsigned int group )
{
	assert( group < sb_Count( sbSoundGroups ) );
	assert( ( volume >= 0.0f ) && ( volume <= 1.0f ) );

	SDL_LockAudioDevice( devID ); {
		sbSoundGroups[group].volume = volume;
	} SDL_UnlockAudioDevice( devID );
}

// Returns an id that can be used to change the volume and pitch
//  loops - if the sound will loop back to the start once it's over
//  volume - how loud the sound will be, in the range [0,1], 0 being off, 1 being loudest
//  pitch - pitch change for the sound, multiplies the sample rate, 1 for normal, lesser for slower, higher for faster
//  pan - how far left or right the sound is, 0 is center, -1 is left, +1 is right
// TODO: Some sort of event system so we can get when a sound has finished playing?
EntityID snd_Play( int sampleID, float volume, float pitch, float pan, unsigned int group )
{
	if( sampleID < 0 ) {
		return INVALID_ENTITY_ID;
	}

	assert( group >= 0 );
	assert( group < sb_Count( sbSoundGroups ) );

	EntityID playingID = INVALID_ENTITY_ID;
	SDL_LockAudioDevice( devID ); {
		playingID = idSet_ClaimID( &playingIDSet );
		if( playingID != INVALID_ENTITY_ID ) {
			int idx = idSet_GetIndex( playingID );
			playingSounds[idx].sample = sampleID;
			playingSounds[idx].volume = volume;
			playingSounds[idx].pitch = pitch;
			playingSounds[idx].pan = pan;
			playingSounds[idx].pos = 0.0f;
			playingSounds[idx].group = group;
		}
	} SDL_UnlockAudioDevice( devID );

	return playingID;
}

// Volume is assumed to be [0,1]
void snd_ChangeSoundVolume( EntityID soundID, float volume )
{
	SDL_LockAudioDevice( devID ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].volume = volume;
		}
	} SDL_UnlockAudioDevice( devID );
}

// Pitch is assumed to be > 0
void snd_ChangeSoundPitch( EntityID soundID, float pitch )
{
	SDL_LockAudioDevice( devID ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].pitch = pitch;
		}
	} SDL_UnlockAudioDevice( devID );
}

// Pan is assumed to be [-1,1]
void snd_ChangeSoundPan( EntityID soundID, float pan )
{
	SDL_LockAudioDevice( devID ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].pan = pan;
		}
	} SDL_UnlockAudioDevice( devID );
}

void snd_Stop( EntityID soundID )
{
	SDL_LockAudioDevice( devID ); {
		idSet_ReleaseID( &playingIDSet, soundID );
	} SDL_UnlockAudioDevice( devID );
}

void snd_UnloadSample( int sampleID )
{
	assert( sampleID >= 0 );
	assert( sampleID < MAX_SAMPLES );

	if( samples[sampleID].data == NULL ) {
		return;
	}

	SDL_LockAudioDevice( devID ); {
		// find all playing sounds using this sample and stop them
		for( EntityID id = idSet_GetFirstValidID( &playingIDSet ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &playingIDSet, id ) ) {
			int idx = idSet_GetIndex( id );
			if( playingSounds[idx].sample == sampleID ) {
				idSet_ReleaseID( &playingIDSet, id );
			}
		}

		mem_Release( samples[sampleID].data );
		samples[sampleID].data = NULL;
	} SDL_UnlockAudioDevice( devID );
}

//***** Streaming
int snd_LoadStreaming( const char* fileName, bool loops, unsigned int group )
{
	assert( group >= 0 );
	assert( group < sb_Count( sbSoundGroups ) );

	int newIdx = -1;
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		if( streamingSounds[i].access == NULL ) {
			newIdx = i;
			break;
		}
	}

	if( newIdx < 0 ) {
		llog( LOG_ERROR, "Unable to find empty slot for streaming sound %s", fileName );
		return -1;
	}

	int error;
	streamingSounds[newIdx].access = stb_vorbis_open_filename( fileName, &error, NULL );
	if( streamingSounds[newIdx].access == NULL ) {
		llog( LOG_ERROR, "Unable to acquire a handle to streaming sound %s", fileName );
		return -1;
	}

	streamingSounds[newIdx].playing = false;
	streamingSounds[newIdx].loops = loops;
	streamingSounds[newIdx].group = group;

	streamingSounds[newIdx].channels = (Uint8)( streamingSounds[newIdx].access->channels );
	if( streamingSounds[newIdx].channels > 2 ) {
		streamingSounds[newIdx].channels = 2;
	}

	return newIdx;
}

void snd_PlayStreaming( int streamID, float volume, float pan ) // todo: fade in?
{
	if( ( streamID < 0 ) || ( streamID >= MAX_STREAMING_SOUNDS ) ) {
		return;
	}

	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );

	if( streamingSounds[streamID].playing ) {
		return;
	}

	SDL_LockAudioDevice( devID ); {
		if( ( streamingSounds[streamID].access != NULL ) && !streamingSounds[streamID].playing ) {

			streamingSounds[streamID].sdlStream = SDL_NewAudioStream( AUDIO_S16,
				(Uint8)( streamingSounds[streamID].access->channels ), streamingSounds[streamID].access->sample_rate,
				WORKING_FORMAT, streamingSounds[streamID].channels, WORKING_RATE );

			if( streamingSounds[streamID].sdlStream == NULL ) {
				llog( LOG_ERROR, "Unable to create SDL_AudioStream for streaming sound." );
				return;
			}

			streamingSounds[streamID].playing = true;
			streamingSounds[streamID].volume = volume;
			streamingSounds[streamID].pan = pan;

			stb_vorbis_seek_start( streamingSounds[streamID].access );

			streamingSounds[streamID].readDone = false;
		}
	} SDL_UnlockAudioDevice( devID );
}

void snd_StopStreaming( int streamID )
{
	if( ( streamID < 0 ) || ( streamID >= MAX_STREAMING_SOUNDS ) ) {
		return;
	}

	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		streamingSounds[streamID].playing = false;
		SDL_FreeAudioStream( streamingSounds[streamID].sdlStream );
		streamingSounds[streamID].sdlStream = NULL;
	} SDL_UnlockAudioDevice( devID );
}

void snd_StopStreamingAllBut( int streamID )
{
	SDL_LockAudioDevice( devID ); {
		for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
			if( i == streamID ) continue;
			if( ( streamingSounds[i].access != NULL ) && streamingSounds[i].playing ) {
				streamingSounds[i].playing = false;
				SDL_FreeAudioStream( streamingSounds[streamID].sdlStream );
				streamingSounds[streamID].sdlStream = NULL;
			}
		}
	} SDL_UnlockAudioDevice( devID );
}

bool snd_IsStreamPlaying( int streamID )
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	return streamingSounds[streamID].playing;
}

void snd_ChangeStreamVolume( int streamID, float volume )
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		streamingSounds[streamID].volume = volume;
	} SDL_UnlockAudioDevice( devID );
}

void snd_ChangeStreamPan( int streamID, float pan )
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		streamingSounds[streamID].pan = pan;
	} SDL_UnlockAudioDevice( devID );
}

void snd_UnloadStream( int streamID )
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		streamingSounds[streamID].playing = false;
		stb_vorbis_close( streamingSounds[streamID].access );
		streamingSounds[streamID].access = NULL;
		SDL_FreeAudioStream( streamingSounds[streamID].sdlStream );
		streamingSounds[streamID].sdlStream = NULL;
	} SDL_UnlockAudioDevice( devID );
}