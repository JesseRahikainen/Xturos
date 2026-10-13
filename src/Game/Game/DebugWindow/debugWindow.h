#ifndef DEBUG_WINDOW_H
#define DEBUG_WINDOW_H

#include <SDL3/SDL.h>

void debugWindow_Open( void );
void debugWindow_Close( void );

SDL_WindowID debugWindow_GetWindowID( void );

void debugWindow_Update( void );
void debugWindow_PreProcessEvents( void );
void debugWindow_ProcessEvent( SDL_Event* evt );
void debugWindow_PostProcessEvents( void );

#endif // inclusion guard