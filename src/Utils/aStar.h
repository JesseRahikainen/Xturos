#ifndef A_STAR_H
#define A_STAR_H

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

// generic implementation of A*
typedef struct {
	int loc;
	float cost;
} AStarFrontierData;

typedef struct {
	int from;
	float cost;
} AStarPathData;

typedef float (*AStar_CostFunc)( void* graph, int fromNodeID, int toNodeID );

// uses -1 as a flag value for no neighbor, it's passed into currNeighborNodeID at the start and should
//  be returned by it when there are no more neighbors
typedef int (*AStar_GetNextNeighborFunc)( void* graph, int nodeID, int currNeighborNodeID );

typedef struct {
	AStarFrontierData* sbFrontier;
	AStarPathData* sbPath;

	int startNodeID;
	int targetNodeID;

	void* graph;

	AStar_CostFunc moveCost;
	AStar_CostFunc heuristic;
	AStar_GetNextNeighborFunc nextNeighbor;
} AStarSearchState;

/*
General usage:
	AStarSearchState searchState;
	aStar_CreateSearchState( graph, graphSize, nodeDist, nodeDist, nextNeighbor, &searchState );
	int* sbPath = NULL;
	if( ( aStar_ProcessPath( &searchState, -1, &sbPath ) ) && ( sbPath != NULL ) ) {
		processPath( sbPath );
		sb_Release( sbPath );
	}
	aStar_CleanUpSearchState( &searchState );
*/

// creates a search state we can send to process and extract, we should call aStar_CleanUpSearchState
//  after we're done with it to clean up any memory we have allocated here
// NOTE: nodeCount has to be greater than the highest node id possible for graph to return
void aStar_CreateSearchState( void* graph, size_t nodeCount, int startNodeID, int targetNodeID,
	AStar_CostFunc moveCost, AStar_CostFunc heuristic, AStar_GetNextNeighborFunc nextNeighbor,
	AStarSearchState* outState );

// in case we need to test for this, if it's invalid aStar_ProcessPath will handle it by returning
//  an empty path
bool aStar_IsValid( AStarSearchState* state );

// process numSteps nodes in the search state, pass in -1 for numSteps to go until the end
//  returns whether processing is done and puts the resulting path into a stretchy buffer in sbOutPaths
bool aStar_ProcessPath( AStarSearchState* state, int numSteps, int** sbOutPaths );

// cleans up all the extra data created by the search state
void aStar_CleanUpSearchState( AStarSearchState* state );

#endif /* inclusion guard */