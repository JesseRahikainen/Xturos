#include "checkBox.h"

#include <math.h>

#include "../Graphics/images.h"
#include "text.h"
#include "../Math/vector3.h"
#include "../Math/matrix4.h"
#include "../Graphics/camera.h"

#define MAX_CHECK_BOXES 32
#define TEXT_LEN 32

static enum CheckBoxState { CBS_NORMAL, CBS_FOCUSED, CBS_CLICKED };

typedef struct {
	int inUse;

	int normalImgId;
	int focusImgId;
	int checkMarkImgId;

	char text[TEXT_LEN+1];
	int fontID;

	char depth;

	CheckBoxResponse response;
	enum CheckBoxState state;

	int isChecked;

	Vector2 position;
	Vector2 halfSize;
	int camFlags;
} CheckBox;

static CheckBox checkBoxes[MAX_CHECK_BOXES];
static int mouseDown;

/* Call this before trying to use any check boxes. */
void chkBox_Init( )
{
	memset( checkBoxes, 0, sizeof( checkBoxes ) );
	mouseDown = 0;
}

/*
Creates a button. All image ids must be valid.
*/
int chkBox_Create( Vector2 position, Vector2 size, const char* text, int fontID, int normalImg, int focusedImg, int checkMarkImg,
	unsigned int camFlags, char depth, CheckBoxResponse response )
{
	int newIdx = 0;
	while( ( newIdx < MAX_CHECK_BOXES ) && ( checkBoxes[newIdx].inUse == 1 ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_CHECK_BOXES ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_SYSTEM, "Unable to create new check box, no open slots." );
		return -1;
	}

	vec2_Scale( &size, 0.5f, &( checkBoxes[newIdx].halfSize ) );
	checkBoxes[newIdx].position = position;
	
	checkBoxes[newIdx].normalImgId = normalImg;
	checkBoxes[newIdx].focusImgId = focusedImg;
	checkBoxes[newIdx].checkMarkImgId = checkMarkImg;
	checkBoxes[newIdx].depth = depth;
	checkBoxes[newIdx].inUse = 1;
	checkBoxes[newIdx].response = response;
	checkBoxes[newIdx].state = CBS_NORMAL;
	checkBoxes[newIdx].camFlags = camFlags;
	checkBoxes[newIdx].fontID = fontID;

	if( text != NULL ) {
		strncpy( checkBoxes[newIdx].text, text, TEXT_LEN );
		checkBoxes[newIdx].text[TEXT_LEN] = 0;
	} else {
		memset( checkBoxes[newIdx].text, 0, sizeof( checkBoxes[newIdx].text ) );
	}

	checkBoxes[newIdx].inUse = 1;

	return newIdx;
}

void chkBox_Destroy( int idx )
{
	if( ( idx < 0 ) || ( idx >= MAX_CHECK_BOXES ) ) {
		return;
	}
	checkBoxes[idx].inUse = 0;
}

void chkBox_DestroyAll( void )
{
	memset( checkBoxes, 0, sizeof( checkBoxes ) );
}

int chkBox_IsChecked( int id )
{
	if( ( id < 0 ) || ( id >= MAX_CHECK_BOXES ) ) {
		return -1;
	}

	if( !checkBoxes[id].inUse ) {
		return -1;
	}

	return checkBoxes[id].inUse;
}

void chkBox_Draw( void )
{
	for( int i = 0; i < MAX_CHECK_BOXES; ++i ) {
		if( !checkBoxes[i].inUse ) {
			continue;
		}

		int img = -1;
		switch( checkBoxes[i].state ) {
		case CBS_NORMAL:
			img = checkBoxes[i].normalImgId;
			break;
		case CBS_FOCUSED:
		case CBS_CLICKED:
			img = checkBoxes[i].focusImgId;
			break;
		}
		img_Draw( img, checkBoxes[i].camFlags, checkBoxes[i].position, checkBoxes[i].position, checkBoxes[i].depth );

		if( checkBoxes[i].isChecked ) {
			img_Draw( checkBoxes[i].checkMarkImgId, checkBoxes[i].camFlags, checkBoxes[i].position, checkBoxes[i].position, checkBoxes[i].depth );
		}

		if( ( checkBoxes[i].text[0] != 0 ) && ( checkBoxes[i].fontID >= 0 ) ) {
			Vector2 textPos = checkBoxes[i].position;
			textPos.x += checkBoxes[i].halfSize.x;
			txt_DisplayString( checkBoxes[i].text, textPos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER,
								checkBoxes[i].fontID, checkBoxes[i].camFlags, checkBoxes[i].depth );
		}
	}
}

void chkBox_Process( void )
{
	int i;
	int mouseX, mouseY;
	Vector3 mousePos = { 0.0f, 0.0f, 0.0f };
	Vector3 transMousePos = { 0.0f, 0.0f, 0.0f };
	Vector2 diff;
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

		for( i = 0; i < MAX_CHECK_BOXES; ++i ) {
			if( checkBoxes[i].inUse == 0 ) {
				continue;
			}

			if( ( checkBoxes[i].camFlags & camFlags ) == 0 ) {
				continue;
			}

			/* see if mouse pos is within the buttons borders */
			prevState = checkBoxes[i].state;
			diff.x = checkBoxes[i].position.x - transMousePos.x;
			diff.y = checkBoxes[i].position.y - transMousePos.y;
			if( ( fabsf( diff.x ) <= checkBoxes[i].halfSize.x ) && ( fabsf( diff.y ) <= checkBoxes[i].halfSize.y ) ) {
				if( checkBoxes[i].state != CBS_CLICKED ) {
					checkBoxes[i].state = CBS_FOCUSED;
				}
			} else {
				checkBoxes[i].state = CBS_NORMAL;
			}

			if( ( checkBoxes[i].state == CBS_CLICKED ) && !mouseDown ) {
				checkBoxes[i].state = CBS_FOCUSED;
			} else if( ( checkBoxes[i].state == CBS_FOCUSED ) && mouseDown ) {
				checkBoxes[i].state = CBS_CLICKED;
				checkBoxes[i].isChecked = !checkBoxes[i].isChecked;
				if( checkBoxes[i].response != NULL ) {
					checkBoxes[i].response( checkBoxes[i].isChecked );
				}
			}
		}
	}
}

void chkBox_ProcessEvents( SDL_Event* sdlEvent )
{
	if( ( sdlEvent->type == SDL_MOUSEBUTTONDOWN ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 1;
	} else if( ( sdlEvent->type == SDL_MOUSEBUTTONUP ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 0;
	}
}