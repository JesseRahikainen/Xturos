#include "testGamePadState.h"

#include <SDL3/SDL.h>

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "IMGUI/nuklearWrapper.h"
#include "Math/mathUtil.h"

// TODO: this is just a reference for the SDL gamepad API, will later want to expand this to be integrated into our input system
//  include a way to treat the triggers like buttons
//  may also want to include the idea of a player into the input

static SDL_JoystickID currentID = 0;

static Sint16 deadZones[SDL_GAMEPAD_AXIS_COUNT];

static void testGamePadState_Enter( void )
{
	gfx_SetClearColor( CLR_BLACK );

	int padCount;
	SDL_JoystickID* ids = SDL_GetGamepads( &padCount );
	for( int i = 0; i < padCount; ++i ) {
		SDL_OpenGamepad( ids[i] );
		if( currentID == 0 ) {
			currentID = ids[i];
		}
	}
	SDL_free( ids );

	for( size_t i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i ) {
		deadZones[i] = 9000;
	}
	deadZones[SDL_GAMEPAD_AXIS_LEFTX] = 5000;
	deadZones[SDL_GAMEPAD_AXIS_LEFTY] = 5000;
	deadZones[SDL_GAMEPAD_AXIS_RIGHTX] = 5000;
	deadZones[SDL_GAMEPAD_AXIS_RIGHTY] = 5000;
	deadZones[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 2000;
	deadZones[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 2000;
}

static void testGamePadState_Exit( void )
{
	int padCount;
	SDL_JoystickID* ids = SDL_GetGamepads( &padCount );
	for( int i = 0; i < padCount; ++i ) {
		SDL_Gamepad* pad = SDL_GetGamepadFromID( ids[i] );
		SDL_CloseGamepad( pad );
	}
	SDL_free( ids );
}

static void testGamePadState_ProcessEvents( SDL_Event* e )
{
	switch( e->type ) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: /**< Gamepad axis motion */
		//llog( LOG_DEBUG, "Axis motion on gamepad %i, axis %i = %i", e->gaxis.which, e->gaxis.axis, e->gaxis.value );
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:          /**< Gamepad button pressed */
		//llog( LOG_DEBUG, "Button down on gamepad %i, button %s", e->gbutton.which, SDL_GetGamepadStringForButton( e->gbutton.button ) );
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_UP:            /**< Gamepad button released */
		//llog( LOG_DEBUG, "Button up on gamepad %i, button %s", e->gbutton.which, SDL_GetGamepadStringForButton( e->gbutton.button ) );
		break;
	case SDL_EVENT_GAMEPAD_ADDED:                /**< A new gamepad has been inserted into the system */
		llog( LOG_DEBUG, "Gamepad added: %s", SDL_GetGamepadNameForID( e->gdevice.which ) );
		SDL_OpenGamepad( e->gdevice.which );
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:              /**< A gamepad has been removed */
		llog( LOG_DEBUG, "Gamepad removed" );
		break;
	case SDL_EVENT_GAMEPAD_REMAPPED:             /**< The gamepad mapping was updated */
		llog( LOG_DEBUG, "Gamepad remapped" );
		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:        /**< Gamepad touchpad was touched */
		llog( LOG_DEBUG, "Touchpad down" );
		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:      /**< Gamepad touchpad finger was moved */
		llog( LOG_DEBUG, "Touchpad motion" );
		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:          /**< Gamepad touchpad finger was lifted */
		llog( LOG_DEBUG, "Touchpad up" );
		break;
	case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:        /**< Gamepad sensor was updated */
		llog( LOG_DEBUG, "Sensor update" );
		break;
	case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:      /**< Gamepad update is complete */
		//llog( LOG_DEBUG, "Update complete" );
		break;
	case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:  /**< Gamepad Steam handle has changed */
		//llog( LOG_DEBUG, "Steam handle updated" );
		break;
	}
}

void padComboBoxGetter( void* data, int idx, const char** outString )
{
	SDL_JoystickID* ids = (SDL_JoystickID*)data;
	(*outString) = SDL_GetGamepadNameForID( ids[idx] );
	if( ( *outString ) == NULL ) {
		( *outString ) = "NULL";
	}
}

static void testGamePadState_Process( void )
{
	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect windowBounds = { 0, 0, (float)renderWidth, (float)renderHeight };
	nk_begin( ctx, "Gamepad", windowBounds, NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ); {

		// general shared config stuff
		nk_layout_row_begin( ctx, NK_DYNAMIC, 80, 1 ); {
			// dead zones for sticks and triggers
			nk_layout_row_push( ctx, 1.0f );
			nk_group_begin( ctx, "Deadzones", NK_WINDOW_BORDER | NK_WINDOW_TITLE ); {
				nk_layout_row_begin( ctx, NK_DYNAMIC, nk_widget_height( ctx ), (int)SDL_GAMEPAD_AXIS_COUNT ); {
					for( int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i ) {
						nk_layout_row_push( ctx, 1.0f / (float)SDL_GAMEPAD_AXIS_COUNT );
						int asInt = (int)deadZones[i];
						nk_property_int( ctx, SDL_GetGamepadStringForAxis( (SDL_GamepadAxis)i ), 0, &asInt, 32767, 1, 5 );
						deadZones[i] = (Sint16)asInt;
					}
				} nk_layout_row_end( ctx );
			} nk_group_end( ctx );
		} nk_layout_row_end( ctx );

		if( SDL_HasGamepad( ) ) {

			int padCount;
			SDL_JoystickID* ids = SDL_GetGamepads( &padCount );
			SDL_Gamepad* gamepad = NULL;

			// choose from any of the active gamepads
			nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 2 ); {

				// get the index of the current id
				int currIdx = -1;
				for( int i = 0; i < padCount; ++i ) {
					if( ids[i] == currentID ) {
						currIdx = i;
					}
				}

				nk_layout_row_push( ctx, 0.5f );
				nk_label( ctx, "Current gamepad", NK_TEXT_RIGHT );

				nk_layout_row_push( ctx, 0.25f );
				nk_combobox_callback( ctx, padComboBoxGetter, (void*)ids, &currIdx, padCount, 30, nk_vec2( nk_widget_width( ctx ), 200 ) );

				currentID = ids[currIdx];
				gamepad = SDL_GetGamepadFromID( currentID );
			} nk_layout_row_end( ctx );

			if( gamepad != NULL ) {
				// draw the state of the current controller
				//  axes
				nk_layout_row_begin( ctx, NK_DYNAMIC, 100, SDL_GAMEPAD_AXIS_COUNT ); {
					
					for( int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i ) {
						SDL_GamepadAxis axis = (SDL_GamepadAxis)i;

						nk_layout_row_push( ctx, 1.0f / (float)SDL_GAMEPAD_AXIS_COUNT );
						nk_group_begin( ctx, SDL_GetGamepadStringForAxis( axis ), NK_WINDOW_TITLE | NK_WINDOW_BORDER ); {

							Sint16 rawAxis = SDL_GetGamepadAxis( gamepad, axis );
							float axisValue = rawAxis > 0 ?
								remap( (float)deadZones[i], (float)SDL_MAX_SINT16, (float)rawAxis, 0.0f, 1.0f ) :
								remap( (float)SDL_MIN_SINT16, -(float)deadZones[i], (float)rawAxis, -1.0f, 0.0f );

							nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 ); {
								nk_layout_row_push( ctx, 1.0f );
								nk_value_int( ctx, "raw", (int)rawAxis );
							} nk_layout_row_end( ctx );

							nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 ); {
								nk_layout_row_push( ctx, 1.0f );
								nk_value_float( ctx, "val", axisValue );
							} nk_layout_row_end( ctx );
						} nk_group_end( ctx );
					}
				} nk_layout_row_end( ctx );

				// buttons
				const int BUTTONS_PER_ROW = 6;
				for( int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i ) {

					if( ( i % BUTTONS_PER_ROW ) == 0 ) {
						if( i != 0 ) nk_layout_row_end( ctx );
						nk_layout_row_begin( ctx, NK_DYNAMIC, 30, BUTTONS_PER_ROW );
					}

					SDL_GamepadButton button = (SDL_GamepadButton)i;
					nk_layout_row_push( ctx, 1.0f / (float)BUTTONS_PER_ROW );
					nk_value_bool( ctx, SDL_GetGamepadStringForButton( button ), SDL_GetGamepadButton( gamepad, button ) );
				}
				nk_layout_row_end( ctx );
			}

			SDL_free( ids );
		} else {
			nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
				nk_layout_row_push( ctx, 1.0f );
				nk_label( ctx, "No gamepads plugged in", NK_TEXT_CENTERED );
			} nk_layout_row_end( ctx );

		}
	} nk_end( ctx );
}

static void testGamePadState_Draw( void )
{

}

static void testGamePadState_PhysicsTick( float dt )
{
}

static void testGamePadState_Render( float t )
{
}

GameState testGamePadState = { testGamePadState_Enter, testGamePadState_Exit, testGamePadState_ProcessEvents,
	testGamePadState_Process, testGamePadState_Draw, testGamePadState_PhysicsTick, testGamePadState_Render };