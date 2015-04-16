#ifndef JTR_SOUND
#define JTR_SOUND

#include <SDL.h>
#include <SDL_mixer.h>

/* Sets up the SDL mixer. Returns 0 on success. */
int initMixer( );

/* Shuts down SDL mixer. */
void shutDownMixer( );


/* Plays a song. */
void playSong( const char* fileName );

/* Stops the currently playing song. */
void stopSong( );


/*
Loads the sound at the file name.
 Returns the id to use for playing, returns -1 on failure.
*/
int loadSound( const char* fileName );

/* Plays a sound. */
void playSound( int soundId );

/* Cleans up memory for a sound, becomes invalid afterwards. */
void cleanSound( int idx );

#endif
