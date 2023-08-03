#pragma once
#include <Windows.h>
#include "StarChicken_decl.h"

namespace Win32 {
	void poll_events(HWND window) {
		MSG message{};
		while (PeekMessageA(&message, window, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}
}