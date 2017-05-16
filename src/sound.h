#ifndef JTR_SOUND
#define JTR_SOUND

#include <stdbool.h>
#include <SDL_types.h>

#include "Utils\idSet.h"

// Sets up the SDL mixer. Returns 0 on success.
int snd_Init( unsigned int numGroups );

// Shuts down SDL mixer.
void snd_CleanUp( );

void snd_SetFocus( bool hasFocus );

float snd_GetMasterVolume( void );
void snd_SetMasterVolume( float volume );

float snd_GetVolume( unsigned int group );
void snd_SetVolume( float volume, unsigned int group );

//***** Loaded all at once
int snd_LoadSample( const char* fileName, Uint8 desiredChannels, bool loops );
void snd_ThreadedLoadSample( const char* fileName, Uint8 desiredChannels, bool loops, int* outID );

// Returns an id that can be used to change the volume and pitch
//  volume - how loud the sound will be, in the range [0,1], 0 being off, 1 being loudest
//  pitch - pitch change for the sound, multiplies the sample rate, 1 for normal, lesser for slower, higher for faster
//  pan - how far left or right the sound is, 0 is center, -1 is left, +1 is right
// TODO: Some sort of event system so we can get when a sound has finished playing?
EntityID snd_Play( int sampleID, float volume, float pitch, float pan, unsigned int group );

void snd_ChangeSoundVolume( EntityID soundID, float volume ); // Volume is assumed to be [0,1]
void snd_ChangeSoundPitch( EntityID soundID, float pitch ); // Pitch is assumed to be > 0
void snd_ChangeSoundPan( EntityID soundID, float pan ); // Pan is assumed to be [-1,1]
void snd_Stop( EntityID soundID );
void snd_UnloadSample( int sampleID );

//***** Streaming
int snd_LoadStreaming( const char* fileName, bool loops, unsigned int group );
void snd_PlayStreaming( int streamID, float volume, float pan ); // todo: fade in?
void snd_StopStreaming( int streamID );
void snd_StopStreamingAllBut( int streamID );
bool snd_IsStreamPlaying( int streamID );
void snd_ChangeStreamVolume( int streamID, float volume );
void snd_ChangeStreamPan( int streamID, float pan );
void snd_UnloadStream( int streamID );

#endif
