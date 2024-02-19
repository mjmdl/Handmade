#include <stdio.h>
#include "handmade.cpp"
#include <windows.h>
#include <dsound.h>

namespace win {

struct WindowBuffer {
	u8 pixel_bytes;
	hm::FrameBuffer frame;
	BITMAPINFO bitmap_info;
};

struct WindowSize {
	u32 width;
	u32 height;
};

struct InputState {
	hm::InputControl *old_input;
	hm::InputControl *new_input;
};

struct WindowData {
	InputState *input;
};

struct FrameTimer {
	bool sleep_is_granular;
	u64 last_ticks;
	u64 end_ticks;
	f32 delta_seconds;
	u64 last_cycles;
	u64 end_cycles;
	u64 delta_cycles;
};

struct SoundSpec {
	u16 channels;
	u32 samples_per_second;
	u16 bytes_per_sample;
	u32 buffer_bytes;
};

static u64 performance_frequency;

static inline u64 get_ticks()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

static inline i64 get_cycles()
{
	return __rdtsc();
}

static inline f32 get_delta_seconds(u64 start, u64 end)
{
	f32 delta = (f32)(end - start);
	f32 seconds = (delta / performance_frequency);
	return seconds;
}

static inline void init_frame_timer(FrameTimer *timer)
{
	u32 scheduler_miliseconds = 1;
	timer->sleep_is_granular = (timeBeginPeriod(scheduler_miliseconds) != TIMERR_NOERROR);
	timer->last_ticks = get_ticks();
	timer->last_cycles = get_cycles();
}

static inline void wait_for_frame(FrameTimer *timer, f32 seconds_per_frame)
{
	u64 ticks = get_ticks();
	f32 delta_seconds = get_delta_seconds(timer->last_ticks, ticks);
	
	while (delta_seconds < seconds_per_frame) {
		if (timer->sleep_is_granular) {
			f32 seconds_left = (seconds_per_frame - delta_seconds);
			u32 miliseconds_left = (u32)(1000.0f * seconds_left);

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
	f32 delta_miliseconds = (timer->delta_seconds * 1000.0f);
	f32 frames_per_second = (1.0f / timer->delta_seconds);
	f32 frame_mega_cycles = ((f32)timer->delta_cycles / 1'000'000.0f);
	
	char buffer[256];
	sprintf_s(buffer, sizeof(buffer), "%.2f ms, %.2f fps, %.2f Mcpf\n",
			  delta_miliseconds, frames_per_second, frame_mega_cycles);
	OutputDebugStringA(buffer);
}

static LPDIRECTSOUNDBUFFER init_direct_sound(SoundSpec *spec, HWND window)
{
	DSBUFFERDESC desc = {};
	WAVEFORMATEX format = {};
	
	HMODULE dsound_lib = LoadLibraryA("dsound.dll");
	if (!dsound_lib)
		return nullptr;

	typedef HRESULT(WINAPI *DirectSoundCreateFn)(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);
	DirectSoundCreateFn DirectSoundCreate = (DirectSoundCreateFn)GetProcAddress(dsound_lib, "DirectSoundCreate");
	if (!DirectSoundCreate)
		goto error_free_library;
	
	LPDIRECTSOUND dsound;
	if (FAILED(DirectSoundCreate(nullptr, &dsound, nullptr)))
		goto error_free_library;

	if (FAILED(dsound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
		goto error_release_dsound;

	desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	LPDIRECTSOUNDBUFFER primary;
	if (FAILED(dsound->CreateSoundBuffer(&desc, &primary, nullptr)))
		goto error_release_dsound;
	
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = spec->channels;
	format.nSamplesPerSec = spec->samples_per_second;
	format.wBitsPerSample = (spec->bytes_per_sample * 8);
	format.nBlockAlign = (format.nChannels * format.wBitsPerSample / 8);
	format.nAvgBytesPerSec = (format.nSamplesPerSec * format.nBlockAlign);
	
	if (FAILED(primary->SetFormat(&format)))
		goto error_free_primary;

	desc.dwFlags = 0;
	desc.dwBufferBytes = spec->buffer_bytes;
	desc.lpwfxFormat = &format;

	LPDIRECTSOUNDBUFFER secondary;
	if (FAILED(dsound->CreateSoundBuffer(&desc, &secondary, nullptr)))
		goto error_free_primary;

	primary->Release();
	return secondary;

error_free_primary:
	primary->Release();
	
error_release_dsound:
	dsound->Release();
	
error_free_library:
	FreeLibrary(dsound_lib);
	return nullptr;
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

static void resize_window_buffer(WindowBuffer *buffer, u32 width, u32 height)
{
	u8 pixel_bytes = (buffer->pixel_bytes = 4);
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

static inline void present_window_buffer(WindowBuffer *buffer, HDC context, u32 width, u32 height)
{
	StretchDIBits(context, 0, 0, width, height, 0, 0, buffer->frame.width, buffer->frame.height,
				  buffer->frame.memory, &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

static inline void handle_button(hm::ButtonState *button, bool is_down)
{
	button->ended_down = is_down;
	button->transition_count++;
}

static bool handle_keyboard(hm::InputControl *input, u64 key_code, u64 flags)
{
	bool is_alt_down = (flags & (1 << 29));
	bool was_down = (flags & (1 << 30));
	bool is_down = !(flags & (1 << 31));

	if (is_down == was_down)
		return false;

	switch (key_code) {
	case 'W':
		handle_button(&input->move.north, is_down);
		return true;
		
	case 'A':
		handle_button(&input->move.west, is_down);
		return true;

	case 'S':
		handle_button(&input->move.south, is_down);
		return true;

	case 'D':
		handle_button(&input->move.east, is_down);
		return true;
	
	default:
		return false;
	}
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

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: {
		WindowData *data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
		if (handle_keyboard(data->input->new_input, w_param, l_param))
			return 0;

		return DefWindowProc(window, message, w_param, l_param);
	}

	case WM_MOUSEMOVE: {
		WindowData *data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
		hm::InputControl *old_input = data->input->old_input;
		hm::InputControl *new_input = data->input->new_input;

		new_input->mouse_x = LOWORD(l_param);
		new_input->mouse_y = HIWORD(l_param);
		new_input->mouse_delta_x = (new_input->mouse_x - old_input->mouse_x);
		new_input->mouse_delta_y = (new_input->mouse_y - old_input->mouse_y);
		
		return 0;
	}
		
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

	i32 screen_width = GetSystemMetrics(SM_CXSCREEN);
	i32 screen_height = GetSystemMetrics(SM_CYSCREEN);

	HWND window = CreateWindowExA(
		0, window_class.lpszClassName, "Handmade", (WS_OVERLAPPEDWINDOW | WS_VISIBLE),
		(screen_width / 8), (screen_height / 8), (screen_width * 3 / 4), (screen_height * 3 / 4),
		NULL, NULL, instance, NULL);

	if (!window)
		return FALSE;

	HDC device_context = GetDC(window);

	if (!device_context)
		return FALSE;

	win::SoundSpec sound_spec = {};
	sound_spec.channels = 2;
	sound_spec.samples_per_second = 48'000;
	sound_spec.bytes_per_sample = (sound_spec.channels * sizeof(i16));
	sound_spec.buffer_bytes = (sound_spec.samples_per_second * sound_spec.bytes_per_sample);
	
	LPDIRECTSOUNDBUFFER sound_buffer = win::init_direct_sound(&sound_spec, window);
	if (sound_buffer) {
		if (FAILED(sound_buffer->Play(0, 0, DSBPLAY_LOOPING))) {
			sound_buffer->Release();
			sound_buffer = nullptr;
		}
	}

	win::WindowBuffer window_buffer = {};
	win::resize_window_buffer(&window_buffer, 1440, 810);

	hm::GameMemory game_memory = {};
	game_memory.persistent_bytes = MEBIBYTES(64);
	game_memory.transient_bytes = GIBIBYTES(1);
	game_memory.persistent = VirtualAlloc(nullptr, game_memory.persistent_bytes, (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
	game_memory.transient = VirtualAlloc(((u8 *)game_memory.persistent + game_memory.persistent_bytes), game_memory.transient_bytes,
										 (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
	ASSERT(game_memory.persistent && game_memory.transient);
	
	hm::InputControl input_controls[2] = {};
	win::InputState input_state;
	input_state.old_input = &input_controls[0];
	input_state.new_input = &input_controls[1];
	
	win::WindowData window_data;
	window_data.input = &input_state;
	SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)&window_data);
	
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

		hm::update_and_render(&game_memory, &window_buffer.frame, input_state.new_input);

		hm::InputControl *temp_old_input = input_state.old_input;
		input_state.old_input = input_state.new_input;
		input_state.new_input = temp_old_input;

		*input_state.new_input = {};
		for (u32 m = 0; (m < ARRAYLENGTH(input_state.old_input->moves)); m++)
			input_state.new_input->moves[m].ended_down = input_state.old_input->moves[m].ended_down;
		
		win::WindowSize window_size = win::get_window_size(window);
		win::present_window_buffer(&window_buffer, device_context, window_size.width, window_size.height);

		win::wait_for_frame(&frame_timer, hm::TargetSecondsPerFrame);
		win::update_frame_timer(&frame_timer);
		win::log_frame_timer(&frame_timer);
	}

	return (int)message.wParam;
}
