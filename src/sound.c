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

#define MAX_SAMPLES 256
#define MAX_PLAYING_SOUNDS 32
#define MAX_STREAMING_SOUNDS 8
#define STREAMING_BUFFER_SAMPLES 4096

#if defined( __EMSCRIPTEN__ )

// can't get sound working with emscripten right now, get it working later
int snd_Init( unsigned int numGroups ) { return 0; }
void snd_CleanUp( ) { }
void snd_SetFocus( bool hasFocus ) { }
float snd_GetMasterVolume( void ) { return 0.0f; }
void snd_SetMasterVolume( float volume ) { }
float snd_GetVolume( unsigned int group ) { return 0.0f; }
void snd_SetVolume( float volume, unsigned int group ) { }
int snd_LoadSample( const char* fileName, Uint8 desiredChannels, bool loops ) { return 0; }
EntityID snd_Play( int sampleID, float volume, float pitch, float pan, unsigned int group ) { return 0; }
void snd_ChangeSoundVolume( EntityID soundID, float volume ) { }
void snd_ChangeSoundPitch( EntityID soundID, float pitch ) { }
void snd_ChangeSoundPan( EntityID soundID, float pan ) { }
void snd_Stop( EntityID soundID ) { }
void snd_UnloadSample( int sampleID ) { }
int snd_LoadStreaming( const char* fileName, bool loops, unsigned int group ) { return 0; }
void snd_PlayStreaming( int streamID, float volume, float pan ) { }
void snd_StopStreaming( int streamID ) { }
void snd_StopStreamingAllBut( int streamID ) { }
bool snd_IsStreamPlaying( int streamID ) { return false; }
void snd_ChangeStreamVolume( int streamID, float volume ) { }
void snd_ChangeStreamPan( int streamID, float pan ) { }
void snd_UnloadStream( int streamID ) { }

#else

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
	float* buffer;

	int bufferCurrPos;
	int bufferUsed;
	int bufferCount;

	stb_vorbis* access;

	float volume;
	float pan;
	bool loops;
	unsigned int group;

	// this stuff may be able to be removed
	int totalSamples; // numSamples * channels ( channels 1 if access has 1 channel, 2 otherwise )
	int totalSize; // the number of bytes in the buffer

	Uint8 channels;

	SDL_AudioCVT cvt;
} StreamingSound;

static SDL_AudioSpec desired;
static SDL_AudioSpec actual;
static SDL_AudioDeviceID devID = 0;

static SDL_AudioCVT workingConverter;

static const Uint8 WORKING_CHANNELS = 2;
static const SDL_AudioFormat WORKING_FORMAT = AUDIO_F32;
static const int WORKING_RATE = 44100;
static Uint16 AUDIO_SAMPLES = 1024;//4096;
static int workingConversionNeeded;
static int workingBufferSize;

static Sample sineWave;

static float* workingBuffer = NULL;

static Sample samples[MAX_SAMPLES];
static Sound playingSounds[MAX_PLAYING_SOUNDS];
static struct IDSet playingIDSet; // we want to be able to change the currently playing sounds, this will help

static StreamingSound streamingSounds[MAX_STREAMING_SOUNDS];

static void* soundCfgFile;

typedef struct {
	float volume;
} SoundGroup;
static SoundGroup* sbSoundGroups;

static float masterVolume = 1.0f;

// stereo LRLRLR order
void mixerCallback( void* userdata, Uint8* stream, int len )
{
	memset( stream, len, actual.silence );
	memset( workingBuffer, 0, workingBufferSize );
	if( workingBuffer == NULL ) {
		return;
	}

	int numSamples = ( ( len / actual.channels ) / ( ( SDL_AUDIO_MASK_BITSIZE & actual.format ) / 8 ) );
	int workingSize = numSamples * WORKING_CHANNELS * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 );

	// advance each playing sound
	EntityID id = idSet_GetFirstValidID( &playingIDSet );
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
		for( int s = 0; ( s < numSamples ) && !soundDone; ++s ) {
			// if the next position would be past what we have loaded then load some more
			if( ( stream->bufferCurrPos + stream->channels ) > stream->bufferUsed ) {
				// copy over the parts that we still need to the start
				int n = 0;
				for( int x = (int)stream->bufferCurrPos; x < stream->bufferUsed; ++x, ++n ) {
					stream->buffer[n] = stream->buffer[x];
				}

				int read = 0;
				int totalReadBytes = 0;
				int samplesRead = n;
				stream->bufferCurrPos = n;
				stream->bufferUsed = n;
				short* readBuffer = (short*)( &( stream->buffer[n] ) );
				bool readDone = false;
				do {
					// request == num shorts
					int request = stream->totalSamples - samplesRead;
					read = stb_vorbis_get_samples_short_interleaved(
						stream->access, stream->access->channels, readBuffer, request ); 
					read *= stream->access->channels;
					totalReadBytes += read * sizeof( short );
					samplesRead += read;

					// reached the end of the file, are we looping?
					if( read != request ) {
						if( stream->loops ) {
							stb_vorbis_seek_start( stream->access );
							readBuffer = (short*)( (uintptr_t)readBuffer + totalReadBytes );
						} else {
							readDone = true;
						}
					}
				} while( !readDone && ( samplesRead < stream->totalSamples ) );

				// now convert what we read in to our working data format
				stream->cvt.buf = (Uint8*)( &( stream->buffer[n] ) );
				stream->cvt.len = totalReadBytes;
				SDL_ConvertAudio( &( stream->cvt ) );
				stream->bufferUsed += stream->cvt.len_cvt / sizeof( stream->buffer[0] );
			}

			if( (int)( stream->bufferCurrPos ) < stream->bufferUsed ) {
			// then just mix those samples
			int streamIdx = ( s * WORKING_CHANNELS );
			
			// we're assuming stereo output here
			if( stream->access->channels == 1 ) {
				float data = stream->buffer[stream->bufferCurrPos] * volume;
/* left */		workingBuffer[streamIdx] += data * inverseLerp( 1.0f, 0.0f, stream->pan );
/* right */		workingBuffer[streamIdx+1] += data * inverseLerp( -1.0f, 0.0f, stream->pan );
				stream->bufferCurrPos += 1;
			} else {
				// if the sample is stereo then we ignore panning
				workingBuffer[streamIdx] += stream->buffer[(int)stream->bufferCurrPos] * volume;
				workingBuffer[streamIdx+1] += stream->buffer[(int)stream->bufferCurrPos + 1] * volume;
				stream->bufferCurrPos += 2;
			}
			} else {
				soundDone = true;
				stream->playing = false;
			}
		}

		if( soundDone ) {
			streamingSounds[i].playing = false;
		}
	}

	// convert working buffer to destination buffer
	workingConverter.len = workingSize;
	workingConverter.buf = (Uint8*)workingBuffer;
	SDL_ConvertAudio( &workingConverter );

	// now copy the data over to the stream
	int cvtSize = (int)( workingConverter.len * workingConverter.len_ratio );
	memcpy( stream, workingConverter.buf, cvtSize );
}

int snd_LoadSample( const char* fileName, Uint8 desiredChannels, bool loops )
{
	// TODO: Get multiple channels working, will probably only support up to 2
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
	short* data;
	int numSamples = stb_vorbis_decode_filename( fileName, &channels, &rate, &data );

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

	SDL_ConvertAudio( &loadConverter ); // convert audio is corrupting memory!

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

/* Sets up the SDL mixer. Returns 0 on success. */
int snd_Init( unsigned int numGroups )
{
	assert( numGroups > 0 );

	// clear out the samples storage
	SDL_memset( samples, 0, ARRAY_SIZE( samples ) * sizeof( samples[0] ) );
	for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
		streamingSounds[i].access = NULL;
		streamingSounds[i].buffer = NULL;
		streamingSounds[i].playing = false;
	}

	SDL_memset( &desired, 0, sizeof( desired ) );
	desired.freq = WORKING_RATE;
	desired.format = AUDIO_S16;
	desired.channels = WORKING_CHANNELS;
	desired.samples = AUDIO_SAMPLES;
	desired.callback = mixerCallback;
	desired.userdata = NULL;

	if( idSet_Init( &playingIDSet, MAX_PLAYING_SOUNDS ) != 0 ) {
		llog( LOG_CRITICAL, "Failed to create playing sounds id set."  );
		return -1;
	}

	devID = SDL_OpenAudioDevice( NULL, 0, &desired, &actual, SDL_AUDIO_ALLOW_FORMAT_CHANGE );

	if( devID == 0 ) {
		llog( LOG_CRITICAL, "Failed to open audio device: %s", SDL_GetError( ) );
		return -1;
	}
	SDL_PauseAudioDevice( devID, 0 );

	workingConversionNeeded = SDL_BuildAudioCVT( &workingConverter,
		WORKING_FORMAT, WORKING_CHANNELS, WORKING_RATE,
		actual.format, actual.channels, actual.freq );

	if( workingConversionNeeded < 0 ) {
		llog( LOG_CRITICAL, "Failed to build conversion: %s", SDL_GetError( ) );
		return -1;
	}

	int numSamples = ( ( actual.size / actual.channels ) / ( ( SDL_AUDIO_MASK_BITSIZE & actual.format ) / 8 ) );
	int workingSize = numSamples * WORKING_CHANNELS * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 );

	SDL_LockAudioDevice( devID );
	workingBufferSize = workingSize * workingConverter.len_mult;
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
		cfg_GetInt( soundCfgFile, "vol", 1, &vol );
		masterVolume = (float)vol;
	} else {
		masterVolume = 1.0f;
	}

	return 0;
}

void snd_CleanUp( )
{
	if( workingBuffer != NULL ) {
		SDL_LockAudioDevice( devID ); {
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
		EntityID id = idSet_GetFirstValidID( &playingIDSet );
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
	streamingSounds[newIdx].access = stb_vorbis_open_filename( fileName, &error, NULL ); // alloc?
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

	if( SDL_BuildAudioCVT( &( streamingSounds[newIdx].cvt ),
		AUDIO_S16, (Uint8)( streamingSounds[newIdx].access->channels ), streamingSounds[newIdx].access->sample_rate,
		WORKING_FORMAT, streamingSounds[newIdx].channels, WORKING_RATE ) < 0 ) {
		llog( LOG_ERROR, "Unable to create converter for streaming sound." );
		stb_vorbis_close( streamingSounds[newIdx].access );
		return -1;
	}


	streamingSounds[newIdx].totalSamples = STREAMING_BUFFER_SAMPLES * streamingSounds[newIdx].channels;
	// so cvt.len can be at most: ( streamingSounds[newIdx].totalSamples * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 ) )
	streamingSounds[newIdx].totalSize = streamingSounds[newIdx].totalSamples * ( ( SDL_AUDIO_MASK_BITSIZE & WORKING_FORMAT ) / 8 ) * streamingSounds[newIdx].cvt.len_mult;

	return newIdx;
}

void snd_PlayStreaming( int streamID, float volume, float pan ) // todo: fade in?
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		if( ( streamingSounds[streamID].access != NULL ) && !streamingSounds[streamID].playing ) {
			streamingSounds[streamID].playing = true;
			streamingSounds[streamID].volume = volume;
			streamingSounds[streamID].pan = pan;

			stb_vorbis_seek_start( streamingSounds[streamID].access );

			// allocate buffer memory for the streaming sound
			streamingSounds[streamID].buffer = mem_Allocate( streamingSounds[streamID].totalSize );
			streamingSounds[streamID].bufferCurrPos = 0;
			streamingSounds[streamID].bufferUsed = 0;
		}
	} SDL_UnlockAudioDevice( devID );
}

void snd_StopStreaming( int streamID )
{
	assert( ( streamID >= 0 ) && ( streamID < MAX_STREAMING_SOUNDS ) );
	SDL_LockAudioDevice( devID ); {
		streamingSounds[streamID].playing = false;
		mem_Release( streamingSounds[streamID].buffer );
		streamingSounds[streamID].buffer = NULL;
	} SDL_UnlockAudioDevice( devID );
}

void snd_StopStreamingAllBut( int streamID )
{
	SDL_LockAudioDevice( devID ); {
		for( int i = 0; i < MAX_STREAMING_SOUNDS; ++i ) {
			if( i == streamID ) continue;
			if( ( streamingSounds[i].access != NULL ) && streamingSounds[i].playing ) {
				streamingSounds[i].playing = false;
				mem_Release( streamingSounds[i].buffer );
				streamingSounds[i].buffer = NULL;
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
	} SDL_UnlockAudioDevice( devID );
}

#endif // emscripten test