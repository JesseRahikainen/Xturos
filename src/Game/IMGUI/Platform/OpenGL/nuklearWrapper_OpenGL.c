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
#include "System/messageBroadcast.h"
#include "Utils/helpers.h"

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

static void* customAlloc( nk_handle handle, void* old, nk_size size )
{
	// this is styled like a realloc, but it's actually just an alloc
	return mem_Allocate( size );
}

static void customFree( nk_handle handle, void* p )
{
	mem_Release( p );
}

static void uploadAtlas( NuklearWrapper* wrapper, const void *image, int width, int height, float fontHeight )
{
	Texture texture;
	gfxUtil_CreateTextureFromRGBABitmap( (uint8_t*)image, width, height, &texture );

	char fontImgID[32];
	SDL_snprintf( fontImgID, ARRAY_SIZE( fontImgID ), "NuklearFont%f", fontHeight );

	wrapper->xu.platform.fontImg = img_CreateFromTexture( &texture, ST_DEFAULT, fontImgID );
	if( wrapper->xu.platform.fontImg < 0 ) {
		llog( LOG_ERROR, "Error creating Nuklear font image." );
		return;
	}
}

static const nk_rune* defaultGlyphRanges( void )
{
	// add any extra glyphs you want to be loaded in here
	static const nk_rune ranges[] = { 0x0020, 0x00FF, 0 };
	return ranges;
}

static void nk_xturos_handleEvent( NuklearWrapper* wrapper, SDL_Event* evt )
{
	struct nk_context* ctx = &( wrapper->base.ctx );

	// mouse position has to be scaled along with the window and render area
	int mX = 0;
	int mY = 0;
	if( wrapper->base.useRelativeMousePos ) {
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

	if( evt->type == SDL_EVENT_KEY_UP || evt->type == SDL_EVENT_KEY_DOWN ) {
		// key events
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
		} else if( sym == SDLK_Z && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_TEXT_UNDO, down );
		} else if( sym == SDLK_R && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_TEXT_REDO, down );
		} else if( sym == SDLK_C && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_COPY, down );
		} else if( sym == SDLK_V && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_PASTE, down );
		} else if( sym == SDLK_X && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_CUT, down );
		} else if( sym == SDLK_B && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_TEXT_LINE_START, down );
		} else if( sym == SDLK_E && state[SDL_SCANCODE_LCTRL] ) {
			nk_input_key( ctx, NK_KEY_TEXT_LINE_END, down );
		} else if( sym == SDLK_LEFT ) {
			if( state[SDL_SCANCODE_LCTRL] )
				nk_input_key( ctx, NK_KEY_TEXT_WORD_LEFT, down );
			else {
				nk_input_key( ctx, NK_KEY_LEFT, down );
			}
		} else if( sym == SDLK_RIGHT ) {
			if( state[SDL_SCANCODE_LCTRL] ) {
				nk_input_key( ctx, NK_KEY_TEXT_WORD_RIGHT, down );
			} else {
				nk_input_key( ctx, NK_KEY_RIGHT, down );
			}
		} else if( down && ( sym >= SDLK_SPACE ) && ( sym <= SDLK_TILDE ) ) {
			// standard ascii input
			nk_input_char( ctx, (char)sym );
		}
	} else if( ( evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN ) || ( evt->type == SDL_EVENT_MOUSE_BUTTON_UP ) ) {
		// mouse button
		int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		if( evt->button.button == SDL_BUTTON_LEFT )		nk_input_button( ctx, NK_BUTTON_LEFT, mX, mY, down );
		if( evt->button.button == SDL_BUTTON_MIDDLE )	nk_input_button( ctx, NK_BUTTON_MIDDLE, mX, mY, down );
		if( evt->button.button == SDL_BUTTON_RIGHT )	nk_input_button( ctx, NK_BUTTON_RIGHT, mX, mY, down );
	} else if( evt->type == SDL_EVENT_MOUSE_MOTION ) {
		nk_input_motion( ctx, mX, mY );
	} else if( evt->type == SDL_EVENT_MOUSE_WHEEL ) {
		struct nk_vec2 scroll;
		scroll.y = (float)evt->wheel.y;
		scroll.x = (float)evt->wheel.x;
		nk_input_scroll( ctx, scroll );
	}
}

static void nk_xturos_fontStashBegin( NuklearWrapper* wrapper, struct nk_font_atlas** atlas )
{
	struct nk_allocator alloc;
	alloc.userdata.ptr = 0;
	alloc.alloc = customAlloc;
	alloc.free = customFree;
	nk_font_atlas_init( &( wrapper->base.fontAtlas ), &alloc );
	nk_font_atlas_begin( &( wrapper->base.fontAtlas ) );
	*atlas = &( wrapper->base.fontAtlas );
}

static void nk_xturos_fontStashEnd( NuklearWrapper* wrapper, float fontHeight )
{
	const void* image;
	int w, h;

	wrapper->base.fontAtlas.config->range = defaultGlyphRanges( );
	image = nk_font_atlas_bake( &( wrapper->base.fontAtlas ), &w, &h, NK_FONT_ATLAS_RGBA32 );
	uploadAtlas( wrapper, image, w, h, fontHeight );
	nk_font_atlas_end( &( wrapper->base.fontAtlas ), nk_handle_id( wrapper->xu.platform.fontImg ), &( wrapper->base.nullTx ) );
	if( wrapper->base.fontAtlas.default_font ) {
		nk_style_set_font( &( wrapper->base.ctx ), &( wrapper->xu.base.fontAtlas.default_font->handle ) );
	}
}

static void nk_xturos_setRenderSize( NuklearWrapper* wrapper, int renderWidth, int renderHeight )
{
	wrapper->base.renderWidth = renderWidth;
	wrapper->base.renderHeight = renderHeight;
}

// TODO: Get it so the clear and render are separate. Allowing the drawing of the ui and the frames to match. As of right now we have to
//  draw any Nuklear UIs every frame instead of every draw.
static void nk_xturos_render( NuklearWrapper* wrapper )
{
#if 1
	Matrix4 ortho;
	mat4_CreateOrthographicProjection( 0.0f, (float)( wrapper->base.renderWidth ), 0.0f, (float)( wrapper->base.renderHeight ), -1000.0f, 1000.0f, &ortho );

	// global state
	GL( glViewport( 0, 0, wrapper->base.renderWidth, wrapper->base.renderHeight ) );
	GL( glEnable( GL_BLEND ) );
	GL( glBlendEquation( GL_FUNC_ADD ) );
	GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
	GL( glDisable( GL_CULL_FACE ) );
	GL( glDisable( GL_DEPTH_TEST ) );
	GL( glEnable( GL_SCISSOR_TEST ) );
	GL( glActiveTexture( GL_TEXTURE0 ) );

	GL( glUseProgram( wrapper->xu.platform.prog.programID ) );
	GL( glUniform1i( wrapper->xu.platform.prog.uniformLocs[1], 0 ) );
	GL( glUniformMatrix4fv( wrapper->xu.platform.prog.uniformLocs[0], 1, GL_FALSE, &( ortho.m[0] ) ) );
	{
		// convert from command queue into draw list and render
		const struct nk_draw_command* cmd = NULL;
		void* vertices = NULL;
		void* elements = NULL;
		const nk_draw_index* offset = NULL;

		// allocate vertex and element buffer
		GL( glBindVertexArray( wrapper->xu.platform.vao ) );
		GL( glBindBuffer( GL_ARRAY_BUFFER, wrapper->xu.platform.vbo ) );
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, wrapper->xu.platform.ebo ) );

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
			config.tex_null = wrapper->base.nullTx;

			// setup buffers to load vertices and elements
			{
				struct nk_buffer vertBfr, elemBfr;
				nk_buffer_init_fixed( &vertBfr, vertices, (size_t)MAX_VERTEX_MEMORY );
				nk_buffer_init_fixed( &elemBfr, elements, (size_t)MAX_ELEMENT_MEMORY );
				nk_convert( &( wrapper->base.ctx ), &( wrapper->base.cmds ), &vertBfr, &elemBfr, &config );
			}
		}
		GL( glUnmapBuffer( GL_ARRAY_BUFFER ) );
		GL( glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER ) );

		// iterate over and execute each draw command
		nk_draw_foreach( cmd, &( wrapper->base.ctx ), &( wrapper->base.cmds ) )
		{
			if( cmd->elem_count == 0 ) continue;

			PlatformTexture platformTexture;
			if( img_GetTextureID( cmd->texture.id, &platformTexture ) >= 0 ) {
				GL( glBindTexture( GL_TEXTURE_2D, platformTexture.id ) );
				glScissor(
					(GLint)( cmd->clip_rect.x ),
					(GLint)( ( wrapper->base.renderHeight - (GLint)( cmd->clip_rect.y + cmd->clip_rect.h ) ) ),
					(GLint)( cmd->clip_rect.w ),
					(GLint)( cmd->clip_rect.h ) );

				GL( glDrawElements( GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset ) );
			}

#if 0 // for debugging rendering, TODO: make this work with the version of nuklear we're using
#pragma warning(push)
#pragma warning(disable: 4305)
			for( unsigned short i = (unsigned short)offset; i < cmd->elem_count; i += 3 ) {
#pragma warning(pop)
				uint16_t vIdx0 = ( (uint16_t*)elements )[i];
				uint16_t vIdx1 = ( (uint16_t*)elements )[i + 1];
				uint16_t vIdx2 = ( (uint16_t*)elements )[i + 2];

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
		
		nk_clear( &( wrapper->base.ctx ) );
		nk_buffer_clear( &( wrapper->base.cmds ) );
	}

	GL( glUseProgram( 0 ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );
	GL( glBindVertexArray( 0 ) );
	GL( glDisable( GL_BLEND ) );
	GL( glDisable( GL_SCISSOR_TEST ) );
#endif
}

static void nk_xturos_shutdown( NuklearWrapper* wrapper )
{
	nk_font_atlas_clear( &( wrapper->base.fontAtlas ) );
	nk_free( &( wrapper->base.ctx ) );

	// destroy the device stuff
	shaders_Destroy( &( wrapper->xu.platform.prog ), 1 );
	img_Clean( wrapper->xu.platform.fontImg );
	GL( glDeleteBuffers( 1, &( wrapper->xu.platform.vbo ) ) );
	GL( glDeleteBuffers( 1, &( wrapper->xu.platform.ebo ) ) );
	
	nk_buffer_free( &( wrapper->xu.base.cmds ) );

	SDL_memset( wrapper, 0, sizeof( NuklearWrapper ) );
}

static void nk_xturos_clear( NuklearWrapper* wrapper )
{
	llog( LOG_DEBUG, "Manual clear" );
	nk_clear( &( wrapper->base.ctx ) );
}

static struct nk_image nk_xturos_loadImage( NuklearWrapper* wrapper, const char* filePath, int* outWidth, int* outHeight )
{
	int id = img_Load( filePath, ST_DEFAULT );

	if( id == -1 ) {
		llog( LOG_ERROR, "Unable to load image %s", filePath );
		return nk_image_id( INVALID_IMAGE_ID );
	}

	Vector2 size;
	img_GetSize( id, &size );
	if( outWidth != NULL ) ( *outWidth ) = (int)size.x;
	if( outHeight != NULL ) ( *outHeight ) = (int)size.y;

	return nk_image_id( id );
}

static void nk_xturos_unloadImage( NuklearWrapper* wrapper, struct nk_image* image )
{
	if( image == NULL ) return;

	img_Clean( image->handle.id );
}

void nk_xturos_init( NuklearWrapper* wrapper )
{
	wrapper->base.type = NWT_XTUROS;

	wrapper->base.handleEvent = nk_xturos_handleEvent;
	wrapper->base.setRenderSize = nk_xturos_setRenderSize;
	wrapper->base.render = nk_xturos_render;
	wrapper->base.shutdown = nk_xturos_shutdown;
	wrapper->base.clear = nk_xturos_clear;
	wrapper->base.loadImage = nk_xturos_loadImage;
	wrapper->base.unloadImage = nk_xturos_unloadImage;
	wrapper->base.fontStashBegin = nk_xturos_fontStashBegin;
	wrapper->base.fontStashEnd = nk_xturos_fontStashEnd;
	
	//  create the shaders, TODO: share this between contexts
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

	size_t numLoaded = shaders_Load( shaderDefs, sizeof( shaderDefs ) / sizeof( shaderDefs[0] ), &progDef, &( wrapper->xu.platform.prog ), 1 );
	ASSERT( numLoaded != 0 );

	GLsizei vertSize = sizeof( nk_xturos_vertex );
	size_t vertPos = offsetof( nk_xturos_vertex, position );
	size_t vertUV = offsetof( nk_xturos_vertex, uv );
	size_t vertClr = offsetof( nk_xturos_vertex, col );

	GL( glGenBuffers( 1, &( wrapper->xu.platform.vbo ) ) );
	GL( glGenBuffers( 1, &( wrapper->xu.platform.ebo ) ) );
	GL( glGenVertexArrays( 1, &( wrapper->xu.platform.vao ) ) );

	GL( glBindVertexArray( wrapper->xu.platform.vao ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, wrapper->xu.platform.vbo ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, wrapper->xu.platform.ebo ) );

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
}