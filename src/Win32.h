#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#pragma warning(push, 0)
#include <Windows.h>
#include <timeapi.h>
#include <hidusage.h>
#include <commdlg.h>;
#undef near
#undef far
#pragma warning(pop)
#include "StarChicken_decl.h"

namespace Win32 {

enum CursorType : U32 {
	CURSOR_TYPE_NONE,
	CURSOR_TYPE_POINTER,
	CURSOR_TYPE_SIZE_VERTICAL,
	CURSOR_TYPE_SIZE_HORIZONTAL,
	CURSOR_TYPE_SIZE_DIAG_FORWARD,
	CURSOR_TYPE_SIZE_DIAG_BACKWARD,
	CURSOR_TYPE_SIZE_ALL,
	CURSOR_TYPE_BEAM,
	CURSOR_TYPE_LOADING,
	CURSOR_TYPE_HAND,
	CURSOR_TYPE_Count
};

enum ButtonState : U32 {
	BUTTON_STATE_UP,
	BUTTON_STATE_DOWN,
	BUTTON_STATE_HOLD
};

union MouseValue {
	ButtonState state;
	F32 scroll;
};

enum MouseButton : U32 {
	MOUSE_BUTTON_WHEEL,
	MOUSE_BUTTON_0,
	MOUSE_BUTTON_1,
	MOUSE_BUTTON_2,
	MOUSE_BUTTON_3,
	MOUSE_BUTTON_4,
	MOUSE_BUTTON_Count,

	MOUSE_BUTTON_LEFT = MOUSE_BUTTON_0,
	MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_1,
	MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_2
};

enum Key {
	KEY_0 = 0x30,
	KEY_1 = 0x31,
	KEY_2 = 0x32,
	KEY_3 = 0x33,
	KEY_4 = 0x34,
	KEY_5 = 0x35,
	KEY_6 = 0x36,
	KEY_7 = 0x37,
	KEY_8 = 0x38,
	KEY_9 = 0x39,
	KEY_A = 0x41,
	KEY_B = 0x42,
	KEY_C = 0x43,
	KEY_D = 0x44,
	KEY_E = 0x45,
	KEY_F = 0x46,
	KEY_G = 0x47,
	KEY_H = 0x48,
	KEY_I = 0x49,
	KEY_J = 0x4A,
	KEY_K = 0x4B,
	KEY_L = 0x4C,
	KEY_M = 0x4D,
	KEY_N = 0x4E,
	KEY_O = 0x4F,
	KEY_P = 0x50,
	KEY_Q = 0x51,
	KEY_R = 0x52,
	KEY_S = 0x53,
	KEY_T = 0x54,
	KEY_U = 0x55,
	KEY_V = 0x56,
	KEY_W = 0x57,
	KEY_X = 0x58,
	KEY_Y = 0x59,
	KEY_Z = 0x5A,
	KEY_NUMPAD0 = 0x60,
	KEY_NUMPAD1 = 0x61,
	KEY_NUMPAD2 = 0x62,
	KEY_NUMPAD3 = 0x63,
	KEY_NUMPAD4 = 0x64,
	KEY_NUMPAD5 = 0x65,
	KEY_NUMPAD6 = 0x66,
	KEY_NUMPAD7 = 0x67,
	KEY_NUMPAD8 = 0x68,
	KEY_NUMPAD9 = 0x69,
	KEY_MULTIPLY = 0x6A,
	KEY_ADD = 0x6B,
	KEY_SUBTRACT = 0x6D,
	KEY_DECIMAL = 0x6E,
	KEY_DIVIDE = 0x6F,

	KEY_TAB = 0x09,
	KEY_RETURN = 0x0D,
	KEY_SHIFT = 0x10,
	KEY_CTRL = 0x11,
	KEY_ALT = 0x12,
	KEY_CAPS_LOCK = 0x14,
	KEY_ESC = 0x1B,
	KEY_SPACE = 0x20,
	KEY_LEFT = 0x25,
	KEY_UP = 0x26,
	KEY_RIGHT = 0x27,
	KEY_DOWN = 0x28,
	KEY_BACKSPACE = 0x08,
	KEY_DELETE = 0x2E,

	KEY_F1 = 0x70,
	KEY_F2 = 0x71,
	KEY_F3 = 0x72,
	KEY_F4 = 0x73,
	KEY_F5 = 0x74,
	KEY_F6 = 0x75,
	KEY_F7 = 0x76,
	KEY_F8 = 0x77,
	KEY_F9 = 0x78,
	KEY_F10 = 0x79,
	KEY_F11 = 0x7A,
	KEY_F12 = 0x7B,

	KEY_SEMICOLON = 0xBA,
	KEY_EQUALS = 0xBB,
	KEY_COMMA = 0xBC,
	KEY_DASH = 0xBD,
	KEY_PERIOD = 0xBE,
	KEY_FORWARD_SLASH = 0xBF,
	KEY_BACKTICK = 0xC0,

	KEY_BRACKET_OPEN = 0xDB,
	KEY_BACKSLASH = 0xDC,
	KEY_BRACKET_CLOSE = 0xDD,
	KEY_QUOTE = 0xDE,

	KEY_Count = 0xFF
};

#define USER32_FUNCTIONS X(MessageBoxA)\
	X(DispatchMessageA)\
	X(TranslateMessage)\
	X(PeekMessageA)\
	X(DefWindowProcA)\
	X(PostQuitMessage)\
	X(RegisterClassExA)\
	X(CreateWindowExA)\
	X(LoadImageA)\
	X(SendMessageA)\
	X(DestroyWindow)\
	X(ShowWindow)\
	X(InvalidateRect)\
	X(GetClientRect)\
	X(SetProcessDPIAware)\
	X(LoadIconA)\
	X(GetCursorPos)\
	X(WindowFromPoint)\
	X(GetForegroundWindow)\
	X(ScreenToClient)\
	X(RegisterRawInputDevices)\
	X(ClientToScreen)\
	X(SetCursorPos)\
	X(SetCursor)\
	X(LoadCursorA)\
	X(GetSystemMetrics)\
	X(OpenClipboard)\
	X(CloseClipboard)\
	X(GetClipboardData)\
	X(SetClipboardData)\
	X(EmptyClipboard)\
	X(GetRawInputData)\
	X(SetCapture)\
	X(ReleaseCapture)\
	X(GetWindowRect)

#define X(name) decltype(name)* p##name;
USER32_FUNCTIONS
#undef X

decltype(timeBeginPeriod)* timeBeginPeriod_ptr;
decltype(timeEndPeriod)* timeEndPeriod_ptr;

decltype(GetOpenFileNameA)* pGetOpenFileNameA;

HMODULE user32DLL;
HMODULE winmmDLL;
HMODULE comdlg32DLL;
HINSTANCE instance;
HWND window;
U32 windowWidth;
U32 windowHeight;
I32 framebufferWidth;
I32 framebufferHeight;
B32 shouldRecreateSwapchain;
B32 windowShouldClose;

HCURSOR cursorPointer;
HCURSOR cursorSizeVertical;
HCURSOR cursorSizeHorizontal;
HCURSOR cursorSizeDiagForward;
HCURSOR cursorSizeDiagBackward;
HCURSOR cursorSizeAll;
HCURSOR cursorBeam;
HCURSOR cursorLoading;
HCURSOR cursorHand;

I32 mouseDX;
I32 mouseDY;
I32 oldMouseX;
I32 oldMouseY;
I32 mouseCaptureX;
I32 mouseCaptureY;
V2F32 lastDeltaMousePos;
B32 mouseCaptured;

B8 keyboardState[KEY_Count];
B8 mouseButtonState[MOUSE_BUTTON_Count];

void (*keyboardCallback)(Key, ButtonState);
void (*mouseCallback)(MouseButton, MouseValue);

CursorType currentCursorType;

V2F32 get_mouse() {
	POINT point{};
	pGetCursorPos(&point);
	pScreenToClient(window, &point);
	return V2F32{ F32(point.x), F32(point.y) };
}
void set_mouse(V2F32 pos) {
	POINT point;
	point.x = LONG(pos.x);
	point.y = LONG(pos.y);
	pClientToScreen(window, &point);
	pSetCursorPos(point.x, point.y);
}
V2F32 get_raw_delta_mouse() {
	V2F32 result{ F32(mouseDX), F32(mouseDY) };
	mouseDX = 0;
	mouseDY = 0;
	return result;
}
V2F32 get_delta_mouse() {
	V2F32 mouse = get_mouse();
	V2F32 result = mouse - lastDeltaMousePos;
	lastDeltaMousePos = mouse;
	return result;
}
void set_cursor(CursorType newCursor) {
	if (mouseCaptured) {
		return;
	}
	currentCursorType = newCursor;
	switch (newCursor) {
	case CURSOR_TYPE_NONE: pSetCursor(NULL); break;
	case CURSOR_TYPE_POINTER: pSetCursor(cursorPointer); break;
	case CURSOR_TYPE_SIZE_VERTICAL: pSetCursor(cursorSizeVertical); break;
	case CURSOR_TYPE_SIZE_HORIZONTAL: pSetCursor(cursorSizeHorizontal); break;
	case CURSOR_TYPE_SIZE_DIAG_FORWARD: pSetCursor(cursorSizeDiagForward); break;
	case CURSOR_TYPE_SIZE_DIAG_BACKWARD: pSetCursor(cursorSizeDiagBackward); break;
	case CURSOR_TYPE_SIZE_ALL: pSetCursor(cursorSizeAll); break;
	case CURSOR_TYPE_BEAM: pSetCursor(cursorBeam); break;
	case CURSOR_TYPE_LOADING: pSetCursor(cursorLoading); break;
	case CURSOR_TYPE_HAND: pSetCursor(cursorHand); break;
	default: break;
	}
}
void set_mouse_captured(B32 captured) {
	if (captured) {
		pSetCapture(window);
		POINT cursorPos;
		pGetCursorPos(&cursorPos);
		mouseCaptureX = cursorPos.x;
		mouseCaptureY = cursorPos.y;
		mouseDX = 0;
		mouseDY = 0;
		pSetCursorPos(mouseCaptureX, mouseCaptureY);
		set_cursor(CURSOR_TYPE_NONE);
		mouseCaptured = true;
	} else {
		mouseCaptured = false;
		pReleaseCapture();
		set_cursor(CURSOR_TYPE_POINTER);
	}
}

char key_to_typed_char(Key key) {
	if (keyboardState[KEY_SHIFT]) {
		if (key >= KEY_0 && key <= KEY_9) {
			return ")!@#$%^&*("[key - KEY_0];
		} else if (key >= KEY_A && key <= KEY_Z) {
			return char(key);
		} else if (key >= KEY_SEMICOLON && key <= KEY_BACKTICK) {
			return ":+<_>?~"[key - KEY_SEMICOLON];
		} else if (key >= KEY_BRACKET_OPEN && key <= KEY_QUOTE) {
			return "{|}\""[key - KEY_BRACKET_OPEN];
		}
	} else {
		if (key >= KEY_0 && key <= KEY_9) {
			return char(key);
		} else if (key >= KEY_A && key <= KEY_Z) {
			return char(key) | ('a' - 'A');
		} else if (key >= KEY_SEMICOLON && key <= KEY_BACKTICK) {
			return ";=,-./`"[key - KEY_SEMICOLON];
		} else if (key >= KEY_BRACKET_OPEN && key <= KEY_QUOTE) {
			return "[\\]'"[key - KEY_BRACKET_OPEN];
		}
	}
	if (key == KEY_SPACE) {
		return char(key);
	}
	if (key >= KEY_NUMPAD0 && key <= KEY_DIVIDE) {
		return "0123456789*+\0-./"[key - KEY_NUMPAD0];
	}
	return '\0';
}

void get_clipboard(char* buf, U32* bufLen) {
	if (pOpenClipboard(window)) {
		HANDLE mem = pGetClipboardData(CF_TEXT);
		if (mem) {
			LPVOID data = GlobalLock(mem);
			if (data) {
				*bufLen = U32(min(strlen(reinterpret_cast<char*>(data)), U64(*bufLen)));
				memcpy(buf, data, *bufLen);
				GlobalUnlock(mem);
			}
		} else {
			*bufLen = 0;
		}
		pCloseClipboard();
	}
}
void set_clipboard(const char* buf, U32 bufLen) {
	if (pOpenClipboard(window)) {
		pEmptyClipboard();
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bufLen + 1);
		if (mem) {
			LPVOID ptr = GlobalLock(mem);
			if (ptr) {
				memcpy(ptr, buf, bufLen);
				reinterpret_cast<char*>(ptr)[bufLen] = '\0';
				GlobalUnlock(mem);
				pSetClipboardData(CF_TEXT, mem);
			} else {
				GlobalFree(mem);
			}
		}
		pCloseClipboard();
	}
}
void set_clipboard(StrA str) {
	set_clipboard(str.str, U32(min(str.length, 0xFFFFFFFFull)));
}


void (*resizeDrawCallback)(void);

void error_box(const char* msg) {
	if (window) {
		pMessageBoxA(window, msg, "Error", MB_ICONERROR | MB_OK);
	}
}

void poll_events() {
	MSG message{};
	//TODO: PeekMessageA blocks when the user resizes or moves the window around, stopping my render loop. This isn't ideal.
	while (pPeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
		pTranslateMessage(&message);
		pDispatchMessageA(&message);
	}
}

LRESULT CALLBACK window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	switch (uMsg) {
	case WM_DESTROY: {
		pPostQuitMessage(0);
	} break;
	case WM_KEYDOWN: {
		if (wParam >= KEY_Count) {
			break;
		}
		keyboardState[wParam] = true;
		if (keyboardCallback) {
			keyboardCallback(Key(wParam), BUTTON_STATE_DOWN);
		}
	} break;
	case WM_KEYUP: {
		if (wParam >= KEY_Count) {
			break;
		}
		keyboardState[wParam] = false;
		if (keyboardCallback) {
			keyboardCallback(Key(wParam), BUTTON_STATE_UP);
		}
	} break;
	case WM_INPUT: {
		if (pGetForegroundWindow() != window) {
			break;
		}
		UINT size = sizeof(RAWINPUT);
		pGetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		MemoryArena& stackArena = get_scratch_arena();
		MEMORY_ARENA_FRAME(stackArena) {
			RAWINPUT* raw = stackArena.alloc_bytes<RAWINPUT>(size);
			pGetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
			if (raw->header.dwType == RIM_TYPEMOUSE) {
				RAWMOUSE rawMouse = raw->data.mouse;
				if ((rawMouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE) {
					B32 isVirtualDesktop = (rawMouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;

					I32 width = pGetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
					I32 height = pGetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

					I32 absoluteX = I32((F32(rawMouse.lLastX) / 65535.0F) * F32(width));
					I32 absoluteY = I32((F32(rawMouse.lLastY) / 65535.0F) * F32(height));
					if (oldMouseX != -1) {
						mouseDX += (absoluteX - oldMouseX);
						mouseDY += (absoluteY - oldMouseY);
					}
					oldMouseX = absoluteX;
					oldMouseY = absoluteY;
				} else {
					mouseDX += rawMouse.lLastX;
					mouseDY += rawMouse.lLastY;
				}

				POINT point{};
				pGetCursorPos(&point);
				pScreenToClient(window, &point);
				if (mouseCallback && point.x >= 0 && point.x < framebufferWidth && point.y >= 0 && point.y < framebufferHeight) {
					for (ULONG i = 0; i < 5; i++) {
						ULONG currentButton = (rawMouse.ulButtons >> (i * 2)) & 1;
						if (currentButton) {
							MouseValue val;
							val.state = BUTTON_STATE_DOWN;
							mouseButtonState[MOUSE_BUTTON_0 + i] = true;
							mouseCallback(MouseButton(MOUSE_BUTTON_0 + i), val);
						}
						currentButton = (rawMouse.ulButtons >> (i * 2 + 1)) & 1;
						if (currentButton) {
							MouseValue val;
							val.state = BUTTON_STATE_UP;
							mouseButtonState[MOUSE_BUTTON_0 + i] = false;
							mouseCallback(MouseButton(MOUSE_BUTTON_0 + i), val);
						}
					}
					if (rawMouse.usButtonFlags & RI_MOUSE_WHEEL) {
						MouseValue val;
						val.scroll = F32(I16(rawMouse.usButtonData));
						mouseCallback(MOUSE_BUTTON_WHEEL, val);
					}
				}
				if (mouseCaptured) {
					pSetCursorPos(mouseCaptureX, mouseCaptureY);
				}
			} else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				RAWKEYBOARD rawKeyboard = raw->data.keyboard;
			}
		}
	} break;
	case WM_SIZE: {
		windowWidth = LOWORD(lParam);
		windowHeight = HIWORD(lParam);
		RECT rect;
		if (pGetClientRect(window, &rect)) {
			framebufferWidth = rect.right - rect.left;
			framebufferHeight = rect.bottom - rect.top;
			shouldRecreateSwapchain = true;
		}
	} break;
	case WM_PAINT: {
		resizeDrawCallback();
		result = pDefWindowProcA(hwnd, uMsg, wParam, lParam);
	} break;
	case WM_CLOSE: {
		windowShouldClose = true;
	} break;
	case WM_ERASEBKGND: {
		result = 1;
	} break;
	default: {
		result = pDefWindowProcA(hwnd, uMsg, wParam, lParam);
	} break;
	}
	return result;
}

void show_window() {
	pShowWindow(window, SW_SHOWDEFAULT);
}

StrA open_filename(MemoryArena& arena) {
	if (!pGetOpenFileNameA) {
		return StrA{};
	}
	OPENFILENAMEA options{};
	options.lStructSize = sizeof(options);
	options.hwndOwner = window;
	options.hInstance = instance;
	options.lpstrFile = (LPSTR)(arena.stackBase + arena.stackPtr);
	options.lpstrFile[0] = 0;
	options.nMaxFile = 1024 * 1024;
	options.Flags = OFN_NOCHANGEDIR;
	bool success = pGetOpenFileNameA(&options);
	if (success) {
		StrA result{ options.lpstrFile, strlen(options.lpstrFile) };
		arena.stackPtr += result.length;
		return result;
	} else {
		return StrA{};
	}
}

void get_program_args(ArenaArrayList<StrA>& argOut) {
	using namespace SerializeTools;
	LPSTR argStr = GetCommandLineA();
	StrA args{ argStr, strlen(argStr) };
	// Command line parsing that hopefully follows https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw
	while (skip_whitespace(&args), args.length) {
		char* result = reinterpret_cast<char*>(argOut.allocator->stackBase) + argOut.allocator->stackPtr;
		U32 resultLength = 0;
		bool inQuotes = false;
		while (args.length) {
			if (args.str[0] == '\\' || args.str[0] == '"') {
				U32 backslashCount = 0;
				while (args.length && args.str[0] == '\\') {
					backslashCount++;
					result[resultLength++] = '\\';
					++args;
				}
				if (args.length && args.str[0] == '"') {
					// "2n backslashes followed by a quotation mark produce n backslashes followed by begin/end quote. This does not become part of the parsed argument, but toggles the "in quotes" mode"
					// "(2n) + 1 backslashes followed by a quotation mark again produce n backslashes followed by a quotation mark literal ("). This does not toggle the "in quotes" mode."
					// "n backslashes not followed by a quotation mark simply produce n backslashes."
					resultLength -= backslashCount - backslashCount / 2;
					if (backslashCount & 1) {
						result[resultLength++] = '"';
					} else {
						inQuotes = !inQuotes;
					}
					++args;
				}
			} else {
				if (!inQuotes && is_whitespace(args.str[0])) {
					break;
				}
				result[resultLength++] = args.str[0];
				++args;
			}
		}
		argOut.allocator->stackPtr += resultLength;
		argOut.push_back(StrA{ result, resultLength });
	}
}

bool init(U32 width, U32 height, void (*resizeDrawCallbackIn)(void), void (*keyboardCallbackIn)(Key, ButtonState), void (*mouseCallbackIn)(MouseButton, MouseValue)) {
	resizeDrawCallback = resizeDrawCallbackIn;
	keyboardCallback = keyboardCallbackIn;
	mouseCallback = mouseCallbackIn;
	bool success = true;
	instance = GetModuleHandleA(nullptr);
	user32DLL = LoadLibraryA("User32.dll");
	winmmDLL = LoadLibraryA("Winmm.dll");
	comdlg32DLL = LoadLibraryA("comdlg32.dll");
	if (user32DLL) {
#define X(name) (p##name = reinterpret_cast<decltype(p##name)>(reinterpret_cast<void*>(GetProcAddress(user32DLL, #name))));
		USER32_FUNCTIONS
#undef X

		HANDLE icon = pLoadImageA(instance, "./resources/textures/cyber_seaquell.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
		WNDCLASSEXA windowClass{};
		windowClass.cbSize = sizeof(WNDCLASSEXA);
		windowClass.lpfnWndProc = window_callback;
		windowClass.hInstance = instance;
		windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		windowClass.hIcon = HICON(icon);
		windowClass.hIconSm = HICON(icon);
		const char* className = "Star Chicken View Window";
		windowClass.lpszClassName = className;
		if (pRegisterClassExA(&windowClass)) {
			window = pCreateWindowExA(0, className, "Star Chicken Desktop View", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, int(width), int(height), NULL, NULL, instance, NULL);
			if (window) {
				windowWidth = width;
				windowHeight = height;
				RECT rect;
				if (pGetClientRect(window, &rect)) {
					framebufferWidth = rect.right - rect.left;
					framebufferHeight = rect.bottom - rect.top;
				}
			} else {
				success = false;
			}
		} else {
			success = false;
		}

		cursorPointer = pLoadCursorA(NULL, IDC_ARROW);
		cursorSizeVertical = pLoadCursorA(NULL, IDC_SIZENS);
		cursorSizeHorizontal = pLoadCursorA(NULL, IDC_SIZEWE);
		cursorSizeDiagForward = pLoadCursorA(NULL, IDC_SIZENESW);
		cursorSizeDiagBackward = pLoadCursorA(NULL, IDC_SIZENWSE);
		cursorSizeAll = pLoadCursorA(NULL, IDC_SIZEALL);
		cursorBeam = pLoadCursorA(NULL, IDC_IBEAM);
		cursorLoading = pLoadCursorA(NULL, IDC_WAIT);
		cursorHand = pLoadCursorA(NULL, IDC_HAND);

		RAWINPUTDEVICE mouse{};
		RAWINPUTDEVICE keyboard{};
		mouse.hwndTarget = window;
		mouse.usUsagePage = HID_USAGE_PAGE_GENERIC;
		mouse.usUsage = HID_USAGE_GENERIC_MOUSE;
		mouse.dwFlags = /*RIDEV_INPUTSINK*/0;
		keyboard.hwndTarget = window;
		keyboard.usUsagePage = HID_USAGE_PAGE_GENERIC;
		keyboard.usUsage = HID_USAGE_GENERIC_KEYBOARD;
		keyboard.dwFlags = /*RIDEV_INPUTSINK*/0;
		RAWINPUTDEVICE devices[] = { mouse, keyboard };
		pRegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));

		// Windows has a feature that renders applications at a lower resolution and does a blurry upscale to make them look bigger
		// This disables that functionality
		pSetProcessDPIAware();
		currentCursorType = CURSOR_TYPE_POINTER;
		oldMouseX = oldMouseY = -1;
	} else {
		success = false;
	}
	if (winmmDLL) {
		timeBeginPeriod_ptr = reinterpret_cast<decltype(timeBeginPeriod_ptr)>(reinterpret_cast<void*>(GetProcAddress(winmmDLL, "timeBeginPeriod")));
		timeEndPeriod_ptr = reinterpret_cast<decltype(timeEndPeriod_ptr)>(reinterpret_cast<void*>(GetProcAddress(winmmDLL, "timeEndPeriod")));
	}
	if (comdlg32DLL) {
		pGetOpenFileNameA = (decltype(pGetOpenFileNameA))(void*)GetProcAddress(comdlg32DLL, "GetOpenFileNameA");
	}
	return success;
}

void destroy() {
	pDestroyWindow(window);
}

// Apparently there's some garbage audio driver called Nahimic that injects itself into other programs and sometimes causes bugs,
// and exporting this symbol acts as a disable switch?
// In any case, it costs me nothing to include this, so why not
extern "C" __declspec(dllexport) void NoHotPatch() {}

}