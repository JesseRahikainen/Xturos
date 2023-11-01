#include "3x3Editor.h"

#include <SDL.h>
#include <limits.h>
#include <stdbool.h>

#include "IMGUI/nuklearWrapper.h"
#include "Utils/stretchyBuffer.h"
#include "editorHelpers.h"
#include "editorHub.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "Graphics/images.h"
#include "Graphics/imageSheets.h"
#include "Math/mathUtil.h"

// load up an image and define where the slices are, do we want a definition file like we do with sprite sheets?

static struct nk_rect windowBounds = { 150.0f, 10.0f, 600.0f, 400.f };

static int loadedImage = -1;
static int xLeft;
static int xRight;
static int yTop;
static int yBottom;

static float zoom = 1.0f;

static int imgWidth = 100;
static int imgHeight = 100;

static char* loadedImageFilePath = NULL;

static void loadImage( const char* filePath )
{
	if( loadedImage >= 0 ) {
		img_Clean( loadedImage );
		mem_Release( loadedImageFilePath );
		loadedImageFilePath = NULL;
		loadedImage = -1;
	}

	loadedImage = editor_loadImageFile( filePath );
	if( loadedImage > 0 ) {
		Vector2 size;
		img_GetSize( loadedImage, &size );
		imgWidth = (int)size.x;
		imgHeight = (int)size.y;
		xLeft = 0;
		xRight = imgWidth;
		yTop = 0;
		yBottom = imgHeight;

		loadedImageFilePath = createStringCopy( filePath );
	}
}

static void exportSpriteSheet( const char* filePath )
{
	if( loadedImage >= 0 ) {
		if( !img_Save3x3( filePath, loadedImageFilePath, imgWidth, imgHeight, xLeft, xRight, yTop, yBottom ) ) {
			hub_CreateDialog( "Export 3x3 Error", "There was an error exporting the sprite sheet. Check the log file for more information.", DT_ERROR, 1, "OK", NULL );
		}
	}
}

void threeByThreeEditor_Init( void )
{

}

void threeByThreeEditor_Show( void )
{

}

void threeByThreeEditor_Hide( void )
{
	if( loadedImage >= 0 ) {
		img_Clean( loadedImage );
		loadedImage = -1;
		mem_Release( loadedImageFilePath );
		loadedImageFilePath = NULL;
	}
}

void threeByThreeEditor_IMGUIProcess( void )
{
	struct nk_context* ctx = &( editorIMGUI.ctx );
	if( nk_begin( ctx, "3x3", windowBounds, NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE ) ) {
		nk_menubar_begin( ctx ); {

			nk_layout_row_begin( ctx, NK_STATIC, 20, INT_MAX );
			nk_layout_row_push( ctx, 50 );
			if( nk_menu_begin_label( ctx, "File", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_vec2( 120, 120 ) ) ) {

				nk_layout_row_dynamic( ctx, 20, 1 );
				if( nk_menu_item_label( ctx, "Load Image...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseLoadFileLocation( "PNG Image", "png", false, loadImage );
				}

				if( nk_menu_item_label( ctx, "Export Sprite Sheet...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseSaveFileLocation( "Sprite Sheet", "spritesheet", exportSpriteSheet );
				}

				nk_menu_end( ctx );
			}

		} nk_menubar_end( ctx );

		// left column holding divider values
		float layoutHeight = nk_window_get_height( ctx ) - 76.0f;
		nk_layout_space_begin( ctx, NK_DYNAMIC, layoutHeight, INT_MAX ); {
			nk_flags groupFlags = NK_WINDOW_BORDER;// | NK_WINDOW_TITLE;
			nk_layout_space_push( ctx, nk_rect( 0.0f, 0.0f, 0.3f, 1.0f ) );
			if( nk_group_begin( ctx, "General", groupFlags ) ) {
				nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 );
				nk_layout_row_push( ctx, 1.0f );

				nk_property_int( ctx, "Top", 0, &yTop, imgHeight, 1, 1.0f );
				yTop = MIN( yTop, yBottom );

				nk_property_int( ctx, "Bottom", 0, &yBottom, imgHeight, 1, 1.0f );
				yBottom = MAX( yTop, yBottom );

				nk_property_int( ctx, "Left", -1000, &xLeft, imgWidth, 1, 1.0f );
				xLeft = MIN( xLeft, xRight );

				nk_property_int( ctx, "Right", 0, &xRight, imgWidth, 1, 1.0f );
				xRight = MAX( xLeft, xRight );

				nk_property_float( ctx, "Zoom", 0.1f, &zoom, 10.0f, 0.1f, 0.1f );

				nk_group_end( ctx );
			}
		
			// right column drawing image and dividers
			nk_layout_space_push( ctx, nk_rect( 0.3f, 0.0f, 0.7f, 1.0f ) );
			if( nk_group_begin( ctx, "Image", groupFlags ) ) {

				float zoomWidth = imgWidth * zoom;
				float zoomHeight = imgHeight * zoom;

				struct nk_command_buffer* buffer = nk_window_get_canvas( ctx );
				float clipCenterX = buffer->clip.x + ( buffer->clip.w / 2.0f );
				float clipCenterY = buffer->clip.y + ( buffer->clip.h / 2.0f );

				nk_stroke_line( nk_window_get_canvas( ctx ), clipCenterX, clipCenterY - 100.0f, clipCenterX, clipCenterY + 100.0f, 1.0f, nk_rgb( 255, 0, 0 ) );
				nk_stroke_line( nk_window_get_canvas( ctx ), clipCenterX - 100.0f, clipCenterY, clipCenterX + 100.0f, clipCenterY, 1.0f, nk_rgb( 255, 0, 0 ) );

				float leftMost = clipCenterX - ( zoomWidth / 2.0f );
				float rightMost = leftMost + zoomWidth;

				float topMost = clipCenterY - ( zoomHeight / 2.0f );
				float bottomMost = topMost + zoomHeight;

				if( loadedImage >= 0 ) {
					struct nk_image image = nk_image_id( loadedImage );
					struct nk_rect drawRect = nk_rect( leftMost, topMost, (float)zoomWidth, (float)zoomHeight );
					nk_draw_image( buffer, drawRect, &image, nk_rgb( 255, 255, 255 ) );
				}

				float zoomLeft = xLeft * zoom;
				float zoomRight = xRight * zoom;
				float zoomTop = yTop * zoom;
				float zoomBottom = yBottom * zoom;

				nk_stroke_line( nk_window_get_canvas( ctx ), leftMost + zoomLeft, topMost, leftMost + zoomLeft, bottomMost, 1.0f, nk_rgb( 0, 255, 0 ) );
				nk_stroke_line( nk_window_get_canvas( ctx ), leftMost + zoomRight, topMost, leftMost + zoomRight, bottomMost, 1.0f, nk_rgb( 0, 255, 0 ) );

				nk_stroke_line( nk_window_get_canvas( ctx ), leftMost, topMost + zoomTop, rightMost, topMost + zoomTop, 1.0f, nk_rgb( 0, 255, 0 ) );
				nk_stroke_line( nk_window_get_canvas( ctx ), leftMost, topMost + zoomBottom, rightMost, topMost + zoomBottom, 1.0f, nk_rgb( 0, 255, 0 ) );

				nk_group_end( ctx );
			}
		} nk_layout_space_end( ctx );

	} nk_end( ctx );
}

void threeByThreeEditor_Tick( float dt )
{

}