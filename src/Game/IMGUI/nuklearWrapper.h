#ifndef NUKLEAR_WRAPPER_H
#define NUKLEAR_WRAPPER_H

/*
We should only need this file in the main game loop, anything that wants to use the ui stuff
should only need to include nuklear.h and use that.
This is just an adaptation of the nuklear_sdl_gl3 example.
*/

#include <stdbool.h>
#include <SDL_events.h>
#include "../Graphics/glDebugging.h"
#include "../Graphics/glPlatform.h"

#include "../Graphics/shaderManager.h"

#include "nuklearHeader.h"

typedef struct {
	SDL_Window* win;

	struct nk_buffer cmds;
	struct nk_draw_null_texture nullTx;

	// just use the default shader
	GLuint vbo;
	GLuint vao;
	GLuint ebo;

	ShaderProgram prog;

	GLuint fontTx;

	int renderWidth;
	int renderHeight;

	bool useRelativeMousePos;

	struct nk_context ctx;
	struct nk_font_atlas fontAtlas;
} NuklearWrapper;

extern NuklearWrapper editorIMGUI;
extern NuklearWrapper inGameIMGUI;

//extern struct nk_context* nkCtx;

void nk_xu_init( NuklearWrapper* xu, SDL_Window* win, bool useRelativeMousePos, int renderWidth, int renderHeight );
void nk_xu_fontStashBegin( NuklearWrapper* xu, struct nk_font_atlas** atlas );
void nk_xu_fontStashEnd( NuklearWrapper* xu );
void nk_xu_handleEvent( NuklearWrapper* xu, SDL_Event* e );
void nk_xu_render( NuklearWrapper* xu );
void nk_xu_shutdown( NuklearWrapper* xu );
void nk_xu_clear( NuklearWrapper* xu );

#endif