#define NK_IMPLEMENTATION
#include "nuklearWrapper.h"

#include <SDL_clipboard.h>

#include "../Graphics/graphics.h"
#include "../Graphics/triRendering.h"
#include "../Graphics/scissor.h"
#include "../System/memory.h"
#include "../Math/matrix4.h"
#include "../Input/input.h"
#include "../System/platformLog.h"

#include "../Graphics/debugRendering.h"

#include "../Graphics/glPlatform.h"

//struct nk_context* nkCtx = NULL;

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
    GL( glGenTextures( 1, &( xu->fontTx ) ) );
    GL( glBindTexture( GL_TEXTURE_2D, xu->fontTx ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image ) );
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
	return mem_Resize( old, size );
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
	//struct nk_xu_device* dev = &( xu.xuDev );
	nk_buffer_init( &( xu->cmds ), &alloc, INITIAL_CMD_BUFFER_SIZE );
	//  create the shader, TODO: share this between contexts
	
	ShaderDefinition shaderDefs[2];
	ShaderProgramDefinition progDef;

	shaderDefs[0].fileName = NULL;
	shaderDefs[0].type = GL_VERTEX_SHADER;
	shaderDefs[1].fileName = NULL;
	shaderDefs[1].type = GL_FRAGMENT_SHADER;
#if defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
	shaderDefs[0].shaderText =	"#version 300 es\n"
								"uniform mat4 vpMatrix;\n"
								"layout(location = 0) in vec2 vVertex;\n"
								"layout(location = 1) in vec2 vTexCoord0;\n"
								"layout(location = 2) in vec4 vColor;\n"
								"out vec2 vTex;\n"
								"out vec4 vCol;\n"
								"void main( void )\n"
								"{\n"
								"	vTex = vTexCoord0;\n"
								"	vCol = vColor;\n"
								"	gl_Position = vpMatrix * vec4( vVertex.xy, 0.0f, 1.0f );\n"
								"}\n";

	
	shaderDefs[1].shaderText =	"#version 300 es\n"
								"in highp vec2 vTex;\n"
								"in highp vec4 vCol;\n"
								"uniform sampler2D textureUnit0;\n"
								"out highp vec4 outCol;\n"
								"void main( void )\n"
								"{\n"
								"	outCol = texture(textureUnit0, vTex) * vCol;\n"
								"}\n";
#else
	shaderDefs[0].shaderText =	"#version 330\n"
								"uniform mat4 vpMatrix;\n"
								"layout(location = 0) in vec2 vVertex;\n"
								"layout(location = 1) in vec2 vTexCoord0;\n"
								"layout(location = 2) in vec4 vColor;\n"
								"out vec2 vTex;\n"
								"out vec4 vCol;\n"
								"void main( void )\n"
								"{\n"
								"	vTex = vTexCoord0;\n"
								"	vCol = vColor;\n"
								"	gl_Position = vpMatrix * vec4( vVertex.xy, 0.0f, 1.0f );\n"
								"}\n";

	
	shaderDefs[1].shaderText =	"#version 330\n"
								"in vec2 vTex;\n"
								"in vec4 vCol;\n"
								"uniform sampler2D textureUnit0;\n"
								"out vec4 outCol;\n"
								"void main( void )\n"
								"{\n"
								"	outCol = texture2D(textureUnit0, vTex) * vCol;\n"
								"}\n";
#endif

	progDef.geometryShader = 0;
	progDef.vertexShader = 1;
	progDef.fragmentShader = -1;
	progDef.uniformNames = "vpMatrix textureUnit0";

	assert( shaders_Load( shaderDefs, sizeof( shaderDefs ) / sizeof( shaderDefs[0] ), &progDef, &( xu->prog ), 1 ) != 0 );

	GLsizei vertSize = sizeof( nk_xturos_vertex );
	size_t vertPos = offsetof( nk_xturos_vertex, position );
	size_t vertUV = offsetof( nk_xturos_vertex, uv );
	size_t vertClr = offsetof( nk_xturos_vertex, col );

	GL( glGenBuffers( 1, &( xu->vbo ) ) );
	GL( glGenBuffers( 1, &( xu->ebo ) ) );
	GL( glGenVertexArrays( 1, &( xu->vao ) ) );

	GL( glBindVertexArray( xu->vao ) );
	GL( glBindBuffer( GL_ARRAY_BUFFER, xu->vbo ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, xu->ebo ) );

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

	image = nk_font_atlas_bake( &( xu->fontAtlas ), &w, &h, NK_FONT_ATLAS_RGBA32 );
	uploadAtlas( xu, image, w, h );
	nk_font_atlas_end(&( xu->fontAtlas ), nk_handle_id((int)xu->fontTx), &( xu->nullTx ) );
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
		if( ( evt->type == SDL_MOUSEBUTTONDOWN ) || ( evt->type == SDL_MOUSEBUTTONUP ) || ( evt->type == SDL_MOUSEMOTION ) ) {
			mX = evt->button.x;
			mY = evt->button.y;
		}
	}
	
	if( !( xu->useRelativeMousePos ) && ( evt->type == SDL_WINDOWEVENT ) && ( evt->window.event == SDL_WINDOWEVENT_RESIZED ) ) {
		xu->renderWidth = evt->window.data1;
		xu->renderHeight = evt->window.data2;
	}

    if (evt->type == SDL_KEYUP || evt->type == SDL_KEYDOWN) {
        /* key events */
        int down = evt->type == SDL_KEYDOWN;
        const Uint8* state = SDL_GetKeyboardState(0);
        SDL_Keycode sym = evt->key.keysym.sym;
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
        } else if( sym == SDLK_z ) {
            nk_input_key( ctx, NK_KEY_TEXT_UNDO, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_r ) {
            nk_input_key( ctx, NK_KEY_TEXT_REDO, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_c ) {
            nk_input_key( ctx, NK_KEY_COPY, down && state[SDL_SCANCODE_LCTRL] );
        } else if( sym == SDLK_v ) {
            nk_input_key( ctx, NK_KEY_PASTE, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_x ) {
            nk_input_key( ctx, NK_KEY_CUT, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_b ) {
            nk_input_key( ctx, NK_KEY_TEXT_LINE_START, down && state[SDL_SCANCODE_LCTRL] );
		} else if( sym == SDLK_e ) {
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
    } else if( ( evt->type == SDL_MOUSEBUTTONDOWN ) || ( evt->type == SDL_MOUSEBUTTONUP ) ) {
        /* mouse button */
        int down = evt->type == SDL_MOUSEBUTTONDOWN;
        if( evt->button.button == SDL_BUTTON_LEFT )		nk_input_button( ctx, NK_BUTTON_LEFT, mX, mY, down );
        if( evt->button.button == SDL_BUTTON_MIDDLE )	nk_input_button( ctx, NK_BUTTON_MIDDLE, mX, mY, down );
        if( evt->button.button == SDL_BUTTON_RIGHT )	nk_input_button( ctx, NK_BUTTON_RIGHT, mX, mY, down );
    } else if( evt->type == SDL_MOUSEMOTION ) {
		nk_input_motion( ctx, mX, mY );
    } else if( evt->type == SDL_TEXTINPUT ) {
        nk_glyph glyph;
        memcpy( glyph, evt->text.text, NK_UTF_SIZE );
        nk_input_glyph( ctx, glyph );
    } else if( evt->type == SDL_MOUSEWHEEL ) {
		struct nk_vec2 scroll;
		scroll.y = (float)evt->wheel.y;
		scroll.x = (float)evt->wheel.x;
        nk_input_scroll( ctx, scroll );
    }
}

void nk_xu_render( NuklearWrapper* xu )
{
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

	GL( glUseProgram( xu->prog.programID ) );
	GL( glUniform1i( xu->prog.uniformLocs[1], 0 ) );
	GL( glUniformMatrix4fv( xu->prog.uniformLocs[0], 1, GL_FALSE, &( ortho.m[0] ) ) );
	{
		// convert from command queue into draw list and render
		const struct nk_draw_command *cmd = NULL;
		void* vertices = NULL;
		void* elements = NULL;
		const nk_draw_index* offset = NULL;

		// allocate vertex adn element buffer
		GL( glBindVertexArray( xu->vao ) );
		GL( glBindBuffer( GL_ARRAY_BUFFER, xu->vbo ) );
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, xu->ebo ) );

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
			config.curve_segment_count = 22;
			config.arc_segment_count = 22;
			config.null = xu->nullTx;

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

			GL( glBindTexture( GL_TEXTURE_2D, (GLuint)( cmd->texture.id ) ) );
			glScissor(
				(GLint)( cmd->clip_rect.x ),
				(GLint)( ( xu->renderHeight - (GLint)( cmd->clip_rect.y + cmd->clip_rect.h ) ) ),
				(GLint)( cmd->clip_rect.w ),
				(GLint)( cmd->clip_rect.h ) );

			GL( glDrawElements( GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset ) );

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
}

void nk_xu_shutdown( NuklearWrapper* xu )
{
	nk_font_atlas_clear( &( xu->fontAtlas ) );
	nk_free( &( xu->ctx ) );

	// destroy the device stuff
	shaders_Destroy( &( xu->prog ), 1 );
	GL( glDeleteTextures( 1, &( xu->fontTx ) ) );
	GL( glDeleteBuffers( 1, &( xu->vbo ) ) );
	GL( glDeleteBuffers( 1, &( xu->ebo ) ) );

	nk_buffer_free( &( xu->cmds ) );

	memset( &xu, 0, sizeof( xu ) );
}