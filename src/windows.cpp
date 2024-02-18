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

static WindowSize get_window_size(HWND window)
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
	}

	return (int)message.wParam;
}
