#include "editorHub.h"

#include <stdarg.h>

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "IMGUI/nuklearWrapper.h"
#include "System/platformLog.h"
#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"

#include "spriteAnimationEditor.h"
#include "spriteSheetEditor.h"

// dialogs, useful for displaying information

#define MAX_DIALOG_BUTTONS 3

static struct nk_image dialogTypeIcons[DIALOG_TYPE_COUNT];
static bool dialogTypeIconsLoaded = false;

typedef struct {
	bool active;
	const char* text;
	void ( *onPress )( void );
} DialogButton;

typedef struct {
	const char* title;
	const char* text;
	DialogType type;
	DialogButton buttons[MAX_DIALOG_BUTTONS];
} Dialog;

static Dialog* sbDialogs = NULL;

static void loadDialogIconImages( void )
{
	if( dialogTypeIconsLoaded ) return;

	dialogTypeIcons[DT_MESSAGE] = nk_xu_loadImage( "Images/uiicons/information.png", NULL, NULL );
	dialogTypeIcons[DT_CHOICE] = nk_xu_loadImage( "Images/uiicons/question.png", NULL, NULL );
	dialogTypeIcons[DT_WARNING] = nk_xu_loadImage( "Images/uiicons/warning.png", NULL, NULL );
	dialogTypeIcons[DT_ERROR] = nk_xu_loadImage( "Images/uiicons/error.png", NULL, NULL );

	dialogTypeIconsLoaded = true;
}

static float calculateNKTextHeight( struct nk_context* ctx, float width, const char* str, int* numLines )
{
	float lineWidth;
	float height = ctx->style.text.padding.y;
	int fitting = 0;
	int done = 0;
	int glyphs = 0;
	nk_rune seperator[] = { ' ' };
	int strLen = nk_strlen( str );
	if( numLines ) ( *numLines ) = 1;

	width = width - 2 * ctx->style.text.padding.x;

	fitting = nk_text_clamp( ctx->style.font, str, strLen, width, &glyphs, &lineWidth, seperator, NK_LEN( seperator ) );
	while( done < strLen ) {
		if( numLines ) ++( *numLines );
		done += fitting;
		height += ctx->style.font->height + 2 * ctx->style.text.padding.y;
		fitting = nk_text_clamp( ctx->style.font, &str[done], strLen - done, width, &glyphs, &lineWidth, seperator, NK_LEN( seperator ) );
	}

	height += 0.01f; // fixes some issues with the height being just under the actual rendered height

	return height;
}

static void processDialogs( struct nk_context* ctx )
{
	loadDialogIconImages( );

	int width, height;
	gfx_GetRenderSize( &width, &height );

	const float DIALOG_WIDTH = 600.0f;
	const float DIALOG_HEIGHT = 500.0f;
	float xPos = ( width / 2.0f ) - ( DIALOG_WIDTH / 2.0f );
	float yPos = ( height / 2.0f ) - ( DIALOG_HEIGHT / 2.0f );
	const float ICON_SIZE = 50.0f;
	const float ICON_PADDING = 20.0f;
	const float BUTTON_GROUP_PADDING = 15.0f;
	const float BUTTON_PADDING = 15.0f;
	const float BUTTON_WIDTH = 120.0f;
	const float BUTTON_HEIGHT = 35.0f;

	float textWidth = DIALOG_WIDTH - ( ctx->style.window.padding.x * 2 ) - ICON_SIZE - ( ICON_PADDING * 2.0f );

	struct nk_rect dialogBounds = nk_rect( xPos, yPos, DIALOG_WIDTH, DIALOG_HEIGHT );
	for( size_t i = 0; i < sb_Count( sbDialogs ); ++i ) {
		char dialogID[32];
		SDL_snprintf( dialogID, 31, "Dialog%i", (int)i );
		bool destroyDialog = false;

		// calculate the height of the text and get the bounds height based on that
		float textHeight = calculateNKTextHeight( ctx, textWidth, sbDialogs[i].text, NULL );

		// header size
		dialogBounds.h = ctx->style.font->height + 2.0f * ctx->style.window.header.padding.y;
		dialogBounds.h += 2.0f * ctx->style.window.header.label_padding.y;

		// internal padding
		dialogBounds.h += ctx->style.window.padding.y * 2.0f;

		// contents height
		dialogBounds.h += MAX( ICON_SIZE, textHeight ) + ( BUTTON_GROUP_PADDING * 2 ) + BUTTON_HEIGHT;

		nk_begin_titled( ctx, dialogID, sbDialogs[i].title, dialogBounds, NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ); {

			float iconY = MAX( 0.0f, ( textHeight / 2.0f ) - ( ICON_SIZE / 2.0f ) );
			nk_layout_space_begin( ctx, NK_STATIC, 50, INT_MAX );
			nk_layout_space_push( ctx, nk_rect( ICON_PADDING, iconY, ICON_SIZE, ICON_SIZE ) );
			nk_image( ctx, dialogTypeIcons[sbDialogs[i].type] );

			float textY = MAX( 0.0f, ( ICON_SIZE / 2.0f ) - ( textHeight / 2.0f ) );
			nk_layout_space_push( ctx, nk_rect( ICON_SIZE + ( ICON_PADDING * 2.0f ), textY, textWidth, textHeight ) );
			nk_label_wrap( ctx, sbDialogs[i].text );

			// figure out how many buttons to show
			int buttonCnt = 0;
			for( int a = 0; a < MAX_DIALOG_BUTTONS; ++a ) {
				if( sbDialogs[i].buttons[a].active ) ++buttonCnt;
			}

			float buttonTop = MAX( iconY + ICON_SIZE, textY + textHeight ) + BUTTON_GROUP_PADDING;
			float buttonsLeft = ( DIALOG_WIDTH / 2.0f ) - ( ( ( BUTTON_WIDTH * buttonCnt ) - ( BUTTON_PADDING * ( buttonCnt - 1 ) ) ) / 2.0f );
			for( int a = 0; a < MAX_DIALOG_BUTTONS; ++a ) {
				if( sbDialogs[i].buttons[a].active ) {
					nk_layout_space_push( ctx, nk_rect( buttonsLeft + ( ( BUTTON_WIDTH + BUTTON_PADDING ) * a ), buttonTop, BUTTON_WIDTH, BUTTON_HEIGHT ) );
					if( nk_button_label( ctx, sbDialogs[i].buttons[a].text ) ) {
						if( sbDialogs[i].buttons[a].onPress != NULL ) sbDialogs[i].buttons[a].onPress( );
						destroyDialog = true;
					}
				}
			}

		} nk_end( ctx );

		if( destroyDialog ) {
			sb_Remove( sbDialogs, i );
			--i;
		}
	}
}

typedef void ( *DialogButtonHandler )( void );

// parameters are assumed to be const char*, void (*handler)(void) pairs
void hub_CreateDialog( const char* title, const char* text, DialogType type, int numButtons, ... )
{
	ASSERT( numButtons > 0 );
	ASSERT( numButtons <= MAX_DIALOG_BUTTONS );

	Dialog* newDialog = sb_Add( sbDialogs, 1 );
	newDialog->title = title;
	newDialog->text = text;
	newDialog->type = type;

	va_list varList;
	va_start( varList, numButtons );
	for( int i = 0; i < MAX_DIALOG_BUTTONS; ++i ) {
		if( i < numButtons ) {
			newDialog->buttons[i].active = true;
			newDialog->buttons[i].text = va_arg( varList, const char* );
			newDialog->buttons[i].onPress = va_arg( varList, DialogButtonHandler );
		} else {
			newDialog->buttons[i].active = false;
		}
	}
	va_end( varList );
}

static void testMessageDialogResponse( void )
{
	llog( LOG_DEBUG, "Close message dialog." );
}

static void testMessageDialog( void )
{
	//hub_CreateDialog( "Test Message", "This is a test message.", DT_MESSAGE, 1, "OK", testMessageDialogResponse );/*
	hub_CreateDialog( "Test Message",
		"This is a test message with lots of text to make sure things size correctly or at least test to see how it's handled. "
		"Even more text and even more text and even more text and even more text and even more text and even more text and even more text and even more text.",
		DT_MESSAGE, 1, "OK", testMessageDialogResponse );//*/
}

static void testChoiceYes( void )
{
	llog( LOG_DEBUG, "Yes." );
}

static void testChoiceNo( void )
{
	llog( LOG_DEBUG, "No." );
}

static void testChoiceCancel( void )
{
	llog( LOG_DEBUG, "Cancel." );
}

static void testChoiceDialog( void )
{
	hub_CreateDialog( "Test Choice",
		"Do you want to do the thing?",
		DT_CHOICE, 3,
		"Yes", testChoiceYes,
		"No", testChoiceNo,
		"Cancel", testChoiceCancel );
}

// end dialogs

typedef struct {
	const char* name;
	void ( *show )( void );
	void ( *hide )( void );
	void ( *process )( void );
	void ( *tick )( float );
	void ( *processEvents )( SDL_Event* e );
	
	bool isShown;
} Editor;

static Editor* sbEditors = NULL;

static void registerEditor( const char* name, void (*show)(void), void (*hide)(void), void (*process)(void), void (*tick)(float), void (*processEvents)(SDL_Event*) )
{
	Editor newEditor;
	newEditor.name = name;
	newEditor.show = show;
	newEditor.hide = hide;
	newEditor.process = process;
	newEditor.tick = tick;
	newEditor.processEvents = processEvents;

	newEditor.isShown = false;

	sb_Push( sbEditors, newEditor );
}

void hub_RegisterAllEditors( void )
{
	registerEditor( "Sprite Animation", spriteAnimationEditor_Show, spriteAnimationEditor_Hide, spriteAnimationEditor_IMGUIProcess, spriteAnimationEditor_Tick, spriteAnimationEditor_ProcessEvents );
	registerEditor( "Sprite Sheets", spriteSheetEditor_Show, spriteSheetEditor_Hide, spriteSheetEditor_IMGUIProcess, spriteSheetEditor_Tick, NULL );

	// testing stuff
	//registerEditor( "Test Info Dialog", testMessageDialog, NULL, NULL, NULL );
	//registerEditor( "Test Choice Dialog", testChoiceDialog, NULL, NULL, NULL );
}

void hub_InitAllEditors( )
{
	spriteAnimationEditor_Init( );
	spriteSheetEditor_Init( );
}

static void toggleEditor( size_t idx )
{
	ASSERT( idx < sb_Count( sbEditors ) );

	if( sbEditors[idx].isShown ) {
		if( sbEditors[idx].hide != NULL ) sbEditors[idx].hide( );
		sbEditors[idx].isShown = false;
	} else {
		if( sbEditors[idx].show != NULL ) sbEditors[idx].show( );
		sbEditors[idx].isShown = true;
	}
}

static void editorHubScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	gfx_SetClearColor( CLR_BLACK );

	hub_InitAllEditors( );
	hub_RegisterAllEditors( );
}

static void editorHubScreen_Exit( void )
{
	sb_Release( sbEditors );
}

static void editorHubScreen_ProcessEvents( SDL_Event* e )
{
	for( size_t i = 0; i < sb_Count( sbEditors ); ++i ) {
		if( !sbEditors[i].isShown ) continue;
		if( sbEditors[i].processEvents != NULL ) sbEditors[i].processEvents( e );
	}
}

static void editorHubScreen_Process( void )
{
	int width, height;
	gfx_GetWindowSize( &width, &height );

	struct nk_context* ctx = &( editorIMGUI.ctx );
	const float BUTTON_HEIGHT = 37.0f;
	float menuHeight = 85.0f + ( sb_Count( sbEditors ) * 44.0f );
	menuHeight = MIN( height / 2.0f, menuHeight );
	float buttonWidth = 204.0f;
	float menuWidth = 228.0f; // TODO: Figure out how to calculate all the sizes so things can fit better
	
	if( nk_begin( ctx, "Tools", nk_rect( 0.0f, 0.0f, menuWidth, menuHeight ), NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE ) ) {
		nk_layout_row_begin( ctx, NK_STATIC, BUTTON_HEIGHT, 1 );
		nk_layout_row_push( ctx, buttonWidth );

		for( size_t i = 0; i < sb_Count( sbEditors ); ++i ) {
			if( nk_button_label( ctx, sbEditors[i].name ) ) {
				toggleEditor( i );
			}
		}
	} nk_end( ctx );

	for( size_t i = 0; i < sb_Count( sbEditors ); ++i ) {
		if( !sbEditors[i].isShown ) continue;
		if( sbEditors[i].process != NULL ) sbEditors[i].process( );
	}

	processDialogs( ctx );
}

static void editorHubScreen_Draw( void )
{
}

static void editorHubScreen_PhysicsTick( float dt )
{
	for( size_t i = 0; i < sb_Count( sbEditors ); ++i ) {
		if( !sbEditors[i].isShown ) continue;
		if( sbEditors[i].tick != NULL ) sbEditors[i].tick( dt );
	}
}

GameState editorHubScreenState = { editorHubScreen_Enter, editorHubScreen_Exit, editorHubScreen_ProcessEvents,
	editorHubScreen_Process, editorHubScreen_Draw, editorHubScreen_PhysicsTick };