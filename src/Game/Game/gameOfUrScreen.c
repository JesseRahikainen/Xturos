#include "gameOfUrScreen.h"

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../Input/input.h"
#include "../System/platformLog.h"

#include "../Utils/helpers.h"
#include "../Utils/stretchyBuffer.h"
#include "../System/random.h"

#include "../UI/button.h"

#include "../gameState.h"

#include "labels.h"

#include "../System/jobQueue.h"

// https://www.youtube.com/watch?v=WZskjLq040I
//  basic rules:
//   roll the dice
//   choose to use an existing piece or place a new piece
//   move the chosen piece as many spaces as you've rolled
//   a piece can't end up on top of another piece
//   placing on the board is one move (so if you roll 3 you place and move two)
//   at the end of the path remove the piece, you have to roll the exact number to move them off the board (so if there are two spaces left to
//    move you have to roll three, one for each space and the final to remove)
//   if there is no valid move you can make your turn is skipped
//   7 pieces per player

#define MCTS_NUM_PLAYERS 2
#define NUM_PIECES 7

/*
[ 6][ 4][ 2][ 0]        [18][16]
[ 8][ 9][10][11][12][13][14][15]
[ 7][ 5][ 3][ 1]        [19][17]
Spots 6, 7, 11, 18, and 19 are safe from capture and moving a piece onto it gives you another turn.
To move a piece off you have to roll the number of spaces until the end plus one (so if at spot 15 you have to
roll a 3). If you roll higher than that you can not move the piece off the board.
Essentially you can count the board as having two phantom spots. A -1 where pieces are stored when not on the board
yet (and moved back to when captured) and 20 where the pieces that have been moved off the board are stored. If
a player has all their pieces in spot 20 they win. Using this idea -1 and 20 can both store more than one piece at
a time.
*/

typedef struct {
	int8_t pieces[MCTS_NUM_PLAYERS][NUM_PIECES]; // [player][piece]
	uint8_t currPlayer : 1;
	int8_t roll;
} BoardState;

typedef enum {
	MT_ROLL,
	MT_PIECE,
	MT_SKIP
} MoveType;

typedef struct {
	MoveType type : 1;
	int8_t piece;
	uint8_t player : 1;
} PieceMove;

typedef struct {
	MoveType type : 1;
	uint8_t player : 1;
} RollMove;

typedef struct {
	MoveType type : 1;
	uint8_t player : 1;
} SkipMove;

typedef union {
	MoveType type : 3;
	PieceMove piece;
	RollMove roll;
	SkipMove skip;
} Move;

#define MCTS_MOVE Move
#define MCTS_BOARD_STATE BoardState
#define MCTS_MAX_STEPS 10000

static BoardState initialBoardState( void )
{
	BoardState state;

	state.currPlayer = 0;
	state.roll = -1;
	for( int p = 0; p < NUM_PIECES; ++p ) {
		for( int i = 0; i < MCTS_NUM_PLAYERS; ++i ) {
			state.pieces[i][p] = -1;
		}
	}

	return state;
}

#define MCTS_OPEN_LOOP
#include "../Utils/MCTS.c"

static bool isSpaceMarked( int8_t pos )
{
	return ( pos == 6 ) || ( pos == 7 ) || ( pos == 11 ) || ( pos == 18 ) || ( pos == 19 );
}

static int8_t movePiece( uint8_t player, int8_t currPos, int8_t amt )
{
	/*
[ 6][ 4][ 2][ 0]        [18][16]
[ 8][ 9][10][11][12][13][14][15]
[ 7][ 5][ 3][ 1]        [19][17]
	*/
	while( amt > 0 ) {
		if( currPos == -1 ) {
			if( player == 0 ) {
				currPos = 0;
			} else {
				currPos = 1;
			}
		} else if( currPos < 7 ) {
			currPos += 2;
		} else if( currPos == 15 ) {
			if( player == 0 ) {
				currPos = 16;
			} else {
				currPos = 17;
			}
		} else if( ( currPos == 18 ) || ( currPos == 19 ) ) {
			currPos = 20;
		} else if( currPos > 15 ) {
			currPos += 2;
		} else {
			currPos += 1;
		}

		--amt;
	}

	return currPos;
}

static int8_t rollDice( void )
{
	// four tetrahedrons each with two colored tips, count up the number of colored tips shown to get the number rolled
	int8_t total = ( rand_GetU16( NULL ) % 2 ) + ( rand_GetU16( NULL ) % 2 ) + ( rand_GetU16( NULL ) % 2 ) + ( rand_GetU16( NULL ) % 2 );
	return total;
}

void ur_applyMove( BoardState* oldState, Move* move, BoardState* newStateOut )
{
	assert( oldState != NULL );
	assert( move != NULL );
	assert( newStateOut != NULL );

	memcpy( newStateOut, oldState, sizeof( BoardState ) );

	switch( move->type ) {
	case MT_ROLL:
		newStateOut->roll = rollDice( );
		break;
	case MT_PIECE:
		newStateOut->pieces[move->piece.player][move->piece.piece] = movePiece( move->piece.player, newStateOut->pieces[move->piece.player][move->piece.piece], oldState->roll );

		// if the opposing player has any pieces on that spot, return them to the start
		if( newStateOut->pieces[move->piece.player][move->piece.piece] != 20 ) {
			int otherPlayer = 1 - move->piece.player;
			for( int i = 0; i < NUM_PIECES; ++i ) {
				if( newStateOut->pieces[move->piece.player][move->piece.piece] == newStateOut->pieces[otherPlayer][i] ) {
					newStateOut->pieces[otherPlayer][i] = -1;
				}
			}
		}

		// moving onto a marked space gives the player another roll
		if( !isSpaceMarked( newStateOut->pieces[move->piece.player][move->piece.piece] ) ) {
			newStateOut->currPlayer = 1 - newStateOut->currPlayer;
		}
		newStateOut->roll = -1;

		break;
	case MT_SKIP:
		newStateOut->roll = -1;
		newStateOut->currPlayer = 1 - newStateOut->currPlayer;
		break;
	}
}

int ur_getCurrentPlayer( BoardState* state )
{
	assert( state != NULL );

	return state->currPlayer;
}

void getRollMoveList( BoardState* state, Move** sbMoveList_Out )
{
	Move rollMove;
	rollMove.type = MT_ROLL;
	rollMove.roll.player = state->currPlayer;

	sb_Push( ( *sbMoveList_Out ), rollMove );
}

void getPieceMoveList( BoardState* state, Move** sbMoveList_Out )
{
	uint8_t player = state->currPlayer;
	uint8_t otherPlayer = 1 - player;
	int8_t roll = state->roll;

	if( roll > 0 ) {
		bool addedInitialMove = false;
		for( int8_t i = 0; i < NUM_PIECES; ++i ) {
			// check to see if the movement for this piece is legal
			//  we also only list one that's at -1
			int8_t testPos = movePiece( player, state->pieces[player][i], roll );

			// if a piece would move over a piece of the same player
			// or over a marked spot that has any piece on it
			// or too far off the board (20 is the storage pos for the pieces that have moved off the board, any more than this and it's an invalid move)
			// then it isn't a valid move
			bool isPastEnd = testPos > 20;
			bool isOverOwn = false;
			bool isOverOtherSafe = false;
			bool isInPen = false; // pen is -1, the spot where pieces are stored before being moved onto the board
			if( testPos < 20 ) {
				for( int a = 0; ( a < NUM_PIECES ) && !isOverOwn && !isOverOtherSafe; ++a ) {
					if( ( state->pieces[player][a] == testPos ) && ( a != i ) ) {
						isOverOwn = true;
					}
					if( ( state->pieces[otherPlayer][a] == testPos ) && isSpaceMarked( testPos ) ) {
						isOverOtherSafe = true;
					}
				}
			}

			if( state->pieces[player][i] == -1 ) {
				isInPen = true;
			}

			if( !isPastEnd && !isOverOwn && !isOverOtherSafe && !( isInPen && addedInitialMove ) ) {
				Move m;
				m.type = MT_PIECE;
				m.piece.piece = i;
				m.piece.player = player;
				sb_Push( ( *sbMoveList_Out ), m );

				if( isInPen ) {
					addedInitialMove = true;
				}
			}
		}
	}

	if( sb_Count( *sbMoveList_Out ) <= 0 ) {
		// no valid moves, add a skip action
		Move m;
		m.type = MT_SKIP;
		m.skip.player = player;
		sb_Push( ( *sbMoveList_Out ), m );
	}
}

// puts the moves into sbMoveList_Out
void ur_getPossibleMoveList( BoardState* state, Move** sbMoveList_Out )
{
	assert( state != NULL );
	assert( sbMoveList_Out != NULL );

	if( state->roll < 0 ) {
		getRollMoveList( state, sbMoveList_Out );
	} else {
		getPieceMoveList( state, sbMoveList_Out );
	}
}

// returns either 0 or 1 if there is a winner, -1 if there isn't
int ur_getWinner( BoardState* state )
{
	assert( state != NULL );

	int winner = -1;
	int count[2] = { 0, 0 };

	for( int i = 0; i < NUM_PIECES; ++i ) {
		count[0] += ( state->pieces[0][i] >= 20 ) ? 1 : 0;
		count[1] += ( state->pieces[1][i] >= 20 ) ? 1 : 0;
	}

	if( ( count[0] >= NUM_PIECES ) && ( count[1] >= NUM_PIECES ) ) {
		return 2;
	} else if( count[0] >= NUM_PIECES ) {
		return 0;
	} else if( count[1] >= NUM_PIECES ) {
		return 1;
	}

	return -1;
}

static MCTSGameDefinition urDefinition = { ur_applyMove, ur_getPossibleMoveList, ur_getCurrentPlayer, ur_getWinner };

//********************************************************

typedef enum {
	PT_HUMAN,
	PT_AI
} PlayerType;

static int boardImg = -1;
static int pieceImg = -1;
static int headProfileImg = -1;
static int gearImg = -1;
static int whiteBoxImg = -1;
static int font = -1;
static int* buttonImg;
static int hiliteImg = -1;

static int lastWinner = -1;

static PlayerType playerTypes[2];

static BoardState currBoardState;
static Move currMove;

static float gearRot = 0.0f;

// state predefines
static int start_Enter( void );
static int start_Exit( void );
static void start_ProcessEvents( SDL_Event* e );
static void start_Process( void );
static void start_Draw( void );
static void start_PhysicsTick( float dt );

static int humanChooseMove_Enter( void );
static int humanChooseMove_Exit( void );
static void humanChooseMove_ProcessEvents( SDL_Event* e );
static void humanChooseMove_Process( void );
static void humanChooseMove_Draw( void );
static void humanChooseMove_PhysicsTick( float dt );

static int aiChooseMove_Enter( void );
static int aiChooseMove_Exit( void );
static void aiChooseMove_ProcessEvents( SDL_Event* e );
static void aiChooseMove_Process( void );
static void aiChooseMove_Draw( void );
static void aiChooseMove_PhysicsTick( float dt );

static int animate_Enter( void );
static int animate_Exit( void );
static void animate_ProcessEvents( SDL_Event* e );
static void animate_Process( void );
static void animate_Draw( void );
static void animate_PhysicsTick( float dt );

static GameState startState = { start_Enter, start_Exit, start_ProcessEvents,
	start_Process, start_Draw, start_PhysicsTick };
static GameState humanChooseMoveState = { humanChooseMove_Enter, humanChooseMove_Exit, humanChooseMove_ProcessEvents,
	humanChooseMove_Process, humanChooseMove_Draw, humanChooseMove_PhysicsTick };
static GameState aiChooseMoveState = { aiChooseMove_Enter, aiChooseMove_Exit, aiChooseMove_ProcessEvents,
	aiChooseMove_Process, aiChooseMove_Draw, aiChooseMove_PhysicsTick };
static GameState animateState = { animate_Enter, animate_Exit, animate_ProcessEvents,
	animate_Process, animate_Draw, animate_PhysicsTick };

static GameStateMachine gameSM;

static Color playerClrs[2] = {
	{ 0.75f, 0.75f, 0.75f, 1.0f },
	{ 0.25f, 0.25f, 0.25f, 1.0f }
};

static Vector2 basePenDraw[2] = {
	{ 320.0f, 65.0f },
	{ 320.0f, 200.0f }
};

static Vector2 penStep = { 2.0f, 5.0f };

static Vector2 basePenButtonPos[2] = {
	{ 330.0f, 85.0f },
	{ 330.0f, 217.0f }
};

static Vector2 humanInputButtonSize = { 60.0f, 60.0f };

static Vector2 boardPositions[20] = {
	{ 265.0f, 85.0f },
	{ 265.0f, 217.0f },
	{ 200.0f, 85.0f },
	{ 200.0f, 217.0f },
	{ 135.0f, 85.0f },
	{ 135.0f, 217.0f },
	{ 70.0f, 85.0f },
	{ 70.0f, 217.0f },
	{ 70.0f, 150.0f },
	{ 135.0f, 150.0f },
	{ 200.0f, 150.0f },
	{ 265.0f, 150.0f },
	{ 330.0f, 150.0f },
	{ 395.0f, 150.0f },
	{ 460.0f, 150.0f },
	{ 525.0f, 150.0f },
	{ 525.0f, 85.0f },
	{ 525.0f, 217.0f },
	{ 460.0f, 85.0f },
	{ 460.0f, 217.0f },
};

static void drawPieces( void )
{
	for( int p = 0; p < 2; ++p ) {
		float penDraw = 0.0f;
		for( int i = 0; i < 7; ++i ) {
			Vector2 pos;
			if( currBoardState.pieces[p][i] == -1 ) {
				vec2_AddScaled( &basePenDraw[p], &penStep, penDraw, &pos );
				penDraw += 1.0f;
			} else if( currBoardState.pieces[p][i] < 20 ) {
				pos = boardPositions[currBoardState.pieces[p][i]];
			} else {
				pos = vec2( -20.0f, -20.0f ); // just draw it off the screen
			}


			int draw = img_CreateDraw( pieceImg, 1, pos, pos, 100 );
			img_SetDrawColor( draw, playerClrs[p], playerClrs[p] );
		}
	}
}

static void dumpPos( void )
{
	Vector2 pos;
	input_GetMousePosition( &pos );
	vec2_Dump( &pos, NULL );
}

static int gameOfUrState_Enter( void )
{
	jq_Initialize( 4 );
	cam_TurnOnFlags( 0, 1 );

	gfx_SetClearColor( CLR_BLACK );

	btn_RegisterSystem( );

	boardImg = img_Load( "Images/ur_board_labeled.png", ST_DEFAULT );
	pieceImg = img_Load( "Images/piece.png", ST_DEFAULT );
	headProfileImg = img_Load( "Images/head_profile.png", ST_DEFAULT );
	gearImg = img_Load( "Images/gear.png", ST_DEFAULT );
	whiteBoxImg = img_Load( "Images/white.png", ST_DEFAULT );
	font = txt_LoadFont( "Fonts/kenpixel.ttf", 24 );
	img_LoadSpriteSheet( "Images/button.ss", ST_DEFAULT, &buttonImg );
	hiliteImg = img_Load( "Images/hilite.png", ST_DEFAULT );

	gsm_EnterState( &gameSM, &startState );

	//input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, dumpPos );

	//rand_Seed( &mctsRandom, 0xFEEDBEEF );
	rand_Seed( &mctsRandom, (uint32_t)time( NULL ) );

	return 1;
}

static int gameOfUrState_Exit( void )
{
	btn_UnRegisterSystem( );
	return 1;
}

static void gameOfUrState_ProcessEvents( SDL_Event* e )
{
	gsm_ProcessEvents( &gameSM, e );
}

static void gameOfUrState_Process( void )
{
	gsm_Process( &gameSM );
}

static void gameOfUrState_Draw( void )
{
	int draw = img_CreateDraw( boardImg, 1, vec2( 300.0f, 150.0f ), vec2( 300.0f, 150.0f ), 0 );
	drawPieces( );
	gsm_Draw( &gameSM );
	drawLabels( );
}

static void gameOfUrState_PhysicsTick( float dt )
{
	gsm_PhysicsTick( &gameSM, dt );
}

GameState gameOfUrScreenState = { gameOfUrState_Enter, gameOfUrState_Exit, gameOfUrState_ProcessEvents,
	gameOfUrState_Process, gameOfUrState_Draw, gameOfUrState_PhysicsTick };

static void createTurnLabel( )
{
	if( currBoardState.currPlayer == 0 ) {
		createLabel( "Turn: White", vec2( 400.0f, 270.0f ), CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, font, 24.0f, 1, 10 );
	} else {
		createLabel( "Turn: Black", vec2( 400.0f, 270.0f ), CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, font, 24.0f, 1, 10 );
	}
}

static void createRollLabel( )
{
	if( currBoardState.roll >= 0 ) {
		char strBuffer[64];
		SDL_snprintf( strBuffer, 64, "Rolled a %i", currBoardState.roll );
		createLabel( strBuffer, vec2( 400.0f, 300.0f ), CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, font, 24.0f, 1, 10 );
	}
}

//****************************************************
// start state
static void chooseMoveState( void )
{
	int winner = ur_getWinner( &currBoardState );
	if( winner == -1 ) {

		if( playerTypes[currBoardState.currPlayer] == PT_HUMAN ) {
			gsm_EnterState( &gameSM, &humanChooseMoveState );
		} else {
			gsm_EnterState( &gameSM, &aiChooseMoveState );
		}
	} else {
		lastWinner = winner;
		gsm_EnterState( &gameSM, &startState );
	}
}

static void pressStart_HumanVsHuman( int id )
{
	playerTypes[0] = PT_HUMAN;
	playerTypes[1] = PT_HUMAN;
	chooseMoveState( );
}

static void pressStart_HumanVsComputer( int id )
{
	playerTypes[0] = PT_HUMAN;
	playerTypes[1] = PT_AI;
	chooseMoveState( );
}

static void pressStart_ComputerVsHuman( int id )
{
	playerTypes[0] = PT_AI;
	playerTypes[1] = PT_HUMAN;
	chooseMoveState( );
}

static void pressStart_ComputerVsComputer( int id )
{
	playerTypes[0] = PT_AI;
	playerTypes[1] = PT_AI;
	chooseMoveState( );
}

static int start_Enter( void )
{
	currBoardState = initialBoardState( );

	if( lastWinner == 0 ) {
		createLabel( "Winner: Player 1", vec2( 275.0f, 40.0f ), CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, font, 12.0f, 1, 0 );
	} else if( lastWinner == 1 ) {
		createLabel( "Winner: Player 2", vec2( 275.0f, 40.0f ), CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, font, 12.0f, 1, 0 );
	} else {
		createLabel( "Winner: None", vec2( 275.0f, 40.0f ), CLR_WHITE, HORIZ_ALIGN_LEFT, VERT_ALIGN_BASE_LINE, font, 24.0f, 1, 0 );
	}

	btn_Create( vec2( 500.0f, 280.0f ), vec2( 200.0f, 40.0f ), vec2( 200.0f, 40.0f ),
		"Start HvH", font, 24.0f, CLR_WHITE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0,
		pressStart_HumanVsHuman, NULL );

	btn_Create( vec2( 500.0f, 320.0f ), vec2( 200.0f, 40.0f ), vec2( 200.0f, 40.0f ),
		"Start HvC", font, 24.0f, CLR_WHITE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0,
		pressStart_HumanVsComputer, NULL );

	btn_Create( vec2( 500.0f, 360.0f ), vec2( 200.0f, 40.0f ), vec2( 200.0f, 40.0f ),
		"Start CvH", font, 24.0f, CLR_WHITE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0,
		pressStart_ComputerVsHuman, NULL );

	btn_Create( vec2( 500.0f, 400.0f ), vec2( 200.0f, 40.0f ), vec2( 200.0f, 40.0f ),
		"Start CvC", font, 24.0f, CLR_WHITE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0,
		pressStart_ComputerVsComputer, NULL );

	return 1;
}

static int start_Exit( void )
{
	destroyAllLabels( );
	btn_DestroyAll( );

	return 1;
}

static void start_ProcessEvents( SDL_Event* e )
{}

static void start_Process( void )
{}

static void start_Draw( void )
{}

static void start_PhysicsTick( float dt )
{}

//****************************************************
// human choose move state
static Move* sbHumanMoveList = NULL;
static int* matchingMoveList = NULL;

static void pressedButton( int id )
{
	for( size_t i = 0; i < sb_Count( sbHumanMoveList ); ++i ) {
		if( id == matchingMoveList[i] ) {
			// execute move
			currMove = sbHumanMoveList[i];
			gsm_EnterState( &gameSM, &animateState );
			return;
		}
	}
}

static int humanChooseMove_Enter( void )
{
	// grab the list of moves and create the buttons for them
	sb_Clear( sbHumanMoveList );

	ur_getPossibleMoveList( &currBoardState, &sbHumanMoveList );

	matchingMoveList = mem_Allocate( sizeof( matchingMoveList[0] ) * sb_Count( sbHumanMoveList ) );
	Vector2 basePos = vec2( 120.0f, 300.0f );
	Vector2 buttonSize = { 200.0f, 40.0f };
	Vector2 padding = { 10.0f, 10.0f };
	for( size_t i = 0; i < sb_Count( sbHumanMoveList ); ++i ) {
		switch( sbHumanMoveList[i].type ) {
		case MT_SKIP:
			matchingMoveList[i] = btn_Create( basePos, buttonSize, buttonSize, "Skip", font, 24.0f, CLR_BLACK, VEC2_ZERO, buttonImg, -1, CLR_WHITE, 1, 10, pressedButton, NULL );
			break;
		case MT_ROLL:
			matchingMoveList[i] = btn_Create( basePos, buttonSize, buttonSize, "Roll", font, 24.0f, CLR_BLACK, VEC2_ZERO, buttonImg, -1, CLR_WHITE, 1, 10, pressedButton, NULL );
			break;
		case MT_PIECE:
		{
			int8_t currPos = currBoardState.pieces[sbHumanMoveList[i].piece.player][sbHumanMoveList[i].piece.piece];
			if( currPos < 0 ) {
				// create button over the penned pieces
				matchingMoveList[i] = btn_Create( basePenButtonPos[sbHumanMoveList[i].piece.player], humanInputButtonSize, humanInputButtonSize,
					NULL, -1, 0.0f, CLR_BLACK, VEC2_ZERO, NULL, hiliteImg, CLR_WHITE, 1, 10, pressedButton, NULL );
			} else {
				// create button over the corresponding piece
				matchingMoveList[i] = btn_Create( boardPositions[currPos], humanInputButtonSize, humanInputButtonSize,
					NULL, -1, 0.0f, CLR_BLACK, VEC2_ZERO, NULL, hiliteImg, CLR_WHITE, 1, 10, pressedButton, NULL );
			}
		} break;
		}

		if( i != 3 ) {
			basePos.y += buttonSize.y + padding.y;
		} else {
			basePos.x += buttonSize.x + padding.x;
			basePos.y = 270.0f;
		}
	}

	createTurnLabel( );
	createRollLabel( );

	return 1;
}

static int humanChooseMove_Exit( void )
{
	mem_Release( matchingMoveList );
	btn_DestroyAll( );
	destroyAllLabels( );
	return 1;
}

static void humanChooseMove_ProcessEvents( SDL_Event* e )
{}

static void humanChooseMove_Process( void )
{}

static void humanChooseMove_Draw( void )
{
}

static void humanChooseMove_PhysicsTick( float dt )
{}


//****************************************************
// ai choose move state
static int aiChooseMove_Enter( void )
{
	createTurnLabel( );
	createRollLabel( );

	float searchConstant = MCTS_DEFAULT_SEARCH_CONSTANT;
	startMCTSThread( urDefinition, searchConstant, &currBoardState );

	return 1;
}

static int aiChooseMove_Exit( void )
{
	destroyAllLabels( );

	return 1;
}

static void aiChooseMove_ProcessEvents( SDL_Event* e )
{

}

static void aiChooseMove_Process( void )
{
	if( aiChosenMoveReady ) {
		currMove = aiChosenMove;
		gsm_EnterState( &gameSM, &animateState );
	}
}

static void aiChooseMove_Draw( void )
{
	Vector2 basePos = vec2( 100.0f, 300.0f );
	img_CreateDraw( headProfileImg, 1, basePos, basePos, 100 );

	Vector2 gearOffset = vec2( 10.0f, -20.0f );
	Vector2 gearPos;
	vec2_Add( &basePos, &gearOffset, &gearPos );
	float newRot = gearRot + 0.1f;

	int draw = img_CreateDraw( gearImg, 1, gearPos, gearPos, 101 );
	img_SetDrawColor( draw, CLR_BLACK, CLR_BLACK );
	img_SetDrawRotation( draw, gearRot, newRot );
	gearRot = newRot;

	Vector2 progressOffset = vec2( 0.0f, 75.0f );
	Vector2 progressPos;
	vec2_Add( &basePos, &progressOffset, &progressPos );

	draw = img_CreateDraw( whiteBoxImg, 1, progressPos, progressPos, 101 );
	img_SetDrawScaleV( draw, vec2( 3.5f, 0.5f ), vec2( 3.5f, 0.5f ) );
	img_SetDrawColor( draw, CLR_DARK_GREY, CLR_DARK_GREY );

	draw = img_CreateDraw( whiteBoxImg, 1, progressPos, progressPos, 102 );
	img_SetDrawScaleV( draw, vec2( 3.5f * amtAIDone, 0.5f ), vec2( 3.5f * amtAIDone, 0.5f ) );
	img_SetDrawColor( draw, CLR_WHITE, CLR_WHITE );
}

static void aiChooseMove_PhysicsTick( float dt )
{}


//****************************************************
// animate state
static bool animateDone;
static float timePassed = 0.0f;

#define HUMAN_TIME_TO_ANIMATE 0.75f
#define AI_TIME_TO_ANIMATE 1.5f

static int animate_Enter( void )
{
	// choose what to animate
	//  roll shouldn't do anything
	//  skip shouldn't do anything
	//  moves will be more complicated based on what happens, from initial, to last, and if a piece was knocked out

	animateDone = false;
	timePassed = 0.0f;
	if( ( currMove.type == MT_ROLL ) || ( currMove.type == MT_SKIP ) ) {
		animateDone = true;
	}

	createTurnLabel( );
	createRollLabel( );

	return 1;
}

static int animate_Exit( void )
{
	destroyAllLabels( );

	return 1;
}

static void animate_ProcessEvents( SDL_Event* e )
{

}

static void animate_Process( void )
{
	if( animateDone ) {
		BoardState newBoardState;
		ur_applyMove( &currBoardState, &currMove, &newBoardState );
		currBoardState = newBoardState;

		chooseMoveState( );
	}
}

static void animate_Draw( void )
{

}

static void animate_PhysicsTick( float dt )
{
	timePassed += dt;
	if( timePassed >= 0.75f ) {
		animateDone = true;
	}
}