#include <stdint.h>

#define HM_TARGET_FRAMES_PER_SECOND 60
#define HM_TARGET_SECONDS_PER_FRAME (1.0f / (float)HM_TARGET_FRAMES_PER_SECOND)

namespace hm {

struct FrameBuffer {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	void *memory;
};

static void render_gradient(FrameBuffer *frame, uint8_t x_offset, uint8_t y_offset)
{
	uint8_t *row = (uint8_t *)frame->memory;
	
	for (uint32_t y = 0; (y < frame->height); y++) {
		uint32_t *pixel = (uint32_t *)row;
		uint8_t green = (uint8_t)(y + y_offset);

		for (uint32_t x = 0; (x < frame->width); x++) {
			uint8_t blue = (uint8_t)(x + x_offset);
			*pixel++ = ((green << 8) | blue);
		}
		
		row += frame->pitch;
	}
}

} // hm
