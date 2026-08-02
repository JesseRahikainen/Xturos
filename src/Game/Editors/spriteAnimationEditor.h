#ifndef SPRITE_ANIMATION_EDITOR
#define SPRITE_ANIMATION_EDITOR

#include <SDL3/SDL.h>

void spriteAnimationEditor_Init( void );
void spriteAnimationEditor_Show( void );
void spriteAnimationEditor_Hide( void );
void spriteAnimationEditor_IMGUIProcess( void );
void spriteAnimationEditor_Tick( float dt );
void spriteAnimationEditor_ProcessEvents( SDL_Event* evt );

#endif