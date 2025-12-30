#include "optionsState.h"

#include "SDL3/SDL.h"
#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "IMGUI/nuklearWrapper.h"
#include "Audio/sound.h"

// simple options screen that can serve as a template, for right now will just have audio and graphics
//  uses IMGUI as it's easy to drop in, will assume this state is pushed onto the state stack

static SDL_Window* sdlWindow = NULL;

typedef enum {
	WT_FULLSCREEN,
	WT_BORDERLESS,
	WT_WINDOW
} WindowedType;

typedef struct {
	int width;
	int height;
} WindowSizeChoice;

typedef struct {
	float masterVolume;
	WindowedType windowedType;
	int windowResolutionWidth;
	int windowResolutionHeight;
	RenderResizeType renderResizeType;
} Options;

static Options activeOptions;

static WindowSizeChoice* sbWindowSizeChoices = NULL;

static int bgSong = -1;

static WindowSizeChoice resolutions[] = {
	{ 1920, 1080 },
	{ 1366, 768 },
	{ 2560, 1440 },
	{ 1440, 900 },
	{ 1600, 900 },
	{ 3840, 2160 },
	{ 1680, 1050 },
	{ 1360, 1050 },
	{ 1280, 1024 },
	{ 2560, 1080 },
	{ 3440, 1440 },
	{ 1920, 1200 },
	{ 1280, 800 },
	{ 1024, 768 },
	{ 1280, 720 },
	{ 640, 360 },
	{ 640, 480 },
	{ 800, 600 },
	{ 1536, 864 },
	{ 2048, 1152 },
	{ 1600, 1200 },
	{ 2048, 1536 },
	{ 2560, 1600 },
	{ 5120, 2880 },
	{ 6144, 3456 },
	{ 7680, 2160 },
	{ 7680, 4320 },
	{ -1, -1 }
};

static char* resolutionsString =
"1920x1080\0"
"1336x768\0"
"2560x1440\0"
"1440x900\0"
"1600x900\0"
"3840x2160\0"
"1680x1050\0"
"1360x1050\0"
"1280x1024\0"
"2560x1080\0"
"3440x1440\0"
"1920x1200\0"
"1280x800\0"
"1024x768\0"
"1280x720\0"
"640x360\0"
"640x480\0"
"800x600\0"
"1536x864\0"
"2048x1152\0"
"1600x1200\0"
"2048x1536\0"
"2560x1600\0"
"5120x2880\0"
"6144x3456\0"
"7680x2160\0"
"7680x4320\0"
"Custom\0";

static int getMatchingResolution( void )
{
	int width, height;
	gfx_GetWindowSize( &width, &height );

	for( size_t i = 0; i < ARRAY_SIZE( resolutions ); ++i ) {
		if( ( resolutions[i].width == width ) && ( resolutions[i].height == height ) ) {
			return (int)i;
		}
	}

	return ARRAY_SIZE( resolutions ) - 1;
}

static void optionsState_Enter( void )
{
	snd_SetMasterVolume( 0.0f ); // for testing, remove later

	// play a song so we know that the audio option changes work
	bgSong = snd_LoadStreaming( "Sounds/NowhereLand.ogg", true, 0 );
	snd_PlayStreaming( bgSong, 1.0f, 0.0f, 0 );

	// grab the initial values for all the options
	
	// audio options
	activeOptions.masterVolume = snd_GetMasterVolume( );

	// need the window for resolution and fullscreen/windowed stuff
	int numWindows;
	SDL_Window** windows = SDL_GetWindows( &numWindows );
	ASSERT_AND_IF_NOT( numWindows == 1 ) {
		// something has gone SERIOUSLY wrong
		llog( LOG_CRITICAL, "Invalid number of windows available." );
#if !defined( __EMSCRIPTEN__ )
		exit( 1 );
#endif
	}
	sdlWindow = windows[0];
	SDL_free( windows );

	bool isFullScreen = SDL_GetWindowFlags( sdlWindow ) & SDL_WINDOW_FULLSCREEN;
	if( !isFullScreen ) {
		activeOptions.windowedType = WT_WINDOW;
	} else {
		if( SDL_GetWindowFullscreenMode( sdlWindow ) == NULL ) {
			activeOptions.windowedType = WT_BORDERLESS;
		} else {
			activeOptions.windowedType = WT_FULLSCREEN;
		}
	}
	SDL_GetWindowSize( sdlWindow, &activeOptions.windowResolutionWidth, &activeOptions.windowResolutionHeight );

	activeOptions.renderResizeType = gfx_GetRenderResizeType( );
}

static void setWindowedMode( WindowedType wt )
{
	switch( wt ) {
	case WT_WINDOW:
		SDL_SetWindowFullscreen( sdlWindow, false );
		break;
	case WT_BORDERLESS:
		SDL_SetWindowFullscreenMode( sdlWindow, NULL );
		SDL_SetWindowFullscreen( sdlWindow, true );
		break;
	case WT_FULLSCREEN:
		{
			int numModes;
			SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes( SDL_GetDisplayForWindow( sdlWindow ), &numModes );
			ASSERT_AND_IF_NOT( modes != NULL ) {
				llog( LOG_ERROR, "Error getting display modes: %s", SDL_GetError( ) );
				return;
			}
			SDL_SetWindowFullscreenMode( sdlWindow, modes[0] );
			SDL_free( modes );
		}
		SDL_SetWindowFullscreen( sdlWindow, true );
		break;
	default:
		ASSERT_ALWAYS( "No implemented case." );
	}
}

static void optionsState_Exit( void )
{
	snd_StopStreaming( bgSong );
	snd_UnloadStream( bgSong );
}

static void optionsState_ProcessEvents( SDL_Event* e )
{
}

struct nk_rect createBounds( float baseWidth, float baseHeight, float pctWidth, float pctHeight )
{
	struct nk_rect bounds;
	bounds.x = baseWidth * ( ( 1.0f - pctWidth ) * 0.5f );
	bounds.y = baseHeight * ( ( 1.0f - pctHeight ) * 0.5f );
	bounds.w = baseWidth * pctWidth;
	bounds.h = baseHeight * pctHeight;
	return bounds;
}

struct nk_rect createCenteredBounds( float baseWidth, float baseHeight, float boundsWidth, float boundsHeight )
{
	struct nk_rect bounds;
	
	bounds.x = ( baseWidth - boundsWidth ) / 2.0f;
	bounds.y = ( baseHeight - boundsHeight ) / 2.0f;
	bounds.w = boundsWidth;
	bounds.h = boundsHeight;
	return bounds;
}

static void optionsState_Process( void )
{
	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect windowBounds = createCenteredBounds( (float)renderWidth, (float)renderHeight, 400.0f, 336.0f );
	nk_begin( ctx, "Options", windowBounds, NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ); {
		// just do a vertical list of stuff
		nk_layout_row_begin( ctx, NK_DYNAMIC, 80, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_group_begin_titled( ctx, "AudioOptions", "Audio", NK_WINDOW_BORDER | NK_WINDOW_TITLE ) ) {
				nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 3 ); {
					nk_layout_row_push( ctx, 0.2f );
					nk_label( ctx, "Volume", NK_TEXT_RIGHT );

					nk_layout_row_push( ctx, 0.1f );
					nk_spacer( ctx );

					nk_layout_row_push( ctx, 0.7f );
					nk_slider_float( ctx, 0.0f, &activeOptions.masterVolume, 1.0f, 0.1f );
					snd_SetMasterVolume( activeOptions.masterVolume );
				} nk_layout_row_end( ctx );
				nk_group_end( ctx );
			}
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 178, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_group_begin_titled( ctx, "GraphicsOptions", "Graphics", NK_WINDOW_BORDER | NK_WINDOW_TITLE ) ) {
				nk_layout_row_begin( ctx, NK_DYNAMIC, 40, 3 ); {

					nk_layout_row_push( ctx, 0.2f );
					nk_label( ctx, "Render Fit", NK_TEXT_RIGHT );

					nk_layout_row_push( ctx, 0.1f );
					nk_spacer( ctx );

					nk_layout_row_push( ctx, 0.69f );
					int rrtAsInt = (int)activeOptions.renderResizeType;
					nk_combobox_string( ctx, "Match\0Fixed Height\0Fixed Width\0Best Fit\0Fit Render Inside\0Fit Render Outside", &rrtAsInt, NUM_RENDER_RESIZE_TYPES, 20, nk_vec2( nk_widget_width( ctx ), 200 ) );
					activeOptions.renderResizeType = (RenderResizeType)rrtAsInt;
					gfx_ChangeRenderResizeType( activeOptions.renderResizeType );
				} nk_layout_row_end( ctx );

				nk_layout_row_begin( ctx, NK_DYNAMIC, 40, 3 ); {
					nk_layout_row_push( ctx, 0.2f );
					nk_label( ctx, "Windowed", NK_TEXT_RIGHT );

					nk_layout_row_push( ctx, 0.1f );
					nk_spacer( ctx );

					nk_layout_row_push( ctx, 0.69f );
					int windowedAsInt = (int)activeOptions.windowedType;
					nk_combobox_string( ctx, "Fullscreen\0Borderless Fullscreen\0Windowed", &windowedAsInt, 3, 20, nk_vec2( nk_widget_width( ctx ), 200 ) );
					WindowedType newWindowedType = (WindowedType)windowedAsInt;
					if( newWindowedType != activeOptions.windowedType ) {

						// set the resolution and then show a dialog with a countdown to confirm the change
						activeOptions.windowedType = newWindowedType;
						setWindowedMode( activeOptions.windowedType );
					}
				} nk_layout_row_end( ctx );


				int selectedResolution = getMatchingResolution( );
				nk_layout_row_begin( ctx, NK_DYNAMIC, 40, 3 ); {
					nk_layout_row_push( ctx, 0.2f );
					nk_label( ctx, "Window Size", NK_TEXT_RIGHT );

					nk_layout_row_push( ctx, 0.1f );
					nk_spacer( ctx );

					nk_layout_row_push( ctx, 0.69f );
					int windowedAsInt = (int)activeOptions.windowedType;
					int numResolutions = ( selectedResolution == ARRAY_SIZE( resolutions ) - 1 ) ? (int)ARRAY_SIZE( resolutions ) : (int)ARRAY_SIZE( resolutions ) - 1;
					int newResolution = selectedResolution;
					nk_combobox_string( ctx, resolutionsString, &newResolution, numResolutions, 20, nk_vec2( nk_widget_width( ctx ), 200 ) );
					if( selectedResolution != newResolution ) {
						SDL_SetWindowSize( sdlWindow, resolutions[newResolution].width, resolutions[newResolution].height );
					}

				} nk_layout_row_end( ctx );

				nk_group_end( ctx );
			}

			// bottom buttons
			nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 3 ); {
				bool hasDifferingOptions = false;

				const float BUTTON_WEIGHT = 10.0f;
				const float SPACING_WEIGHT = 5.0f;
				const float TOTAL_WEIGHT = ( 1.0f * BUTTON_WEIGHT ) + ( 2.0f * SPACING_WEIGHT );

				nk_layout_row_push( ctx, SPACING_WEIGHT / TOTAL_WEIGHT );
				nk_spacer( ctx );

				nk_layout_row_push( ctx, BUTTON_WEIGHT / TOTAL_WEIGHT );
				if( nk_button_label( ctx, "Exit" ) ) {
					gsm_PopState( &globalFSM );
				}

				nk_layout_row_push( ctx, SPACING_WEIGHT / TOTAL_WEIGHT );
				nk_spacer( ctx );
			} nk_layout_row_end( ctx );

		} nk_layout_row_end( ctx );
	} nk_end( ctx );
}

static void optionsState_Draw( void )
{
}

static void optionsState_PhysicsTick( float dt )
{
}

static void optionsState_Render( float t )
{
}

GameState optionsState = { optionsState_Enter, optionsState_Exit, optionsState_ProcessEvents,
	optionsState_Process, optionsState_Draw, optionsState_PhysicsTick, optionsState_Render };