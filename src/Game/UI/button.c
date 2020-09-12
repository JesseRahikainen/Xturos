#include "button.h"

#include <SDL_mouse.h>
#include <memory.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "../Graphics/images.h"
#include "../Math/vector3.h"
#include "../Graphics/camera.h"
#include "text.h"
#include "../Input/input.h"
#include "../System/systems.h"
#include "../System/platformLog.h"
#include "../Graphics/debugRendering.h"
#include "../tween.h"

#define ANIM_START_LENGTH ( 0.75f / 2.0f )
#define ANIM_LENGTH 0.75f
#define MAX_BUTTONS 64
#define BUTTON_TEXT_LEN 32
enum ButtonState { BS_NORMAL, BS_FOCUSED, BS_CLICKED, NUM_STATES };

static int systemID = -1;

struct Button {

	int* slicedBorder;
	int imgID;
	Color borderColor;

	Vector2 collisionHalfSize;

	Vector2 normalDrawBorderSize;
	Vector2 clickedDrawBorderSize;

	Vector2 normalImgScale;
	Vector2 clickedImgScale;
	
	Color fontColor;
	int fontID;
	float fontPixelSize;
	char text[BUTTON_TEXT_LEN+1];
	Vector2 textOffset;

	float timeInAnim;
	Vector2 prevSize;
	Vector2 prevScale;

	ButtonResponse pressResponse;
	ButtonResponse releaseResponse;

	char depth;
	Vector2 position;

	enum ButtonState state;

	bool inUse;

	unsigned int camFlags;
};

static struct Button buttons[MAX_BUTTONS];
static char mouseDown;

void btn_Init( )
{
	memset( buttons, 0, sizeof( buttons ) );
	mouseDown = 0;
}

int btn_Create( Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int* slicedBorder, int imgID, Color imgColor,
	unsigned int camFlags, char layer, ButtonResponse pressResponse, ButtonResponse releaseResponse )
{
	int newIdx;

	newIdx = 0;
	while( ( newIdx < MAX_BUTTONS ) && ( buttons[newIdx].inUse == 1 ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_BUTTONS ) {
		llog( LOG_WARN, "Unable to create new button, no open slots." );
		return -1;
	}

	buttons[newIdx].position = position;
	buttons[newIdx].fontID = fontID;
	buttons[newIdx].fontPixelSize = fontPixelSize;
	buttons[newIdx].fontColor = fontColor;
	buttons[newIdx].textOffset = textOffset;
	buttons[newIdx].slicedBorder = slicedBorder;
	buttons[newIdx].borderColor = imgColor;
	buttons[newIdx].camFlags = camFlags;
	buttons[newIdx].depth = layer;
	buttons[newIdx].pressResponse = pressResponse;
	buttons[newIdx].releaseResponse = releaseResponse;
	vec2_Scale( &normalSize, 0.5f, &( buttons[newIdx].collisionHalfSize ) );
	if( buttons[newIdx].collisionHalfSize.x < 0 ) buttons[newIdx].collisionHalfSize.x *= -1;
	if( buttons[newIdx].collisionHalfSize.y < 0 ) buttons[newIdx].collisionHalfSize.y *= -1;
	buttons[newIdx].normalDrawBorderSize = normalSize;
	buttons[newIdx].clickedDrawBorderSize = clickedSize;
	buttons[newIdx].timeInAnim = ANIM_LENGTH;
	buttons[newIdx].prevSize = normalSize;
	buttons[newIdx].state = BS_NORMAL;
	buttons[newIdx].imgID = imgID;

	if( imgID >= 0 ) {
		Vector2 imageSize;
		img_GetSize( imgID, &imageSize );

		buttons[newIdx].normalImgScale.x = normalSize.x / imageSize.x;
		buttons[newIdx].normalImgScale.y = normalSize.y / imageSize.y;

		buttons[newIdx].clickedImgScale.x = clickedSize.x / imageSize.x;
		buttons[newIdx].clickedImgScale.y = clickedSize.y / imageSize.y;
	} else {
		buttons[newIdx].normalImgScale = VEC2_ZERO;
		buttons[newIdx].clickedImgScale = VEC2_ZERO;
	}
	buttons[newIdx].prevScale = buttons[newIdx].normalImgScale;

	if( text != NULL ) {
		strncpy( buttons[newIdx].text, text, BUTTON_TEXT_LEN );
		buttons[newIdx].text[BUTTON_TEXT_LEN] = 0;
	} else {
		memset( buttons[newIdx].text, 0, sizeof( buttons[newIdx].text ) );
	}

	buttons[newIdx].inUse = true;

	return newIdx;
}

void btn_Destroy( int idx )
{
	if( ( idx < 0 ) || ( idx >= MAX_BUTTONS ) ) {
		return;
	}
	buttons[idx].inUse = 0;
}

void btn_DestroyAll( void )
{
	memset( buttons, 0, sizeof( buttons ) );
}

int btn_RegisterSystem( void )
{
	if( systemID != -1 ) return systemID; // don't register twice
	systemID = sys_Register( btn_ProcessEvents, btn_Process, btn_Draw, btn_Update );
	return systemID;
}

void btn_UnRegisterSystem( void )
{
	sys_UnRegister( systemID );
	systemID = -1;
}

void btn_Update( float dt )
{
	for( int i = 0; i < MAX_BUTTONS; ++i ) {
		if( buttons[i].inUse ) {
			float max = ( buttons[i].state == BS_CLICKED ) ? ANIM_START_LENGTH : ANIM_LENGTH;
			buttons[i].timeInAnim = clamp( 0.0f, max, buttons[i].timeInAnim + dt );
		}
	}
}

void btn_Draw( void )
{
	int i;
	int img = -1;

	for( i = 0; i < MAX_BUTTONS; ++i ) {
		if( buttons[i].inUse ) {

			float t = buttons[i].timeInAnim / ANIM_LENGTH;

			if( t < ( ( ANIM_START_LENGTH / ANIM_LENGTH ) * 0.5f ) ) {
				t = inverseLerp( 0.0f, ( ( ANIM_START_LENGTH / ANIM_LENGTH ) * 0.5f ), t );
				t = easeInQuad( t );
			} else if( t < ( ANIM_START_LENGTH / ANIM_LENGTH ) ) {
				t = inverseLerp( ( ( ANIM_START_LENGTH / ANIM_LENGTH ) * 0.5f ), ( ANIM_START_LENGTH / ANIM_LENGTH ), t );
				t = ( 1.0f - t );
				t = ( ( ( -2.0f * t * t * t) + ( 3.0f * t * t ) ) / 2.0f ) + 0.5f;
			} else { // past the start/clicked portion
				t = inverseLerp( ( ANIM_START_LENGTH / ANIM_LENGTH ), 1.0f, t );
				t = easeOutQuad( 1.0f - t ) * 0.5f;
			}

			if( buttons[i].imgID >= 0 ) {
				Vector2 scale;
				vec2_Lerp( &( buttons[i].normalImgScale ), &( buttons[i].clickedImgScale ), t, &scale );

				int drawID = img_CreateDraw( buttons[i].imgID, buttons[i].camFlags, buttons[i].position, buttons[i].position, buttons[i].depth );
				img_SetDrawScaleV( drawID, buttons[i].prevScale, scale );
				img_SetDrawColor( drawID, buttons[i].borderColor, buttons[i].borderColor );
				/*img_Draw_sv_c( buttons[i].imgID, buttons[i].camFlags, buttons[i].position, buttons[i].position, buttons[i].prevScale,
					scale, buttons[i].borderColor, buttons[i].borderColor, buttons[i].depth );//*/
				buttons[i].prevScale = scale;
			}
			
			if( buttons[i].slicedBorder != NULL ) {
				Vector2 size;
				vec2_Lerp( &( buttons[i].normalDrawBorderSize ), &( buttons[i].clickedDrawBorderSize ), t, &size );
				img_Draw3x3v_c( buttons[i].slicedBorder, buttons[i].camFlags, buttons[i].position, buttons[i].position, buttons[i].prevSize, size,
					buttons[i].borderColor, buttons[i].borderColor, buttons[i].depth );
				buttons[i].prevSize = size;
			}

			if( ( buttons[i].text[0] != 0 ) && ( buttons[i].fontID >= 0 ) ) {
				Vector2 textPos;
				vec2_Add( &( buttons[i].position ), &( buttons[i].textOffset ), &textPos );
				txt_DisplayString( buttons[i].text, textPos, buttons[i].fontColor, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER,
									buttons[i].fontID, buttons[i].camFlags, buttons[i].depth, buttons[i].fontPixelSize );
			}
		}
	}
}

void btn_Process( void )
{
	int i;
	Vector2 transMousePos = { 0.0f, 0.0f };
	Vector2 diff;
	enum ButtonState prevState;

	/* see if the mouse is positioned over any buttons */
	Vector2 mousePos;
	if( !input_GetMousePosition( &mousePos ) ) {
		return;
	}

	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		unsigned int camFlags = cam_GetFlags( currCamera );
		cam_ScreenPosToWorldPos( currCamera, &mousePos, &transMousePos );

		for( i = 0; i < MAX_BUTTONS; ++i ) {
			if( buttons[i].inUse == 0 ) {
				continue;
			}

			if( ( buttons[i].camFlags & camFlags ) == 0 ) {
				continue;
			}

			/* see if mouse pos is within the buttons borders */
			prevState = buttons[i].state;
			diff.x = buttons[i].position.x - transMousePos.x;
			diff.y = buttons[i].position.y - transMousePos.y;
			if( ( fabsf( diff.x ) <= buttons[i].collisionHalfSize.x ) && ( fabsf( diff.y ) <= buttons[i].collisionHalfSize.y ) ) {
				if( mouseDown ) {
					buttons[i].state = BS_CLICKED;
				} else {
					buttons[i].state = BS_FOCUSED;
				}
			} else {
				buttons[i].state = BS_NORMAL;
			}

#if defined( __ANDROID__ ) // touch screens respond differently, don't need focus
			if( ( prevState != BS_CLICKED ) && ( buttons[i].state == BS_CLICKED ) ) {
				buttons[i].timeInAnim = 0.0f;
				if( buttons[i].pressResponse != NULL ) buttons[i].pressResponse( i );
			} else if( ( prevState == BS_CLICKED ) && ( buttons[i].state != BS_CLICKED ) && ( buttons[i].releaseResponse != NULL ) ) {
				buttons[i].releaseResponse( i );
			}
#else
			if( ( prevState == BS_FOCUSED ) && ( buttons[i].state == BS_CLICKED ) ) {
				buttons[i].timeInAnim = 0.0f;
				if( buttons[i].pressResponse != NULL ) buttons[i].pressResponse( i );
			} else if( ( prevState == BS_CLICKED ) && ( buttons[i].state == BS_FOCUSED ) && ( buttons[i].releaseResponse != NULL ) ) {
				buttons[i].releaseResponse( i );
			}
#endif
		}
	}
}

void btn_ProcessEvents( SDL_Event* sdlEvent )
{
	if( ( sdlEvent->type == SDL_MOUSEBUTTONDOWN ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 1;
	} else if( ( sdlEvent->type == SDL_MOUSEBUTTONUP ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 0;
	}
}

void btn_DebugDraw( Color idle, Color hover, Color clicked )
{
	for( int i = 0; i < MAX_BUTTONS; ++i ) {
		if( buttons[i].inUse ) {
			Color clr;
			switch( buttons[i].state ) {
			case BS_NORMAL:
				clr = idle;
				break;
			case BS_FOCUSED:
				clr = hover;
				break;
			case BS_CLICKED:
			default:
				clr = clicked;
				break;
			}

			Vector2 topLeft;
			Vector2 size;
			vec2_Subtract( &( buttons[i].position ), &( buttons[i].collisionHalfSize ), &topLeft );
			vec2_Scale( &( buttons[i].collisionHalfSize ), 2.0f, &size );
			debugRenderer_AABB( buttons[i].camFlags, topLeft, size, clr );
		}
	}
}