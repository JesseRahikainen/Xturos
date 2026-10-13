#include "debugWindow.h"

#include <stdio.h>

#include <SDL3/SDL.h>

#include "System/memory.h"
#include "System/platformLog.h"

#include "IMGUI/nuklearHeader.h"

#define NK_SDL3_RENDERER_IMPLEMENTATION
#pragma warning( push )
#pragma warning( disable : 4116 )
#pragma warning( disable : 4244 )
#include "Others/nuklear_sdl3_renderer.h"
#pragma warning( pop )

void entityTreeDebugWindow( struct nk_context* ctx );

// this is window using the standard SDL renderer, with a Nuklear implementation so we can display debugging information in a separate window
//  and not have it take up space on the main game screen

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static struct nk_context* nkCtx = NULL;

const int WIDTH = 1920;
const int HEIGHT = 1080;

static void* customAlloc( nk_handle handle, void* old, nk_size size )
{
	// this is styled like a realloc, but it's actually just an alloc
	return mem_Allocate( size );
}

static void customFree( nk_handle handle, void* p )
{
	mem_Release( p );
}

void debugWindow_Open( void )
{
	if( window != NULL ) {
		llog( LOG_ERROR, "Debugger window already open." );
		return;
	}
	if( !SDL_CreateWindowAndRenderer( "Debugger", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS, &window, &renderer ) ) {
		llog( LOG_ERROR, "Error creating debug window: %s", SDL_GetError( ) );
		return;
	}
	//SDL_SetRenderLogicalPresentation( renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX );
	SDL_SetRenderLogicalPresentation( renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_DISABLED );

	// now load the font, initilize the SDL3 Nuklear interface (maybe later when we switch to SDL_GPU stuff we can just use that instead)
	struct nk_allocator allocator = { .alloc = customAlloc, .free = customFree, .userdata = nk_handle_id( 0 ) };
	nkCtx = nk_sdl_init( window, renderer, allocator );

	float font_scale = 1;
	{
		// This scaling logic was kept simple for the demo purpose.
		// On some platforms, this might not be the exact scale
		// that you want to use. For more information, see:
		// https://wiki.libsdl.org/SDL3/README-highdpi
		const float scale = SDL_GetWindowDisplayScale( window );
		SDL_SetRenderScale( renderer, scale, scale );
		font_scale = scale;
	}

	struct nk_font_atlas* atlas;
	struct nk_font_config config = nk_font_config( 0 );
	struct nk_font* font;

	// set up the font atlas and add desired font; note that font sizes are
	// multiplied by font_scale to produce better results at higher DPIs
	atlas = nk_sdl_font_stash_begin( nkCtx );
	font = nk_font_atlas_add_default( atlas, 13 * font_scale, &config );
	//font = nk_font_atlas_add_from_file( atlas, "font_file_here.ttf", 14 * font_scale, &config );
	nk_sdl_font_stash_end( nkCtx );

	/* this hack makes the font appear to be scaled down to the desired
	 * size and is only necessary when font_scale > 1 */
	font->handle.height /= font_scale;
	/*nk_style_load_all_cursors(ctx, atlas->cursors);*/
	nk_style_set_font( nkCtx, &font->handle );
}

void debugWindow_Close( void )
{
	if( renderer != NULL ) {
		SDL_DestroyRenderer( renderer );
		renderer = NULL;
	}

	if( window != NULL ) {
		SDL_DestroyWindow( window );
		window = NULL;
	}
}

SDL_WindowID debugWindow_GetWindowID( void )
{
	if( window == NULL ) return 0;
	return SDL_GetWindowID( window );
}

// main update function
void debugWindow_Update( void )
{
	if( renderer == NULL ) return;

	nk_input_end( nkCtx );

	// do the debugger stuff here
	entityTreeDebugWindow( nkCtx );

	// draw everything
	SDL_SetRenderDrawColor( renderer, 0, 0, 16, 255 );
	SDL_RenderClear( renderer );

	nk_sdl_render( nkCtx, NK_ANTI_ALIASING_ON );
	nk_sdl_update_TextInput( nkCtx );

	SDL_RenderPresent( renderer );

	nk_input_begin( nkCtx );
}

void debugWindow_PreProcessEvents( void )
{
}

void debugWindow_ProcessEvent( SDL_Event* evt )
{
	if( nkCtx == NULL ) return;

	SDL_ConvertEventToRenderCoordinates( renderer, evt );
	nk_sdl_handle_event( nkCtx, evt );
}

void debugWindow_PostProcessEvents( void )
{
}

//====================================================================
#include "DefaultECPS/defaultECPS.h"
#include "Utils/stretchyBuffer.h"

static void displayEntity( struct nk_context* ctx, ECPS* ecps, EntityID id )
{
	for( ComponentID compID = 0; compID < sb_Count( ecps->componentTypes.sbTypes ); ++compID ) {
		if( ecps_DoesEntityHaveComponentByID( ecps, id, compID ) ) {

		}
	}
}

static bool* sbIsExpanded = NULL;
static EntityID displayedEntity = INVALID_ENTITY_ID;
static EntityID* sbDestroyThis = NULL;

void entityTreeDebug_Entity( struct nk_context* ctx, ECPS* ecps, EntityID id, int depth )
{
	uint16_t idx = idSet_GetIndex( id );
	GCTransformData* tfData = NULL;
	ecps_GetComponentFromEntityByID( ecps, id, gcTransformCompID, &tfData );

	bool hasChildren = ( tfData != NULL ) && ( tfData->firstChildID != INVALID_ENTITY_ID );

	struct nk_style_button style = ctx->style.button;
	style.text_alignment = NK_TEXT_LEFT;

	char name[256];
	SDL_snprintf( name, ARRAY_SIZE( name ), "%*s%u", depth, "", (unsigned int)id );
	nk_layout_row_begin( ctx, NK_DYNAMIC, 16.0f, 3 ); {
		nk_layout_row_push( ctx, 0.1f );
		if( hasChildren ) {
			const char* str = ( idx < sb_Count( sbIsExpanded ) ) && sbIsExpanded[idx] ? "-" : "+";
			if( nk_button_label( ctx, str ) && ( idx < sb_Count( sbIsExpanded ) ) ) {
				sbIsExpanded[idx] = !sbIsExpanded[idx];
			}
		} else {
			nk_spacer( ctx );
		}

		nk_layout_row_push( ctx, 0.8f );
		if( nk_button_label_styled( ctx, &style, name ) ) {
			displayedEntity = id;
		}

		nk_layout_row_push( ctx, 0.1f );
		if( nk_button_label( ctx, "X" ) ) {
			// destroy the entity
			sb_Push( sbDestroyThis, id );
		}
	} nk_layout_row_end( ctx );

	if( tfData == NULL ) return; // no linkages to worry about, just finish up

	if( ( idx < sb_Count( sbIsExpanded ) ) && sbIsExpanded[idx] && ( tfData->firstChildID != INVALID_ENTITY_ID ) ) {
		entityTreeDebug_Entity( ctx, ecps, tfData->firstChildID, depth + 1 );
	}

	if( tfData->nextSiblingID != INVALID_ENTITY_ID ) {
		entityTreeDebug_Entity( ctx, ecps, tfData->nextSiblingID, depth );
	}
}

void entityTreeDebugWindow( struct nk_context* ctx )
{
	ECPS* ecps = &defaultECPS;

	if( sb_Count( sbIsExpanded ) < idSet_GetMaxCount( &ecps->idSet ) ) {
		size_t toAdd = idSet_GetMaxCount( &ecps->idSet ) - sb_Count( sbIsExpanded );
		bool* first = sb_Add( sbIsExpanded, toAdd );
		SDL_memset( first, 0, sizeof( sbIsExpanded[0] ) * toAdd );
	}

	int renderWidth, renderHeight;
	const float scale = SDL_GetWindowDisplayScale( window );
	SDL_GetRenderOutputSize( renderer, &renderWidth, &renderHeight );
	struct nk_rect windowBounds = { 0.0f, 0.0f, 192.0f, (float)renderHeight / scale };

	if( nk_begin( ctx, "Entities", windowBounds, NK_WINDOW_TITLE ) ) {
		// entity list
		for( EntityID id = idSet_GetFirstValidID( &ecps->idSet ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &ecps->idSet, id ) ) {
			GCTransformData* tfData = NULL;
			ecps_GetComponentFromEntityByID( ecps, id, gcTransformCompID, &tfData );
			if( ( tfData == NULL ) || ( tfData->parentID == INVALID_ENTITY_ID ) ) {
				entityTreeDebug_Entity( ctx, ecps, id, 0 );
			}
		}

		// destroy any entities that we chose to be destroyed
		for( size_t i = 0; i < sb_Count( sbDestroyThis ); ++i ) {
			if( displayedEntity == sbDestroyThis[i] ) {
				displayedEntity = INVALID_ENTITY_ID;
			}
			ecps_DestroyEntityByID( ecps, sbDestroyThis[i] );
		}
		sb_Clear( sbDestroyThis );

		// create an empty entity that we can then add components to
		nk_layout_row_begin( ctx, NK_DYNAMIC, 16.0f, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_button_label( ctx, "Add New Entity" ) ) {
				EntityID newID = ecps_CreateEntity( ecps, 0 );
				displayedEntity = newID;
			}
		} nk_layout_row_end( ctx );

		// display chosen entity
		if( ecps_DoesEntityExistByID( ecps, displayedEntity ) ) {
			displayEntity( ctx, ecps, displayedEntity );
		}
	} nk_end( ctx );
}