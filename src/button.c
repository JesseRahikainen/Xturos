#include "button.h"

#include <SDL_log.h>
#include <SDL_mouse.h>
#include <memory.h>
#include <assert.h>

#include "Graphics/graphics.h"
#include "Math/vector3.h"
#include "Graphics/camera.h"

#define MAX_BUTTONS 32

static enum ButtonState { BS_NORMAL, BS_FOCUSED, BS_CLICKED };

static struct Button {
	int normalImgId;
	int focusImgId;
	int clickedImdId;

	enum ButtonState state;
	ButtonResponse response;

	ButtonResponse pressResponse;
	ButtonResponse releaseResponse;

	char layer;
	Vector2 position;
	Vector2 size;

	char inUse;

	unsigned int camFlags;
};

static struct Button buttons[MAX_BUTTONS];
static char mouseDown;

void initButtons( )
{
	memset( buttons, 0, sizeof( buttons ) );
	mouseDown = 0;
}

int createButton( Vector2 position, Vector2 size, int normalImg, int focusedImg, int clickedImg, unsigned int camFlags,
	char layer, ButtonResponse pressResponse, ButtonResponse releaseResponse )
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

	buttons[newIdx].normalImgId = normalImg;
	buttons[newIdx].focusImgId = focusedImg;
	buttons[newIdx].clickedImdId = clickedImg;
	buttons[newIdx].position = position;
	buttons[newIdx].size = size;
	buttons[newIdx].layer = layer;
	buttons[newIdx].inUse = 1;
	buttons[newIdx].pressResponse = pressResponse;
	buttons[newIdx].releaseResponse = releaseResponse;
	buttons[newIdx].state = BS_NORMAL;
	buttons[newIdx].camFlags = camFlags;

	return newIdx;
}

void clearButton( int idx )
{
	if( ( idx < 0 ) || ( idx >= MAX_BUTTONS ) ) {
		return;
	}
	buttons[idx].inUse = 0;
}

void clearAllButtons( )
{
	memset( buttons, 0, sizeof( buttons ) );
}

void drawButtons( )
{
	int i;
	int img = -1;

	for( i = 0; i < MAX_BUTTONS; ++i ) {
		if( buttons[i].inUse == 1 ) {
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
			queueRenderImage( img, buttons[i].camFlags, buttons[i].position, buttons[i].position, buttons[i].layer );
		}
	}
}

void processButtons( )
{
	int i;
	int mouseX, mouseY;
	Vector3 mousePos = { 0.0f, 0.0f, 0.0f };
	Vector3 transMousePos = { 0.0f, 0.0f, 0.0f };
	enum ButtonState prevState;

	/* see if the mouse is positioned over any buttons */
	SDL_GetMouseState( &mouseX, &mouseY );
	mousePos.x = (float)mouseX;
	mousePos.y = (float)mouseY;

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
			if( ( transMousePos.v[0] >= buttons[i].position.v[0] ) && ( transMousePos.v[0] <= ( buttons[i].position.v[0] + buttons[i].size.v[0] ) ) &&
				( transMousePos.v[1] >= buttons[i].position.v[1] ) && ( transMousePos.v[1] <= ( buttons[i].position.v[1] + buttons[i].size.v[1] ) ) ) {
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

void processButtonEvents( SDL_Event* sdlEvent )
{
	if( ( sdlEvent->type == SDL_MOUSEBUTTONDOWN ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 1;
	} else if( ( sdlEvent->type == SDL_MOUSEBUTTONUP ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 0;
	}
}