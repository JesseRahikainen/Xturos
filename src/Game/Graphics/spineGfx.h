#ifndef SPINE_GFX_H
#define SPINE_GFX_H

#include <stdint.h>
#include <spine/spine.h>
#include "../Math/vector2.h"
#include "../Graphics/color.h"

// general system functions
void spine_Init( void );

void spine_CleanEverything( void );

// template handling
/*
Loads a set of spine files. Assumes there's three files: fileNameBase.json, fileNameBase.atlas, and fileNameBase.png.
 The template is created from these three files.
 Returns the index of the template if the loading was successfull, -1 if it was not.
*/
int spine_LoadTemplate( const char* fileNameBase );

/*
Cleans up a template, freeing it's spot.
 Also checks to see if there are any existing instances using this template.
*/
void spine_CleanTemplate( int idx );

/*
Sets the duration of the blending between the two animations. The templateIdx passed in should be a
value returned from spine_LoadTemplate that hasn't been cleaned up yet.
*/
void spine_SetTemplateMix( int templateIdx, spAnimation* from, spAnimation* to, float duration );

/*
Sets the duration of the blending between the fromName to the toName animation. The templateIdx passed in should be a
value returned from spine_LoadTemplate that hasn't been cleaned up yet.
*/
void spine_SetTemplateMixByName( int templateIdx, const char* fromName, const char* toName, float duration );

// instance handling
/*
Creates an instance of a template. The templateIdx passed in should be a value returns from spine_LoadTemplate that
 hasn't been cleaned up.
Returns an id to use in other functions. Returns -1 if there's a problem.
*/
int spine_CreateInstance( int templateIdx, Vector2 pos, int cameraFlags, char depth, spAnimationStateListener listener, void* object );

/*
Cleans up a spine instance.
*/
void spine_CleanInstance( int id );

/*
Cleans up all instances.
*/
void spine_CleanAllInstances( void );

/*
Sets the future position of the spine instance.
*/
void spine_SetInstancePosition( int id, const Vector2* pos );

/*
Flips the positions used for rendering the instances.
*/
void spine_FlipInstancePositions( void );

/*
Returns the skeleton data for the template, if theres'a an issue returns NULL.
*/
spSkeletonData* spine_GetTemplateSkeletonData( int id );

/*
Returns the skeleton of the spine instance, if there's an issue returns NULL.
 Note: Adjustments to the skeletons x and y are overwritten in spine_RenderInstances( ).
*/
spSkeleton* spine_GetInstanceSkeleton( int id );

/*
Returns the animation state of the spine instance, if there's an issue returns NULL.
*/
spAnimationState* spine_GetInstanceAnimState( int id );

/*
Updates all the instance animations.
*/
void spine_UpdateInstances( float dt );

/*
Draws all the spine instances.
*/
void spine_RenderInstances( float normTimeElapsed );

#endif