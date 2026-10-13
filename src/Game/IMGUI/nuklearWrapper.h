#ifndef NUKLEAR_WRAPPER_H
#define NUKLEAR_WRAPPER_H

// We should only need this file in the main game loop, anything that wants to use the ui stuff
// should only need to include nuklear.h and use that.
// This is just an adaptation of the nuklear_sdl_gl3 example.
// In a screen state you should draw the UI every frame in the Process function instead of in
// the Draw function.

#include <stdbool.h>
#include <SDL3/SDL_events.h>

#if defined( WIN32 ) || defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
	#include "IMGUI/Platform/OpenGL/nuklearWrapper_OpenGL.h"
#else
	#warning "NOTHING IMPLEMENTED FOR THIS GRAPHICS PLATFORM!"
#endif

#include "nuklearHeader.h"

typedef enum {
	NWT_XTUROS
} NuklearWrapperType;

typedef union NuklearWrapper NuklearWrapper;

typedef struct NuklearWrapperBase {
	NuklearWrapperType type;

	// shared data
	SDL_Window* win;
	struct nk_buffer cmds;
	struct nk_draw_null_texture nullTx;
	int renderWidth;
	int renderHeight;
	bool useRelativeMousePos;
	struct nk_context ctx;
	struct nk_font_atlas fontAtlas;

	// wrapper type specific functions
	void (*handleEvent)( NuklearWrapper* wrapper, SDL_Event* e );
	void (*setRenderSize)( NuklearWrapper* wrapper, int renderWidth, int renderHeight );
	void (*render)( NuklearWrapper* wrapper );
	void (*shutdown)( NuklearWrapper* wrapper );
	void (*clear)( NuklearWrapper* wrapper );
	struct nk_image (*loadImage)( NuklearWrapper* wrapper, const char* filePath, int* outWidth, int* outHeight );
	void (*unloadImage)( NuklearWrapper* wrapper, struct nk_image* image );
	void (*fontStashBegin)( NuklearWrapper* wrapper, struct nk_font_atlas** atlas );
	void (*fontStashEnd)( NuklearWrapper* wrapper, float fontHeight );
} NuklearWrapperBase;

typedef struct {
	NuklearWrapperBase base;
	NuklearWrapper_Platform platform;
} NuklearWrapperXturos;

typedef union NuklearWrapper {
	NuklearWrapperBase base;
	NuklearWrapperXturos xu;
} NuklearWrapper;

extern NuklearWrapper editorIMGUI;
extern NuklearWrapper inGameIMGUI;

void nk_xturos_init( NuklearWrapper* wrapper );

void nk_wrapper_InitMessageListeners( void );
void nk_wrapper_Init( NuklearWrapperType type, SDL_Window* win, bool useRelativeMousePos, int renderWidth, int renderHeight, NuklearWrapper* outWrapper );

// will just call the wrappers function
void nk_wrapper_HandleEvent( NuklearWrapper* wrapper, SDL_Event* e );
void nk_wrapper_SetRenderSize( NuklearWrapper* wrapper, int renderWidth, int renderHeight );
void nk_wrapper_Render( NuklearWrapper* wrapper );
void nk_wrapper_Shutdown( NuklearWrapper* wrapper );
void nk_wrapper_Clear( NuklearWrapper* wrapper );
struct nk_image nk_wrapper_LoadImage( NuklearWrapper* wrapper, const char* filePath, int* outWidth, int* outHeight );
void nk_wrapper_UnloadImage( NuklearWrapper* wrapper, struct nk_image* image );
void nk_wrapper_FontStashBegin( NuklearWrapper* wrapper, struct nk_font_atlas** atlas );
void nk_wrapper_FontStashEnd( NuklearWrapper* wrapper, float fontHeight );

#endif