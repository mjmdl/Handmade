#include <stdio.h>
#include "handmade.cpp"
#include <windows.h>

namespace win {

struct WindowBuffer {
	uint8_t pixel_bytes;
	hm::FrameBuffer frame;
	BITMAPINFO bitmap_info;
};

struct WindowSize {
	uint32_t width;
	uint32_t height;
};

struct FrameTimer {
	bool sleep_is_granular;
	uint64_t last_ticks;
	uint64_t end_ticks;
	float delta_seconds;
	uint64_t last_cycles;
	uint64_t end_cycles;
	uint64_t delta_cycles;
};

static uint64_t performance_frequency;

static inline uint64_t get_ticks()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

static inline int64_t get_cycles()
{
	return __rdtsc();
}

static inline float get_delta_seconds(uint64_t start, uint64_t end)
{
	float delta = (float)(end - start);
	float seconds = (delta / performance_frequency);
	return seconds;
}

static inline void init_frame_timer(FrameTimer *timer)
{
	uint32_t scheduler_miliseconds = 1;
	timer->sleep_is_granular = (timeBeginPeriod(scheduler_miliseconds) != TIMERR_NOERROR);
	timer->last_ticks = get_ticks();
	timer->last_cycles = get_cycles();
}

static inline void wait_for_frame(FrameTimer *timer, float seconds_per_frame)
{
	uint64_t ticks = get_ticks();
	float delta_seconds = get_delta_seconds(timer->last_ticks, ticks);
	
	while (delta_seconds < seconds_per_frame) {
		if (timer->sleep_is_granular) {
			float seconds_left = (seconds_per_frame - delta_seconds);
			uint32_t miliseconds_left = (uint32_t)(1000.0f * seconds_left);

			if (miliseconds_left > 0)
				Sleep(miliseconds_left);
		}

		ticks = win::get_ticks();
		delta_seconds = win::get_delta_seconds(timer->last_ticks, ticks);
	}	
}

static inline void update_frame_timer(FrameTimer *timer)
{
	timer->end_ticks = get_ticks();
	timer->delta_seconds = get_delta_seconds(timer->last_ticks, timer->end_ticks);
	timer->last_ticks = timer->end_ticks;

	timer->end_cycles = get_cycles();
	timer->delta_cycles = (timer->end_cycles - timer->last_cycles);
	timer->last_cycles = timer->end_cycles;
}

static inline void log_frame_timer(FrameTimer *timer)
{
	float delta_miliseconds = (timer->delta_seconds * 1000.0f);
	float frames_per_second = (1.0f / timer->delta_seconds);
	float frame_mega_cycles = ((float)timer->delta_cycles / 1'000'000.0f);
	
	char buffer[256];
	sprintf_s(buffer, sizeof(buffer), "%.2f ms, %.2f fps, %.2f Mcpf\n", delta_miliseconds, frames_per_second, frame_mega_cycles);
	OutputDebugStringA(buffer);
}

static inline WindowSize get_window_size(HWND window)
{
	RECT rect;
	GetClientRect(window, &rect);

	WindowSize size;
	size.width = (rect.right - rect.left);
	size.height = (rect.bottom - rect.top);
	return size;
}

static void resize_window_buffer(WindowBuffer *buffer, uint32_t width, uint32_t height)
{
	uint8_t pixel_bytes = (buffer->pixel_bytes = 4);
	buffer->frame.width = width;
	buffer->frame.height = height;
	buffer->frame.pitch = (pixel_bytes * width);

	if (buffer->frame.memory)
		VirtualFree(buffer->frame.memory, 0, MEM_RELEASE);
	buffer->frame.memory = VirtualAlloc(NULL, (width * height * pixel_bytes), (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);

	buffer->bitmap_info.bmiHeader.biSize = sizeof(buffer->bitmap_info.bmiHeader);
	buffer->bitmap_info.bmiHeader.biWidth = width;
	buffer->bitmap_info.bmiHeader.biHeight = (height * -1);
	buffer->bitmap_info.bmiHeader.biPlanes = 1;
	buffer->bitmap_info.bmiHeader.biBitCount = (pixel_bytes * 8);
	buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;
}

static inline void present_window_buffer(WindowBuffer *buffer, HDC context, uint32_t width, uint32_t height)
{
	StretchDIBits(context, 0, 0, width, height, 0, 0, buffer->frame.width, buffer->frame.height,
				  buffer->frame.memory, &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	switch (message) {
	case WM_CLOSE:
		DestroyWindow(window);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(window, message, w_param, l_param);
	}
}

} // win

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int command_show)
{
	QueryPerformanceFrequency((LARGE_INTEGER *)&win::performance_frequency);

	win::FrameTimer frame_timer = {};
	init_frame_timer(&frame_timer);
	
	WNDCLASSA window_class = {};
	window_class.style = (CS_OWNDC | CS_HREDRAW | CS_VREDRAW);
	window_class.lpfnWndProc = win::window_procedure;
	window_class.hInstance = instance;
	window_class.lpszClassName = "Handmade_Window_Class";

	if (!RegisterClassA(&window_class))
		return FALSE;

	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);

	HWND window = CreateWindowExA(
		0, window_class.lpszClassName, "Handmade", (WS_OVERLAPPEDWINDOW | WS_VISIBLE),
		(screen_width / 8), (screen_height / 8), (screen_width * 3 / 4), (screen_height * 3 / 4),
		NULL, NULL, instance, NULL);

	if (!window)
		return FALSE;

	HDC device_context = GetDC(window);

	if (!device_context)
		return FALSE;

	win::WindowBuffer window_buffer = {};
	win::resize_window_buffer(&window_buffer, 1440, 810);
	uint8_t render_x_offset = 0;
	uint8_t render_y_offset = 0;
	
	bool keep_running = true;
	MSG message = {};

	while (keep_running) {
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT)
				keep_running = false;
			else {
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}

		hm::render_gradient(&window_buffer.frame, render_x_offset++, render_y_offset++);		

		win::WindowSize window_size = win::get_window_size(window);
		win::present_window_buffer(&window_buffer, device_context, window_size.width, window_size.height);

		win::wait_for_frame(&frame_timer, hm::TargetSecondsPerFrame);
		win::update_frame_timer(&frame_timer);
		win::log_frame_timer(&frame_timer);
	}

	return (int)message.wParam;
}
