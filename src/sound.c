#include "sound.h"

#include <stdio.h>
#include <string.h>

#define MAX_SOUNDS 256
#define MAX_SONGS 16

static int audioRate = 22050;
static Uint16 audioFormat = AUDIO_S16; /* 16-bit stereo */
static int audioChannels = 2;
/* static int audioBuffers = 4096; */
static int audioBuffers = 1024;

static Mix_Music* currMusic = NULL;
static Mix_Chunk* sounds[MAX_SOUNDS];

/* Sets up the SDL mixer. Returns 0 on success. */
int initMixer( )
{
	int typeFlags = MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_MODPLUG;
	int initRet = Mix_Init(typeFlags);

	if( Mix_OpenAudio( audioRate, audioFormat, audioChannels, audioBuffers ) != 0 ) {
		return -1;
	}

	initRet = Mix_Init(typeFlags);
	if( ( initRet & typeFlags ) != typeFlags) {
		printf("Mix_Init: %s\n", Mix_GetError());
		return -1;
	}

	Mix_VolumeMusic( MIX_MAX_VOLUME / 8 );

	memset( sounds, 0, sizeof( sounds ) );

	return 0;
}

void shutDownMixer( )
{
	stopSong( );

	while( Mix_Init( 0 ) ) {
		Mix_Quit( );
	}

	Mix_CloseAudio( );
}

/* Plays a song. */
void playSong( const char* fileName )
{
	stopSong( );

	currMusic = Mix_LoadMUS( fileName );
	if( currMusic != NULL ) {
		Mix_PlayMusic( currMusic, -1 );
	}
}

/* Stops the currently playing song. */
void stopSong( )
{
	if( currMusic != NULL ) {
		Mix_HaltMusic( );
		Mix_FreeMusic( currMusic );
		currMusic = NULL;
	}
}

/*
Loads the sound at the file name.
 Returns the id to use for playing, returns -1 on failure.
*/
int loadSound( const char* fileName )
{
	int newIdx;

	/* find the first open spot */
	newIdx = 0;
	while( ( newIdx < MAX_SOUNDS ) && ( sounds[newIdx] != NULL ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_SOUNDS ) {
		SDL_LogError( SDL_LOG_CATEGORY_AUDIO, "Unable to load sound %s! Sound storage full.", fileName );
		return -1;
	}

	/* found a spot, load it */
	sounds[newIdx] = Mix_LoadWAV( fileName );
	if( sounds[newIdx] == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_AUDIO, "Error: %s on file %s", Mix_GetError( ), fileName );
		return -1;
	}

	return newIdx;
}

/* Plays a sound. */
void playSound( int idx )
{
	// NOTE: This queues up the playing of a sound, can lead to problems if you call this multiple times in a row.
	if( sounds[idx] != NULL ) {
		Mix_PlayChannel( -1, sounds[idx], 0 );
	}
}

/* Cleans up memory for a sound, becomes invalid afterwards. */
void cleanSound( int idx )
{
	if( ( idx >= MAX_SOUNDS ) || ( sounds[idx] == NULL ) ) {
		return;
	}

	Mix_FreeChunk( sounds[idx] );
	sounds[idx] = NULL;
}