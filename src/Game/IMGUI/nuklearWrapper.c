#include "nuklearWrapper.h"

#include "Graphics/graphics.h"
#include "System/messageBroadcast.h"

#define INITIAL_CMD_BUFFER_SIZE ( 4 * 1024 )

static void* customAlloc( nk_handle handle, void* old, nk_size size )
{
	// this is styled like a realloc, but it's actually just an alloc
	return mem_Allocate( size );
}

static void customFree( nk_handle handle, void* p )
{
	mem_Release( p );
}

static void handleRenderResize( void* data )
{
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	nk_wrapper_SetRenderSize( &inGameIMGUI, renderWidth, renderHeight );
}

static void handleWindowResize( void* data )
{
	int windowWidth, windowHeight;
	gfx_GetWindowSize( &windowWidth, &windowHeight );
	nk_wrapper_SetRenderSize( &editorIMGUI, windowWidth, windowHeight );
}

// includes stuff shared between everything
void nk_wrapper_InitMessageListeners( void )
{
	mb_RegisterListener( MSG_WINDOW_RESIZED, handleWindowResize );
	mb_RegisterListener( MSG_RENDER_RESIZED, handleRenderResize );
}

static void clipboardPaste( nk_handle usr, struct nk_text_edit* edit )
{
	const char* text = SDL_GetClipboardText( );
	if( text ) {
		nk_textedit_paste( edit, text, nk_strlen( text ) );
	}

	(void)usr;
}

static void clipboardCopy( nk_handle usr, const char* text, int len )
{
	char* str = 0;
	(void)usr;
	if( !len ) return;
	str = (char*)mem_Allocate( (size_t)( len + 1 ) );
	if( !str ) return;
	memcpy( str, text, (size_t)len );
	str[len] = '\0';
	SDL_SetClipboardText( str );
	mem_Release( str );
}

void nk_wrapper_Init( NuklearWrapperType type, SDL_Window* win, bool useRelativeMousePos, int renderWidth, int renderHeight, NuklearWrapper* outWrapper )
{
	ASSERT_AND_IF_NOT( outWrapper != NULL ) return;

	// setup basic shared stuff
	outWrapper->base.win = win;

	struct nk_allocator alloc;
	alloc.userdata.ptr = 0;
	alloc.alloc = customAlloc;
	alloc.free = customFree;
	nk_init( &( outWrapper->base.ctx ), &alloc, 0 );

	// setup clipboard functionality
	outWrapper->base.ctx.clip.copy = clipboardCopy;
	outWrapper->base.ctx.clip.paste = clipboardPaste;
	outWrapper->base.ctx.clip.userdata = nk_handle_ptr( 0 );

	// setup memory management
	outWrapper->base.ctx.memory.pool.alloc = customAlloc;
	outWrapper->base.ctx.memory.pool.free = customFree;
	// no extra user data, may be handy later if we want to separate this out into it's own pool
	outWrapper->base.ctx.memory.pool.userdata = nk_handle_ptr( 0 );

	// device creation
	nk_buffer_init( &( outWrapper->base.cmds ), &alloc, INITIAL_CMD_BUFFER_SIZE );

	outWrapper->base.renderWidth = renderWidth;
	outWrapper->base.renderHeight = renderHeight;

	outWrapper->base.useRelativeMousePos = useRelativeMousePos;

	// slightly different default colors, all but NK_COLOR_TEXT pull from the default style table used
	//  would prefer a way to just grab this table instead of recreating it
	struct nk_color styleTable[NK_COLOR_COUNT];
	styleTable[NK_COLOR_TEXT] = nk_rgb( 255, 255, 255 );
	styleTable[NK_COLOR_WINDOW] = nk_rgb( 45, 45, 45 );
	styleTable[NK_COLOR_HEADER] = nk_rgb( 40, 40, 40 );
	styleTable[NK_COLOR_BORDER] = nk_rgb( 65, 65, 65 );
	styleTable[NK_COLOR_BUTTON] = nk_rgb( 50, 50, 50 );
	styleTable[NK_COLOR_BUTTON_HOVER] = nk_rgb( 40, 40, 40 );
	styleTable[NK_COLOR_BUTTON_ACTIVE] = nk_rgb( 35, 35, 35 );
	styleTable[NK_COLOR_TOGGLE] = nk_rgb( 100, 100, 100 );
	styleTable[NK_COLOR_TOGGLE_HOVER] = nk_rgb( 120, 120, 120 );
	styleTable[NK_COLOR_TOGGLE_CURSOR] = nk_rgb( 45, 45, 45 );
	styleTable[NK_COLOR_SELECT] = nk_rgb( 45, 45, 45 );
	styleTable[NK_COLOR_SELECT_ACTIVE] = nk_rgb( 35, 35, 35 );
	styleTable[NK_COLOR_SLIDER] = nk_rgb( 38, 38, 38 );
	styleTable[NK_COLOR_SLIDER_CURSOR] = nk_rgb( 100, 100, 100 );
	styleTable[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb( 120, 120, 120 );
	styleTable[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb( 150, 150, 150 );
	styleTable[NK_COLOR_PROPERTY] = nk_rgb( 38, 38, 38 );
	styleTable[NK_COLOR_EDIT] = nk_rgb( 38, 38, 38 );
	styleTable[NK_COLOR_EDIT_CURSOR] = nk_rgb( 175, 175, 175 );
	styleTable[NK_COLOR_COMBO] = nk_rgb( 45, 45, 45 );
	styleTable[NK_COLOR_CHART] = nk_rgb( 120, 120, 120 );
	styleTable[NK_COLOR_CHART_COLOR] = nk_rgb( 45, 45, 45 );
	styleTable[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb( 255, 0, 0 );
	styleTable[NK_COLOR_SCROLLBAR] = nk_rgb( 40, 40, 40 );
	styleTable[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb( 100, 100, 100 );
	styleTable[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb( 120, 120, 120 );
	styleTable[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb( 150, 150, 150 );
	styleTable[NK_COLOR_TAB_HEADER] = nk_rgb( 40, 40, 40 );

	nk_style_from_table( &( outWrapper->base.ctx ), styleTable );

	// then platform specific stuff
	if( type == NWT_XTUROS ) {
		// this is the platform specific implementation
		nk_xturos_init( outWrapper );
	} else {
		ASSERT_ALWAYS( "Unknown wrapper type." );
	}
}

// will just call the wrappers function
void nk_wrapper_HandleEvent( NuklearWrapper* wrapper, SDL_Event* e )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.handleEvent != NULL ) return;
	wrapper->base.handleEvent( wrapper, e );
}

void nk_wrapper_SetRenderSize( NuklearWrapper* wrapper, int renderWidth, int renderHeight )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.setRenderSize != NULL ) return;
	wrapper->base.setRenderSize( wrapper, renderWidth, renderHeight );
}

void nk_wrapper_Render( NuklearWrapper* wrapper )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.render != NULL ) return;
	wrapper->base.render( wrapper );
}

void nk_wrapper_Shutdown( NuklearWrapper* wrapper )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.shutdown != NULL ) return;
	wrapper->base.shutdown( wrapper );
}

void nk_wrapper_Clear( NuklearWrapper* wrapper )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.clear != NULL ) return;
	wrapper->base.clear( wrapper );
}

struct nk_image nk_wrapper_LoadImage( NuklearWrapper* wrapper, const char* filePath, int* outWidth, int* outHeight )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return nk_image_id( -1 );
	ASSERT_AND_IF_NOT( wrapper->base.loadImage != NULL ) return nk_image_id( -1 );
	return wrapper->base.loadImage( wrapper, filePath, outWidth, outHeight );
}

void nk_wrapper_UnloadImage( NuklearWrapper* wrapper, struct nk_image* image )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.unloadImage != NULL ) return;
	wrapper->base.unloadImage( wrapper, image );
}

void nk_wrapper_FontStashBegin( NuklearWrapper* wrapper, struct nk_font_atlas** atlas )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.fontStashBegin != NULL ) return;
	wrapper->base.fontStashBegin( wrapper, atlas );
}

void nk_wrapper_FontStashEnd( NuklearWrapper* wrapper, float fontHeight )
{
	ASSERT_AND_IF_NOT( wrapper != NULL ) return;
	ASSERT_AND_IF_NOT( wrapper->base.fontStashEnd != NULL ) return;
	wrapper->base.fontStashEnd( wrapper, fontHeight );
}