#include "uiEntities.h"

#include <stdint.h>
#include <memory.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "Graphics/images.h"
#include "DefaultECPS/generalComponents.h"

EntityID createLabel( ECPS* ecps, const char* utf8Str, Vector2 position, Color fontColor, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, float fontPixelSize, int camFlags, int8_t depth, bool useTextArea )
{
	// calculate the size
	Vector2 size;
	txt_CalculateStringRenderSize( utf8Str, fontID, fontPixelSize, &size );

	GCTransformData tf = gc_CreateTransformPos( position );

	GCColorData clr;
	clr.currClr = fontColor;
	clr.futureClr = fontColor;

	GCTextData textData;
	textData.camFlags = camFlags;
	textData.depth = depth + 1;
	textData.fontID = fontID;
	textData.pixelSize = fontPixelSize;
	textData.text = (uint8_t*)createStringCopy( utf8Str );
	textData.textIsDynamic = true;
	textData.horizAlign = hAlign;
	textData.vertAlign = vAlign;
	textData.useTextArea = useTextArea;

	GCSizeData sizeData;
	sizeData.currentSize = sizeData.futureSize = size;

	return ecps_CreateEntity( ecps, 4,
		gcTransformCompID, &tf,
		gcTextCompID, &textData,
		gcSizeCompID, &sizeData,
		gcClrCompID, &clr );
}
// TODO: Animate the button states.

void setTextString( ECPS* ecps, EntityID entity, const char* utf8Str )
{
	GCTextData* txt = NULL;
	GCSizeData* size = NULL;

	if( !ecps_GetComponentFromEntityByID( ecps, entity, gcTextCompID, &txt ) ) {
		return;
	}

	if( txt->textIsDynamic ) {
		mem_Release( txt->text );
	}
	txt->text = (uint8_t*)createStringCopy( utf8Str );

	if( ecps_GetComponentFromEntityByID( ecps, entity, gcSizeCompID, &size ) ) {
		Vector2 calcSize;
		txt_CalculateStringRenderSize( utf8Str, txt->fontID, txt->pixelSize, &calcSize );
		size->currentSize = size->futureSize = calcSize;
	}
}


EntityID button_CreateEmpty( ECPS* ecps, Vector2 position, Vector2 size, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse )
{
	GCTransformData tf = gc_CreateTransformPos( position );

	GCPointerCollisionData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &size, 0.5f, &( ptr.collisionHalfDim ) );
	ptr.priority = 1;

	GCPointerResponseData response;
	response.leaveResponse = createSourceCallback( leaveResponse );
	response.overResponse = createSourceCallback( overResponse );
	response.pressResponse = createSourceCallback( pressResponse );
	response.releaseResponse = createSourceCallback( releaseResponse );

	EntityID buttonID = ecps_CreateEntity( ecps, 3,
		gcTransformCompID, &tf,
		gcPointerCollisionCompID, &ptr,
		gcPointerResponseCompID, &response );

	return buttonID;
}

EntityID button_CreateImageButton( ECPS* ecps, Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int imgID, Color imgColor, unsigned int camFlags, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse )
{
	// first create the text entity
	EntityID textID = INVALID_ENTITY_ID;
	{
		GCTransformData tf = gc_CreateTransformPos( position );

		GCTextData textData;
		textData.camFlags = camFlags;
		textData.depth = depth + 1;
		textData.fontID = fontID;
		textData.pixelSize = fontPixelSize;
		textData.text = (uint8_t*)createStringCopy( text );
		textData.textIsDynamic = true;
		textData.horizAlign = HORIZ_ALIGN_CENTER;
		textData.vertAlign = VERT_ALIGN_CENTER;

		GCColorData clr;
		clr.currClr = clr.currClr = fontColor;

		GCSizeData sizeData;
		sizeData.currentSize = sizeData.futureSize = normalSize;

		textID = ecps_CreateEntity( ecps, 4,
			gcTransformCompID, &tf,
			gcTextCompID, &textData,
			gcSizeCompID, &sizeData,
			gcClrCompID, &clr );
	}

	ASSERT_AND_IF_NOT( textID != INVALID_ENTITY_ID )
	{
		return INVALID_ENTITY_ID;
	}

	// then create the button background entity
	Vector2 imgSize;
	img_GetSize( imgID, &imgSize );

	Vector2 scale = vec2( normalSize.x / imgSize.x, normalSize.y / imgSize.y );
	GCTransformData tf = gc_CreateTransformPosScale( position, scale );

	GCColorData clr;
	clr.currClr = clr.futureClr = imgColor;

	GCPointerCollisionData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &normalSize, 0.5f, &( ptr.collisionHalfDim ) );
	ptr.priority = 1;

	GCPointerResponseData response;
	response.leaveResponse = createSourceCallback( leaveResponse );
	response.overResponse = createSourceCallback( overResponse );
	response.pressResponse = createSourceCallback( pressResponse );
	response.releaseResponse = createSourceCallback( releaseResponse );

	GCSpriteData sprite;
	sprite.camFlags = camFlags;
	sprite.depth = depth;
	sprite.img = imgID;

	EntityID buttonID = ecps_CreateEntity( ecps, 5,
		gcSpriteCompID, &sprite,
		gcTransformCompID, &tf,
		gcClrCompID, &clr,
		gcPointerCollisionCompID, &ptr,
		gcPointerResponseCompID, &response );

	gc_MountEntity( ecps, buttonID, textID );

	return buttonID;
}

static EntityID create3x3Button( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int img, uint32_t topBorder, uint32_t bottomBorder, uint32_t leftBorder, uint32_t rightBorder,
	Color imgColor, unsigned int camFlags, int8_t depth )
{
	// first create the text entity
	EntityID textID = INVALID_ENTITY_ID;
	{
		GCTransformData tf = gc_CreateTransformPos( position );

		GCTextData textData;
		textData.camFlags = camFlags;
		textData.depth = depth + 1;
		textData.fontID = fontID;
		textData.pixelSize = fontPixelSize;
		textData.text = (uint8_t*)createStringCopy( text );
		textData.textIsDynamic = true;
		textData.horizAlign = HORIZ_ALIGN_CENTER;
		textData.vertAlign = VERT_ALIGN_CENTER;

		GCColorData clr;
		clr.currClr = clr.currClr = fontColor;

		GCSizeData sizeData;
		sizeData.currentSize = sizeData.futureSize = size;

		textID = ecps_CreateEntity( ecps, 4,
			gcTransformCompID, &tf,
			gcTextCompID, &textData,
			gcSizeCompID, &sizeData,
			gcClrCompID, &clr );
	}

	ASSERT_AND_IF_NOT( textID != INVALID_ENTITY_ID )
	{
		return INVALID_ENTITY_ID;
	}

	// then create the button background entity
	GCTransformData tf = gc_CreateTransformPos( position );

	GCColorData clr;
	clr.currClr = clr.futureClr = imgColor;

	GCPointerCollisionData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &size, 0.5f, &( ptr.collisionHalfDim ) );
	ptr.priority = 1;

	GC3x3SpriteData sprite;
	sprite.camFlags = camFlags;
	sprite.depth = depth;
	sprite.img = img;
	sprite.leftBorder = leftBorder;
	sprite.rightBorder = rightBorder;
	sprite.topBorder = topBorder;
	sprite.bottomBorder = bottomBorder;
	sprite.size = size;

	EntityID buttonID = ecps_CreateEntity( ecps, 4,
		gc3x3SpriteCompID, &sprite,
		gcTransformCompID, &tf,
		gcClrCompID, &clr,
		gcPointerCollisionCompID, &ptr );

	gc_MountEntity( ecps, buttonID, textID );

	return buttonID;
}

EntityID button_Create3x3Button( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int img, uint32_t topBorder, uint32_t bottomBorder, uint32_t leftBorder, uint32_t rightBorder, Color imgColor, unsigned int camFlags, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse )
{
	EntityID buttonID = create3x3Button( ecps, position, size, text, fontID, fontPixelSize, fontColor, textOffset, img, topBorder, bottomBorder, leftBorder, rightBorder, imgColor, camFlags, depth );
	if( buttonID != INVALID_ENTITY_ID ) {
		GCPointerResponseData response;
		response.leaveResponse = createSourceCallback( leaveResponse );
		response.overResponse = createSourceCallback( overResponse );
		response.pressResponse = createSourceCallback( pressResponse );
		response.releaseResponse = createSourceCallback( releaseResponse );

		ecps_AddComponentToEntityByID( ecps, buttonID, gcPointerResponseCompID, &response );
	}
	return buttonID;
}

EntityID button_Create3x3ScriptButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int img, uint32_t topBorder, uint32_t bottomBorder, uint32_t leftBorder, uint32_t rightBorder,
	Color imgColor, unsigned int camFlags, int8_t depth, const char* pressResponse, const char* releaseResponse, const char* overResponse, const char* leaveResponse )
{
	EntityID buttonID = create3x3Button( ecps, position, size, text, fontID, fontPixelSize, fontColor, textOffset, img, topBorder, bottomBorder, leftBorder, rightBorder, imgColor, camFlags, depth );
	if( buttonID != INVALID_ENTITY_ID ) {
		GCPointerResponseData response;
		response.leaveResponse = createScriptCallback( overResponse );
		response.overResponse = createScriptCallback( leaveResponse );
		response.pressResponse = createScriptCallback( pressResponse );
		response.releaseResponse = createScriptCallback( releaseResponse );

		ecps_AddComponentToEntityByID( ecps, buttonID, gcPointerResponseCompID, &response );
	}
	return buttonID;
}

EntityID button_CreateTextButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	unsigned int camFlags, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse )
{
	GCTransformData tf = gc_CreateTransformPos( position );

	GCTextData textData;
	textData.camFlags = camFlags;
	textData.depth = depth + 1;
	textData.fontID = fontID;
	textData.pixelSize = fontPixelSize;
	textData.text = (uint8_t*)createStringCopy( text );
	textData.textIsDynamic = true;
	textData.horizAlign = HORIZ_ALIGN_CENTER;
	textData.vertAlign = VERT_ALIGN_CENTER;

	GCSizeData sizeData;
	sizeData.currentSize = sizeData.futureSize = size;

	GCColorData clr;
	clr.currClr = clr.currClr = fontColor;

	GCPointerCollisionData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &size, 0.5f, &( ptr.collisionHalfDim ) );
	ptr.priority = 1;

	GCPointerResponseData response;
	response.leaveResponse = createSourceCallback( leaveResponse );
	response.overResponse = createSourceCallback( overResponse );
	response.pressResponse = createSourceCallback( pressResponse );
	response.releaseResponse = createSourceCallback( releaseResponse );

	EntityID buttonID = ecps_CreateEntity( ecps, 6,
		gcTransformCompID, &tf,
		gcTextCompID, &textData,
		gcSizeCompID, &sizeData,
		gcPointerCollisionCompID, &ptr,
		gcPointerResponseCompID, &response,
		gcClrCompID, &clr );

	return buttonID;
}

EntityID button_CreateImageOnlyButton( ECPS* ecps, Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	int imgID, Color imgColor, uint32_t camFlags, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse )
{
	Vector2 scale;
	img_GetDesiredScale( imgID, normalSize, &scale );
	GCTransformData tf = gc_CreateTransformPosScale( position, scale );

	GCColorData clr;
	clr.currClr = clr.futureClr = imgColor;

	GCPointerCollisionData ptr;
	ptr.camFlags = camFlags;
	vec2_Scale( &normalSize, 0.5f, &( ptr.collisionHalfDim ) );
	ptr.priority = depth;

	GCPointerResponseData response;
	response.leaveResponse = createSourceCallback( leaveResponse );
	response.overResponse = createSourceCallback( overResponse );
	response.pressResponse = createSourceCallback( pressResponse );
	response.releaseResponse = createSourceCallback( releaseResponse );

	GCSpriteData sprite;
	sprite.camFlags = camFlags;
	sprite.depth = depth;
	sprite.img = imgID;

	EntityID buttonID = ecps_CreateEntity( ecps, 5,
		gcSpriteCompID, &sprite,
		gcTransformCompID, &tf,
		gcClrCompID, &clr,
		gcPointerCollisionCompID, &ptr,
		gcPointerResponseCompID, &response );

	return buttonID;
}