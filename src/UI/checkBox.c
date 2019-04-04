#include "checkBox.h"

#include <string.h>
#include <math.h>

#include "../Graphics/images.h"
#include "text.h"
#include "../Math/vector3.h"
#include "../Math/matrix4.h"
#include "../Graphics/camera.h"
#include "../Input/input.h"
#include "../System/platformLog.h"
#include "../System/systems.h"

#define MAX_CHECK_BOXES 32
#define TEXT_LEN 32

enum CheckBoxState { CBS_NORMAL, CBS_FOCUSED, CBS_CLICKED };

typedef struct {
	int inUse;

	int normalImgId;
	int checkMarkImgId;

	char text[TEXT_LEN+1];
	int fontID;

	char depth;

	CheckBoxResponse response;
	enum CheckBoxState state;

	bool isChecked;

	Vector2 position;
	Vector2 collisionHalfSize;
	int camFlags;

	Vector2 drawScale;
} CheckBox;

static CheckBox checkBoxes[MAX_CHECK_BOXES];
static int mouseDown;

static int checkBoxSystem;

static void draw( void )
{
	for( int i = 0; i < MAX_CHECK_BOXES; ++i ) {
		if( !checkBoxes[i].inUse ) {
			continue;
		}

		if( checkBoxes[i].isChecked ) {
			img_Draw_sv( checkBoxes[i].checkMarkImgId, checkBoxes[i].camFlags,
				checkBoxes[i].position, checkBoxes[i].position,
				checkBoxes[i].drawScale, checkBoxes[i].drawScale, checkBoxes[i].depth );
		} else {
			img_Draw_sv( checkBoxes[i].normalImgId, checkBoxes[i].camFlags,
				checkBoxes[i].position, checkBoxes[i].position,
				checkBoxes[i].drawScale, checkBoxes[i].drawScale, checkBoxes[i].depth );
		}

		if( ( checkBoxes[i].text[0] != 0 ) && ( checkBoxes[i].fontID >= 0 ) ) {
			Vector2 textPos = checkBoxes[i].position;
			textPos.x += checkBoxes[i].collisionHalfSize.x;
			txt_DisplayString( checkBoxes[i].text, textPos, CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER,
								checkBoxes[i].fontID, checkBoxes[i].camFlags, checkBoxes[i].depth );
		}
	}
}

static void process( void )
{
	int i;
	Vector3 mousePos;
	Vector3 transMousePos = { 0.0f, 0.0f, 0.0f };
	Vector2 diff;
	enum CheckBoxState prevState;

	/* see if the mouse is positioned over any buttons */
	Vector2 mouse2DPos;
	if( !input_GetMousePosition( &mouse2DPos ) ) {
		return;
	}
	vec2ToVec3( &mouse2DPos, 0.0f, &mousePos );

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
			if( ( fabsf( diff.x ) <= checkBoxes[i].collisionHalfSize.x ) && ( fabsf( diff.y ) <= checkBoxes[i].collisionHalfSize.y ) ) {
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

static void processEvents( SDL_Event* sdlEvent )
{
	if( ( sdlEvent->type == SDL_MOUSEBUTTONDOWN ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 1;
	} else if( ( sdlEvent->type == SDL_MOUSEBUTTONUP ) && ( sdlEvent->button.button == SDL_BUTTON_LEFT ) ) {
		mouseDown = 0;
	}
}

/* Call this before trying to use any check boxes. */
void chkBox_Init( )
{
	memset( checkBoxes, 0, sizeof( checkBoxes ) );
	mouseDown = 0;

	checkBoxSystem = sys_Register( processEvents, process, draw, NULL );
}

void chkBox_CleanUp( )
{
	chkBox_DestroyAll( );
	sys_UnRegister( checkBoxSystem );
}

/*
Creates a button. All image ids must be valid.
*/
int chkBox_Create( Vector2 position, Vector2 size, const char* text, int fontID, int normalImg, int checkMarkImg,
	unsigned int camFlags, char depth, CheckBoxResponse response )
{
	int newIdx = 0;
	while( ( newIdx < MAX_CHECK_BOXES ) && ( checkBoxes[newIdx].inUse == 1 ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_CHECK_BOXES ) {
		llog( LOG_WARN, "Unable to create new check box, no open slots." );
		return -1;
	}

	vec2_Scale( &size, 0.5f, &( checkBoxes[newIdx].collisionHalfSize ) );
	checkBoxes[newIdx].position = position;
	
	checkBoxes[newIdx].normalImgId = normalImg;
	checkBoxes[newIdx].checkMarkImgId = checkMarkImg;
	checkBoxes[newIdx].depth = depth;
	checkBoxes[newIdx].inUse = 1;
	checkBoxes[newIdx].response = response;
	checkBoxes[newIdx].state = CBS_NORMAL;
	checkBoxes[newIdx].camFlags = camFlags;
	checkBoxes[newIdx].fontID = fontID;

	Vector2 imgSize;
	img_GetSize( normalImg, &imgSize );
	checkBoxes[newIdx].drawScale.x = size.x / imgSize.x; 
	checkBoxes[newIdx].drawScale.y = size.y / imgSize.y;

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

bool chkBox_IsChecked( int id )
{
	if( ( id < 0 ) || ( id >= MAX_CHECK_BOXES ) ) {
		return -1;
	}

	if( !checkBoxes[id].inUse ) {
		return -1;
	}

	return checkBoxes[id].isChecked;
}

void chkBox_SetChecked( int id, bool val, bool respond )
{
	if( ( id < 0 ) || ( id >= MAX_CHECK_BOXES ) ) return;
	if( !checkBoxes[id].inUse ) return;

	checkBoxes[id].isChecked = val;
	if( respond && ( checkBoxes[id].response != NULL ) ) {
		checkBoxes[id].response( val );
	}
}