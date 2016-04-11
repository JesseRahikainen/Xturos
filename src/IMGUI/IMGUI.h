#ifndef IMGUI_H
#define IMGUI_H

#include <stdint.h>
#include <stdbool.h>

#include <SDL_events.h>

#include "../Math/vector2.h"
#include "../Graphics/color.h"
#include "../UI/text.h"

// generate consistent widget ids
// LINE
// BASE
// INTERNAL
// INDEX
// could also auto generate based on call order
#ifdef IMGUI_BASE_ID
#define IMGUI_ID ( (int)( IMGUI_BASE_ID << 16 ) & (int)( __LINE__ ) )
#else
#define IMGUI_ID ( (uint32_t)( __LINE__ ) )
#define IMGUI_ID_IDX( i )( ( ( ( 0x7FFF & (uint32_t)__LINE__ ) ) << 16 ) & ( ( (uint32_t)i ) & 0xFFFF ) )
#endif

// simple little immediate mode GUI, useful for quickly throwing stuff together

/*
Loads up the assets that will be used.
*/
int imgui_Init( void );
void imgui_CleanUp( void );

void imgui_ProcessEvents( SDL_Event* e );

// call this before starting the rendering of the ui for a frame
void imgui_Start( void );

// call this after doing rendering the ui for a frame
void imgui_End( void );

// draws a simple panel
void imgui_Panel( const Vector2* upperLeft, const Vector2* size, Color clr, uint32_t camFlags, int8_t depth );

// just a wrapper around txt_DisplayString with a default font to be used
void imgui_Text( const char* utf8Str, const Vector2* pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign, int camFlags, char depth );

// draws a button with some text displayed centered in it
//  the text can go out of bounds if it's too small for the size given
//  returns whether the button was clicked
// TODO: Repeating functionality for the buttons, to allow click and drag sort of stuff
// TODO: Be able to pass in a 3x3 image for the panel and base button
// TODO: Separate position, size, and collision, eventually
bool imgui_Button( const Vector2* upperLeft, const Vector2* size,
	const char* text, Color normalTextClr, Color hotTextClr, Color activeTextClr,
	Color normalBGClr, Color hotBGClr, Color activeBGClr,
	bool repeats,
	uint32_t camFlags, int8_t depth );

// draws a button without the background, just the text
bool imgui_TextButton( const Vector2* upperLeft, const Vector2* size,
	const char* text, Color normalColor, Color hotColor, Color activeColor,
	HorizTextAlignment hAlign, VertTextAlignment vAlign,
	bool repeats,
	uint32_t camFlags, int8_t depth );

// draws an image that acts as a button
bool imgui_ImageButton( const Vector2* upperLeft, const Vector2* size,
	int normalImage, int hotImage, int activeImage,
	Color normalClr, Color hotClr, Color activeClr,
	bool repeats,
	uint32_t camFlags, int8_t depth );

// draws a slider, the min will always be at the top/left and the max will always be at the bottom/right
bool imgui_Slider( const Vector2* upperLeft, const Vector2* size,
	bool vertical, float leftTopVal, float rightBotVal, float* value, float step,
	uint32_t camFlags, int8_t depth );

// text input
void imgui_TextInput( const Vector2* upperLeft, const Vector2* size,
	uint8_t* inputBuffer, size_t inputBufferSize,
	uint32_t camFlags, int8_t depth );

// check box
bool imgui_CheckBox( const Vector2* upperLeft, const Vector2* size,
	bool isChecked,
	Color normalClr, Color hotClr, Color activeClr,
	uint32_t camFlags, int8_t depth );

// radio button
bool imgui_RadioButton( const Vector2* upperLeft, const Vector2* size,
	int* checked, int groupID,
	Color normalClr, Color hotClr, Color activeClr,
	uint32_t camFlags, int8_t depth );

// window
void imgui_StartWindow( Vector2* upperLeft, Vector2* size, const uint8_t* title, bool* showing,
	const Vector2* viewAreaSize, Vector2* scrollPosition,
	uint32_t camFlags, int8_t depth );
void imgui_EndWindow( );

// clipping regions w/ scrollbars
//  everything rendered between these two commands are local to the region
void imgui_StartRegion( const Vector2* upperLeft, const Vector2* size, const Vector2* viewAreaSize, Vector2* scrollPosition,
	uint32_t camFlags, int8_t depth );
void imgui_EndRegion(  );

// TODO:
//  tooltips
//  menus

#endif /* inclusion guard */

// http://docs.unity3d.com/Manual/GUIScriptingGuide.html