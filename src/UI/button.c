#include "button.h"

#include <SDL_log.h>
#include <SDL_mouse.h>
#include <memory.h>
#include <assert.h>
#include <math.h>

#include "../Graphics/images.h"
#include "../Math/vector3.h"
#include "../Graphics/camera.h"
#include "text.h"
#include "../Input/input.h"
#include "../System/systems.h"

#define MAX_BUTTONS 32
#define BUTTON_TEXT_LEN 32
static enum ButtonState { BS_NORMAL, BS_FOCUSED, BS_CLICKED };

static int systemID = -1;

static struct Button {
	int normalImgId;
	int focusImgId;
	int clickedImdId;

	int fontID;
	char text[BUTTON_TEXT_LEN+1];

	enum ButtonState state;
	ButtonResponse response;

	ButtonResponse pressResponse;
	ButtonResponse releaseResponse;

	char layer;
	Vector2 position;
	Vector2 halfSize;

	char inUse;

	unsigned int camFlags;
};

static struct Button buttons[MAX_BUTTONS];
static char mouseDown;

void btn_Init( )
{
	memset( buttons, 0, sizeof( buttons ) );
	mouseDown = 0;
}

int btn_Create( Vector2 position, Vector2 size, const char* text, int fontID, int normalImg, int focusedImg, int clickedImg,
	unsigned int camFlags, char layer, ButtonResponse pressResponse, ButtonResponse releaseResponse )
{
	int newIdx;

	newIdx = 0;
	while( ( newIdx < MAX_BUTTONS ) && ( buttons[newIdx].inUse == 1 ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_BUTTONS ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_SYSTEM, "Unable to create new button, no open slots." );
		return -1;
	}

	vec2_Scale( &size, 0.5f, &( buttons[newIdx].halfSize ) );
	buttons[newIdx].position = position;
	
	buttons[newIdx].normalImgId = normalImg;
	buttons[newIdx].focusImgId = focusedImg;
	buttons[newIdx].clickedImdId = clickedImg;
	buttons[newIdx].layer = layer;
	buttons[newIdx].inUse = 1;
	buttons[newIdx].pressResponse = pressResponse;
	buttons[newIdx].releaseResponse = releaseResponse;
	buttons[newIdx].state = BS_NORMAL;
	buttons[newIdx].camFlags = camFlags;
	buttons[newIdx].fontID = fontID;

	if( text != NULL ) {
		strncpy( buttons[newIdx].text, text, BUTTON_TEXT_LEN );
		buttons[newIdx].text[BUTTON_TEXT_LEN] = 0;
	} else {
		memset( buttons[newIdx].text, 0, sizeof( buttons[newIdx].text ) );
	}

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
	systemID = sys_Register( btn_ProcessEvents, btn_Process, btn_Draw, NULL );
	return systemID;
}

void btn_UnRegisterSystem( void )
{
	sys_UnRegister( systemID );
}

void btn_Draw( void )
{
	int i;
	int img = -1;

	for( i = 0; i < MAX_BUTTONS; ++i ) {
		if( buttons[i].inUse ) {
			switch( buttons[i].state ) {
			case BS_NORMAL:
				img = buttons[i].normalImgId;
				break;
			case BS_FOCUSED:
				img = buttons[i].focusImgId;
				break;
			case BS_CLICKED:
				img = buttons[i].clickedImdId;
				break;
			}
			img_Draw( img, buttons[i].camFlags, buttons[i].position, buttons[i].position, buttons[i].layer );
			if( ( buttons[i].text[0] != 0 ) && ( buttons[i].fontID >= 0 ) ) {
				txt_DisplayString( buttons[i].text, buttons[i].position, CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER,
									buttons[i].fontID, buttons[i].camFlags, buttons[i].layer );
			}
		}
	}
}

void btn_Process( void )
{
	int i;
	Vector3 mousePos;
	Vector3 transMousePos = { 0.0f, 0.0f, 0.0f };
	Vector2 diff;
	enum ButtonState prevState;

	/* see if the mouse is positioned over any buttons */
	Vector2 mouse2DPos;
	if( !input_GetMousePostion( &mouse2DPos ) ) {
		return;
	}
	vec2ToVec3( &mouse2DPos, 0.0f, &mousePos );

	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		unsigned int camFlags = cam_GetFlags( currCamera );
		Matrix4 camMat;
		cam_GetInverseViewMatrix( currCamera, &camMat );
		mat4_TransformVec3Pos( &camMat, &mousePos, &transMousePos );

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
			if( ( fabsf( diff.x ) <= buttons[i].halfSize.x ) && ( fabsf( diff.y ) <= buttons[i].halfSize.y ) ) {
				if( mouseDown ) {
					buttons[i].state = BS_CLICKED;
				} else {
					buttons[i].state = BS_FOCUSED;
				}
			} else {
				buttons[i].state = BS_NORMAL;
			}

			if( ( prevState == BS_FOCUSED ) && ( buttons[i].state == BS_CLICKED ) && ( buttons[i].pressResponse != NULL ) ) {
				buttons[i].pressResponse( );
			} else if( ( prevState == BS_CLICKED ) && ( buttons[i].state == BS_FOCUSED ) && ( buttons[i].releaseResponse != NULL ) ) {
				buttons[i].releaseResponse( );
			}
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