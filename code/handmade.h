#if !defined(HANDMADE_H)

#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;


/*
 * NOTE: Services that the platform provides to the game.
*/

/*
 * NOTE: Services that the game provides to the platform layer.
 * This may expand in the future, sound on separate thread, etc.
*/

struct game_offscreen_buffer
{
    void *Memory;
    int Width; 
    int Height;
    int Pitch;
};

// Needs three things: timing, controller/keyboard input, bitmap to use, sound buffer to use
internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

#define HANDMADE_H
#endif
