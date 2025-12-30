#include "sound.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <stdlib.h>
#include <math.h>

#include "System/platformLog.h"
#include "Math/mathUtil.h"
#include "System/memory.h"
#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"
#include "Utils/cfgFile.h"
#include "System/jobQueue.h"
#include "Utils/hashMap.h"

#include "Others/stb_vorbis_sdl.c"

#define MAX_SAMPLES 256
#define MAX_PLAYING_SOUNDS 32
#define MAX_STREAMING_SOUNDS 8
#define STREAMING_BUFFER_SAMPLES 4096
#define INITIAL_WORKING_BUFFER_SIZE 8192

// TODO: Get a good way to be able to run the game without any audio processing.
//  it can cause issues occasionally so it's nice to be able to turn it off quickly
// TODO: Go through and see if all the audio stream locks are actually necessary or not.

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
	unsigned int loopPoint;
	unsigned int group;

	Uint8 channels;

	bool readDone;
	SDL_AudioStream* sdlStream; // does all the conversion automatically

	unsigned int timesLoaded;
} StreamingSound;

HashMap streamingSoundHashMap;

static int workingSilence = 0;
static SDL_AudioStream* mainAudioStream = NULL;

#define WORKING_CHANNELS 2
#define WORKING_FORMAT SDL_AUDIO_F32LE

// if the WORKING_RATE isn't 48000 some browsers use too small of an audio buffer for the mixer callback which causes popping
// TODO: Figure out why and fix it, or make sure it's something entirely on their end and therefore nothing i have control over
#if defined( __EMSCRIPTEN__ )
  static const int WORKING_RATE = 48000;
#else
  static const int WORKING_RATE = 44100;
#endif
static Uint16 AUDIO_SAMPLES = 1024;

static Sample sineWave;

static float* sbWorkingBuffer = NULL;

static Sample samples[MAX_SAMPLES];
static Sound playingSounds[MAX_PLAYING_SOUNDS];
static IDSet playingIDSet; // we want to be able to change the currently playing sounds, this will help

static StreamingSound streamingSounds[MAX_STREAMING_SOUNDS];

typedef struct {
	float volume;
} SoundGroup;
static SoundGroup* sbSoundGroups;

static float masterVolume = 1.0f;

static float testTimePassed = 0.0f;

#define STREAM_READ_BUFFER_SIZE ( STREAMING_BUFFER_SAMPLES * WORKING_CHANNELS * sizeof( short ) )
static short streamReadBuffer[STREAM_READ_BUFFER_SIZE];
static float* sbStreamWorkingBuffer = NULL;

void mixerCallback_OLD( void* userdata, Uint8* streamData, int len )
{
	// unless we actually have any data it should be silence
	SDL_memset( streamData, workingSilence, len );

	// allocate more memory for the working buffer if needed
	size_t workingBufferSize = sb_Count( sbWorkingBuffer );
	if( workingBufferSize < len ) {
		sb_Add( sbWorkingBuffer, len - workingBufferSize );
		workingBufferSize = sb_Count( sbWorkingBuffer );
	}
	SDL_memset( sbWorkingBuffer, 0, workingBufferSize );

	int numSamples = ( len / WORKING_CHANNELS ) / SDL_AUDIO_BYTESIZE( WORKING_FORMAT );

//#error sine wave isn't playing, not even calling this
#if 0
	int soundFreq = 440; // wave length
	float step = 1.0f / WORKING_RATE;
	// for testing audio output
	for( int s = 0; s < numSamples; ++s ) {
		int streamIdx = ( s * WORKING_CHANNELS );
		testTimePassed += 1.0f / WORKING_RATE;
		while( testTimePassed >= 1000.0f ) testTimePassed -= 1000.0f;
		float v = sinf( testTimePassed * soundFreq * M_TWO_PI_F );
		sbWorkingBuffer[streamIdx] = v * 0.2f;
		sbWorkingBuffer[streamIdx+1] = v * 0.2f;
	}
#else
	// advance each playing sound
	for( EntityID id = idSet_GetFirstValidID( &playingIDSet ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &playingIDSet, id ) ) {
		int i = idSet_GetIndex( id );
		Sound* snd = &( playingSounds[i] );
		Sample* sample = &( samples[snd->sample] );
		
		bool soundDone = false;
		float volume = snd->volume * sbSoundGroups[snd->group].volume * masterVolume;
		for( int s = 0; ( s < numSamples ) && !soundDone; ++s ) {

			int streamIdx = ( s * WORKING_CHANNELS );

			// we're assuming stereo output here
			if( sample->numChannels == 1 ) {
				float data = sample->data[(int)snd->pos] * volume;
/* left */		sbWorkingBuffer[streamIdx] += data * inverseLerp( 1.0f, 0.0f, snd->pan );
/* right */		sbWorkingBuffer[streamIdx+1] += data * inverseLerp( -1.0f, 0.0f, snd->pan );
				snd->pos += snd->pitch; // TODO: do we want to take an average of the samples?
			} else {
				// if the sample is stereo then we ignore panning
				//  NOTE: Pitch change doesn't work with stereo samples yet
				sbWorkingBuffer[streamIdx] += sample->data[(int)snd->pos] * volume;
				sbWorkingBuffer[streamIdx+1] += sample->data[(int)snd->pos+1] * volume;
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

#if 1
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		if( !streamingSounds[i].playing ) continue;

		StreamingSound* stream = &( streamingSounds[i] );
		bool soundDone = false;
		float volume = stream->volume * sbSoundGroups[stream->group].volume * masterVolume;

		// if the next buffer fill would be past what we have loaded then load some more
		//  note: the SDL_AudioStreamAvailable return value is in bytes
		while( ( SDL_GetAudioStreamAvailable( stream->sdlStream ) < len ) && !( stream->readDone ) ) {
			int read = 0;
			int request = STREAM_READ_BUFFER_SIZE / sizeof( short );
			size_t test = ARRAY_SIZE( streamReadBuffer );
			// returns the number of samples stored per channel
			int samplesPerChannel = stb_vorbis_get_samples_short_interleaved(
				stream->access, stream->access->channels, streamReadBuffer, request );
			read = samplesPerChannel * sizeof( short );

			SDL_PutAudioStreamData( stream->sdlStream, streamReadBuffer, samplesPerChannel * stream->channels * sizeof( short ) );

			// reached the end of the file, are we looping?
			if( read != request ) {
				if( stream->loops ) {
					stb_vorbis_seek_frame( stream->access, stream->loopPoint );
				} else {
					stream->readDone = true;
					SDL_FlushAudioStream( stream->sdlStream );
				}
			}
		}

		// read data from the audio stream until there is no more left or the end of the buffer to fill
		int bytesToStream = numSamples * stream->channels * sizeof( sbStreamWorkingBuffer[0] );
		sb_Reserve( sbStreamWorkingBuffer, (size_t)bytesToStream );
		int gotten = SDL_GetAudioStreamData( stream->sdlStream, sbStreamWorkingBuffer, bytesToStream );
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
				sbWorkingBuffer[streamIdx] += data * inverseLerp( 1.0f, 0.0f, stream->pan );		// left
				sbWorkingBuffer[streamIdx+1] += data * inverseLerp( -1.0f, 0.0f, stream->pan );   // right
			} else {
				// if the sample is stereo then we ignore panning
				sbWorkingBuffer[streamIdx] += sbStreamWorkingBuffer[workingIdx] * volume;
				sbWorkingBuffer[streamIdx+1] += sbStreamWorkingBuffer[workingIdx + 1] * volume;
			}
		}
	}
#endif
#endif

	SDL_memcpy( streamData, sbWorkingBuffer, len );
}

// stereo LRLRLR order
void mixerCallback( void* userData, SDL_AudioStream* stream, int additionalAmount, int totalAmount )
{
	// for right now we'll just use the old mixer callback to get something up and running
	if( additionalAmount > 0 ) {
		Uint8* data = SDL_stack_alloc( Uint8, additionalAmount );
		if( data != NULL ) {
			mixerCallback_OLD( userData, data, additionalAmount );
			SDL_PutAudioStreamData( stream, data, additionalAmount );
			SDL_stack_free( data );
		}
	}
}

int snd_LoadSample( const char* fileName, Uint8 desiredChannels, bool loops )
{
	SDL_assert( ( desiredChannels >= 1 ) && ( desiredChannels <= 2 ) );

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
	short* loadData = NULL;
	int numSamples = stb_vorbis_decode_filename( fileName, &channels, &rate, &loadData );
	Uint8* destData = NULL;

	if( numSamples <= 0 ) {
		newIdx = -1;
		llog( LOG_ERROR, "No samples in sound file or file doesn't exist." );
		goto clean_up;
	}

	// convert it
	int destLen = 0;
	const SDL_AudioSpec srcSpec = { SDL_AUDIO_S16LE, channels, rate };
	const SDL_AudioSpec destSpec = { WORKING_FORMAT, desiredChannels, WORKING_RATE };
	if( !SDL_ConvertAudioSamples( &srcSpec, (const Uint8*)loadData, numSamples * channels * sizeof( loadData[0] ), &destSpec, &destData, &destLen ) ) {
		llog( LOG_ERROR, "Unable to convert sound: %s", SDL_GetError( ) );
		goto clean_up;
	}

	// store it
	samples[newIdx].data = mem_Allocate( destLen );

	memcpy( samples[newIdx].data, destData, destLen );

	samples[newIdx].numChannels = desiredChannels;
	samples[newIdx].numSamples = destLen / ( desiredChannels * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 ) );
	samples[newIdx].loops = loops;

clean_up:
	// clean up the working data
	mem_Release( loadData );
	SDL_free( destData );
	return newIdx;
}

typedef struct {
	const char* fileName;
	Uint8 desiredChannels;
	bool loops;
	int* outID;

	SDL_AudioStream* audioStream;

	float* audioData;
	int audioDataLen;

	//SDL_AudioCVT loadConverter;
	void ( *onLoadDone )( int );
} ThreadedSoundLoadData;

static void cleanUpThreadedSoundLoadData( ThreadedSoundLoadData* data )
{
	mem_Release( data->audioData );
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
	samples[newIdx].data = mem_Allocate( loadData->audioDataLen );
	SDL_memcpy( samples[newIdx].data, loadData->audioData, loadData->audioDataLen );

	samples[newIdx].numChannels = loadData->desiredChannels;
	samples[newIdx].numSamples = loadData->audioDataLen / ( loadData->desiredChannels * SDL_AUDIO_BYTESIZE( WORKING_FORMAT ) );
	samples[newIdx].loops = loadData->loops;

	(*(loadData->outID)) = newIdx;

clean_up:

	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( *( loadData->outID ) );

	cleanUpThreadedSoundLoadData( loadData );
}

static void loadSampleFailedJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into loadSampleFailedJob!" );
		return;
	}

	ThreadedSoundLoadData* loadData = (ThreadedSoundLoadData*)data;

	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( -1 );

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
	int destLen = 0;
	Uint8* destData = NULL;
	const SDL_AudioSpec srcSpec = { SDL_AUDIO_S16LE, channels, rate };
	const SDL_AudioSpec destSpec = { WORKING_FORMAT, loadData->desiredChannels, WORKING_RATE };
	if( !SDL_ConvertAudioSamples( &srcSpec, (const Uint8*)buffer, numSamples * channels * sizeof( buffer[0] ), &destSpec, &destData, &destLen ) ) {
		llog( LOG_ERROR, "Unable to convert sound: %s", SDL_GetError( ) );
		goto error;
	}

	// copy resulting data
	loadData->audioData = mem_Allocate( destLen );
	loadData->audioDataLen = destLen;
	SDL_memcpy( loadData->audioData, destData, destLen );
	SDL_free( destData );

	jq_AddMainThreadJob( bindSampleJob, (void*)loadData );

	return;

error:
	jq_AddMainThreadJob( loadSampleFailedJob, (void*)loadData );
}

void snd_ThreadedLoadSample( const char* fileName, Uint8 desiredChannels, bool loops, int* outID, void (*onLoadDone)( int ) )
{
	SDL_assert( ( desiredChannels >= 1 ) && ( desiredChannels <= 2 ) );
	SDL_assert( outID != NULL );

	(*outID) = -1;

	ThreadedSoundLoadData* loadData = mem_Allocate( sizeof( ThreadedSoundLoadData ) );
	if( loadData == NULL ) {
		llog( LOG_ERROR, "Unable to allocated data struct for threaded loading of sound sample" );
		if( onLoadDone != NULL ) onLoadDone( -1 );
		return;
	}

	loadData->fileName = fileName;
	loadData->desiredChannels = desiredChannels;
	loadData->loops = loops;
	loadData->outID = outID;
	loadData->onLoadDone = onLoadDone;
	loadData->audioData = NULL;

	jq_AddJob( loadSampleJob, (void*)loadData );
}

/* Sets up the SDL mixer. Returns 0 on success. */
int snd_Init( unsigned int numGroups )
{
	SDL_assert( numGroups > 0 );

	// clear out the samples storage
	SDL_memset( samples, 0, ARRAY_SIZE( samples ) * sizeof( samples[0] ) );
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		streamingSounds[i].access = NULL;
		streamingSounds[i].sdlStream = NULL;
		streamingSounds[i].playing = false;
		streamingSounds[i].timesLoaded = 0;
	}

	hashMap_Init( &streamingSoundHashMap, MAX_STREAMING_SOUNDS, NULL );

	if( idSet_Init( &playingIDSet, MAX_PLAYING_SOUNDS ) != 0 ) {
		llog( LOG_CRITICAL, "Failed to create playing sounds id set."  );
		return -1;
	}

	const SDL_AudioSpec spec = { WORKING_FORMAT, WORKING_CHANNELS, WORKING_RATE };
	mainAudioStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mixerCallback, NULL );

	if( mainAudioStream == NULL ) {
		llog( LOG_CRITICAL, "Failed to open audio device: %s", SDL_GetError( ) );
		return -1;
	}

	/*llog( LOG_DEBUG, "Audio: freq - %i  %i", desired.freq, delivered.freq );
	llog( LOG_DEBUG, "Audio: format - %i  %i", desired.format, delivered.format );
	llog( LOG_DEBUG, "Audio: channels - %i  %i", desired.channels, delivered.channels );
	llog( LOG_DEBUG, "Audio: samples - %i  %i", desired.samples, delivered.samples );//*/

	workingSilence = SDL_GetSilenceValueForFormat( WORKING_FORMAT );

	SDL_LockAudioStream( mainAudioStream );
	sb_Add( sbWorkingBuffer, INITIAL_WORKING_BUFFER_SIZE );
	SDL_UnlockAudioStream( mainAudioStream );
	if( sbWorkingBuffer == NULL ) {
		llog( LOG_CRITICAL, "Failed to create audio working buffer." );
		return -1;
	}

	sb_Add( sbSoundGroups, numGroups );
	for( size_t i = 0; i < sb_Count( sbSoundGroups ); ++i ) {
		sbSoundGroups[i].volume = 1.0f;
	}

	// load the master volume
	masterVolume = 1.0f;

	SDL_ResumeAudioStreamDevice( mainAudioStream );

	return 0;
}

void snd_CleanUp( )
{
	snd_StopStreamingAllBut( -1 );
	if( sbWorkingBuffer != NULL ) {
		SDL_LockAudioStream( mainAudioStream ); {
			sb_Release( sbStreamWorkingBuffer );
			sb_Release( sbWorkingBuffer );
		} SDL_LockAudioStream( mainAudioStream );
	}

	if( mainAudioStream == NULL ) return;
	SDL_DestroyAudioStream( mainAudioStream );
}

void snd_SetFocus( bool hasFocus )
{
	if( hasFocus ) {
		SDL_ResumeAudioStreamDevice( mainAudioStream );
	} else {
		SDL_PauseAudioStreamDevice( mainAudioStream );
	}
}

float snd_GetMasterVolume( void )
{
	return masterVolume;
}

void snd_SetMasterVolume( float volume )
{
	SDL_LockAudioStream( mainAudioStream ); {
		masterVolume = volume;
	} SDL_UnlockAudioStream( mainAudioStream );
}

float snd_GetVolume( unsigned int group )
{
    if( mainAudioStream == NULL ) return 0.0f;
    
	SDL_assert( group < sb_Count( sbSoundGroups ) );

	return sbSoundGroups[group].volume;
}

void snd_SetVolume( float volume, unsigned int group )
{
    if( mainAudioStream == NULL ) return;
    
	SDL_assert( group < sb_Count( sbSoundGroups ) );
	SDL_assert( ( volume >= 0.0f ) && ( volume <= 1.0f ) );

	SDL_LockAudioStream( mainAudioStream ); {
		sbSoundGroups[group].volume = volume;
	} SDL_UnlockAudioStream( mainAudioStream );
}

float snd_dBToVolume( float dB )
{
	return SDL_powf( 10.0f, 0.05f * dB );
}

float snd_VolumeTodB( float volume )
{
	return 20.0f * SDL_log10f( volume );
}

// Returns an id that can be used to change the volume and pitch
//  loops - if the sound will loop back to the start once it's over
//  volume - how loud the sound will be, in the range [0,1], 0 being off, 1 being loudest
//  pitch - pitch change for the sound, multiplies the sample rate, 1 for normal, lesser for slower, higher for faster
//  pan - how far left or right the sound is, 0 is center, -1 is left, +1 is right
// TODO: Some sort of event system so we can get when a sound has finished playing?
EntityID snd_Play( int sampleID, float volume, float pitch, float pan, unsigned int group )
{
    if( mainAudioStream == NULL ) {
        return INVALID_ENTITY_ID;
    }
    
	if( sampleID < 0 ) {
		return INVALID_ENTITY_ID;
	}

	SDL_assert( group >= 0 );
	SDL_assert( group < sb_Count( sbSoundGroups ) );

	EntityID playingID = INVALID_ENTITY_ID;
	SDL_LockAudioStream( mainAudioStream ); {
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
	} SDL_UnlockAudioStream( mainAudioStream );

	return playingID;
}

// Volume is assumed to be [0,1]
void snd_ChangeSoundVolume( EntityID soundID, float volume )
{
	SDL_LockAudioStream( mainAudioStream ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].volume = volume;
		}
	} SDL_UnlockAudioStream( mainAudioStream );
}

// Pitch is assumed to be > 0
void snd_ChangeSoundPitch( EntityID soundID, float pitch )
{
	SDL_LockAudioStream( mainAudioStream ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].pitch = pitch;
		}
	} SDL_UnlockAudioStream( mainAudioStream );
}

// Pan is assumed to be [-1,1]
void snd_ChangeSoundPan( EntityID soundID, float pan )
{
	SDL_LockAudioStream( mainAudioStream ); {
		if( idSet_IsIDValid( &playingIDSet, soundID ) ) {
			int idx = idSet_GetIndex( soundID );
			playingSounds[idx].pan = pan;
		}
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_Stop( EntityID soundID )
{
	SDL_LockAudioStream( mainAudioStream ); {
		idSet_ReleaseID( &playingIDSet, soundID );
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_UnloadSample( int sampleID )
{
	ASSERT_AND_IF_NOT( sampleID >= 0 ) return;
	ASSERT_AND_IF_NOT( sampleID < MAX_SAMPLES ) return;

	if( samples[sampleID].data == NULL ) {
		return;
	}

	SDL_LockAudioStream( mainAudioStream ); {
		// find all playing sounds using this sample and stop them
		for( EntityID id = idSet_GetFirstValidID( &playingIDSet ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &playingIDSet, id ) ) {
			int idx = idSet_GetIndex( id );
			if( playingSounds[idx].sample == sampleID ) {
				idSet_ReleaseID( &playingIDSet, id );
			}
		}

		mem_Release( samples[sampleID].data );
		samples[sampleID].data = NULL;
	} SDL_UnlockAudioStream( mainAudioStream );
}

//***** Streaming
int snd_LoadStreaming( const char* fileName, bool loops, unsigned int group )
{
	SDL_assert( group >= 0 );
	SDL_assert( group < sb_Count( sbSoundGroups ) );

	int foundValue;
	if( hashMap_Find( &streamingSoundHashMap, fileName, &foundValue ) ) {
		// this file is already loaded
		streamingSounds[foundValue].timesLoaded++;
		return foundValue;
	}

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
	streamingSounds[newIdx].loopPoint = 0;
	streamingSounds[newIdx].group = group;
	streamingSounds[newIdx].timesLoaded = 1;

	streamingSounds[newIdx].channels = (Uint8)( streamingSounds[newIdx].access->channels );
	if( streamingSounds[newIdx].channels > 2 ) {
		streamingSounds[newIdx].channels = 2;
	}

	hashMap_Set( &streamingSoundHashMap, fileName, newIdx );

	return newIdx;
}

typedef struct {
	const char* fileName;
	bool loops;
	unsigned int group;
	stb_vorbis* access;
	int* outID;
	void (*onLoadDone)( int );
} ThreadedLoadStreamingData;

static void cleanupThreadedLoadStreamingData( ThreadedLoadStreamingData* data )
{
	mem_Release( data );
}

static void bindStreamingJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into bindStreamingJob!" );
		return;
	}

	ThreadedLoadStreamingData* loadData = (ThreadedLoadStreamingData*)data;

	( *( loadData->outID ) ) = -1;

	// find spot to bind to
	int newIdx = -1;
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		if( streamingSounds[i].access == NULL ) {
			newIdx = i;
			break;
		}
	}

	if( newIdx < 0 ) {
		llog( LOG_ERROR, "Unable to find empty slot for streaming sound %s", loadData->fileName );
		goto clean_up;
	}

	streamingSounds[newIdx].access = loadData->access;
	streamingSounds[newIdx].playing = false;
	streamingSounds[newIdx].loops = loadData->loops;
	streamingSounds[newIdx].loopPoint = 0;
	streamingSounds[newIdx].group = loadData->group;

	streamingSounds[newIdx].channels = (Uint8)( streamingSounds[newIdx].access->channels );
	if( streamingSounds[newIdx].channels > 2 ) {
		streamingSounds[newIdx].channels = 2;
	}

	*( loadData->outID ) = newIdx;

clean_up:
	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( *( loadData->outID ) );
	mem_Release( data );
}

static void streamingLoadFailedJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into streamingLoadFailedJob!" );
		return;
	}

	ThreadedLoadStreamingData* loadData = (ThreadedLoadStreamingData*)data;
	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( -1 );

	mem_Release( data );
}

static void loadStreamingJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed into loadStreamingJob!" );
		return;
	}

	ThreadedLoadStreamingData* loadData = (ThreadedLoadStreamingData*)data;
	
	int error;
	loadData->access = stb_vorbis_open_filename( loadData->fileName, &error, NULL );
	if( loadData->access == NULL ) {
		llog( LOG_ERROR, "Unable to acquire a handle to streaming sound %s", loadData->fileName );
		goto error;
	}

	jq_AddMainThreadJob( bindStreamingJob, data );
	return;

error:
	jq_AddMainThreadJob( streamingLoadFailedJob, data );
}

void snd_ThreadedLoadStreaming( const char* fileName, bool loops, unsigned int group, int* outID, void (*onLoadDone)( int ) )
{
    if( mainAudioStream == NULL ) {
        if( onLoadDone != NULL ) {
            onLoadDone( 0 );
        }
        return;
    }
    
	SDL_assert( group >= 0 );
	SDL_assert( group < sb_Count( sbSoundGroups ) );

	(*outID) = -1;

	ThreadedLoadStreamingData* loadData = mem_Allocate( sizeof( ThreadedLoadStreamingData ) );
	if( loadData == NULL ) {
		llog( LOG_ERROR, "Unable to allocate data struct for threaded loading of music" );
		if( onLoadDone != NULL ) onLoadDone( -1 );
		return;
	}

	loadData->fileName = fileName;
	loadData->loops = loops;
	loadData->group = group;
	loadData->access = NULL;
	loadData->onLoadDone = onLoadDone;
	loadData->outID = outID;

	jq_AddJob( loadStreamingJob, (void*)loadData );
}

// if the stream loops, where it will start again
void snd_ChangeStreamLoopPoint( int streamID, unsigned int loopPoint )
{
	if( ( streamID < 0 ) || ( streamID >= MAX_STREAMING_SOUNDS ) ) {
		return;
	}

	streamingSounds[streamID].loopPoint = loopPoint;
}

void snd_PlayStreaming( int streamID, float volume, float pan, unsigned int startSample ) // todo: fade in?
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	if( ( streamID < 0 ) || ( streamID >= MAX_STREAMING_SOUNDS ) ) {
		return;
	}

	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );

	if( streamingSounds[streamID].playing ) {
		return;
	}

	SDL_LockAudioStream( mainAudioStream ); {
		if( ( streamingSounds[streamID].access != NULL ) && !streamingSounds[streamID].playing ) {

			const SDL_AudioSpec srcSpec = { SDL_AUDIO_S16LE, streamingSounds[streamID].access->channels, streamingSounds[streamID].access->sample_rate };
			const SDL_AudioSpec destSpec = { WORKING_FORMAT, WORKING_CHANNELS, WORKING_RATE };
			streamingSounds[streamID].sdlStream = SDL_CreateAudioStream( &srcSpec, &destSpec );

			if( streamingSounds[streamID].sdlStream == NULL ) {
				llog( LOG_ERROR, "Unable to create SDL_AudioStream for streaming sound." );
				return;
			}

			streamingSounds[streamID].playing = true;
			streamingSounds[streamID].volume = volume;
			streamingSounds[streamID].pan = pan;

			//stb_vorbis_seek_start( streamingSounds[streamID].access );
			stb_vorbis_seek_frame( streamingSounds[streamID].access, startSample );

			streamingSounds[streamID].readDone = false;
		}
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_StopStreaming( int streamID )
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	if( ( streamID < 0 ) || ( streamID >= MAX_STREAMING_SOUNDS ) ) {
		return;
	}

	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioStream( mainAudioStream ); {
		streamingSounds[streamID].playing = false;
		SDL_DestroyAudioStream( streamingSounds[streamID].sdlStream );
		streamingSounds[streamID].sdlStream = NULL;
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_StopStreamingAllBut( int streamID )
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	SDL_LockAudioStream( mainAudioStream ); {
		for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
			if( i == streamID ) continue;
			if( ( streamingSounds[i].access != NULL ) && streamingSounds[i].playing ) {
				streamingSounds[i].playing = false;
				SDL_DestroyAudioStream( streamingSounds[i].sdlStream );
				streamingSounds[i].sdlStream = NULL;
			}
		}
	} SDL_UnlockAudioStream( mainAudioStream );
}

bool snd_IsStreamPlaying( int streamID )
{
	if( mainAudioStream == NULL ) {
		return false;
	}
    
	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	return streamingSounds[streamID].playing;
}

void snd_ChangeStreamVolume( int streamID, float volume )
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioStream( mainAudioStream ); {
		streamingSounds[streamID].volume = volume;
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_ChangeStreamPan( int streamID, float pan )
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioStream( mainAudioStream ); {
		streamingSounds[streamID].pan = pan;
	} SDL_UnlockAudioStream( mainAudioStream );
}

void snd_UnloadStream( int streamID )
{
	if( mainAudioStream == NULL ) {
		return;
	}
    
	SDL_assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	if( streamingSounds[streamID].timesLoaded >= 1 ) {
		--streamingSounds[streamID].timesLoaded;
		SDL_LockAudioStream( mainAudioStream ); {
			streamingSounds[streamID].playing = false;
			stb_vorbis_close( streamingSounds[streamID].access );
			streamingSounds[streamID].access = NULL;
			SDL_DestroyAudioStream( streamingSounds[streamID].sdlStream );
			streamingSounds[streamID].sdlStream = NULL;
			hashMap_RemoveFirstByValue( &streamingSoundHashMap, streamID );
		} SDL_UnlockAudioStream( mainAudioStream );
	}
}
