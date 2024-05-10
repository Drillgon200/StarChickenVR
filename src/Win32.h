#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)
#include "StarChicken_decl.h"

namespace Win32 {

decltype(MessageBoxA)* MessageBoxA_ptr;
decltype(DispatchMessageA)* DispatchMessageA_ptr;
decltype(TranslateMessage)* TranslateMessage_ptr;
decltype(PeekMessageA)* PeekMessageA_ptr;
decltype(DefWindowProcA)* DefWindowProcA_ptr;
decltype(PostQuitMessage)* PostQuitMessage_ptr;
decltype(RegisterClassA)* RegisterClassA_ptr;
decltype(CreateWindowExA)* CreateWindowExA_ptr;
decltype(ShowWindow)* ShowWindow_ptr;
decltype(GetWindowRect)* GetWindowRect_ptr;
decltype(SetProcessDPIAware)* SetProcessDPIAware_ptr;

HMODULE user32DLL;
HINSTANCE instance;
HWND window;
U32 windowWidth;
U32 windowHeight;
I32 framebufferWidth;
I32 framebufferHeight;
B32 shouldRecreateSwapchain;
B32 windowShouldClose;

void error_box(const char* msg) {
	if (window) {
		MessageBoxA_ptr(window, msg, "Error", MB_ICONERROR | MB_OK);
	}
}

void poll_events() {
	MSG message{};
	//TODO: PeekMessageA blocks when the user resizes or moves the window around, stopping my game loop. This isn't ideal.
	while (PeekMessageA_ptr(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage_ptr(&message);
		DispatchMessageA_ptr(&message);
	}
}

LRESULT CALLBACK window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	switch (uMsg) {
	case WM_DESTROY: {
		PostQuitMessage_ptr(0);
	} break;
	case WM_SIZE: {
		windowWidth = LOWORD(lParam);
		windowHeight = HIWORD(lParam);
		RECT rect;
		if (GetWindowRect_ptr(window, &rect)) {
			framebufferWidth = rect.right - rect.left;
			framebufferHeight = rect.bottom - rect.top;
			shouldRecreateSwapchain = true;
		}
	} break;
	case WM_PAINT: {
		result = DefWindowProcA_ptr(hwnd, uMsg, wParam, lParam);
	} break;
	case WM_CLOSE: {
		windowShouldClose = true;
	} break;
	default: {
		result = DefWindowProcA_ptr(hwnd, uMsg, wParam, lParam);
	} break;
	}
	return result;
}

bool init(U32 width, U32 height) {
	bool success = true;
	instance = GetModuleHandleA(nullptr);
	user32DLL = LoadLibraryA("User32.dll");
	if (user32DLL == NULL) {
		success = false;
	} else {
		MessageBoxA_ptr = reinterpret_cast<decltype(MessageBoxA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "MessageBoxA")));
		DispatchMessageA_ptr = reinterpret_cast<decltype(DispatchMessageA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "DispatchMessageA")));
		TranslateMessage_ptr = reinterpret_cast<decltype(TranslateMessage_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "TranslateMessage")));
		PeekMessageA_ptr = reinterpret_cast<decltype(PeekMessageA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "PeekMessageA")));
		DefWindowProcA_ptr = reinterpret_cast<decltype(DefWindowProcA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "DefWindowProcA")));
		PostQuitMessage_ptr = reinterpret_cast<decltype(PostQuitMessage_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "PostQuitMessage")));
		RegisterClassA_ptr = reinterpret_cast<decltype(RegisterClassA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "RegisterClassA")));
		CreateWindowExA_ptr = reinterpret_cast<decltype(CreateWindowExA_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "CreateWindowExA")));
		ShowWindow_ptr = reinterpret_cast<decltype(ShowWindow_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "ShowWindow")));
		GetWindowRect_ptr = reinterpret_cast<decltype(GetWindowRect_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "GetWindowRect")));
		SetProcessDPIAware_ptr = reinterpret_cast<decltype(SetProcessDPIAware_ptr)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, "SetProcessDPIAware")));

		WNDCLASSA windowClass{};
		windowClass.lpfnWndProc = window_callback;
		windowClass.hInstance = instance;
		windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		const char* className = "Star Chicken View Window";
		windowClass.lpszClassName = className;
		RegisterClassA_ptr(&windowClass);
		window = CreateWindowExA_ptr(0, className, "Star Chicken Desktop View", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, int(width), int(height), NULL, NULL, instance, NULL);
		if (!window) {
			success = false;
		} else {
			ShowWindow_ptr(window, SW_SHOWDEFAULT);
			windowWidth = width;
			windowHeight = height;
			RECT rect;
			if (GetWindowRect_ptr(window, &rect)) {
				framebufferWidth = rect.right - rect.left;
				framebufferHeight = rect.bottom - rect.top;
			}
		}
	}
	if (SetProcessDPIAware_ptr) {
		// Windows has a feature that renders applications at a lower resolution and does a blurry upscale to make them look bigger
		// This disables that functionality
		SetProcessDPIAware_ptr();
	}
	return success;
}

}