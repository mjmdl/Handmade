#include "handmade.hh"

namespace hm {

constexpr u32 TargetFramesPerSecond = 60;
constexpr f32 TargetSecondsPerFrame = (1.0f / (f32)TargetFramesPerSecond);

struct FrameBuffer {
	u32 width;
	u32 height;
	u32 pitch;
	void *memory;
};

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

} // hm
