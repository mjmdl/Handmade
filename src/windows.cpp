#include <windows.h>

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int command_show)
{
	MessageBoxA(NULL, "Hello, World!", "Message", (MB_ICONINFORMATION | MB_OK));
	return 0;
}
