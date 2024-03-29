#include <stdint.h>

#define ASSERT(EXPRESSION)						\
	do {										\
		if (!(EXPRESSION))						\
			*(int *)NULL = 0;					\
	} while (0)

#define KIBIBYTES(N) ((N) * 1024)
#define MEBIBYTES(N) ((N) * 1024 * 1024)
#define GIBIBYTES(N) ((N) * 1024 * 1024 * 1024)
#define TEBIBYTES(N) ((N) * 1024 * 1024 * 1024 * 1024)
#define ARRAYLENGTH(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define CLAMP(N, FLOOR, CEIL) MAX((FLOOR), MIN((CEIL), (N)))
#define ABS(N) ((N) >= 0 ? (N) : ((N) * -1))

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;
typedef uint32_t b32;
typedef size_t usize;

namespace hm {

struct FrameBuffer {
	u32 width;
	u32 height;
	u32 pitch;
	void *memory;
};

struct GameMemory {
	bool is_initialized;

	usize persistent_bytes;
	void *persistent;

	usize transient_bytes;
	void *transient;
};

struct GameState {
	u8 render_x;
	u8 render_y;
};

struct ButtonState {
	u32 transition_count;
	bool ended_down;
};

struct InputControl {
	u32 mouse_x;
	u32 mouse_y;
	u32 mouse_delta_x;
	u32 mouse_delta_y;

	union {
		struct {
			ButtonState north;
			ButtonState south;
			ButtonState east;
			ButtonState west;
		} move;

		ButtonState moves[4];
	};
};

constexpr u32 TargetFramesPerSecond = 60;
constexpr f32 TargetSecondsPerFrame = (1.0f / (f32)TargetFramesPerSecond);

static void render_gradient(FrameBuffer *frame, u8 x_offset, u8 y_offset)
{
	u8 *row = (u8 *)frame->memory;
	
	for (u32 y = 0; (y < frame->height); y++) {
		u32 *pixel = (u32 *)row;
		u8 green = (u8)(y + y_offset);

		for (u32 x = 0; (x < frame->width); x++) {
			u8 blue = (u8)(x + x_offset);
			*pixel++ = ((green << 8) | blue);
		}
		
		row += frame->pitch;
	}
}

static void update_and_render(GameMemory *memory, FrameBuffer *frame, InputControl *input)
{
	ASSERT(sizeof(GameState) <= memory->persistent_bytes);
	GameState *state = (GameState *)memory->persistent;

	if (!memory->is_initialized) {
		state->render_x = 0;
		state->render_y = 0;

		memory->is_initialized = true;
	}

	i8 h_input = (input->move.east.ended_down - input->move.west.ended_down);
	i8 v_input = (input->move.south.ended_down - input->move.north.ended_down);
	
	i8 speed = 5;
	state->render_x += (i8)(h_input * speed);
	state->render_y += (i8)(v_input * speed);
	
	render_gradient(frame, state->render_x, state->render_y);
}

} // hm
