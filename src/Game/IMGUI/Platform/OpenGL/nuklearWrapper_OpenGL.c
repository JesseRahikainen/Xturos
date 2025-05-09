#define NK_IMPLEMENTATION
#include "IMGUI/nuklearWrapper.h"

#include <SDL3/SDL.h>

#include "Graphics/graphics.h"
#include "Graphics/triRendering.h"
#include "Graphics/gfxUtil.h"
#include "System/memory.h"
#include "Math/matrix4.h"
#include "Input/input.h"
#include "System/platformLog.h"
#include "Graphics/images.h"

#include "Graphics/debugRendering.h"

#include "Graphics/Platform/OpenGL/glPlatform.h"

#include "Graphics/Platform/OpenGL/glShaderManager.h"

#include "Graphics/Platform/OpenGL/glDebugging.h"

#define MAX_VERTEX_MEMORY ( 512 * 1024 )
#define MAX_ELEMENT_MEMORY ( 128 * 1024 )
#define INITIAL_CMD_BUFFER_SIZE ( 4 * 1024 )

NuklearWrapper editorIMGUI = { 0 };
NuklearWrapper inGameIMGUI = { 0 };

typedef struct {
    float position[2];
    float uv[2];
    nk_byte col[4];
} nk_xturos_vertex;

static void uploadAtlas( NuklearWrapper* xu, const void *image, int width, int height)
{
	Texture texture;
	gfxUtil_CreateTextureFromRGBABitmap( (uint8_t*)image, width, height, &texture );

	xu->platform.fontImg = img_CreateFromTexture( &texture, ST_DEFAULT, "NuklearFont" );
	if( xu->platform.fontImg < 0 ) {
		llog( LOG_ERROR, "Error creating Nuklear font image." );
		return;
	}
}

static void clipboardPaste( nk_handle usr, struct nk_text_edit *edit )
{
    const char *text = SDL_GetClipboardText( );
    if( text ) {
		nk_textedit_paste( edit, text, nk_strlen( text ) );
	}

    (void)usr;
}

static void clipboardCopy( nk_handle usr, const char *text, int len )
{
    char *str = 0;
    (void)usr;
    if( !len ) return;
	str = (char*)mem_Allocate( (size_t)( len + 1 ) );
    if( !str ) return;
    memcpy( str, text, (size_t)len );
    str[len] = '\0';
    SDL_SetClipboardText( str );
	mem_Release( str );
}

static void* customAlloc( nk_handle handle, void* old, nk_size size )
{
	// this is styled like a realloc, but it's actually just an alloc
	return mem_Allocate( size );
}

static void customFree( nk_handle handle, void* p )
{
	mem_Release( p );
}

void nk_xu_init( NuklearWrapper* xu, SDL_Window* win, bool useRelativeMousePos, int renderWidth, int renderHeight )
{
	xu->win = win;

	struct nk_allocator alloc;
	alloc.userdata.ptr = 0;
	alloc.alloc = customAlloc;
	alloc.free = customFree;
	nk_init( &( xu->ctx ), &alloc, 0 );

	// setup clipboard functionality
    xu->ctx.clip.copy = clipboardCopy;
    xu->ctx.clip.paste = clipboardPaste;
    xu->ctx.clip.userdata = nk_handle_ptr( 0 );

	// setup memory management
	xu->ctx.memory.pool.alloc = customAlloc;
	xu->ctx.memory.pool.free = customFree;
	// no extra user data, may be handy later if we want to separate this out into it's own pool
	xu->ctx.memory.pool.userdata = nk_handle_ptr( 0 );
	
	// device creation
	nk_buffer_init( &( xu->cmds ), &alloc, INITIAL_CMD_BUFFER_SIZE );
	//  create the shader, TODO: share this between contexts
	
	ShaderDefinition shaderDefs[2];
	ShaderProgramDefinition progDef;

	shaderDefs[0].type = GL_VERTEX_SHADER;
	shaderDefs[0].fileName = "Shaders/vertNuklear.glsl";
	shaderDefs[0].shaderText = NULL;

	shaderDefs[1].type = GL_FRAGMENT_SHADER;
	shaderDefs[1].fileName = "Shaders/fragNuklear.glsl";
	shaderDefs[1].shaderText = NULL;

	progDef.vertexShader = 0;
	progDef.fragmentShader = 1;
	progDef.geometryShader = -1;

	SDL_assert( shaders_Load( shaderDefs, sizeof( shaderDefs ) / sizeof( shaderDefs[0] ), &progDef, &( xu->platform.prog ), 1 ) != 0 );

	GLsizei vertSize = sizeof( nk_xturos_vertex );
	size_t vertPos = offsetof( nk_xturos_vertex, position );
	size_t vertUV = offsetof( nk_xturos_vertex, uv );
	size_t vertClr = offsetof( nk_xturos_vertex, col );

	GL( glGenBuffers( 1, &( xu->platform.vbo ) ) );
	GL( glGenBuffers( 1, &( xu->platform.ebo ) ) );
	GL( glGenVertexArrays( 1, &( xu->platform.vao ) ) );

	GL( glBindVertexArray( xu->platform.vao ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, xu->platform.vbo ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, xu->platform.ebo ) );

	GL( glEnableVertexAttribArray( 0 ) ); // vert pos
	GL( glEnableVertexAttribArray( 1 ) ); // vert uv
	GL( glEnableVertexAttribArray( 2 ) ); // vert clr

	GL( glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, vertSize, (void*)vertPos ) );
	GL( glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, vertSize, (void*)vertUV ) );
	GL( glVertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertSize, (void*)vertClr ) );

	GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );
	GL( glBindVertexArray( 0 ) );

	xu->renderWidth = renderWidth;
	xu->renderHeight = renderHeight;

	xu->useRelativeMousePos = useRelativeMousePos;

	// slightly different default colors
	struct nk_color styleTable[SDL_arraysize( nk_default_color_style )];
	SDL_memcpy( styleTable, nk_default_color_style, sizeof( nk_default_color_style[0] ) * SDL_arraysize( nk_default_color_style ) );
	styleTable[NK_COLOR_TEXT] = nk_rgb( 255, 255, 255 );
	nk_style_from_table( &(xu->ctx), styleTable );
}

static const nk_rune* defaultGlyphRanges( void )
{
	// add any extra glyphs you want to be loaded in here
	static const nk_rune ranges[] = { 0x0020, 0x00FF, 0 };
	return ranges;
}

void nk_xu_fontStashBegin( NuklearWrapper* xu, struct nk_font_atlas** atlas )
{
	struct nk_allocator alloc;
	alloc.userdata.ptr = 0;
	alloc.alloc = customAlloc;
	alloc.free = customFree;
	nk_font_atlas_init( &( xu->fontAtlas ), &alloc );
	nk_font_atlas_begin( &( xu->fontAtlas ) );
	*atlas = &( xu->fontAtlas );
}

void nk_xu_fontStashEnd( NuklearWrapper* xu )
{
	const void *image;
	int w, h;

	xu->fontAtlas.config->range = defaultGlyphRanges( );
	image = nk_font_atlas_bake( &( xu->fontAtlas ), &w, &h, NK_FONT_ATLAS_RGBA32 );
	uploadAtlas( xu, image, w, h );
	nk_font_atlas_end( &( xu->fontAtlas ), nk_handle_id( xu->platform.fontImg ), &( xu->nullTx ) );
	if( xu->fontAtlas.default_font ) {
		nk_style_set_font( &( xu->ctx ), &( xu->fontAtlas.default_font->handle ) );
	}
}

void nk_xu_handleEvent( NuklearWrapper* xu, SDL_Event* evt )
{
	struct nk_context *ctx = &( xu->ctx );

	// mouse position has to be scaled along with the window and render area
	int mX = 0;
	int mY = 0;
	if( xu->useRelativeMousePos ) {
		Vector2 mousePos;
		input_GetMousePosition( &mousePos );
		mX = (int)mousePos.x;
		mY = (int)mousePos.y;
	} else {
		if( ( evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN ) || ( evt->type == SDL_EVENT_MOUSE_BUTTON_UP ) || ( evt->type == SDL_EVENT_MOUSE_MOTION ) ) {
			mX = (int)evt->button.x;
			mY = (int)evt->button.y;
		}
	}
	
	if( !( xu->useRelativeMousePos ) && ( evt->type == SDL_EVENT_WINDOW_RESIZED ) ) {
		xu->renderWidth = evt->window.data1;
		xu->renderHeight = evt->window.data2;
	}

    if (evt->type == SDL_EVENT_KEY_UP || evt->type == SDL_EVENT_KEY_DOWN) {
        /* key events */
        int down = evt->type == SDL_EVENT_KEY_DOWN;
        const bool* state = SDL_GetKeyboardState( NULL );
        SDL_Keycode sym = evt->key.key;
        if( ( sym == SDLK_RSHIFT ) || ( sym == SDLK_LSHIFT ) ) {
            nk_input_key( ctx, NK_KEY_SHIFT, down );
		} else if( sym == SDLK_DELETE ) {
            nk_input_key( ctx, NK_KEY_DEL, down );
		} else if( sym == SDLK_RETURN ) {
            nk_input_key( ctx, NK_KEY_ENTER, down );
        } else if( sym == SDLK_TAB ) {
            nk_input_key( ctx, NK_KEY_TAB, down );
        } else if( sym == SDLK_BACKSPACE ) {
            nk_input_key( ctx, NK_KEY_BACKSPACE, down );
        } else if( sym == SDLK_HOME ) {
            nk_input_key( ctx, NK_KEY_TEXT_START, down );
		} else if( sym == SDLK_END ) {
            nk_input_key( ctx, NK_KEY_TEXT_END, down );
        } else if( sym == SDLK_Z ) {
            nk_input_key( ctx, NK_KEY_TEXT_UNDO, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_R ) {
            nk_input_key( ctx, NK_KEY_TEXT_REDO, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_C ) {
            nk_input_key( ctx, NK_KEY_COPY, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_V ) {
            nk_input_key( ctx, NK_KEY_PASTE, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_X ) {
            nk_input_key( ctx, NK_KEY_CUT, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_B ) {
            nk_input_key( ctx, NK_KEY_TEXT_LINE_START, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_E ) {
            nk_input_key( ctx, NK_KEY_TEXT_LINE_END, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_LEFT ) {
            if( state[SDL_SCANCODE_LCTRL] )
                nk_input_key( ctx, NK_KEY_TEXT_WORD_LEFT, down);
            else {
				nk_input_key( ctx, NK_KEY_LEFT, down);
			}
        } else if( sym == SDLK_RIGHT ) {
            if( state[SDL_SCANCODE_LCTRL] ) {
                nk_input_key( ctx, NK_KEY_TEXT_WORD_RIGHT, down );
			} else {
				nk_input_key( ctx, NK_KEY_RIGHT, down );
			}
        }
    } else if( ( evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN ) || ( evt->type == SDL_EVENT_MOUSE_BUTTON_UP ) ) {
        /* mouse button */
        int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if( evt->button.button == SDL_BUTTON_LEFT )		nk_input_button( ctx, NK_BUTTON_LEFT, mX, mY, down );
        if( evt->button.button == SDL_BUTTON_MIDDLE )	nk_input_button( ctx, NK_BUTTON_MIDDLE, mX, mY, down );
        if( evt->button.button == SDL_BUTTON_RIGHT )	nk_input_button( ctx, NK_BUTTON_RIGHT, mX, mY, down );
    } else if( evt->type == SDL_EVENT_MOUSE_MOTION ) {
		nk_input_motion( ctx, mX, mY );
    } else if( evt->type == SDL_EVENT_TEXT_INPUT ) {
        nk_glyph glyph;
        memcpy( glyph, evt->text.text, NK_UTF_SIZE );
        nk_input_glyph( ctx, glyph );
    } else if( evt->type == SDL_EVENT_MOUSE_WHEEL ) {
		struct nk_vec2 scroll;
		scroll.y = (float)evt->wheel.y;
		scroll.x = (float)evt->wheel.x;
        nk_input_scroll( ctx, scroll );
    }
}

// TODO: Get it so the clear and render are separate. Allowing the drawing of the ui and the frames to match. As of right now we have to
//  draw any Nuklear UIs every frame instead of every draw.
void nk_xu_render( NuklearWrapper* xu )
{
#if 0
	Matrix4 ortho;
	mat4_CreateOrthographicProjection( 0.0f, (float)( xu->renderWidth ), 0.0f, (float)( xu->renderHeight ), -1000.0f, 1000.0f, &ortho );

	// global state
	glViewport( 0, 0, xu->renderWidth, xu->renderHeight );
	GL( glEnable( GL_BLEND ) );
	GL( glBlendEquation( GL_FUNC_ADD ) );
	GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
	GL( glDisable( GL_CULL_FACE ) );
	GL( glDisable( GL_DEPTH_TEST ) );
	GL( glEnable( GL_SCISSOR_TEST ) );
	GL( glActiveTexture( GL_TEXTURE0 ) );

	GL( glUseProgram( xu->platform.prog.programID ) );
	GL( glUniform1i( xu->platform.prog.uniformLocs[1], 0 ) );
	GL( glUniformMatrix4fv( xu->platform.prog.uniformLocs[0], 1, GL_FALSE, &( ortho.m[0] ) ) );
	{
		// convert from command queue into draw list and render
		const struct nk_draw_command *cmd = NULL;
		void* vertices = NULL;
		void* elements = NULL;
		const nk_draw_index* offset = NULL;

		// allocate vertex adn element buffer
		GL( glBindVertexArray( xu->platform.vao ) );
		GL( glBindBuffer( GL_ARRAY_BUFFER, xu->platform.vbo ) );
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, xu->platform.ebo ) );

		GL( glBufferData( GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW ) );
		GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW ) );

		// load draw vertices and elements directly into vertex + element buffer
		GLR( vertices, glMapBufferRange( GL_ARRAY_BUFFER, 0, MAX_VERTEX_MEMORY, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ) );
		GLR( elements, glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, MAX_ELEMENT_MEMORY, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ) );
		{
			// fill the convert configuration
			struct nk_convert_config config;
			memset( &config, 0, sizeof( config ) );

			static const struct nk_draw_vertex_layout_element vertexLayout[] = {
				{ NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF( nk_xturos_vertex, position ) },
				{ NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF( nk_xturos_vertex, uv ) },
				{ NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF( nk_xturos_vertex, col ) },
				{ NK_VERTEX_LAYOUT_END }
			};
			config.vertex_layout = vertexLayout;
			config.vertex_size = sizeof( nk_xturos_vertex );
			config.vertex_alignment = NK_ALIGNOF( nk_xturos_vertex );

			config.global_alpha = 1.0f;
			config.shape_AA = NK_ANTI_ALIASING_OFF;
			config.line_AA = NK_ANTI_ALIASING_OFF;
			config.circle_segment_count = 22;
			config.arc_segment_count = 22;
			config.curve_segment_count = 22;
			config.tex_null = xu->nullTx;

			// setup buffers to load vertices and elements
			{
				struct nk_buffer vertBfr, elemBfr;
				nk_buffer_init_fixed( &vertBfr, vertices, (size_t)MAX_VERTEX_MEMORY );
				nk_buffer_init_fixed( &elemBfr, elements, (size_t)MAX_ELEMENT_MEMORY );
				nk_convert( &( xu->ctx ), &( xu->cmds ), &vertBfr, &elemBfr, &config );
			}
		}
		GL( glUnmapBuffer( GL_ARRAY_BUFFER ) );
		GL( glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER ) );

		// iterate over and execute each draw command
		nk_draw_foreach( cmd, &( xu->ctx ), &( xu->cmds ) ) {
			if( cmd->elem_count == 0 ) continue;

			PlatformTexture platformTexture;
			if( img_GetTextureID( cmd->texture.id, &platformTexture ) >= 0 ) {
				GL( glBindTexture( GL_TEXTURE_2D, platformTexture.id ) );
				glScissor(
					(GLint)( cmd->clip_rect.x ),
					(GLint)( ( xu->renderHeight - (GLint)( cmd->clip_rect.y + cmd->clip_rect.h ) ) ),
					(GLint)( cmd->clip_rect.w ),
					(GLint)( cmd->clip_rect.h ) );

				GL( glDrawElements( GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset ) );
			}

#if 0 // for debugging rendering, TODO: make this work with the version of nuklear we're using
#pragma warning(push)
#pragma warning(disable: 4305)
			for( unsigned short i = (unsigned short)offset; i < cmd->elem_count; i+=3 ) {
#pragma warning(pop)
				uint16_t vIdx0 = ( (uint16_t*)elements )[i];
				uint16_t vIdx1 = ( (uint16_t*)elements )[i+1];
				uint16_t vIdx2 = ( (uint16_t*)elements )[i+2];

				struct nk_draw_vertex vert0 = ( (struct nk_draw_vertex*)vertices )[vIdx0];
				struct nk_draw_vertex vert1 = ( (struct nk_draw_vertex*)vertices )[vIdx1];
				struct nk_draw_vertex vert2 = ( (struct nk_draw_vertex*)vertices )[vIdx2];

				Vector2 v2_0, v2_1, v2_2;
				v2_0.x = (float)vert0.position.x;
				v2_0.y = (float)vert0.position.y;

				v2_1.x = (float)vert1.position.x;
				v2_1.y = (float)vert1.position.y;

				v2_2.x = (float)vert2.position.x;
				v2_2.y = (float)vert2.position.y;

				Color clr;
				
				clr.r = (float)( vert0.col & 0xFF ) / ( 255.0f );
				clr.g = (float)( ( vert0.col >> 8 ) & 0xFF ) / ( 255.0f );
				clr.b = (float)( ( vert0.col >> 16 ) & 0xFF ) / ( 255.0f );
				clr.a = (float)( ( vert0.col >> 24 ) & 0xFF ) / ( 255.0f );

				clr = CLR_MAGENTA;

				debugRenderer_Line( 0xFFFFFFFF, v2_0, v2_1, clr );
			}
#endif
			offset += cmd->elem_count;
		}
		nk_clear( &( xu->ctx ) );
	}

	GL( glUseProgram( 0 ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );
	GL( glBindVertexArray( 0 ) );
	GL( glDisable( GL_BLEND ) );
	GL( glDisable( GL_SCISSOR_TEST ) );
#endif
}

void nk_xu_shutdown( NuklearWrapper* xu )
{
	nk_font_atlas_clear( &( xu->fontAtlas ) );
	nk_free( &( xu->ctx ) );

	// destroy the device stuff
	shaders_Destroy( &( xu->platform.prog ), 1 );
	img_Clean( xu->platform.fontImg );
	GL( glDeleteBuffers( 1, &( xu->platform.vbo ) ) );
	GL( glDeleteBuffers( 1, &( xu->platform.ebo ) ) );

	nk_buffer_free( &( xu->cmds ) );

	memset( &xu, 0, sizeof( xu ) );
}

void nk_xu_clear( NuklearWrapper* xu )
{
	llog( LOG_DEBUG, "Manual clear" );
	nk_clear( &( xu->ctx ) );
}

struct nk_image nk_xu_loadImage( const char* filePath, int* outWidth, int* outHeight )
{
	int id = img_Load( filePath, ST_DEFAULT );
	
	if( id == -1 ) {
		llog( LOG_ERROR, "Unable to load image %s", filePath );
		return nk_image_id( -1 );
	}

	Vector2 size;
	img_GetSize( id, &size );
	if( outWidth != NULL ) ( *outWidth ) = (int)size.x;
	if( outHeight != NULL ) ( *outHeight ) = (int)size.y;

	return nk_image_id( id );
}

void nk_xu_unloadImage( struct nk_image* image )
{
	if( image == NULL ) return;

	img_Clean( image->handle.id );
}