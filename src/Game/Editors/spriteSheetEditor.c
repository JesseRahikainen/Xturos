#include "spriteSheetEditor.h"

#include <SDL3/SDL.h>
#include <limits.h>
#include <stdbool.h>

#include "IMGUI/nuklearWrapper.h"
#include "Utils/stretchyBuffer.h"
#include "editorHelpers.h"
#include "editorHub.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "Graphics/imageSheets.h"

// this will be used to generate sprite sheets. for right now it will just be working with lists of files
// will want to safe out sprite sheet setups that will just be a list of file locations, will let us to more easily
//  rebuild sprite sheets when needed

static SpriteSheetEntry* sbEntries = NULL;

static struct nk_rect windowBounds = { 150.0f, 10.0f, 800.0f, 600.f };

static int xPadding = 2;
static int yPadding = 2;
static int maxSize = 4096;

static void addFileEntry( const char* path )
{
	char* sbPathCopy = NULL;
	sb_Add( sbPathCopy, SDL_strlen( path ) + 1 );
	SDL_memcpy( sbPathCopy, path, sizeof( path[0] ) * ( SDL_strlen( path ) + 1 ) );

	SpriteSheetEntry newEntry;
	newEntry.sbPath = sbPathCopy;
	sb_Push( sbEntries, newEntry );
}

static void clearEntryList( void )
{
	for(size_t i = 0; i < sb_Count( sbEntries ); ++i) {
		sb_Release( sbEntries[i].sbPath );
	}
	sb_Clear( sbEntries );
}

static void createNewSpriteSheet( void )
{
	clearEntryList( );
}

static void saveSpriteSheetSetup( const char* filePath )
{
	SDL_IOStream* ioStream = SDL_IOFromFile( filePath, "w" );

	if( ioStream == NULL ) {
		hub_CreateDialog( "Save Setup Error", SDL_GetError( ), DT_ERROR, 1, "OK", NULL );
		return;
	}

	// simple text based file, will just be a list of files
	for( size_t i = 0; i < sb_Count( sbEntries ); ++i ) {
		size_t size = sizeof( sbEntries[i].sbPath[0] ) * sb_Count( sbEntries[i].sbPath );
		size_t numWritten = SDL_WriteIO( ioStream, sbEntries[i].sbPath, size );
		if( numWritten != size ) {
			hub_CreateDialog( "Save Setup Error", SDL_GetError( ), DT_ERROR, 1, "OK", NULL );
			goto clean_up;
		}
	}

clean_up:
	SDL_CloseIO( ioStream );
}

static void loadSpriteSheetSetup( const char* filePath )
{
	clearEntryList( );

	SDL_IOStream* ioStream = SDL_IOFromFile( filePath, "r" );
	if( ioStream == NULL ) {
		hub_CreateDialog( "Load Setup Error", SDL_GetError( ), DT_ERROR, 1, "OK", NULL );
		return;
	}

	char buffer[512];
	char* sbCurrString = NULL;
	sb_Reserve( sbCurrString, 512 );

	
	size_t amtRead = SDL_ReadIO( ioStream, buffer, sizeof( buffer[0] ) * ARRAY_SIZE( buffer ) );
	while( amtRead > 0 ) {

		// find the next null and add the resulting string to the buffer
		for( size_t i = 0; i < amtRead; ++i ) {
			sb_Push( sbCurrString, buffer[i] );
			if( buffer[i] == 0 ) {
				if( sb_Count( sbCurrString ) > 0 ) {
					addFileEntry( sbCurrString );
				}
				sb_Clear( sbCurrString );
			}
		}

		amtRead = SDL_ReadIO( ioStream, buffer, sizeof( buffer[0] ) * ARRAY_SIZE( buffer ) );
	}

	const char* error = SDL_GetError( );
	if( SDL_strlen( error ) > 0 ) {
		hub_CreateDialog( "Load Setup Error", error, DT_ERROR, 1, "OK", NULL );
	}

	sb_Release( sbCurrString );
	SDL_CloseIO( ioStream );
}

static void exportSpriteSheet( const char* filePath )
{
	if( !img_SaveSpriteSheet( filePath, sbEntries, maxSize, xPadding, yPadding ) ) {
		hub_CreateDialog( "Export Sprite Sheet Error", "There was an error exporting the sprite sheet. Check the log file for more information.", DT_ERROR, 1, "OK", NULL );
	}
}

void spriteSheetEditor_Init( void )
{

}

void spriteSheetEditor_Show( void )
{

}

void spriteSheetEditor_Hide( void )
{
	//cleanUpEntryStrings( );
	sb_Release( sbEntries );
}

void spriteSheetEditor_IMGUIProcess( void )
{
	struct nk_context* ctx = &( editorIMGUI.ctx );
	if(nk_begin( ctx, "Sprite Sheet", windowBounds, NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE )) {
		nk_menubar_begin( ctx ); {

			nk_layout_row_begin( ctx, NK_STATIC, 34, INT_MAX );
			nk_layout_row_push( ctx, 85 );
			if( nk_menu_begin_label( ctx, "File", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_vec2( 204, 204 ) ) ) {

				nk_layout_row_dynamic( ctx, 34, 1 );
				if( nk_menu_item_label( ctx, "New", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					createNewSpriteSheet( );
				}

				if( nk_menu_item_label( ctx, "Save Setup...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseSaveFileLocation( "Sprite Sheet Setup", "sheetsetup", saveSpriteSheetSetup );
				}

				if( nk_menu_item_label( ctx, "Load Setup...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseLoadFileLocation( "Sprite Sheet Setup", "sheetsetup", false, loadSpriteSheetSetup );
				}

				if( nk_menu_item_label( ctx, "Export Sprite Sheet...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseSaveFileLocation( "Sprite Sheet", "spritesheet", exportSpriteSheet );
				}

				nk_menu_end( ctx );
			}

			if( nk_menu_begin_label( ctx, "Images", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_vec2( 204, 136 ) ) ) {
				nk_layout_row_dynamic( ctx, 34, 1 );
				if( nk_menu_item_label( ctx, "Select files...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseLoadFileLocation( "PNG Image", "png", true, addFileEntry );
				}

				nk_menu_end( ctx );
			}

		} nk_menubar_end( ctx );

		float layoutHeight = nk_window_get_height( ctx ) - 74.0f;
		float rowWidth = nk_window_get_width( ctx ) - 26.0f;

		nk_layout_row_begin( ctx, NK_STATIC, 1, 1 ); // padding

		nk_layout_row_begin( ctx, NK_DYNAMIC, 34, 3 ); {
			nk_layout_row_push( ctx, 0.3f );
			nk_property_int( ctx, "X-Padding", 0, &xPadding, 100, 1, 0.0f );
			nk_property_int( ctx, "Y-Padding", 0, &yPadding, 100, 1, 0.0f );
			nk_property_int( ctx, "Max Dim Size", 256, &maxSize, 32768, 1, 0.0f );
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_STATIC, 1, 1 ); // padding
		size_t itemToDelete = SIZE_MAX;
		// list of locations: path, and delete button
		for( size_t i = 0; i < sb_Count( sbEntries ); ++i ) {
			nk_layout_row_begin( ctx, NK_STATIC, 34, 2 ); {
				nk_layout_row_push( ctx, rowWidth - 34.0f );
				nk_label( ctx, sbEntries[i].sbPath, NK_TEXT_LEFT );
				nk_layout_row_push( ctx, 34.0f );
				if( nk_button_label( ctx, "X" ) ) {
					itemToDelete = i;
				}
			} nk_layout_row_end( ctx );
		}

		if( itemToDelete != SIZE_MAX ) {
			sb_Remove( sbEntries, itemToDelete );
		}

	} nk_end( ctx );
}

void spriteSheetEditor_Tick( float dt )
{

}