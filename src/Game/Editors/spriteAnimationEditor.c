#include "spriteAnimationEditor.h"

#include <SDL3/SDL.h>
#include <limits.h>

// BIG TODO: make this using an event based approach so we can support undo and redo.

#include "Graphics/spriteAnimation.h"

#include "editorHub.h"
#include "editorHelpers.h"
#include "IMGUI/nuklearWrapper.h"
#include "System/platformLog.h"
#include "Utils/stretchyBuffer.h"
#include "collisionDetection.h"
#include "Math/mathUtil.h"
#include "Utils/hashMap.h"
#include "Graphics/images.h"
#include "Graphics/imageSheets.h"
#include "Utils/helpers.h"
#include "Graphics/Platform/graphicsPlatform.h"
#include "System/jobQueue.h"
#include "Utils/cfgFile.h"

#include "Graphics/Platform/OpenGL/glDebugging.h"

const float STANDARD_ROW_HEIGHT = 34.0f;
const float STANRARD_BUTTON_SIZE = 50.0f;

static SpriteAnimation editorSpriteAnimation = { NULL, 10.0f, 5, true, NULL, -1 };
static PlayingSpriteAnimation editorPlayingSprite = { &editorSpriteAnimation, 0.0f };

static size_t selectedEvent = SIZE_MAX;

static struct nk_image playNKImage;
static struct nk_image pauseNKImage;
static struct nk_image stopNKImage;
static bool imagesLoaded = false;

static bool playingAnimation = false;
static bool hitAnimationEnd = false;

static char* currentFileName = NULL;

const float MIN_SCALE = 0.1f;
const float MAX_SCALE = 5.0f;
const float SCALE_WHEEL_AMT = 0.1f;

static float spriteScale = 1.0f;
static float prevImageAlpha = 0.25f;
static float nextImageAlpha = 0.0f;
static AnimEvent_SwitchImage preAnimSprite;
static AnimEvent_SwitchImage postAnimSprite;

static struct nk_colorf spriteBGColor = { 0.25f, 0.25f, 0.25f, 1.0f };

#define FLOAT_MIN -1000000.0f
#define FLOAT_MAX 1000000.0f
#define FLOAT_SMALL 0.01f

typedef struct {
	struct nk_color clr;
	const char* label;
} EventDisplay;

static EventDisplay eventDisplays[] = {
	{ { 255,   0,   0, 255 }, "S" }, // AET_SWITCH_IMAGE
	{ {   0, 255,   0, 255 }, "B" }, // AET_SET_AAB_COLLIDER
	{ {   0,   0, 255, 255 }, "C" }, // AET_SET_CIRCLE_COLLIDER
	{ { 255, 255,   0, 255 }, "D" } // AET_DEACTIVATE_COLLIDER
};

static const char* eventTypeNames[] = {
	"Switch Image",
	"Set AAB Collider",
	"Set Circle Collider",
	"Deactivate Collider"
};

static struct nk_color chosenEvent = { 0, 0, 0, 255 };

typedef struct {
	Collider collider;
	struct nk_colorf color;
} EditorCollider;

static EditorCollider* sbColliders;

static struct nk_rect windowBounds = { 150.0f, 10.0f, 1000.0f, 1000.f };

static ImageID drawnSprite = INVALID_IMAGE_ID;
static Vector2 drawnOffset = { 0.0f, 0.0f };

static ImageID* sbSpriteSheetImages = NULL;

typedef struct {
	const char* name;
	int imageId;
} LoadedImageDisplay;

static LoadedImageDisplay* sbLoadedImageDisplays = NULL;

static Vector2 displayOffset = { 0.0f, 0.0f };

static bool overDisplay = false;
static bool movingDrawnOffset = false;

const float SAVE_CHECK_TIME = 1.0f;
static float saveCheckCountdown = -1.0f;

static void resetSaveCheckTime( )
{
	saveCheckCountdown = SAVE_CHECK_TIME;
}

static void saveAnimEditorConfig( )
{
	void* cfg = cfg_OpenFile( "animEditor.cfg" );
	if( cfg == NULL ) {
		hub_CreateDialog( "Error", "Unable to open animEditor.cfg.", DT_ERROR, 1, "OK", NULL );
		return;
	}

	cfg_SetFloat( cfg, "Background_Red", spriteBGColor.r );
	cfg_SetFloat( cfg, "Background_Green", spriteBGColor.g );
	cfg_SetFloat( cfg, "Background_Blue", spriteBGColor.b );

	cfg_SetFloat( cfg, "Scale", spriteScale );

	cfg_SetFloat( cfg, "Prev_Alpha", prevImageAlpha );
	cfg_SetFloat( cfg, "Next_Alpha", nextImageAlpha );

	cfg_SetInt( cfg, "Num_Visible_Collisions", (int)sb_Count( sbColliders ) );
	char buffer[128];
	for( size_t i = 0; i < sb_Count( sbColliders ); ++i ) {
		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Red", (int)i );
		cfg_SetFloat( cfg, buffer, sbColliders[i].color.r );
		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Green", (int)i );
		cfg_SetFloat( cfg, buffer, sbColliders[i].color.g );
		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Blue", (int)i );
		cfg_SetFloat( cfg, buffer, sbColliders[i].color.b );
	}

	cfg_SaveFile( cfg );
	cfg_CloseFile( cfg );
}

static void loadAnimEditorConfig( )
{
	void* cfg = cfg_OpenFile( "animEditor.cfg" );
	if( cfg == NULL ) {
		hub_CreateDialog( "Error", "Unable to open animEditor.cfg.", DT_ERROR, 1, "OK", NULL );
		return;
	}

	spriteBGColor.r = cfg_GetFloat( cfg, "Background_Red", 0.25f );
	spriteBGColor.g = cfg_GetFloat( cfg, "Background_Green", 0.25f );
	spriteBGColor.b = cfg_GetFloat( cfg, "Background_Blue", 0.25f );
	spriteBGColor.a = 1.0f;

	spriteScale = cfg_SetFloat( cfg, "Scale", 1.0f );

	prevImageAlpha = cfg_GetFloat( cfg, "Prev_Alpha", 0.25f );
	nextImageAlpha = cfg_GetFloat( cfg, "Next_Alpha", 0.0f );

	int numColliders = cfg_GetInt( cfg, "Num_Visible_Collisions", 0 );
	sb_Clear( sbColliders );
	char buffer[128];
	for( int i = 0; i < numColliders; ++i ) {
		EditorCollider newCollider;
		newCollider.collider.type = CT_DEACTIVATED;

		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Red", (int)i );
		newCollider.color.r = cfg_GetFloat( cfg, buffer, 0.0f );
		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Green", (int)i );
		newCollider.color.g = cfg_GetFloat( cfg, buffer, 0.0f );
		SDL_snprintf( buffer, ARRAY_SIZE( buffer ), "Collider_%i_Blue", (int)i );
		newCollider.color.b = cfg_GetFloat( cfg, buffer, 1.0f );
		newCollider.color.a = 1.0f;
		
		sb_Push( sbColliders, newCollider );
	}

	cfg_CloseFile( cfg );
}

static void unloadCurrentSpriteSheet( )
{
	// unload the old images and release the array
	img_UnloadSpriteSheet( editorSpriteAnimation.spriteSheetPackageID );
	sb_Release( sbSpriteSheetImages );
	
	// erase all image references in existing events
	for( size_t i = 0; i < sb_Count( editorSpriteAnimation.sbEvents ); ++i ) {
		if( editorSpriteAnimation.sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
			editorSpriteAnimation.sbEvents[i].switchImg.imgID = INVALID_IMAGE_ID;
		}
	}
}

static LoadedImageDisplay newLoadedImageDisplay( const char* name, int id )
{
	LoadedImageDisplay val;
	val.name = name;
	val.imageId = id;
	return val;
}

static void spriteSheetChosen( void* data )
{
	char* filePath = (char*)data;
	// attempt to load the new sprite sheet 
	ImageID* sbNewlyLoadedImages = NULL;
	int newPackageID = editor_loadSpriteSheetFile( filePath, &sbNewlyLoadedImages );

	if( newPackageID < 0 ) {
		hub_CreateDialog( "Error Loading Sprite Sheet", "There was an error loading the sprite sheet. Check error log for more details.", DT_ERROR, 1, "OK", NULL );
		mem_Release( data );
		return;
	}

	unloadCurrentSpriteSheet( );
	
	// set the new data
	editorSpriteAnimation.spriteSheetPackageID = newPackageID;
	mem_Release( editorSpriteAnimation.spriteSheetFile );
	editorSpriteAnimation.spriteSheetFile = filePath; // claiming the string for ourselves, release later
	sbSpriteSheetImages = sbNewlyLoadedImages; 

	// update the images we can select from
	sb_Clear( sbLoadedImageDisplays );
	int currentSelected = 0;

	// add unused
	sb_Push( sbLoadedImageDisplays, newLoadedImageDisplay( "-- NOT SET --", SIZE_MAX ) );

	for( size_t i = 0; i < sb_Count( sbSpriteSheetImages ); ++i ) {
		ImageID id = sbSpriteSheetImages[i];
		if( img_IsValidImage( id ) ) {
			sb_Push( sbLoadedImageDisplays, newLoadedImageDisplay( img_GetImgStringID( id ), id ) );
		}
	}
}

static void spriteSheetChosenCallback( const char* filePath )
{
	// have to make sure we're calling stuff on the main thread because we're creating textures
	char* filePathCopy = createStringCopy( filePath );
	jq_AddMainThreadJob( spriteSheetChosen, filePathCopy );
}

static void chooseSpriteSheet( void )
{
	checkAndLogErrors( "pre-editor_chooseLoadFileLocation failure" );
	editor_chooseLoadFileLocation( "Sprite Sheet", "spritesheet", false, spriteSheetChosenCallback );
}

static void loadAndAssociateSpriteSheet( void )
{
	if( editorSpriteAnimation.spriteSheetFile != NULL ) {
		hub_CreateDialog( "Overwrite Existing Sprite Sheet?",
			"Loading a different sprite sheet will result in loss of currently assigned sprites. Do you wish to continue?",
			DT_WARNING, 2,
			"Yes", chooseSpriteSheet,
			"No", NULL );
	} else {
		chooseSpriteSheet( );
	}
}

static void editorSwitchImage( void* data, ImageID imgID, const Vector2* offset )
{
	drawnSprite = imgID;
}

static void editorSetAABCollider( void* data, int colliderID, float centerX, float centerY, float width, float height )
{
	if( colliderID >= (int)sb_Count( sbColliders ) ) return;
	if( colliderID < 0 ) return;

	sbColliders[colliderID].collider.type = CT_AAB;
	sbColliders[colliderID].collider.aabb.center.x = centerX;
	sbColliders[colliderID].collider.aabb.center.y = centerY;
	sbColliders[colliderID].collider.aabb.halfDim.w = width / 2.0f;
	sbColliders[colliderID].collider.aabb.halfDim.h = height / 2.0f;
}

static void editorSetCircleCollider( void* data, int colliderID, float centerX, float centerY, float radius )
{
	if( colliderID >= (int)sb_Count( sbColliders ) ) return;
	if( colliderID < 0 ) return;

	sbColliders[colliderID].collider.type = CT_CIRCLE;
	sbColliders[colliderID].collider.circle.center.x = centerX;
	sbColliders[colliderID].collider.circle.center.y = centerY;
	sbColliders[colliderID].collider.circle.radius = radius;
}

static void editorDeactivateCollider( void* data, int colliderID )
{
	if( colliderID >= (int)sb_Count( sbColliders ) ) return;
	if( colliderID < 0 ) return;

	sbColliders[colliderID].collider.type = CT_DEACTIVATED;
}

static void editorAnimationCompleted( void* data, bool loops )
{
	if( !loops ) hitAnimationEnd = true;
}

static AnimEventHandler editorEventHandler = {
	NULL, // data;
	editorSwitchImage,
	editorSetAABCollider,
	editorSetCircleCollider,
	editorDeactivateCollider,
	editorAnimationCompleted,
};

static void saveCurrentAnimation( const char* filePath )
{
	ASSERT( filePath != NULL );
	llog( LOG_DEBUG, "Saving out to %s...", filePath );

	//editorSpriteAnimation.sbEvents[0].switchImg.frameName = ? ;
	if( !sprAnim_Save( filePath, &editorSpriteAnimation ) ) {
		hub_CreateDialog( "Error Saving Animated Sprite", "Error saving animated sprite. Check error log for more details.", DT_ERROR, 1, "OK", NULL );
	}

	mem_Release( currentFileName );
	currentFileName = createStringCopy( filePath );
}

static void reprocessFrames( void )
{
	float timePassed = editorPlayingSprite.timePassed;
	sprAnim_StartAnim( &editorPlayingSprite, &editorSpriteAnimation, &editorEventHandler );
	// treat animation as non-looping for this, avoids issue with displaying the first frame when moving the scrubber all the way to the right
	bool looping = editorSpriteAnimation.loops;
	editorSpriteAnimation.loops = false;
	sprAnim_ProcessAnim( &editorPlayingSprite, &editorEventHandler, timePassed );
	editorSpriteAnimation.loops = looping;
}

static void resetPreAndPostAnimSprites( void )
{
	preAnimSprite.base.type = AET_SWITCH_IMAGE;
	preAnimSprite.frameName = NULL;
	preAnimSprite.imgID = INVALID_IMAGE_ID;
	preAnimSprite.offset = vec2( 0.0f, 0.0f );

	postAnimSprite.base.type = AET_SWITCH_IMAGE;
	postAnimSprite.frameName = NULL;
	postAnimSprite.imgID = INVALID_IMAGE_ID;
	postAnimSprite.offset = vec2( 0.0f, 0.0f );
}

static void loadCurrentAnimation( const char* filePath )
{
	ASSERT( filePath != NULL );
	llog( LOG_DEBUG, "Loading from %s...", filePath );

	selectedEvent = SIZE_MAX;
	resetPreAndPostAnimSprites( );

	sprAnim_Clean( &editorSpriteAnimation );

	if( !sprAnim_Load( filePath, &editorSpriteAnimation ) ) {
		hub_CreateDialog( "Error Loading Animated Sprite", "Error loading animated sprite. Check error log for more details.", DT_ERROR, 1, "OK", NULL );
		return;
	}

	if( ( editorSpriteAnimation.spriteSheetFile != NULL ) ) {
		char* spriteSheetFileCopy = createStringCopy( editorSpriteAnimation.spriteSheetFile );
		spriteSheetChosen( spriteSheetFileCopy );
	}

	// hook up the images
	for( size_t i = 0; i < sb_Count( editorSpriteAnimation.sbEvents ); ++i ) {
		if( editorSpriteAnimation.sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
			editorSpriteAnimation.sbEvents[i].switchImg.imgID = img_GetExistingByStrID( editorSpriteAnimation.sbEvents[i].switchImg.frameName );
			mem_Release( editorSpriteAnimation.sbEvents[i].switchImg.frameName );
			editorSpriteAnimation.sbEvents[i].switchImg.frameName = NULL;
		}
	}

	mem_Release( currentFileName );
	currentFileName = createStringCopy( filePath );

	reprocessFrames( );
}

static void mainThreadLoadAnimation( void* data )
{
	char* filePath = (char*)data;
	loadCurrentAnimation( filePath );
	mem_Release( filePath );
}

static void loadCurrentAnimation_OffMainThread( const char* filePath )
{
	char* filePathCopy = createStringCopy( filePath );
	jq_AddMainThreadJob( mainThreadLoadAnimation, filePathCopy );
}

static void newAnimation( void )
{
	llog( LOG_DEBUG, "Creating new animation" );
	sprAnim_Clean( &editorSpriteAnimation );
	editorSpriteAnimation = spriteAnimation( );
	playingAnimation = false;

	editorPlayingSprite.animation = &editorSpriteAnimation;
	editorPlayingSprite.timePassed = 0.0f;

	selectedEvent = SIZE_MAX;
	preAnimSprite.imgID = INVALID_IMAGE_ID;
	preAnimSprite.offset = vec2( 0.0f, 0.0f );
	postAnimSprite.imgID = INVALID_IMAGE_ID;
	postAnimSprite.offset = vec2( 0.0f, 0.0f );

	mem_Release( currentFileName );
	currentFileName = NULL;
}

static void loadUIImages( void )
{
	playNKImage = nk_xu_loadImage( "Images/uiicons/forward.png", NULL, NULL );
	pauseNKImage = nk_xu_loadImage( "Images/uiicons/pause.png", NULL, NULL );
	stopNKImage = nk_xu_loadImage( "Images/uiicons/stop.png", NULL, NULL );
}

void spriteAnimationEditor_Init( void )
{
	loadUIImages( );
}

void spriteAnimationEditor_Show( void )
{
	overDisplay = false;
	movingDrawnOffset = false;
	loadAnimEditorConfig( );
	newAnimation( );
}

void spriteAnimationEditor_Hide( void )
{
	saveAnimEditorConfig( );
	sprAnim_Clean( &editorSpriteAnimation );
	sb_Release( sbSpriteSheetImages );
	mem_Release( currentFileName );
	currentFileName = NULL;
	drawnSprite = INVALID_IMAGE_ID;
}

static uint32_t* sbHeights = NULL;
static uint32_t getAnimGridHeight( void )
{
	// go through all the events and find the frame with the most events in it
	sb_Add( sbHeights, editorSpriteAnimation.durationFrames );
	memset( sbHeights, 0, sizeof( sbHeights[0] ) * sb_Count( sbHeights ) );

	uint32_t maxHeight = 0;
	for( size_t i = 0; i < sb_Count( editorSpriteAnimation.sbEvents ); ++i ) {
		uint32_t frame = editorSpriteAnimation.sbEvents[i].base.frame;
		sbHeights[frame] += 1;
		if( sbHeights[frame] > maxHeight ) {
			maxHeight = sbHeights[frame];
		}
	}

	sb_Clear( sbHeights );
	return maxHeight;
}

static size_t getAnimEventIndex( uint32_t frame, uint32_t height )
{
	// find the index for the height-th event in the specified frame
	//  returns SIZE_MAX if nothing is found
	uint32_t frameCount = 0;
	for( size_t i = 0; i < sb_Count( editorSpriteAnimation.sbEvents ); ++i ) {
		if( frame == editorSpriteAnimation.sbEvents[i].base.frame ) {
			if( frameCount == height ) {
				return i;
			}
			++frameCount;
		}
	}

	return SIZE_MAX;
}

static void addDefaultEvent( uint32_t frame )
{
	sprAnim_AddEvent( &editorSpriteAnimation, createEvent_DeactivateCollider( frame, 0 ) );
	selectedEvent = sb_Count( editorSpriteAnimation.sbEvents ) - 1;
}

static void switchImageComboxBoxItemGetter( void* data, int id, const char** outText )
{
	if( ( id < 0 ) || ( id >= (int)sb_Count( sbLoadedImageDisplays ) ) )	{
		*outText = "";
	} else {
		*outText = sbLoadedImageDisplays[id].name;
	}
}

// show the combobox to select which image to use
static bool chooseImage( struct nk_context* ctx, ImageID current, ImageID* out )
{
	int currentSelected = 0;
	size_t count = sb_Count( sbLoadedImageDisplays );
	for( size_t i = 0; i < sb_Count( sbLoadedImageDisplays ); ++i ) {
		ImageID id = sbLoadedImageDisplays[i].imageId;
		if( img_IsValidImage( id ) && ( id == current ) ) {
			currentSelected = (int)i;
		}
	}

	int lastSelected = currentSelected;
	nk_combobox_callback( ctx, switchImageComboxBoxItemGetter, NULL, &currentSelected, (int)sb_Count( sbLoadedImageDisplays ), (int)STANDARD_ROW_HEIGHT, nk_vec2( 510.0f, 510.0f ) );
	
	if( currentSelected > 0 ) {
		(*out) = sbLoadedImageDisplays[currentSelected].imageId;
	} else {
		(*out) = INVALID_IMAGE_ID;
	}

	return ( *out ) != current;
}

static void displayEventData_SwitchImageBase( struct nk_context* ctx, AnimEvent_SwitchImage* evt )
{
	nk_property_float( ctx, "#OffsetX", FLOAT_MIN, &evt->offset.x, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "#OffsetY", FLOAT_MIN, &evt->offset.y, FLOAT_MAX, 0.1f, 0.01f );

	nk_label( ctx, "Image", NK_TEXT_LEFT );

	chooseImage( ctx, evt->imgID, &( evt->imgID ) );
}

static void displayEventData_SwitchImage( struct nk_context* ctx, AnimEvent* evt )
{
	displayEventData_SwitchImageBase( ctx, &(evt->switchImg) );
}

static void displayEventData_SetAABCollider( struct nk_context* ctx, AnimEvent* evt )
{
	nk_property_int( ctx, "ID", 0, &evt->setAABCollider.colliderID, INT_MAX, 1, 1 );
	nk_property_float( ctx, "CenterX", FLOAT_MIN, &evt->setAABCollider.centerX, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "CenterY", FLOAT_MIN, &evt->setAABCollider.centerY, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "Width", FLOAT_SMALL, &evt->setAABCollider.width, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "Height", FLOAT_SMALL, &evt->setAABCollider.height, FLOAT_MAX, 0.1f, 0.01f );
}

static void displayEventData_SetCircleCollider( struct nk_context* ctx, AnimEvent* evt )
{
	nk_property_int( ctx, "ID", 0, &evt->setCircleCollider.colliderID, INT_MAX, 1, 1 );
	nk_property_float( ctx, "CenterX", FLOAT_MIN, &evt->setCircleCollider.centerX, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "CenterY", FLOAT_MIN, &evt->setCircleCollider.centerY, FLOAT_MAX, 0.1f, 0.01f );
	nk_property_float( ctx, "Radius", FLOAT_SMALL, &evt->setCircleCollider.radius, FLOAT_MAX, 0.1f, 0.01f );
}

static void displayEventData_DeactivateCollider( struct nk_context* ctx, AnimEvent* evt )
{
	nk_property_int( ctx, "ID", 0, &evt->deactivateCollider.colliderID, INT_MAX, 1, 1 );
}

static bool displayEventData( struct nk_context* ctx, size_t eventIdx )
{
	ASSERT( eventIdx <= sb_Count( editorSpriteAnimation.sbEvents ) );

	AnimEvent* evt = &editorSpriteAnimation.sbEvents[eventIdx];
	AnimEvent oldEvent = *evt;

	nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
	nk_layout_row_push( ctx, 1.0f );

	// edit the type of event it is
	int currType = (int)evt->base.type;
	nk_combobox( ctx, eventTypeNames, MAX_ANIM_EVENT_TYPES, &currType, 20, nk_vec2( 255.0f, 180.0f ) );
	if( currType != evt->base.type ) {
		switch( currType ) {
		case AET_SWITCH_IMAGE:
			*evt = createEvent_SetImage( evt->base.frame, NULL, &VEC2_ZERO );
			break;
		case AET_SET_AAB_COLLIDER:
			*evt = createEvent_SetAABCollider( evt->base.frame, 0, 0.0f, 0.0f, 10.0f, 10.0f );
			displayEventData_SetAABCollider( ctx, evt );
			break;
		case AET_SET_CIRCLE_COLLIDER:
			*evt = createEvent_SetCircleCollider( evt->base.frame, 0, 0.0f, 0.0f, 10.0f );
			displayEventData_SetCircleCollider( ctx, evt );
			break;
		case AET_DEACTIVATE_COLLIDER:
			*evt = createEvent_DeactivateCollider( evt->base.frame, 0 );
			break;
		default:
			llog( LOG_ERROR, "Tried switching to an unknown animation event type." );
			break;
		}
	}

	// edit the frame the event is on
	int frame = (int)evt->base.frame;
	int maxFrame = (int)editorSpriteAnimation.durationFrames;
	nk_property_int( ctx, "Frame", 0, &frame, maxFrame - 1, 1, 1.0f );
	evt->base.frame = (uint32_t)frame;

	// edit the event itself
	switch( evt->base.type ) {
	case AET_SWITCH_IMAGE:
		displayEventData_SwitchImage( ctx, evt );
		break;
	case AET_SET_AAB_COLLIDER:
		displayEventData_SetAABCollider( ctx, evt );
		break;
	case AET_SET_CIRCLE_COLLIDER:
		displayEventData_SetCircleCollider( ctx, evt );
		break;
	case AET_DEACTIVATE_COLLIDER:
		displayEventData_DeactivateCollider( ctx, evt );
		break;
	default:
		llog( LOG_ERROR, "Unknown animation event type." );
		break;
	}

	bool needsUpdate = false;

	if( nk_button_label( ctx, "Delete" ) ) {
		sb_Remove( editorSpriteAnimation.sbEvents, eventIdx );
		selectedEvent = SIZE_MAX;
		needsUpdate = true;
	} else {
		needsUpdate = SDL_memcmp( evt, &oldEvent, sizeof( oldEvent ) ) != 0;
	}

	return needsUpdate;
}

static void drawCollisions( struct nk_command_buffer* buffer )
{
	float clipCenterX = buffer->clip.x + ( buffer->clip.w / 2.0f ) + drawnOffset.x;
	float clipCenterY = buffer->clip.y + ( buffer->clip.h / 2.0f ) + drawnOffset.y;
	for( size_t i = 0; i < sb_Count( sbColliders ); ++i ) {
		switch( sbColliders[i].collider.type ) {
		case CT_AAB: {
			ColliderAAB* aab = &sbColliders[i].collider.aabb;
			nk_stroke_rect( buffer,
				nk_rect( clipCenterX + ( aab->center.x - aab->halfDim.w ) * spriteScale, clipCenterY + ( aab->center.y - aab->halfDim.h ) * spriteScale, 2.0f * aab->halfDim.w * spriteScale, 2.0f * aab->halfDim.h * spriteScale ),
				0.0f, 1.0f, nk_rgb_cf( sbColliders[i].color ) );
		} break;
		case CT_CIRCLE: {
			ColliderCircle* circle = &sbColliders[i].collider.circle;
			nk_stroke_circle( buffer,
				nk_rect( clipCenterX + circle->center.x - ( circle->radius * spriteScale ), clipCenterY + circle->center.y - ( circle->radius * spriteScale ), circle->radius * 2.0f * spriteScale, circle->radius * 2.0f * spriteScale ),
				1.0f, nk_rgb_cf( sbColliders[i].color ) );
		} break;
		case CT_DEACTIVATED:
			// nothing to draw
			break;
		default:
			llog( LOG_ERROR, "Attempting to draw unhandled collision type." );
			break;
		}
	}
}

static void drawCenterCrossHair( struct nk_command_buffer* buffer )
{
	float clipCenterX = buffer->clip.x + ( buffer->clip.w / 2.0f ) + drawnOffset.x;
	float clipCenterY = buffer->clip.y + ( buffer->clip.h / 2.0f ) + drawnOffset.y;

	nk_stroke_line( buffer, clipCenterX, clipCenterY + 10.0f, clipCenterX, clipCenterY - 10.0f, 1.0f, nk_rgba_f( 1.0f - spriteBGColor.r, 1.0f - spriteBGColor.g, 1.0f - spriteBGColor.b, 0.5f ) );
	nk_stroke_line( buffer, clipCenterX + 10.0f, clipCenterY, clipCenterX - 10.0f, clipCenterY, 1.0f, nk_rgba_f( 1.0f - spriteBGColor.r, 1.0f - spriteBGColor.g, 1.0f - spriteBGColor.b, 0.5f ) );
}

static void drawImage( AnimEvent_SwitchImage* event, float alpha, struct nk_command_buffer* buffer )
{
	int imgID = event->imgID;
	Vector2 offset = event->offset;

	if( !img_IsValidImage( imgID ) ) return;

	Vector2 size;
	img_GetSize( imgID, &size );

	PlatformTexture texture;
	img_GetTextureID( imgID, &texture );

	// needs full texture width and height here
	int textureWidth;
	int textureHeight;
	gfxPlatform_GetPlatformTextureSize( &texture, &textureWidth, &textureHeight );

	float clipCenterX = buffer->clip.x + ( buffer->clip.w / 2.0f ) + drawnOffset.x;
	float clipCenterY = buffer->clip.y + ( buffer->clip.h / 2.0f ) + drawnOffset.y;

	Vector2 renderSize;
	vec2_Scale( &size, spriteScale, &renderSize );

	Vector2 renderPos;
	// need the center minus the size to get the top left corner, then add the offset to get it to the right position
	renderPos.x = clipCenterX - ( size.w / 2.0f * spriteScale ) + ( offset.x * spriteScale );
	renderPos.y = clipCenterY - ( size.h / 2.0f * spriteScale ) + ( offset.y * spriteScale );

	Vector2 uvMin, uvMax;
	img_GetUVCoordinates( imgID, &uvMin, &uvMax );

	Vector2 subImageMin = vec2( uvMin.x * textureWidth, uvMin.y * textureHeight );
	Vector2 subImageMax = vec2( uvMax.x * textureWidth, uvMax.y * textureHeight );
	struct nk_rect subImageRect = nk_rect( subImageMin.x, subImageMin.y, subImageMax.x - subImageMin.x, subImageMax.y - subImageMin.y );
	struct nk_image image = nk_subimage_id( imgID, (nk_ushort)textureWidth, (nk_ushort)textureHeight, subImageRect );

	struct nk_rect drawRect = nk_rect( renderPos.x, renderPos.y, renderSize.w, renderSize.h );

	nk_draw_image( buffer, drawRect, &image, nk_rgba_f( 1.0f, 1.0f, 1.0f, alpha ) );
}

static void drawImageEvent( uint32_t eventIdx, float alpha, struct nk_command_buffer* buffer )
{
	if( eventIdx == INVALID_ANIM_EVENT_IDX ) return;
	ASSERT_AND_IF_NOT( editorSpriteAnimation.sbEvents[eventIdx].base.type == AET_SWITCH_IMAGE ) return;

	ASSERT_AND_IF_NOT( eventIdx < sb_Count( editorSpriteAnimation.sbEvents ) ) return;
	drawImage( &editorSpriteAnimation.sbEvents[eventIdx].switchImg, alpha, buffer );
}

static void drawCurrentImage( struct nk_command_buffer* buffer )
{
	// we need to do the onion skinning, to do that we want to get and draw the previous and next images

	uint32_t currFrame = sprAnim_GetCurrentFrame( &editorPlayingSprite );

	uint32_t currEvent = sprAnim_NextImageEvent( &editorSpriteAnimation, currFrame, false, true ); // image for current frame
	uint32_t prevEvent = INVALID_ANIM_EVENT_IDX;
	uint32_t nextEvent = sprAnim_NextImageEvent( &editorSpriteAnimation, currFrame, true, false ); // next image

	if( currEvent != INVALID_ANIM_EVENT_IDX ) {
		prevEvent = sprAnim_NextImageEvent( &editorSpriteAnimation, editorSpriteAnimation.sbEvents[currEvent].base.frame, false, false ); // previous image
	}

	if( prevEvent != INVALID_ANIM_EVENT_IDX ) {
		drawImageEvent( prevEvent, prevImageAlpha, buffer );
	} else {
		drawImage( &preAnimSprite, prevImageAlpha, buffer );
	}

	drawImageEvent( currEvent, 1.0f, buffer );

	if( nextEvent != INVALID_ANIM_EVENT_IDX ) {
		drawImageEvent( nextEvent, nextImageAlpha, buffer );
	} else {
		drawImage( &postAnimSprite, nextImageAlpha, buffer );
	}
}

void spriteAnimationEditor_IMGUIProcess( void )
{
	overDisplay = false;
	//llog( LOG_DEBUG, "Animation editor process" );
	struct nk_context* ctx = &( editorIMGUI.ctx );
	char titleBuffer[128] = "";
	if( ( currentFileName == NULL ) || ( SDL_strlen( currentFileName ) <= 0 ) ) {
		SDL_snprintf( titleBuffer, ARRAY_SIZE( titleBuffer ), "Sprite Animator ()" );
	} else {
		SDL_snprintf( titleBuffer, ARRAY_SIZE( titleBuffer ), "Sprite Animator (%s)", currentFileName );
	}
	if( nk_begin_titled( ctx, "Sprite Animator", titleBuffer, windowBounds, NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE) ) {

		windowBounds = nk_window_get_bounds( ctx );

		nk_menubar_begin( ctx ); {

			nk_layout_row_begin( ctx, NK_STATIC, STANDARD_ROW_HEIGHT, INT_MAX );
			nk_layout_row_push( ctx, 85 );
			if( nk_menu_begin_label( ctx, "File", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_vec2( 180, 170 ) ) ) {
				
				nk_layout_row_dynamic( ctx, STANDARD_ROW_HEIGHT, 1 );
				if( nk_menu_item_label( ctx, "New", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					newAnimation( );
				}

				if( nk_menu_item_label( ctx, "Load...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseLoadFileLocation( "Animated Sprite", "aspr", false, loadCurrentAnimation_OffMainThread );
				}

				if( nk_menu_item_label( ctx, "Save", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					if( currentFileName == NULL ) {
						editor_chooseSaveFileLocation( "Animated Sprite", "aspr", saveCurrentAnimation );
					} else {
						saveCurrentAnimation( currentFileName );
					}
				}

				if( nk_menu_item_label( ctx, "Save As...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE ) ) {
					editor_chooseSaveFileLocation( "Animated Sprite", "aspr", saveCurrentAnimation );
				}

				nk_menu_end( ctx );
			}
		} nk_menubar_end( ctx );

		float layoutHeight = nk_window_get_height( ctx ) - 130.0f;
		nk_layout_space_begin( ctx, NK_DYNAMIC, layoutHeight, INT_MAX ); {
			// general section, have tabs to show various things like file information and configuration options
			nk_flags groupFlags = NK_WINDOW_BORDER;// | NK_WINDOW_TITLE;
			nk_layout_space_push( ctx, nk_rect( 0.0f, 0.0f, 0.3f, 0.5f ) );
			if( nk_group_begin( ctx, "General", groupFlags ) ) {
				if( nk_tree_push( ctx, NK_TREE_TAB, "Animation", NK_MINIMIZED ) ) {
					// configuration of the animation
					
					//  fps
					nk_label( ctx, "FPS:", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );
					editorSpriteAnimation.fps = nk_propertyf( ctx, "FPS", 1.0f, editorSpriteAnimation.fps, 120.0f, 1.0f, 1.0f );

					//  duration in frames
					nk_label( ctx, "Duration in frames:", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );
					uint32_t newDuration = nk_propertyi( ctx, "Duration", 1, editorSpriteAnimation.durationFrames, INT_MAX, 1, 1 );
					if( newDuration < editorSpriteAnimation.durationFrames ) {
						// fewer frames, may have to remove some events and handle changing the selected event
						sprAnim_RemoveEventsPostFrame( &editorSpriteAnimation, &selectedEvent, newDuration - 1 );
					}
					editorSpriteAnimation.durationFrames = newDuration;

					// if it loops
					nk_bool loops = editorSpriteAnimation.loops;
					nk_checkbox_label( ctx, "Loops", &loops );
					editorSpriteAnimation.loops = loops ? true : false;

					// associated sprite sheet
					nk_label( ctx, "Sprite Sheet:", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );
					nk_label( ctx, editorSpriteAnimation.spriteSheetFile == NULL ? "NONE SELECTED" : editorSpriteAnimation.spriteSheetFile, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );
					if( nk_button_label( ctx, "Load different sprite sheet" ) ) {
						loadAndAssociateSpriteSheet( );
					}

					// onion skinning pre and post animation sprites
					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_labelf( ctx, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "-- Pre-Anim Preview --" );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					displayEventData_SwitchImageBase( ctx, &preAnimSprite );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_labelf( ctx, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "-- Post-Anim Preview --" );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					displayEventData_SwitchImageBase( ctx, &postAnimSprite );

					nk_tree_pop( ctx );
				}

				if( nk_tree_push( ctx, NK_TREE_TAB, "Config", NK_MINIMIZED ) ) {
					// general configuration stuff

					// sprite scale
					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_labelf( ctx, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "Sprite Draw Scale: %.2f", spriteScale );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					if( nk_slider_float( ctx, MIN_SCALE, &spriteScale, MAX_SCALE, 0.01f ) ) resetSaveCheckTime( );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					if( nk_button_label( ctx, "Reset Scale" ) ) {
						resetSaveCheckTime( );
						spriteScale = 1.0f;
					}

					if( nk_button_label( ctx, "Reset Center Offset" ) ) {
						drawnOffset.x = 0.0f;
						drawnOffset.y = 0.0f;
					}

					// onion skinning opacity
					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_labelf( ctx, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "Prev Image Alpha: %.2f", prevImageAlpha );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					if( nk_slider_float( ctx, 0.0f, &prevImageAlpha, 1.0f, 0.01f ) ) resetSaveCheckTime( );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_labelf( ctx, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "Next Image Alpha: %.2f", nextImageAlpha );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					if( nk_slider_float( ctx, 0.0f, &nextImageAlpha, 1.0f, 0.01f ) ) resetSaveCheckTime( );

					// background color
					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_label( ctx, "Background Color", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );

					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT * 4.0f, 1 );
					nk_layout_row_push( ctx, 1.0f );
					if( nk_color_pick( ctx, &spriteBGColor, NK_RGB ) ) resetSaveCheckTime( );

					// how many collision bounds we want to display
					nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
					nk_layout_row_push( ctx, 1.0f );
					nk_label( ctx, "Colliders to show:", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );
					int numCollidersToShow = (int)sb_Count( sbColliders );
					int newCollidersToShow = nk_propertyi( ctx, "Colliders", 0, numCollidersToShow, INT_MAX, 1, 1 );
					if( newCollidersToShow != numCollidersToShow ) {
						// resize the colliders list
						if( newCollidersToShow > numCollidersToShow ) {
							while( newCollidersToShow > (int)sb_Count( sbColliders ) ) {
								EditorCollider newCollider;
								newCollider.collider.type = CT_DEACTIVATED;
								newCollider.color = nk_color_cf( nk_rgb( 0, 0, 255 ) );

								sb_Push( sbColliders, newCollider );
								resetSaveCheckTime( );
							}
						} else {
							while( ( sb_Count( sbColliders ) > 0 ) && ( newCollidersToShow < (int)sb_Count( sbColliders ) ) ) {
								sb_Pop( sbColliders );
								resetSaveCheckTime( );
							}
						}
					}
					
					// what color each of the collision bounds is
					for( size_t i = 0; i < sb_Count( sbColliders ); ++i ) {
						nk_style_push_color( ctx, &ctx->style.text.color, nk_rgba_cf( sbColliders[i].color ) );
						char colliderName[25];
						SDL_snprintf( colliderName, 24, "Collider %i", i );
						nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT, 1 );
						nk_layout_row_push( ctx, 1.0f );
						nk_label( ctx, colliderName, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE );

						nk_layout_row_begin( ctx, NK_DYNAMIC, STANDARD_ROW_HEIGHT * 4.0f, 1 );
						nk_layout_row_push( ctx, 1.0f );
						// TODO: make the picker bigger, or have it just set RGB instead of using the picker
						if( nk_color_pick( ctx, &( sbColliders[i].color ), NK_RGB ) ) resetSaveCheckTime( );

						nk_style_pop_color( ctx );
					}

					nk_tree_pop( ctx );
				}
				
				nk_group_end( ctx );
			}

			// canvas for drawing the sprite
			nk_layout_space_push( ctx, nk_rect( 0.3f, 0.0f, 0.7f, 0.5f ) );

			nk_style_push_style_item( ctx, &ctx->style.window.fixed_background, nk_style_item_color( nk_rgba_cf( spriteBGColor ) ) );
			if( nk_group_begin( ctx, "Canvas", groupFlags ) ) {

				// this doesn't count the very edges, but that shouldn't cause issues
				struct nk_rect c = ctx->current->layout->clip;
				overDisplay = nk_input_is_mouse_hovering_rect( &ctx->input, c );

				drawCurrentImage( nk_window_get_canvas( ctx ) );
				drawCollisions( nk_window_get_canvas( ctx ) );
				drawCenterCrossHair( nk_window_get_canvas( ctx ) );
				nk_group_end( ctx );
			}
			nk_style_pop_style_item( ctx );

			// event and play controls
			nk_layout_space_push( ctx, nk_rect( 0.0f, 0.5f, 0.3f, 0.5f ) );
			if( nk_group_begin( ctx, "EventAndControls", groupFlags ) ) {
				// First the controls: play, pause, stop
				nk_layout_row_begin( ctx, NK_STATIC, STANRARD_BUTTON_SIZE, 3 );
				nk_layout_row_push( ctx, STANRARD_BUTTON_SIZE );
				if( nk_button_image( ctx, playNKImage ) ) {
					// if the animation isn't looping, and reached the end then restart it
					//  otherwise start playing from the current time
					playingAnimation = true;

					if( hitAnimationEnd ) {
						editorPlayingSprite.timePassed = 0.0f;
					}
					hitAnimationEnd = false;
				}

				if( nk_button_image( ctx, pauseNKImage ) ) {
					// pause the time advancement
					playingAnimation = false;
				}

				if( nk_button_image( ctx, stopNKImage ) ) {
					// pause and reset the time advancement
					playingAnimation = false;
					editorPlayingSprite.timePassed = 0.0f;
				}

				// then the information for the selected event
				if( selectedEvent != SIZE_MAX ) {
					if( displayEventData( ctx, selectedEvent ) ) {
						// update the display if there were any updates
						//  NOTE: collision may display oddly if you're modifying the same collider multiple times in a single frame
						reprocessFrames( );
					}
				}

				nk_group_end( ctx );
			}

			// timeline
			nk_layout_space_push( ctx, nk_rect( 0.3f, 0.5f, 0.7f, 0.5f ) );
			if( nk_group_begin( ctx, "Timeline", groupFlags ) ) {

				const float FRAME_DIM = 34.0f;

				// calculate the width of the component
				float width = editorSpriteAnimation.durationFrames * FRAME_DIM;

				// at the top draw the time indicator
				//nk_layout_row_begin( ctx, NK_DYNAMIC, 17.0f, 1 );
				nk_layout_row_begin( ctx, NK_STATIC, 17.0f, 1 );
				nk_layout_row_push( ctx, width );
				float percentDone = editorPlayingSprite.timePassed / sprAnim_GetTotalTime( &editorSpriteAnimation );
				if( nk_slider_float( ctx, 0.0f, &percentDone, 1.0f, 0.0001f ) ) {
					playingAnimation = false;

					// go through and process all the events until the frame it's currently in
					editorPlayingSprite.timePassed = percentDone * sprAnim_GetTotalTime( &editorSpriteAnimation );
					reprocessFrames( );
				}

				// draw the frame events as a grid
				// then draw a horizontal set of frames that hold the events
				nk_style_push_vec2( ctx, &ctx->style.window.spacing, nk_vec2( 0.0f, 0.0f ) ); // no seperation between rows and columns
				nk_layout_row_begin( ctx, NK_STATIC, FRAME_DIM, INT_MAX );
				nk_layout_row_push( ctx, FRAME_DIM );

				// first draw the frame number
				uint32_t currFrame = sprAnim_GetCurrentFrame( &editorPlayingSprite );
				currFrame = MIN( currFrame, editorSpriteAnimation.durationFrames - 1 );
				for( uint32_t f = 0; f < editorSpriteAnimation.durationFrames; ++f ) {
					const char* prefix = "";
					if( f == currFrame ) {
						prefix = "*";
					}
					nk_labelf( ctx, NK_TEXT_CENTERED, "%s%u", prefix, f );
				}

				nk_layout_row_begin( ctx, NK_STATIC, FRAME_DIM, INT_MAX );
				nk_layout_row_push( ctx, FRAME_DIM );
				// then draw the add event buttons
				for( uint32_t f = 0; f < editorSpriteAnimation.durationFrames; ++f ) {
					
					if( nk_button_label( ctx, "+" ) ) {
						// add a new event on this frame
						addDefaultEvent( f );
					}
				}

				// now do all the rows of events
				uint32_t maxHeight = getAnimGridHeight( );
				for( uint32_t h = 0; h < maxHeight; ++h ) {
					nk_layout_row_begin( ctx, NK_STATIC, FRAME_DIM, INT_MAX );
					nk_layout_row_push( ctx, FRAME_DIM );
					for( uint32_t f = 0; f < editorSpriteAnimation.durationFrames; ++f ) {

						size_t eventIdx = getAnimEventIndex( f, h );
						if( eventIdx == SIZE_MAX ) {
							nk_label( ctx, " ", NK_TEXT_CENTERED );
						} else {

							AnimEventTypes type = editorSpriteAnimation.sbEvents[eventIdx].base.type;
							const char* label = eventDisplays[type].label;
							struct nk_color clr = eventDisplays[type].clr;
							nk_style_push_color( ctx, &ctx->style.button.border_color, clr );
							
							bool popStyle = false;
							if( eventIdx == selectedEvent ) {
								popStyle = true;
								nk_style_push_style_item( ctx, &ctx->style.button.normal, nk_style_item_color( nk_rgb( 255, 255, 255 ) ) );
								nk_style_push_style_item( ctx, &ctx->style.button.active, nk_style_item_color( nk_rgb( 255, 255, 255 ) ) );
								nk_style_push_style_item( ctx, &ctx->style.button.hover, nk_style_item_color( nk_rgb( 255, 255, 255 ) ) );
							}
							if( nk_button_label( ctx, label ) ) {
								// select this event as the current one to edit, or deselect if it's the current one
								if( selectedEvent == eventIdx ) {
									selectedEvent = SIZE_MAX;
								} else {
									selectedEvent = eventIdx;
								}
							}
							if( popStyle ) {
								nk_style_pop_style_item( ctx );
								nk_style_pop_style_item( ctx );
								nk_style_pop_style_item( ctx );
							}
							nk_style_pop_color( ctx );
						}
					}
				}

				nk_style_pop_vec2( ctx );

				nk_group_end( ctx );
			}
		} nk_layout_space_end( ctx );
	} nk_end( ctx );
}

void spriteAnimationEditor_Tick( float dt )
{
	if( playingAnimation ) {
		sprAnim_ProcessAnim( &editorPlayingSprite, &editorEventHandler, dt );
	}

	if( saveCheckCountdown > 0.0f ) {
		saveCheckCountdown -= dt;
		if( saveCheckCountdown <= 0.0f ) {
			saveAnimEditorConfig( );
		}
	}
}

void spriteAnimationEditor_ProcessEvents( SDL_Event* evt )
{
	//movingDrawnOffset = false;

	// if we're hovering over the sprite display then process some inputs
	if( overDisplay ) {
		switch( evt->type ) {
		case SDL_EVENT_MOUSE_WHEEL:
			spriteScale = clamp( MIN_SCALE, MAX_SCALE, spriteScale + ( evt->wheel.y * SCALE_WHEEL_AMT ) );
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if( evt->button.button == SDL_BUTTON_RIGHT ) {
				movingDrawnOffset = true;
			}
			break;
		}
	}

	switch( evt->type ) {
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if( evt->button.button == SDL_BUTTON_RIGHT ) {
			movingDrawnOffset = false;
		}
		break;
	}

	if( movingDrawnOffset && ( evt->type == SDL_EVENT_MOUSE_MOTION ) ) {
		drawnOffset.x += evt->motion.xrel * 1.5f;
		drawnOffset.y += evt->motion.yrel * 1.5f;
	}

	// TODO: undo and redo
	/*if( evt->type == SDL_EVENT_KEY_DOWN ) {
		// ctrl-z
		if( ( evt->key.key == SDLK_Z ) && ( evt->key.mod & SDL_KMOD_CTRL ) ) {
			// undo
		}

		// ctrl-y
		if( ( evt->key.key == SDLK_Y ) && ( evt->key.mod & SDL_KMOD_CTRL ) ) {
			// redo
		}
	}//*/
}