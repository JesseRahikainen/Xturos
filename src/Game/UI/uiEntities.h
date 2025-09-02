#ifndef UI_ENTITIES_H
#define UI_ENTITIES_H

#include "Math/vector2.h"
#include "Graphics/color.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "UI/text.h"

// Labels
EntityID createLabel( ECPS* ecps, const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, float fontSize, int camFlags, int8_t depth, bool useTextArea );

void setTextString( ECPS* ecps, EntityID entity, const char* utf8Str );

// Buttons
EntityID button_CreateEmpty( ECPS* ecps, Vector2 position, Vector2 size, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse );

EntityID button_CreateImageButton( ECPS* ecps, Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int imgID, Color imgColor, unsigned int camFlags, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse );

EntityID button_Create3x3Button( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int img, uint32_t topBorder, uint32_t bottomBorder, uint32_t leftBorder, uint32_t rightBorder, Color imgColor, unsigned int camFlags, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse );

EntityID button_Create3x3ScriptButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int img, uint32_t topBorder, uint32_t bottomBorder, uint32_t leftBorder, uint32_t rightBorder,
	Color imgColor, unsigned int camFlags, int8_t depth, const char* pressResponse, const char* releaseResponse, const char* overResponse, const char* leaveResponse );

EntityID button_CreateTextButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	unsigned int camFlags, int8_t depth, 
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse );

EntityID button_CreateImageOnlyButton( ECPS* ecps, Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	int imgID, Color imgColor, uint32_t camFlags, int8_t depth,
	TrackedCallback pressResponse, TrackedCallback releaseResponse, TrackedCallback overResponse, TrackedCallback leaveResponse );

#endif // inclusion guard