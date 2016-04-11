#include "IMGUI.h"

#include <stdbool.h>

#include "../Utils/stretchyBuffer.h"
#include "../Graphics/triRendering.h"
#include "../Graphics/imageSheets.h"
#include "../Graphics/images.h"
#include "../Graphics/debugRendering.h"
#include "../Input/input.h"
#include "../System/systems.h"
#include "../Math/mathUtil.h"
#include "../Graphics/scissor.h"

// TODO: Skinning and customization

typedef struct {
	Vector2 mousePos;
	Vector2 mouseDelta;
	bool mouseLeftDown;

	int hotItem;		// item currently hovered over
	int activeItem;		// item currently interacting with
	int lastActiveItem;	// which item was last active

	Vector2 relativeOffset;

	int currID;
} IMGUI_State;

static IMGUI_State state;

static int* sbButtonSliced = NULL;
static int* sbPanelSliced = NULL;
static int* sbSliderBGSliced = NULL;
static int* sbScrollSliced = NULL;
static int* sbWindowSliced = NULL;
static int sliderNubImg = -1;

static int font = -1;
static int checkBoxImg_Unchecked = -1;
static int checkBoxImg_Checked = -1;
static int radioButtonImg_Unchecked = -1;
static int radioButtonImg_Checked = -1;
static int windowExpandedButtonImg = -1;
static int windowCollapsedButtonImg = -1;
static int windowResizeButtonImg = -1;

static float vertScrollBarWidth = 0.0f;
static float horizScrollBarHeight = 0.0f;
static float fontHeight = 0.0f;

// adjust these an necessary, eventually put these into the skinning
static float windowHeaderHeight = 29.0f;
static float windowHeaderTopMargin = 5.0f;
static float windowHeaderBottomMargin = 5.0f;
static float windowHeaderLeftMargin = 5.0f;
static float windowHeaderRightMargin = 5.0f;
static Vector2 windowShowButtonSize = { 0.0f, 0.0f };
static Vector2 windowShowButtonOffset = { 0.0f, 0.0f }; // from the top-left offset by the margins
static Vector2 windowHeaderTextOffset = { 4.0f, 0.0f };

static float windowBodyTopMargin = 5.0f;
static float windowBodyBottomMargin = 5.0f;
static float windowBodyLeftMargin = 5.0f;
static float windowBodyRightMargin = 5.0f;
static Vector2 windowResizeButtonSize = { 0.0f, 0.0f };

float windowTotalMinWidth = 40.0f;
float windowTotalMinHeight = 70.0f;


static uint32_t* sbTextInputBuffer = NULL;

static const uint32_t LINE_FEED = 0xA;
static const uint32_t BACKSPACE = 0x8;

static bool isInRegion( const Vector2* testPos, const Vector2* upperLeft, const Vector2* size )
{
	Vector2 diff;
	vec2_Subtract( testPos, upperLeft, &diff );

	if( ( diff.x >= 0.0f ) && ( diff.y >= 0.0f ) && 
		( diff.x <= size->x ) && ( diff.y <= size->y ) ) {
		return true;
	}

	return false;
}

static bool isInClipping( const Vector2* testPos )
{
	// first make sure testPos is in the topmost clipping region
	Vector2 clipUL, clipSize;
	scissor_GetScissorArea( scissor_GetTopID( ), &clipUL, &clipSize );
	return isInRegion( testPos, &clipUL, &clipSize );
}

static bool isMouseInRegion( const Vector2* upperLeft, const Vector2* size )
{
	return ( isInClipping( &( state.mousePos ) ) && isInRegion( &( state.mousePos ), upperLeft, size ) );
}

static void drawSliced( int* slicedSheet, const Vector2* upperLeft, const Vector2* size, Color clr, uint32_t camFlags, int8_t depth )
{
	// assuming the order of the stored sliced images is
	//  0 1 2
	//  3 4 5
	//  6 7 8
	// and that the dimensions of each column and row are the same
	// so for heights: 0=1=2, 3=4=5, 6=7=8
	// for widths: 0=3=6, 1=4=7, 2=5=8
	// also assumes the width and height to draw greater than the sum of the widths and heights
	Vector2 sliceSize;
	img_GetSize( slicedSheet[0], &sliceSize );

	float imgLeftColumnWidth = sliceSize.x;
	float imgTopRowHeight = sliceSize.y;

	img_GetSize( slicedSheet[4], &sliceSize );

	float imgMidColumnWidth = sliceSize.x;
	float imgMidRowHeight = sliceSize.y;

	img_GetSize( slicedSheet[8], &sliceSize );

	float imgRightColumnWidth = sliceSize.x;
	float imgRottomRowHeight = sliceSize.y;

	// calculate the scales
	float midWidth = MAX( 0.0f, ( size->x - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthScale = midWidth / imgMidColumnWidth;

	float midHeight = MAX( 0.0f, ( size->y - imgTopRowHeight - imgRottomRowHeight ) );
	float midHeightScale = midHeight / imgMidRowHeight;

	// calculate the positions
	float leftPos = upperLeft->x + ( imgLeftColumnWidth / 2.0f );
	float midHorizPos = upperLeft->x + imgLeftColumnWidth + ( midWidth / 2.0f );
	float rightPos = upperLeft->x + size->x - ( imgRightColumnWidth / 2.0f );

	float topPos = upperLeft->y + ( imgTopRowHeight / 2.0f );
	float midVertPos = upperLeft->y + imgTopRowHeight + ( midHeight / 2.0f );
	float bottomPos = upperLeft->y + size->y - ( imgRottomRowHeight / 2.0f );

	Vector2 pos;
	Vector2 scale;

	pos.y = topPos;
	pos.x = leftPos;
	img_Draw_c( slicedSheet[0], camFlags, pos, pos, clr, clr, depth );

	pos.x = midHorizPos;
	scale.x = midWidthScale;
	scale.y = 1.0f;
	img_Draw_sv_c( slicedSheet[1], camFlags, pos, pos, scale, scale, clr, clr, depth );

	pos.x = rightPos;
	img_Draw_c( slicedSheet[2], camFlags, pos, pos, clr, clr, depth );


	pos.y = midVertPos;
	pos.x = leftPos;
	scale.x = 1.0f;
	scale.y = midHeightScale;
	img_Draw_sv_c( slicedSheet[3], camFlags, pos, pos, scale, scale, clr, clr, depth );

	pos.x = midHorizPos;
	scale.x = midWidthScale;
	scale.y = midHeightScale;
	img_Draw_sv_c( slicedSheet[4], camFlags, pos, pos, scale, scale, clr, clr, depth );

	pos.x = rightPos;
	scale.x = 1.0f;
	scale.y = midHeightScale;
	img_Draw_sv_c( slicedSheet[5], camFlags, pos, pos, scale, scale, clr, clr, depth );


	pos.y = bottomPos;
	pos.x = leftPos;
	img_Draw_c( slicedSheet[6], camFlags, pos, pos, clr, clr, depth );

	pos.x = midHorizPos;
	scale.x = midWidthScale;
	scale.y = 1.0f;
	img_Draw_sv_c( slicedSheet[7], camFlags, pos, pos, scale, scale, clr, clr, depth );

	pos.x = rightPos;
	img_Draw_c( slicedSheet[8], camFlags, pos, pos, clr, clr, depth );
}

static void leftMouseButtonDown( void )
{
	state.mouseLeftDown = true;
}

static void leftMouseButtonUp( void )
{
	state.mouseLeftDown = false;
}

int imgui_Init( void )
{
	sbButtonSliced = NULL;
	sbPanelSliced = NULL;
	sbSliderBGSliced = NULL;
	sbScrollSliced = NULL;
	sbWindowSliced = NULL;
	sliderNubImg = -1;
	font = -1;
	sbTextInputBuffer = NULL;
	checkBoxImg_Checked = -1;
	checkBoxImg_Unchecked = -1;
	radioButtonImg_Unchecked = -1;
	radioButtonImg_Checked = -1;
	windowResizeButtonImg = -1;
	windowCollapsedButtonImg = -1;
	windowExpandedButtonImg = -1;

	input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, leftMouseButtonDown );
	input_BindOnMouseButtonRelease( SDL_BUTTON_LEFT, leftMouseButtonUp );

	if( img_LoadSpriteSheet( "Images/Editor/panel.ss", ST_DEFAULT, &sbPanelSliced ) < 0 ) {
		goto fail_clean_up;
	}

	if( img_LoadSpriteSheet( "Images/Editor/button.ss", ST_DEFAULT, &sbButtonSliced ) < 0 ) {
		goto fail_clean_up;
	}

	if( img_LoadSpriteSheet( "Images/Editor/sliderBG.ss", ST_DEFAULT, &sbSliderBGSliced ) < 0 ) {
		goto fail_clean_up;
	}

	if( img_LoadSpriteSheet( "Images/Editor/scroll.ss", ST_DEFAULT, &sbScrollSliced ) < 0 ) {
		goto fail_clean_up;
	}

	if( img_LoadSpriteSheet( "Images/Editor/window.ss", ST_DEFAULT, &sbWindowSliced ) < 0 ) {
		goto fail_clean_up;
	}

	sliderNubImg = img_Load( "Images/Editor/grey_tickGrey.png", ST_DEFAULT );
	if( sliderNubImg < 0 ) {
		goto fail_clean_up;
	}

	checkBoxImg_Checked = img_Load( "Images/Editor/grey_boxCheckmark.png", ST_DEFAULT );
	if( checkBoxImg_Checked < 0 ) {
		goto fail_clean_up;
	}

	checkBoxImg_Unchecked = img_Load( "Images/Editor/grey_box.png", ST_DEFAULT );
	if( checkBoxImg_Unchecked < 0 ) {
		goto fail_clean_up;
	}

	radioButtonImg_Checked = img_Load( "Images/Editor/grey_boxTick.png", ST_DEFAULT );
	if( radioButtonImg_Checked < 0 ) {
		goto fail_clean_up;
	}

	radioButtonImg_Unchecked = img_Load( "Images/Editor/grey_circle.png", ST_DEFAULT );
	if( radioButtonImg_Unchecked < 0 ) {
		goto fail_clean_up;
	}

	windowExpandedButtonImg = img_Load( "Images/Editor/expandedButton.png", ST_DEFAULT );
	if( windowExpandedButtonImg < 0 ) {
		goto fail_clean_up;
	}

	windowCollapsedButtonImg = img_Load( "Images/Editor/collapsedButton.png", ST_DEFAULT );
	if( windowCollapsedButtonImg < 0 ) {
		goto fail_clean_up;
	}

	windowResizeButtonImg = img_Load( "Images/Editor/windowResize.png", ST_DEFAULT );
	if( windowResizeButtonImg < 0 ) {
		goto fail_clean_up;
	}

	txt_Init( ); // just in case it wasn't called before
	font = txt_LoadFont( "Fonts/Editor/kenpixel.ttf", 16 );
	fontHeight = 16.0f;
	if( font < 0 ) {
		goto fail_clean_up;
	}

	// calculate sizing stuff
	vertScrollBarWidth = 0.0f;
	horizScrollBarHeight = 0.0f;

	Vector2 sliceSize;
	img_GetSize( sbScrollSliced[0], &sliceSize );
	vertScrollBarWidth += sliceSize.w;
	horizScrollBarHeight += sliceSize.h;

	img_GetSize( sbScrollSliced[8], &sliceSize );
	vertScrollBarWidth += sliceSize.w;
	horizScrollBarHeight += sliceSize.h;

	img_GetSize( windowResizeButtonImg, &windowResizeButtonSize );

	Vector2 imgSize;
	img_GetSize( windowExpandedButtonImg, &windowShowButtonSize );
	img_GetSize( windowCollapsedButtonImg, &imgSize );
	windowShowButtonSize.w = MAX( windowShowButtonSize.w, imgSize.w );
	windowShowButtonSize.h = MAX( windowShowButtonSize.h, imgSize.h );


	// initialize state
	state.hotItem = 0;
	state.activeItem = -1;
	state.lastActiveItem = -1;
	state.mouseLeftDown = false;

	sb_Reserve( sbTextInputBuffer, 32 );

	return 0;

fail_clean_up:
	imgui_CleanUp( );
	return -1;
}

void imgui_CleanUp( void )
{
#define CLEAN_UP_IMG( i ) if( (i) >= 0 ) { img_Clean( i ); (i) = -1; }
#define CLEAN_UP_SS( s ) if( s != NULL ) { img_UnloadSpriteSheet( s ); s = NULL; }

	CLEAN_UP_IMG( checkBoxImg_Checked );
	CLEAN_UP_IMG( checkBoxImg_Unchecked );
	CLEAN_UP_IMG( sliderNubImg );
	CLEAN_UP_IMG( windowExpandedButtonImg );
	CLEAN_UP_IMG( windowCollapsedButtonImg );
	CLEAN_UP_IMG( windowResizeButtonImg );

	CLEAN_UP_SS( sbWindowSliced );
	CLEAN_UP_SS( sbSliderBGSliced );
	CLEAN_UP_SS( sbButtonSliced );
	CLEAN_UP_SS( sbPanelSliced );
	CLEAN_UP_SS( sbScrollSliced );

	if( font >= 0 ) {
		txt_UnloadFont( font );
		font = -1;
	}

	input_ClearMouseButtonReponse( leftMouseButtonDown );
	input_ClearMouseButtonReponse( leftMouseButtonUp );

#undef CLEAN_UP_SS
#undef CLEAN_UP_IMG
}

void imgui_Start( void )
{
	Vector2 newMousePos;
	input_GetMousePostion( &newMousePos );

	vec2_Subtract( &newMousePos, &( state.mousePos ), &( state.mouseDelta ) );
	state.mousePos = newMousePos;

	state.hotItem = 0;
	state.relativeOffset = VEC2_ZERO;
	state.currID = 1;
}

void imgui_End( void )
{
	if( state.activeItem > 0 ) {
		if( !state.mouseLeftDown && ( state.activeItem != state.lastActiveItem ) ) {
			SDL_Log( "...stopping text input." );
			SDL_StopTextInput( );
		}
		state.lastActiveItem = state.activeItem;
	}

	if( !state.mouseLeftDown ) {
		state.activeItem = 0;
	} else {
		if( state.activeItem == 0 ) {
			state.activeItem = -1;
		}
	}

	sb_Clear( sbTextInputBuffer );
}

void imgui_ProcessEvents( SDL_Event* e )
{
	// TODO: Proper support of multi-language input
	switch( e->type ) {
	case SDL_TEXTINPUT:
		if( sb_Count( sbTextInputBuffer ) < sb_Reserved( sbTextInputBuffer ) ) {
			sb_Push( sbTextInputBuffer, e->text.text[0] );
		}
		break;
	case SDL_KEYDOWN:
		// handle backspace
		if( e->key.keysym.sym == SDLK_BACKSPACE ) {
			sb_Push( sbTextInputBuffer, BACKSPACE );
		} else if( e->key.keysym.sym == SDLK_RETURN ) {
			sb_Push( sbTextInputBuffer, LINE_FEED );
		}
		break;
	}
}

void imgui_Panel( const Vector2* upperLeft, const Vector2* size, Color clr, uint32_t camFlags, int8_t depth )
{
	Vector2 adjPos;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjPos );
	drawSliced( sbPanelSliced, &adjPos, size, clr, camFlags, depth );
}

// just a wrapper around txt_DisplayString with a default font to be used
void imgui_Text( const char* utf8Str, const Vector2* pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign, int camFlags, char depth )
{
	Vector2 adjPos;
	vec2_Add( pos, &( state.relativeOffset ), &adjPos );
	txt_DisplayString( utf8Str, adjPos, clr, hAlign, vAlign, font, camFlags, depth );
}

static bool baseButton( int id, const Vector2* upperLeft, const Vector2* size, bool repeats )
{
	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );
	if( isMouseInRegion( &adjUpperLeft, size ) ) {
		state.hotItem = id;
		if( ( state.activeItem == -1 ) && state.mouseLeftDown ) {
			state.activeItem = id;
		}
	}

	if( repeats ) {
		if( state.mouseLeftDown && ( state.activeItem == id ) ) {
			return true;
		}
	} else {
		if( !state.mouseLeftDown && ( state.hotItem == id ) && ( state.activeItem == id ) ) {
			return true;
		}
	}

	return false;
}

// draws a button with some text displayed centered in it
//  the text can go out of bounds if it's too small for the size given
//  returns whether the button was clicked
bool imgui_Button( const Vector2* upperLeft, const Vector2* size,
	const char* text, Color normalTextClr, Color hotTextClr, Color activeTextClr,
	Color normalBGClr, Color hotBGClr, Color activeBGClr,
	bool repeats,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;
	bool ret = baseButton( id, upperLeft, size, repeats );

	Color textClr = normalTextClr;
	Color bgClr = normalBGClr;
	if( state.hotItem == id ) {
		textClr = hotTextClr;
		bgClr = hotBGClr;
		if( state.activeItem == id ) {
			textClr = activeTextClr;
			bgClr = activeBGClr;
		}
	}

	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );

	drawSliced( sbButtonSliced, &adjUpperLeft, size, bgClr, camFlags, depth );
	// just draw the text centered
	Vector2 center;
	vec2_AddScaled( &adjUpperLeft, size, 0.5f, &center );
	txt_DisplayTextArea( (uint8_t*)text, adjUpperLeft, *size, textClr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, font, camFlags, depth );

	return ret;
}

bool imgui_TextButton( const Vector2* upperLeft, const Vector2* size,
	const char* text, Color normalColor, Color hotColor, Color activeColor,
	HorizTextAlignment hAlign, VertTextAlignment vAlign,
	bool repeats,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;
	bool ret = baseButton( id, upperLeft, size, repeats );
	
	Color clr = normalColor;
	if( state.hotItem == id ) {
		clr = hotColor;
		if( state.activeItem == id ) {
			clr = activeColor;
		}
	}

	// draw the text based on the alignment
	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );

	Vector2 drawPos;

	switch( hAlign ) {
	case HORIZ_ALIGN_LEFT:
		drawPos.x = adjUpperLeft.x;
		break;
	case HORIZ_ALIGN_RIGHT:
		drawPos.x = adjUpperLeft.x + size->x;
		break;
	case HORIZ_ALIGN_CENTER:
	default:
		drawPos.x = adjUpperLeft.x + ( size->x * 0.5f );
		break;
	}

	switch( vAlign ) {
	case VERT_ALIGN_TOP:
		drawPos.y = adjUpperLeft.y;
		break;
	case VERT_ALIGN_BOTTOM:
		drawPos.y = adjUpperLeft.y + size->y;
		break;
	case VERT_ALIGN_CENTER:
	case VERT_ALIGN_BASE_LINE:
	default:
		drawPos.y = adjUpperLeft.y + ( size->y * 0.5f );
		break;
	}

	txt_DisplayString( text, drawPos, clr, hAlign, vAlign, font, camFlags, depth );

	//debugRenderer_AABB( camFlags, adjUpperLeft, *size, clr );

	return ret;
}

bool imgui_ImageButton( const Vector2* upperLeft, const Vector2* size,
	int normalImage, int hotImage, int activeImage,
	Color normalClr, Color hotClr, Color activeClr,
	bool repeats,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;
	bool ret = baseButton( id, upperLeft, size, repeats );

	int img = normalImage;
	Color clr = normalClr;
	if( state.hotItem == id ) {
		img = hotImage;
		clr = hotClr;
		if( state.activeItem == id ) {
			activeImage;
			clr = activeClr;
		}
	}

	Vector2 imgSize;
	img_GetSize( img, &imgSize );
	imgSize.x /= size->x;
	imgSize.y /= size->y;

	Vector2 renderPos;
	vec2_AddScaled( upperLeft, size, 0.5f, &renderPos );
	vec2_Add( &renderPos, &( state.relativeOffset ), &renderPos );

	img_Draw_sv_c( img, camFlags, renderPos, renderPos, imgSize, imgSize, clr, clr, depth );

	return ret;
}

bool imgui_Slider( const Vector2* upperLeft, const Vector2* size,
	bool vertical, float leftTopVal, float rightBotVal, float* value, float step,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;
	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );

	// calculate the size of the nub, for right now lets make it always 20
	Vector2 nubSize;
	if( vertical ) {
		nubSize.x = nubSize.y = size->x;
	} else {
		nubSize.x = nubSize.y = size->y;
	}

	// get relative position, used for updating the value and rendering the nub
	float relativePos;
	float minPos;
	float maxPos;
	if( vertical ) {
		minPos = adjUpperLeft.y + ( nubSize.y / 2.0f );
		maxPos = adjUpperLeft.y + size->y - ( nubSize.y / 2.0f );
		relativePos = inverseLerp( minPos, maxPos, state.mousePos.y );
	} else {
		minPos = adjUpperLeft.x + ( nubSize.x / 2.0f );
		maxPos = adjUpperLeft.x + size->x - ( nubSize.x / 2.0f );
		relativePos = inverseLerp( minPos, maxPos, state.mousePos.x );
	}

	// process input
	if( isMouseInRegion( &adjUpperLeft, size ) ) {
		state.hotItem = id;
		if( ( state.activeItem == 0 ) && state.mouseLeftDown ) {
			state.activeItem = id;
		}
	}

	// draw slider background
	drawSliced( sbSliderBGSliced, &adjUpperLeft, size, CLR_WHITE, camFlags, depth );

	// draw the slider
	Vector2 sliderPos;
	if( vertical ) {
		sliderPos.x = adjUpperLeft.x + ( size->x * 0.5f );
		sliderPos.y = lerp( minPos, maxPos, inverseLerp( leftTopVal, rightBotVal, *value ) );
	} else {
		sliderPos.y = adjUpperLeft.y + ( size->y * 0.5f );
		sliderPos.x = lerp( minPos, maxPos, inverseLerp( leftTopVal, rightBotVal, *value ) );
	}

	Vector2 nubImageSize;
	img_GetSize( sliderNubImg, &nubImageSize );
	nubSize.x /= nubImageSize.x;
	nubSize.y /= nubImageSize.y;

	if( state.activeItem == id ) {
		img_Draw_sv( sliderNubImg, camFlags, sliderPos, sliderPos, nubSize, nubSize, depth );
	} else if( state.hotItem == id ) {
		img_Draw_sv( sliderNubImg, camFlags, sliderPos, sliderPos, nubSize, nubSize, depth );
	} else {
		img_Draw_sv( sliderNubImg, camFlags, sliderPos, sliderPos, nubSize, nubSize, depth );
	}

	// update widget value
	if( state.activeItem == id ) {
		if( relativePos != (*value) ) {
			(*value) = lerp( leftTopVal, rightBotVal, relativePos );
			return true;
		}
	}

	return false;
}

void imgui_TextInput( const Vector2* upperLeft, const Vector2* size,
	uint8_t* inputBuffer, size_t inputBufferMaxSize,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;

	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );
	if( isMouseInRegion( &adjUpperLeft, size ) ) {
		state.hotItem = id;
		if( ( state.activeItem == 0 ) && ( state.mouseLeftDown ) ) {
			state.activeItem = id;
		}
	}

	// append all the characters to the string
	size_t len = SDL_strlen( (char*)inputBuffer );
	for( size_t i = 0; i < sb_Count( sbTextInputBuffer ); ++i ) {
		// TODO: Proper support of multi-language input
		if( sbTextInputBuffer[i] == BACKSPACE ) {
			if( len >= 1 ) {
				inputBuffer[len-1] = 0;
				--len;
			}
		} else if( len < inputBufferMaxSize ) {
			inputBuffer[len] = (uint8_t)sbTextInputBuffer[i];
			++len;
		}
	}
	
	if( state.lastActiveItem == id ) {
		debugRenderer_AABB( camFlags, adjUpperLeft, *size, CLR_RED );
	} else if( state.activeItem == id ) {
		debugRenderer_AABB( camFlags, adjUpperLeft, *size, CLR_CYAN );
		SDL_StartTextInput( );
	} else if( state.hotItem == id ) {
		debugRenderer_AABB( camFlags, adjUpperLeft, *size, CLR_MAGENTA );
	} else {
		debugRenderer_AABB( camFlags, adjUpperLeft, *size, CLR_BLUE );
	}

	txt_DisplayTextArea( inputBuffer, adjUpperLeft, *size, CLR_BLACK, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, font, camFlags, depth );
}

static bool toggleButton( int id, const Vector2* upperLeft, const Vector2* size,
	bool isChecked,
	int offImg, int onImg,
	Color normalClr, Color hotClr, Color activeClr,
	uint32_t camFlags, int8_t depth )
{
	Vector2 adjUpperLeft;
	vec2_Add( upperLeft, &( state.relativeOffset ), &adjUpperLeft );

	bool ret;
	if( isMouseInRegion( &adjUpperLeft, size ) ) {
		state.hotItem = id;
		if( ( state.activeItem == 0 ) && ( state.mouseLeftDown ) ) {
			state.activeItem = id;
		}
	}

	Color clr = normalClr;
	if( state.hotItem == id ) {
		if( state.activeItem == id ) {
			clr = activeClr;
		} else {
			clr = hotClr;
		}
	}

	if( !state.mouseLeftDown && ( state.hotItem == id ) && ( state.activeItem == id ) ) {
		ret = !isChecked;
	} else {
		ret = isChecked;
	}

	int img = isChecked ? onImg : offImg;

	Vector2 imgSize;
	Vector2 scale;

	img_GetSize( img, &imgSize );
	scale.x = size->x / imgSize.x;
	scale.y = size->y / imgSize.y;

	Vector2 drawPos;
	vec2_AddScaled( &adjUpperLeft, size, 0.5f, &drawPos );

	img_Draw_sv_c( img, camFlags, drawPos, drawPos, scale, scale, CLR_WHITE, CLR_WHITE, depth );

	return ret;
}

bool imgui_CheckBox( const Vector2* upperLeft, const Vector2* size,
	bool isChecked,
	Color normalClr, Color hotClr, Color activeClr,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;

	return toggleButton( id, upperLeft, size, isChecked,
		checkBoxImg_Unchecked, checkBoxImg_Checked,
		normalClr, hotClr, activeClr, camFlags, depth );
}

bool imgui_RadioButton( const Vector2* upperLeft, const Vector2* size,
	int* checked, int groupID,
	Color normalClr, Color hotClr, Color activeClr,
	uint32_t camFlags, int8_t depth )
{
	int id = state.currID++;

	bool pressed = toggleButton( id, upperLeft, size, ( (*checked) == groupID ),
		radioButtonImg_Unchecked, radioButtonImg_Checked,
		normalClr, hotClr, activeClr, camFlags, depth );

	if( pressed ) {
		(*checked) = groupID;
	}

	return pressed;
}

static void vertScrollBar( const Vector2* upperLeft, const Vector2* finalSize, float viewAreaHeight, float* scrollPosition,
	uint32_t camFlags, int8_t depth )
{
	// base bar
	Vector2 barTopLeft;
	barTopLeft.x = ( upperLeft->x + finalSize->w );
	barTopLeft.y = upperLeft->y;

	Vector2 barSize;
	barSize.w = vertScrollBarWidth;
	barSize.h = finalSize->h;

	drawSliced( sbScrollSliced, &barTopLeft, &barSize, CLR_WHITE, camFlags, depth );

	// nub
	Vector2 nubSize = barSize;
	nubSize.h = MAX( vertScrollBarWidth, barSize.h * ( finalSize->h / viewAreaHeight ) );

	Vector2 nubPos = barTopLeft;
	nubPos.y += (*scrollPosition) * ( barSize.h - nubSize.h );

	int id = state.currID++;
	baseButton( id, &nubPos, &nubSize, true );

	Color clr = CLR_LIGHT_GREY;
	if( state.activeItem == id ) {
		clr = CLR_DARK_GREY;
	} else if( state.hotItem == id ) {
		clr = CLR_GREY;
	}

	if( state.mouseLeftDown && ( state.activeItem == id ) ) {
		float topMostCenter = barTopLeft.y + ( nubSize.h / 2.0f );
		float bottomMostCenter = barTopLeft.y + barSize.h - ( nubSize.h / 2.0f );
		(*scrollPosition) += ( state.mouseDelta.y / ( bottomMostCenter - topMostCenter ) );
		(*scrollPosition) = clamp( 0.0f, 1.0f, (*scrollPosition) );
	}

	drawSliced( sbScrollSliced, &nubPos, &nubSize, clr, camFlags, depth );
}

static void horizScrollBar( const Vector2* upperLeft, const Vector2* finalSize, float viewAreaWidth, float* scrollPosition,
	uint32_t camFlags, int8_t depth )
{
	Vector2 barTopLeft;
	barTopLeft.x = upperLeft->x;
	barTopLeft.y = ( upperLeft->y + finalSize->h );

	Vector2 barSize;
	barSize.w = finalSize->w;
	barSize.h = horizScrollBarHeight;

	drawSliced( sbScrollSliced, &barTopLeft, &barSize, CLR_WHITE, camFlags, depth ); 

	// nub
	Vector2 nubSize = barSize;
	nubSize.w = MAX( horizScrollBarHeight, barSize.w * ( finalSize->w / viewAreaWidth ) );

	Vector2 nubPos = barTopLeft;
	nubPos.x += (*scrollPosition) * ( barSize.w - nubSize.w );

	int id = state.currID++;
	baseButton( id, &nubPos, &nubSize, true );
		
	Color clr = CLR_LIGHT_GREY;
	if( state.activeItem == id ) {
		clr = CLR_DARK_GREY;
	} else if( state.hotItem == id ) {
		clr = CLR_GREY;
	}

	if( state.mouseLeftDown && ( state.activeItem == id ) ) {
		// adjust the scroll position, use the center of the nub as the where want to position it
		float leftMostCenter = barTopLeft.x + ( nubSize.w / 2.0f );
		float rightMostCenter = barTopLeft.x + barSize.w - ( nubSize.w / 2.0f );
		(*scrollPosition) += ( state.mouseDelta.x / ( rightMostCenter - leftMostCenter ) );
		(*scrollPosition) = clamp( 0.0f, 1.0f, (*scrollPosition) );
	}

	drawSliced( sbScrollSliced, &nubPos, &nubSize, clr, camFlags, depth );
}

static void calculateClippingArea( const Vector2* displayAreaSize, const Vector2* viewAreaSize, 
	Vector2* outFinalSize, bool* outUseHorizScrollBar, bool* outUseVertScrollBar )
{
	bool useHorizScrollBar = ( viewAreaSize->w > displayAreaSize->w );
	bool useVertScrollBar = ( viewAreaSize->h > displayAreaSize->h );

	outFinalSize->w = displayAreaSize->w;
	outFinalSize->h = displayAreaSize->h;

	if( useHorizScrollBar ) {
		outFinalSize->h -= horizScrollBarHeight;
	}

	if( useVertScrollBar ) {
		outFinalSize->w -= vertScrollBarWidth;
	}

	(*outUseHorizScrollBar) = useHorizScrollBar;
	(*outUseVertScrollBar) = useVertScrollBar;
}

static void processClippingScrollBars( const Vector2* upperLeft, const Vector2* finalSize, const Vector2* viewAreaSize, Vector2* scrollPosition,
	bool useHorizScrollBar, bool useVertScrollBar, uint32_t camFlags, int8_t depth )
{
	if( useHorizScrollBar ) {
		horizScrollBar( upperLeft, finalSize, viewAreaSize->w, &( scrollPosition->x ), camFlags, depth );
	}

	if( useVertScrollBar ) {
		vertScrollBar( upperLeft, finalSize, viewAreaSize->h, &( scrollPosition->y ), camFlags, depth );
	}
}

static void setClippingArea( const Vector2* upperLeft, const Vector2* displayAreaSize, const Vector2* finalSize,
	const Vector2* viewAreaSize, const Vector2* scrollPosition )
{
	Vector2 clipMaxOffset;
	vec2_Subtract( viewAreaSize, displayAreaSize, &clipMaxOffset );
	clipMaxOffset.x = -MAX( clipMaxOffset.x, 0.0f );
	clipMaxOffset.y = -MAX( clipMaxOffset.y, 0.0f );

	Vector2 clipUpperLeft;
	vec2_HadamardProd( &clipMaxOffset, scrollPosition, &clipUpperLeft );
	vec2_Add( &clipUpperLeft, upperLeft, &clipUpperLeft );

	state.relativeOffset = clipUpperLeft;
	scissor_Push( upperLeft, finalSize );
}

static void endClippingArea( void )
{
	state.relativeOffset = VEC2_ZERO;
	scissor_Pop( );
}

// can adjust the upperLeft, size, and showing inside here
//#error a whole bunch of shit in here, too many explicitly defined numbers
void imgui_StartWindow( Vector2* upperLeft, Vector2* size, const uint8_t* title, bool* showing,
	const Vector2* viewAreaSize, Vector2* scrollPosition,
	uint32_t camFlags, int8_t depth )
{
	// clamp to a minimum size
	size->w = MAX( size->w, windowTotalMinWidth );
	size->h = MAX( size->h, windowTotalMinHeight );

	// the window will have a title, a body area, horizontal scroll bar, vertical scroll bar, and resize button
	//  the title will have a show/hide button

	// start with a simple window we can drag and drop around
	if( *showing ) {
		drawSliced( sbWindowSliced, upperLeft, size, CLR_WHITE, camFlags, depth );
	} else {
		Vector2 smallerSize;
		smallerSize.x = size->w;
		smallerSize.y = windowHeaderHeight;
		drawSliced( sbPanelSliced, upperLeft, &smallerSize, CLR_WHITE, camFlags, depth );
	}

	// draw and process the title bar
	int showButtonImg;
	if( *showing ) {
		showButtonImg = windowExpandedButtonImg;
	} else {
		showButtonImg = windowCollapsedButtonImg;
	}

	Vector2 showButtonPos = *upperLeft;
	vec2_Add( &showButtonPos, &windowShowButtonOffset, &showButtonPos );
	showButtonPos.x += windowHeaderLeftMargin;
	showButtonPos.y += windowHeaderTopMargin;
	Vector2 showButtonSize;
	img_GetSize( showButtonImg, &showButtonSize );

	bool showButtonPressed = imgui_ImageButton( &showButtonPos, &showButtonSize,
			showButtonImg, showButtonImg, showButtonImg, 
			CLR_WHITE, CLR_WHITE, CLR_WHITE, false, camFlags, depth );
	if( showButtonPressed ) {
		(*showing) = !(*showing);
	}

	Vector2 textUpperLeft = showButtonPos;
	textUpperLeft.x += windowShowButtonSize.w;
	textUpperLeft.y -= windowShowButtonOffset.y - ( fontHeight / 2.0f );
	vec2_Add( &textUpperLeft, &windowHeaderTextOffset, &textUpperLeft );
	Vector2 textSize;
	textSize.w = size->w - textUpperLeft.x - windowHeaderRightMargin;
	textSize.h = fontHeight;
	txt_DisplayTextArea( title, textUpperLeft, textSize, CLR_BLACK, HORIZ_ALIGN_LEFT, VERT_ALIGN_CENTER, font, camFlags, depth );

	Vector2 windowMoveUpperLeft = *upperLeft;
	Vector2 windowMoveSize = *size;
	windowMoveSize.h = windowHeaderHeight;
	int windowDragID = state.currID++;
	if( baseButton( windowDragID, &windowMoveUpperLeft, &windowMoveSize, true ) ) {
		vec2_Add( upperLeft, &( state.mouseDelta ), upperLeft );
	}
	
	if( *showing ) {

		// set the region
		Vector2 regionUL = { 4.0f, 29.0f };
		Vector2 regionSize = *size;

		vec2_Add( &regionUL, upperLeft, &regionUL );
		regionSize.w -= 10.0f;
		regionSize.h -= 35.0f;

		Vector2 finalSize;
		bool useHorizScrollBar;
		bool useVertScrollBar;
		calculateClippingArea( &regionSize, viewAreaSize, &finalSize, &useHorizScrollBar, &useVertScrollBar );
		processClippingScrollBars( &regionUL, &finalSize, viewAreaSize, scrollPosition, useHorizScrollBar, useVertScrollBar, camFlags, depth );

		Vector2 windowResizeUpperLeft;
		windowResizeUpperLeft.x = upperLeft->x + size->w - 5.0f - windowResizeButtonSize.w;
		windowResizeUpperLeft.y = upperLeft->y + size->h - 5.0f - windowResizeButtonSize.h;
		if( imgui_ImageButton( &windowResizeUpperLeft, &windowResizeButtonSize, windowResizeButtonImg, windowResizeButtonImg, windowResizeButtonImg,
				CLR_WHITE, CLR_WHITE, CLR_WHITE, true, camFlags, depth ) ) {
			vec2_Add( &( state.mouseDelta ), size, size );
		}

		setClippingArea( &regionUL, &regionSize, &finalSize, viewAreaSize, scrollPosition );
	} else {
		// set the clipping region to zero
		scissor_Push( &VEC2_ZERO, &VEC2_ZERO );
	}
}

void imgui_EndWindow( void )
{
	endClippingArea( );
}

// clipping regions w/ scrollbars
//  everything rendered between these two commands are local to the region
// NOTE: Regions do NOT nest, doing a start region will override the
//  the current region.
// TODO: Make regions nest.
void imgui_StartRegion( const Vector2* upperLeft, const Vector2* size, const Vector2* viewAreaSize, Vector2* scrollPosition,
	uint32_t camFlags, int8_t depth )
{
	Vector2 finalSize;
	bool useHorizScrollBar;
	bool useVertScrollBar;
	calculateClippingArea( size, viewAreaSize, &finalSize, &useHorizScrollBar, &useVertScrollBar );
	processClippingScrollBars( upperLeft, &finalSize, viewAreaSize, scrollPosition, useHorizScrollBar, useVertScrollBar, camFlags, depth );
	setClippingArea( upperLeft, size, &finalSize, viewAreaSize, scrollPosition );
}

void imgui_EndRegion( void )
{
	endClippingArea( );
}