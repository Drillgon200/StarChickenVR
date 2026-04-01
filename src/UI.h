#pragma once
#include "DrillLib.h"
#include "Resources.h"
#include "DynamicVertexBuffer.h"
#include "TextRenderer.h"
#include "SerializeTools.h"

// Placement new from <new> (I don't want to include thousands of lines of C++ headers just for this)
// Just to avoid potential UB issues when putting capturing lambdas in boxes
inline void* __cdecl operator new(size_t _Size, void* _Where) {
	(void)_Size;
	return _Where;
}

namespace UI {

RWSpinLock modificationLock;

struct {
	RGBA8 header{ 0x6F, 0x10, 0x10, 0xFF };
	RGBA8 subheader{ 0x3F, 0x18, 0x18, 0xFF };
	RGBA8 background{ 0x20, 0x20, 0x20, 0xFF };
	RGBA8 foreground{ 0x28, 0x28, 0x28, 0xFF };
	RGBA8 inputField{ 0x10, 0x10, 0x10, 0xFF };
	RGBA8 selectionOutline{ 0x3F, 0x3F, 0x3F, 0xFF };
	RGBA8 button{ 0xD0, 0xD0, 0xD0, 0xFF };
	RGBA8 text{ 0xE1, 0xEA, 0xF2, 0xFF };
	RGBA8 defaultText{ 0xB2, 0xB9, 0xBF, 0xFF };
	RGBA8 highlight{ 0x48, 0x6E, 0xB5, 0x7F };
} themeColor;

enum LayoutDirection : U8 {
	// UP and LEFT are the same as DOWN and RIGHT, but the children are reversed.
	// Use with the corresponding AlignMode to layout from opposite sides
	LAYOUT_DIRECTION_DOWN,
	LAYOUT_DIRECTION_UP,
	LAYOUT_DIRECTION_RIGHT,
	LAYOUT_DIRECTION_LEFT
};
enum SizeMode : U8 {
	SIZE_MODE_FIT_CHILDREN,
	SIZE_MODE_GROW_TO_PARENT,
	SIZE_MODE_PARENT_PERCENT,
	SIZE_MODE_ABSOLUTE
};
enum AlignMode : U8 {
	ALIGN_MODE_TOP_LEFT = 0b0000,
	ALIGN_MODE_TOP_CENTER = 0b0001,
	ALIGN_MODE_TOP_RIGHT = 0b0010,
	ALIGN_MODE_CENTER_LEFT = 0b0100,
	ALIGN_MODE_CENTER_CENTER = 0b0101,
	ALIGN_MODE_CENTER_RIGHT = 0b0110,
	ALIGN_MODE_BOTTOM_LEFT = 0b1000,
	ALIGN_MODE_BOTTOM_CENTER = 0b1001,
	ALIGN_MODE_BOTTOM_RIGHT = 0b1010
};

ArenaArrayList<V2F32> sizeStack;
ArenaArrayList<RGBA8> textColorStack;
ArenaArrayList<RGBA8> borderColorStack;
ArenaArrayList<RGBA8> backgroundColorStack;
ArenaArrayList<F32> textSizeStack;
ArenaArrayList<F32> borderWidthStack;
ArenaArrayList<F32> paddingStack;
ArenaArrayList<Flags32> defaultFlagsStack;
ArenaArrayList<LayoutDirection> layoutDirectionStack;
ArenaArrayList<SizeMode> sizeModeXStack;
ArenaArrayList<SizeMode> sizeModeYStack;
ArenaArrayList<AlignMode> alignModeStack;

#define UI_MAX_Z_OFFSET 2048.0F

#define UI_SIZE(newSize) DEFER_LOOP(UI::sizeStack.push_back(newSize), UI::sizeStack.pop_back())
#define UI_BACKGROUND_COLOR(newColor) DEFER_LOOP(UI::backgroundColorStack.push_back(newColor), UI::backgroundColorStack.pop_back())
#define UI_TEXT_COLOR(newColor) DEFER_LOOP(UI::textColorStack.push_back(newColor), UI::textColorStack.pop_back())
#define UI_BORDER_COLOR(newColor) DEFER_LOOP(UI::borderColorStack.push_back(newColor), UI::borderColorStack.pop_back())
#define UI_TEXT_SIZE(newSize) DEFER_LOOP(UI::textSizeStack.push_back(newSize), UI::textSizeStack.pop_back())
#define UI_BORDER_WIDTH(newWidth) DEFER_LOOP(UI::borderWidthStack.push_back(newWidth), UI::borderWidthStack.pop_back())
#define UI_PADDING(newPadding) DEFER_LOOP(UI::paddingStack.push_back(newPadding), UI::paddingStack.pop_back())
#define UI_FLAGS(newFlags) DEFER_LOOP(UI::defaultFlagsStack.push_back(newFlags), UI::defaultFlagsStack.pop_back())
#define UI_LAYOUT_DIRECTION(newLayoutDirection) DEFER_LOOP(UI::layoutDirectionStack.push_back(newLayoutDirection), UI::layoutDirectionStack.pop_back())
#define UI_SIZE_MODE_X(newSizeMode) DEFER_LOOP(UI::sizeModeXStack.push_back(newSizeMode), UI::sizeModeXStack.pop_back())
#define UI_SIZE_MODE_Y(newSizeMode) DEFER_LOOP(UI::sizeModeYStack.push_back(newSizeMode), UI::sizeModeYStack.pop_back())
#define UI_SIZE_MODE(newSizeMode) DEFER_LOOP((UI::sizeModeYStack.push_back(newSizeMode), UI::sizeModeXStack.push_back(newSizeMode)), (UI::sizeModeXStack.pop_back(), UI::sizeModeYStack.pop_back()))
#define UI_ALIGN_MODE(newAlignMode) DEFER_LOOP(UI::alignModeStack.push_back(newAlignMode), UI::alignModeStack.pop_back())
// Suppress "hides previous local declaration" and "local variable is initialized but not referenced", intended behavior for this construct
#define UI_WORKING_BOX(newBox) __pragma(warning(suppress : 4456 4189))\
	for (UI::Box* oldWorkingBox = UI::workingBox, * irrelevant = UI::workingBox = (newBox); oldWorkingBox; UI::workingBox = oldWorkingBox, oldWorkingBox = nullptr)

const U32 MAX_CLIP_BOXES = 0x10000;
ArenaArrayList<U32> clipBoxIndexStack;
ArenaArrayList<Rng2F32> clipBoxStack;
VK::DedicatedBuffer clipBoxBuffers[VK::FRAMES_IN_FLIGHT];
U32 currentClipBoxCount;

const U32 MAX_TEXT_INPUT = 2048;
const F32 DOUBLE_CLICK_TIME = 0.2F;

struct Box;
// Amalgamation of anything interesting that might happen to a box.
// Each of these fields can be optionally handled in the action callback.
struct UserCommunication {
	V2F32 mousePos;
	F32 scrollInput;
	Flags32 leftClicked : 1;
	Flags32 rightClicked : 1;
	Flags32 middleClicked : 1;
	Flags32 mouse4Clicked : 1;
	Flags32 mouse5Clicked : 1;
	Flags32 leftClickStart : 1;
	Flags32 rightClickStart : 1;
	Flags32 middleClickStart : 1;
	Flags32 mouse4ClickStart : 1;
	Flags32 mouse5ClickStart : 1;
	Flags32 mouseHovered : 1;
	Box* draggedTo;
	V2F32 drag;
	Win32::Key keyPressed;
	char charTyped;
	DynamicVertexBuffer::Tessellator* tessellator;
	Rng2F32 renderArea;
	Rng2F32 renderClipBox;
	U32 clipBoxIndex;
	F32 renderZ;
	F32 scale;
	B32 boxIsBeingDestroyed;
};
enum ActionResult : U32 {
	ACTION_PASS,
	ACTION_HANDLED
};

enum BoxFlag : U32 {
	BOX_FLAG_INVISIBLE = 1 << 0,
	BOX_FLAG_DISABLED = 1 << 1,
	BOX_FLAG_CLIP_CHILDREN = 1 << 2,
	BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION = 1 << 3,
	BOX_FLAG_CUSTOM_DRAW = 1 << 4,
	BOX_FLAG_DONT_CLOSE_CONTEXT_MENU_ON_INTERACTION = 1 << 5,
	BOX_FLAG_WRAP_TEXT = 1 << 6,
	BOX_FLAG_DONT_FIT_CHILDREN = 1 << 7,
	BOX_FLAG_FLOATING = 1 << 8
};
typedef Flags32 BoxFlags;
const F32 BOX_INF_SIZE = 100000.0F;

typedef ActionResult (*BoxActionCallback)(Box* box, UserCommunication& com);
typedef void (*BoxConsumer)(Box* box);

// Having such a large box struct might be memory inefficient, but this setup makes it very easy to combine features, and the total UI memory usage isn't even that much.
// If there are 10000 boxes on the screen, that's only a couple megabytes total.
struct Box {
	BoxFlags flags;
	LayoutDirection layoutDirection;
	SizeMode sizeModeX;
	SizeMode sizeModeY;
	AlignMode align;
	U64 generation;

	// Up the box tree
	Box* parent;
	// Forms a linked list with boxes at the current level of the tree
	Box* next;
	Box* prev;
	// Links to the children
	Box* childFirst;
	Box* childLast;

	V2F pos;
	V2F size;
	V2F maxSize; // Treated as infinity if less than size, otherwise the size will be clamped to this
	V2F parentSizePercent;
	F32 padding; // Padding around edges as well as between children
	F32 zOffset;

	V2F computedPos; // Position relative to parent after the last layout pass
	V2F computedSize; // Size after the last layout pass

	V2F renderPos; // Absolute screen pos after the last render pass

	StrA text;
	StrA tooltip;
	// If typedTextBuffer is not null, text acts as a prompt for the user to type
	char* typedTextBuffer;
	U32 numTypedCharacters;
	F32 textSize;

	F32 borderWidth;
	Win32::CursorType hoverCursor;
	RGBA8 textColor;
	RGBA8 borderColor;
	RGBA8 backgroundColor;
	Resources::Texture* backgroundTexture;

	BoxActionCallback actionCallback;
	union {
		BoxConsumer boxConsumerCallback;
	};
	alignas(16) char callbackData[32];
};
template<typename Callback>
ActionResult action_callback_adapter(Box* box, UserCommunication& comm) {
	void* thisPtr = box->callbackData;
	return (*reinterpret_cast<Callback*>(thisPtr))(box, comm);
}
template<typename Callback>
void box_consumer_adapter(Box* box) {
	void* thisPtr = box->callbackData;
	(*reinterpret_cast<Callback*>(thisPtr))(box);
}
// Callback of type BoxActionCallback (ActionResult callback(Box*, UserCommunication&))
template<typename Callback>
void set_box_callback(Box* box, Callback&& cb) {
	static_assert(sizeof(Callback) <= sizeof(box->callbackData));
	//*reinterpret_cast<Callback*>(callbackData) = std::move(cb);
	new (box->callbackData) Callback(static_cast<Callback&&>(cb));
	box->actionCallback = action_callback_adapter<Callback>;
}
// Callback of type BoxConsumer (void callback(Box*))
template<typename Callback>
void set_box_consumer_box_callback(Box* box, Callback&& cb) {
	static_assert(sizeof(Callback) <= sizeof(box->callbackData));
	//*reinterpret_cast<Callback*>(callbackData) = std::move(cb);
	new (&box->callbackData[0]) Callback(static_cast<Callback&&>(cb));
	box->boxConsumerCallback = box_consumer_adapter<Callback>;
}

struct BoxHandle {
	Box* unsafeBox;
	U64 generation;

	FINLINE Box* get() {
		return !unsafeBox || unsafeBox->generation == 0 || unsafeBox->generation > generation ? nullptr : unsafeBox;
	}
};

struct TypedTextBuffer {
	char* buffer;
	U32 bufferCap;
	U32 textLength;
	// cursorAnchor and cursor represent a highlighted range, both values are the same if nothing is selected
	I32 cursorAnchor;
	I32 cursor;
	F64 lastCursorClickedTime; // For double/triple click
	B8 allowMultiLine;

	void set_buffer(char* data, U32 length, U32 cap) {
		buffer = data;
		bufferCap = cap;
		textLength = length;
		cursor = cursorAnchor = length;
	}

	StrA stra() {
		return StrA{ buffer, textLength };
	}

	enum CharClass {
		// Could be expanded to group special characters or digits separately, not sure if I want that or not
		CHAR_CLASS_WHITESPACE,
		CHAR_CLASS_NON_WHITESPACE
	};
	CharClass char_class(char c) {
		return SerializeTools::is_whitespace(c) ? CHAR_CLASS_WHITESPACE : CHAR_CLASS_NON_WHITESPACE;
	}

	void move_left() {
		if (cursorAnchor != cursor) {
			cursorAnchor = cursor = min(cursor, cursorAnchor);
		} else if (cursor > 0) {
			cursor--, cursorAnchor--;
		}
	}
	void move_right() {
		if (cursorAnchor != cursor) {
			cursorAnchor = cursor = max(cursor, cursorAnchor);
		} else if (cursor < textLength) {
			cursor++, cursorAnchor++;
		}
	}
	void select_left() {
		cursor = max(cursor - 1, 0);
	}
	void select_right() {
		cursor = min(cursor + 1, I32(textLength));
	}
	void select_group_left() {
		// Skip any whitespace, then a block of characters. Seems to match behavior of most editors I've tried.
		for (; cursor > 0 && SerializeTools::is_whitespace(buffer[cursor - 1]); cursor--);
		for (; cursor > 0 && !SerializeTools::is_whitespace(buffer[cursor - 1]); cursor--);
	}
	void select_group_right() {
		// Skip a block of characters, then any whitespace that comes after. Seems to match behavior of most editors I've tried.
		for (; cursor < textLength && !SerializeTools::is_whitespace(buffer[cursor]); cursor++);
		for (; cursor < textLength && SerializeTools::is_whitespace(buffer[cursor]); cursor++);
	}
	void delete_selected() {
		Rng1I32 selected; selected.init(cursor, cursorAnchor);
		memmove(buffer + selected.minX, buffer + selected.maxX, textLength - selected.maxX);
		textLength -= selected.area();
		cursor = cursorAnchor = selected.minX;
	}
	void handle_key_press(Win32::Key key, F32 wrapWidth, F32 sizeY) {
		if (Win32::keyboardState[Win32::KEY_CTRL]) {
			switch (key) {
			case Win32::KEY_A: {
				cursorAnchor = 0;
				cursor = textLength;
			} break;
			case Win32::KEY_C: {
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				Win32::set_clipboard(buffer + selected.minX, selected.area());
			} break;
			case Win32::KEY_X: {
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				Win32::set_clipboard(buffer + selected.minX, selected.area());
				delete_selected();
			} break;
			case Win32::KEY_V: {
				delete_selected();
				MemoryArena& arena = get_scratch_arena();
				char* clipData = (char*)arena.stackBase + arena.stackPtr;
				U32 clipLength = GIGABYTE;
				Win32::get_clipboard(clipData, &clipLength);
				if (!allowMultiLine) {
					for (U32 i = 0; i < clipLength; i++) {
						if (clipData[i] == '\r' || clipData[i] == '\n') {
							clipLength = i;
							break;
						}
					}
				}
				if (textLength + clipLength <= bufferCap) {
					memmove(buffer + cursor + clipLength, buffer + cursor, textLength - cursor);
					memcpy(buffer + cursor, clipData, clipLength);
					textLength += clipLength;
					cursor = cursorAnchor = cursor + clipLength;
				}
			} break;
			case Win32::KEY_LEFT: {
				select_group_left();
				if (!Win32::keyboardState[Win32::KEY_SHIFT]) {
					cursorAnchor = cursor;
				}
			} break;
			case Win32::KEY_RIGHT: {
				select_group_right();
				if (!Win32::keyboardState[Win32::KEY_SHIFT]) {
					cursorAnchor = cursor;
				}
			} break;
			case Win32::KEY_BACKSPACE: {
				if (cursor == cursorAnchor) {
					select_group_left();
				}
				delete_selected();
			} break;
			case Win32::KEY_DELETE: {
				if (cursor == cursorAnchor) {
					select_group_right();
				}
				delete_selected();
			} break;
			default: break;
			}
		} else {
			switch (key) {
			case Win32::KEY_LEFT: {
				if (Win32::keyboardState[Win32::KEY_SHIFT]) {
					select_left();
				} else {
					move_left();
				}
			} break;
			case Win32::KEY_RIGHT: {
				if (Win32::keyboardState[Win32::KEY_SHIFT]) {
					select_right();
				} else {
					move_right();
				}
			} break;
			case Win32::KEY_UP:
			case Win32::KEY_DOWN: {
				if (allowMultiLine) {
					MemoryArena& arena = get_scratch_arena();
					MEMORY_ARENA_FRAME(arena) {
						U32 lineCount;
						U32* originalOffsets;
						StrA* lines = TextRenderer::wrap_text(arena, &lineCount, &originalOffsets, stra(), wrapWidth, sizeY);
						U32 cursorLine = lineCount - 1;
						for (U32 i = 0; i < lineCount; i++) {
							if (cursor >= originalOffsets[i] && cursor < originalOffsets[i] + I32(lines[i].length)) {
								cursorLine = i;
								break;
							}
						}
						I32 cursorColumn = cursor - I32(originalOffsets[cursorLine]);
						if (key == Win32::KEY_UP) {
							if (cursorLine > 0) {
								// Line wrapping has the somewhat annoying limitation of not being able to represent a difference between
								// the cursor position at the end of a line and the cursor position at the start of the next line.
								// We'll just subtract 1 so the "end" of the line is actually one before the end.
								// This works well most of the time, since lines typically wrap on newlines or spaces
								cursor = I32(originalOffsets[cursorLine - 1]) + min(cursorColumn, max(I32(lines[cursorLine - 1].length) - 1, 0));
							} else {
								cursor = 0;
							}
						} else { // KEY_DOWN
							if (cursorLine < lineCount - 1) {
								cursor = I32(originalOffsets[cursorLine + 1]) + min(cursorColumn, max(I32(lines[cursorLine + 1].length) - 1, 0));
							} else {
								cursor = textLength;
							}
						}
						if (!Win32::keyboardState[Win32::KEY_SHIFT]) {
							cursorAnchor = cursor;
						}
					}
				}
			} break;
			case Win32::KEY_BACKSPACE: {
				if (cursor == cursorAnchor) {
					select_left();
				}
				delete_selected();
			} break;
			case Win32::KEY_DELETE: {
				if (cursor == cursorAnchor) {
					select_right();
				}
				delete_selected();
			} break;
			case Win32::KEY_HOME: {
				if (Win32::keyboardState[Win32::KEY_SHIFT]) {
					cursor = 0;
				} else {
					cursor = cursorAnchor = 0;
				}
			} break;
			case Win32::KEY_END: {
				if (Win32::keyboardState[Win32::KEY_SHIFT]) {
					cursor = textLength;
				} else {
					cursor = cursorAnchor = textLength;
				}
			} break;
			default: {
				char letter = Win32::key_to_typed_char(key);
				if (key == Win32::KEY_RETURN && allowMultiLine) {
					letter = '\n';
				}
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				if (letter != '\0' && textLength - selected.area() < bufferCap) {
					memmove(buffer + selected.minX + 1, buffer + selected.maxX, textLength - selected.maxX);
					buffer[selected.minX] = letter;
					textLength = textLength - selected.area() + 1;
					cursor = cursorAnchor = selected.minX + 1;
				}
			} break;
			}
		}
	}
	void handle_mouse_action(V2F pos, bool drag, bool wrap, F32 wrapWidth, F32 sizeY) {
		MemoryArena& arena = get_scratch_arena();
		MEMORY_ARENA_FRAME(arena) {
			StrA baseStr{ buffer, textLength };
			StrA* lines = &baseStr;
			U32 lineCount = 1;
			U32 zero = 0;
			U32* originalLineOffsets = &zero;
			if (wrap) {
				lines = TextRenderer::wrap_text(arena, &lineCount, &originalLineOffsets, baseStr, wrapWidth, sizeY);
			}
			I32 selectedLine = TextRenderer::get_line_idx_from_position(pos.y, sizeY);
			I32 selectedColumn = I32(pos.x / TextRenderer::get_character_width(' ', sizeY) + 0.5F);
			if (selectedLine < 0) {
				selectedLine = 0;
				selectedColumn = 0;
			}
			if (selectedLine >= lineCount) {
				selectedLine = lineCount - 1;
				selectedColumn = I32_MAX;
			}
			selectedColumn = clamp(selectedColumn, 0, I32(lines[selectedLine].length));
			if (drag) {
				cursor = originalLineOffsets[selectedLine] + selectedColumn;
			} else {
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				cursorAnchor = cursor = originalLineOffsets[selectedLine] + selectedColumn;
				F64 time = current_time_seconds();
				if (F32(time - lastCursorClickedTime) < DOUBLE_CLICK_TIME) {
					if (selected.area() == 0 && selected.minX == cursor) {
						// Expand selection to group
						if (textLength > 0) {
							CharClass selectedClass = char_class(buffer[cursor == textLength ? textLength - 1 : cursor]);
							for (; selected.minX > 0 && char_class(buffer[selected.minX - 1]) == selectedClass; selected.minX--);
							for (; selected.maxX < textLength && char_class(buffer[selected.maxX]) == selectedClass; selected.maxX++);
							cursorAnchor = selected.minX;
							cursor = selected.maxX;
						}
					} else if (selected.contains_point(cursor)) {
						// Expand selection to whole line
						for (; selected.minX > 0 && buffer[selected.minX - 1] != '\n'; selected.minX--);
						for (; selected.maxX < textLength && buffer[selected.maxX] != '\n'; selected.maxX++);
						cursorAnchor = selected.minX;
						cursor = selected.maxX;
					}
				}
				lastCursorClickedTime = time;
			}
		}
	}
};

Box* root;
Box* workingBox;

ArenaArrayList<BoxHandle> contextMenuStack;
BoxHandle tooltip;

// Hot is an element a user is about to interact with (mouse hovering over, keyboard selected it with tab, etc)
BoxHandle hotBox;
F64 hotBoxStartTimeSeconds;
// Active is an element a user started an interaction with (mouse click down, keyboard enter down)
BoxHandle activeBox;
F32 activeBoxTotalScale;
// The box currently selected for typing
BoxHandle activeTextBox;
TypedTextBuffer textInputHandler;
// For single line 
F32 activeTextBoxTextRenderOffset;
F64 lastKeyTypedSeconds;

U64 currentGeneration = 1;
Box* boxFreeList = nullptr;

char* textInputFreeList = nullptr;

void make_default_settings(Box* box) {
	box->flags = defaultFlagsStack.back();
	box->text = StrA{};

	box->pos = V2F32{};
	box->size = sizeStack.back();
	box->zOffset = 0.0F;

	box->textSize = textSizeStack.back();
	box->padding = paddingStack.back();
	box->borderWidth = borderWidthStack.back();
	box->hoverCursor = Win32::CURSOR_TYPE_POINTER;
	box->textColor = textColorStack.back();
	box->borderColor = borderColorStack.back();
	box->backgroundColor = backgroundColorStack.back();
	box->backgroundTexture = nullptr;
	box->actionCallback = nullptr;
	box->layoutDirection = layoutDirectionStack.back();
	box->sizeModeX = sizeModeXStack.back();
	box->sizeModeY = sizeModeYStack.back();
	box->align = alignModeStack.back();
}

char* alloc_text_input() {
	if (textInputFreeList == nullptr) {
		textInputFreeList = globalArena.alloc<char>(MAX_TEXT_INPUT);
		STORE_LE64(textInputFreeList, 0);
	}
	char* result = textInputFreeList;
	textInputFreeList = reinterpret_cast<char*>(LOAD_LE64(result));
	return result;
}
void free_text_input(char* buffer) {
	STORE_LE64(buffer, UPtr(textInputFreeList));
	textInputFreeList = buffer;
}
BoxHandle alloc_box() {
	if (!boxFreeList) {
		const U32 amountToAlloc = 256;
		boxFreeList = globalArena.alloc<Box>(amountToAlloc);
		for (U32 i = 0; i < amountToAlloc; i++) {
			boxFreeList[i].next = &boxFreeList[i + 1];
		}
		boxFreeList[amountToAlloc - 1].next = nullptr;
	}
	Box* box = boxFreeList;
	boxFreeList = box->next;
	*box = Box{};
	make_default_settings(box);
	box->generation = currentGeneration++;
	return BoxHandle{ box, box->generation };
}
void free_box(BoxHandle boxHandle) {
	if (Box* box = boxHandle.get()) {
		if (box->actionCallback) {
			UserCommunication comm{};
			comm.boxIsBeingDestroyed = true;
			box->actionCallback(box, comm);
		}
		for (Box* child = box->childFirst; child; child = child->next) {
			free_box(BoxHandle{ child, child->generation });
		}
		if (box->parent) {
			DLL_REMOVE(box, box->parent->childFirst, box->parent->childLast, prev, next);
		}
		if (box->typedTextBuffer) {
			free_text_input(box->typedTextBuffer);
		}
		box->parent = nullptr;
		box->generation = 0;
		box->next = boxFreeList;
		boxFreeList = box;
	}
}

void move_box_to_front(Box* box) {
	if (box->parent) {
		DLL_REMOVE(box, box->parent->childFirst, box->parent->childLast, prev, next);
		DLL_INSERT_HEAD(box, box->parent->childFirst, box->parent->childLast, prev, next);
	}
}

void context_menu(BoxHandle parent, BoxHandle box, V2F32 pos) {
	U32 newContextMenuStackSize = contextMenuStack.size;
	if (Box* parentBox = parent.get()) {
		for (U32 i = contextMenuStack.size; i --> 0;) {
			Box* contextMenuCurrentBox = contextMenuStack.data[i].get();
			if (contextMenuCurrentBox == nullptr) {
				newContextMenuStackSize = i;
			} else if (contextMenuCurrentBox == parentBox) {
				newContextMenuStackSize = i + 1;
			}
		}
	} else {
		newContextMenuStackSize = 0;
	}
	if (!box.get()) {
		newContextMenuStackSize = 0;
	}
	for (U32 i = newContextMenuStackSize; i < contextMenuStack.size; i++) {
		free_box(contextMenuStack.data[i]);
	}
	contextMenuStack.resize(newContextMenuStackSize);
	if (Box* newMenu = box.get()) {
		newMenu->pos = pos;
		contextMenuStack.push_back(box);
	}
}

void clear_context_menu() {
	context_menu(BoxHandle{}, BoxHandle{}, V2F32{});
}

void init_ui() {
	sizeStack.reserve(16);
	sizeStack.push_back(V2F32{ 16.0F, 16.0F });
	textColorStack.reserve(16);
	textColorStack.push_back(RGBA8{ 255, 255, 255, 255 });
	borderColorStack.reserve(16);
	borderColorStack.push_back(RGBA8{ 245, 155, 59, 255 });
	backgroundColorStack.reserve(16);
	backgroundColorStack.push_back(RGBA8{ 61, 61, 61, 255 });
	textSizeStack.reserve(16);
	textSizeStack.push_back(12.0F);
	borderWidthStack.reserve(16);
	borderWidthStack.push_back(0.0F);
	paddingStack.reserve(16);
	paddingStack.push_back(0.0F);
	defaultFlagsStack.reserve(16);
	defaultFlagsStack.push_back(0);
	layoutDirectionStack.reserve(16);
	layoutDirectionStack.push_back(LAYOUT_DIRECTION_DOWN);
	sizeModeXStack.reserve(16);
	sizeModeXStack.push_back(SIZE_MODE_FIT_CHILDREN);
	sizeModeYStack.reserve(16);
	sizeModeYStack.push_back(SIZE_MODE_FIT_CHILDREN);
	alignModeStack.reserve(16);
	alignModeStack.push_back(ALIGN_MODE_TOP_LEFT);

	contextMenuStack.reserve(16);

	workingBox = root = alloc_box().unsafeBox;
	root->flags |= BOX_FLAG_INVISIBLE;
	root->zOffset = UI_MAX_Z_OFFSET * 0.5F;

	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		clipBoxBuffers[i].create(MAX_CLIP_BOXES * sizeof(Rng2F32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK::hostMemoryTypeIndex);
	}
}

void destroy_ui() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		clipBoxBuffers[i].destroy();
	}
}

// Layout based on Clay's algorithm
// There's a significant amount of code duplication between the X and Y procedures, but my attempts at solving this have resulted in worse code
void compute_min_sizes_x_recurse(Box* box) {
	F32 size = box->size.x;
	F32 padding = box->padding;

	if (!box->text.is_empty()) {
		if (box->flags & BOX_FLAG_WRAP_TEXT) {
			// Arbitrary choice, don't wrap with three or less characters
			size = max(size, TextRenderer::string_size_x("   "a, box->textSize) + padding * 2.0F);
			//maxSize.x = min(maxSize.x < size.x ? F32_LARGE : maxSize.x, TextRenderer::string_size_x(box->text, box->textSize));
		} else {
			size = max(size, TextRenderer::string_size_x(box->text, box->textSize) + padding * 2.0F);
		}
	}

	Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;

	F32 current = padding;
	for (Box* child = box->childFirst; child; child = child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_min_sizes_x_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				current += child->computedSize.x + (child->next ? padding : 0.0F);
			} else {
				current = max(current, padding + child->computedPos.x + child->computedSize.x);
			}
		}
	}
	size = max(size, current + padding);
	if (box->sizeModeX == SIZE_MODE_ABSOLUTE || box->flags & BOX_FLAG_DONT_FIT_CHILDREN) {
		size = box->size.x;
	}
	if (box->maxSize.x != 0.0F) {
		size = min(size, box->maxSize.x);
	}
	box->computedSize.x = size;
	box->computedPos.x = box->pos.x;
}
void compute_min_sizes_y_recurse(Box* box) {
	F32 size = box->size.y;
	F32 padding = box->padding;

	StrA boxText = box->text;
	if (box->numTypedCharacters) {
		boxText = StrA{ box->typedTextBuffer, box->numTypedCharacters };
	}
	if (!boxText.is_empty()) {
		if (box->flags & BOX_FLAG_WRAP_TEXT) {
			V2F wrappedSize = TextRenderer::wrapped_size(boxText, box->computedSize.x - padding * 2.0F, box->textSize);
			size = max(size, wrappedSize.y);
		} else {
			size = max(size, TextRenderer::string_size_y(boxText, box->textSize) + padding * 2.0F);
		}
	}

	Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;

	F32 current = padding;
	for (Box* child = box->childFirst; child; child = child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_min_sizes_y_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				current = max(current, padding + child->computedPos.y + child->computedSize.y);
			} else {
				current += child->computedSize.y + (child->next ? padding : 0.0F);
			}
		}
	}
	size = max(size, current + padding);
	if (box->sizeModeY == SIZE_MODE_ABSOLUTE || box->flags & BOX_FLAG_DONT_FIT_CHILDREN) {
		size = box->size.y;
	}
	if (box->maxSize.y != 0.0F) {
		size = min(size, box->maxSize.y);
	}
	box->computedSize.y = size;
	box->computedPos.y = box->pos.y;
}

void compute_final_sizes_and_positions_x_recurse(Box* box) {
	Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;
	bool layoutReversed = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_LEFT;
	F32 padding = box->padding;
	// LEFT = 0
	// CENTER = 1
	// RIGHT = 2
	U32 alignMode = box->align & 0b11u;

	if (layoutAxis == AXIS2_X) {
		F32 totalSpace = box->computedSize.x;
		F32 childrenSpace = 0.0F;
		F32 percentageGrowMinSpace = 0.0F;
		U32 growableCount = 0;
		U32 childCount = 0;
		Box** growables = scratchArena0.alloc<Box*>(0);
		for (Box* child = box->childFirst; child; child = child->next) {
			if (!(child->flags & BOX_FLAG_FLOATING)) {
				childrenSpace += child->computedSize.x;
			}
			if (child->sizeModeX == SIZE_MODE_GROW_TO_PARENT || child->sizeModeX == SIZE_MODE_PARENT_PERCENT) {
				growables[growableCount++] = child;
			}
			if (child->sizeModeX == SIZE_MODE_PARENT_PERCENT) {
				percentageGrowMinSpace += child->computedSize.x;
			}
			childCount++;
		}
		F32 spaceLeft = totalSpace - childrenSpace - F32(childCount - 1) * padding - padding * 2.0F;
		F32 percentageGrowUsableSpace = spaceLeft + percentageGrowMinSpace;
		while (growableCount && spaceLeft > 0.0F) {
			F32 minSize = F32_LARGE;
			F32 secondMinSize = F32_LARGE;
			F32 maxSize = F32_LARGE;
			U32 toGrowCount = 0;
			for (U32 childIdx = 0; childIdx < growableCount; childIdx++) {
				Box* child = growables[childIdx];
				if (child->computedSize.x < minSize) {
					secondMinSize = minSize;
					minSize = child->computedSize.x;
					toGrowCount = 0;
					maxSize = F32_LARGE;
				} else if (child->computedSize.x != minSize && child->computedSize.x < secondMinSize) {
					secondMinSize = child->computedSize.x;
				}
				if (child->computedSize.x == minSize) {
					toGrowCount++;
					if (child->maxSize.x != 0.0F) {
						maxSize = min(maxSize, child->maxSize.x);
					}
					if (child->sizeModeX == SIZE_MODE_PARENT_PERCENT) {
						maxSize = min(maxSize, max(child->computedSize.x, child->parentSizePercent.x * percentageGrowUsableSpace));
					}
				}
			}
			F32 idealGrowth = min(maxSize, secondMinSize) - minSize;
			F32 widthPerBox = min(idealGrowth, spaceLeft / F32(toGrowCount));
			F32 growTo = idealGrowth * F32(toGrowCount) <= spaceLeft ? min(maxSize, secondMinSize) : minSize + widthPerBox;
			for (U32 childIdx = 0; childIdx < growableCount; childIdx++) {
				Box* child = growables[childIdx];
				if (child->computedSize.x == minSize) {
					if (child->maxSize.x != 0.0F && child->maxSize.x == growTo ||
						child->sizeModeX == SIZE_MODE_PARENT_PERCENT && max(child->computedSize.x, child->parentSizePercent.x * percentageGrowUsableSpace) == growTo) {
						growables[childIdx] = growables[--growableCount], childIdx--;
					}
					child->computedSize.x = growTo;
				}
			}
			if (idealGrowth * F32(toGrowCount) > spaceLeft) {
				spaceLeft = 0.0F;
				break;
			}
			spaceLeft -= widthPerBox * F32(toGrowCount);
		}
		if (spaceLeft > 0.0F) {
			F32 alignSpacing = floorf32(spaceLeft * 0.5F * F32(alignMode));
			for (Box* child = box->childFirst; child; child = child->next) {
				child->computedPos.x += alignSpacing;
			}
		}
	} else {
		F32 growSizeX = box->computedSize.x - padding * 2.0F;
		for (Box* child = box->childFirst; child; child = child->next) {
			if (child->sizeModeX == SIZE_MODE_GROW_TO_PARENT) {
				child->computedSize.x = clamp(growSizeX, child->computedSize.x, child->maxSize.x == 0.0F ? F32_LARGE : child->maxSize.x);
			} else if (child->sizeModeX == SIZE_MODE_PARENT_PERCENT) {
				child->computedSize.x = clamp(growSizeX, child->computedSize.x, max(child->padding * 2.0F, child->parentSizePercent.x * growSizeX));
			}
			F32 spaceLeft = floorf32(box->computedSize.x - child->computedSize.x - padding * 2.0F);
			if (spaceLeft > 0.0F) {
				child->computedPos.x += spaceLeft * 0.5F * F32(alignMode);
			}
		}
	}
	
	F32 current = padding;
	for (Box* child = layoutReversed ? box->childLast : box->childFirst; child; child = layoutReversed ? child->prev : child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_final_sizes_and_positions_x_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				child->computedPos.x += current;
				current += child->computedSize.x + padding;
			} else {
				child->computedPos.x += padding;
			}
		}
	}
}

void compute_final_sizes_and_positions_y_recurse(Box* box) {
	Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;
	bool layoutReversed = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_LEFT;
	F32 padding = box->padding;
	// TOP = 0
	// CENTER = 1
	// BOTTOM = 2
	U32 alignMode = U32(box->align) >> 2u;

	if (layoutAxis == AXIS2_X) {
		F32 growSizeY = box->computedSize.y - padding * 2.0F;
		for (Box* child = box->childFirst; child; child = child->next) {
			if (child->sizeModeY == SIZE_MODE_GROW_TO_PARENT) {
				child->computedSize.y = clamp(growSizeY, child->computedSize.y, child->maxSize.y == 0.0F ? F32_LARGE : child->maxSize.y);
			} else if (child->sizeModeY == SIZE_MODE_PARENT_PERCENT) {
				child->computedSize.y = clamp(growSizeY, child->computedSize.y, max(child->padding * 2.0F, child->parentSizePercent.y * growSizeY));
			}
			F32 spaceLeft = box->computedSize.y - child->computedSize.y - padding * 2.0F;
			if (spaceLeft > 0.0F) {
				child->computedPos.y += floorf32(spaceLeft * 0.5F * F32(alignMode));
			}
		}
	} else {
		F32 totalSpace = box->computedSize.y;
		F32 childrenSpace = 0.0F;
		F32 percentageGrowMinSpace = 0.0F;
		U32 growableCount = 0;
		U32 childCount = 0;
		Box** growables = scratchArena0.alloc<Box*>(0);
		for (Box* child = box->childFirst; child; child = child->next) {
			if (!(child->flags & BOX_FLAG_FLOATING)) {
				childrenSpace += child->computedSize.y;
			}
			if (child->sizeModeY == SIZE_MODE_GROW_TO_PARENT || child->sizeModeY == SIZE_MODE_PARENT_PERCENT) {
				growables[growableCount++] = child;
			}
			if (child->sizeModeY == SIZE_MODE_PARENT_PERCENT) {
				percentageGrowMinSpace += child->computedSize.y;
			}
			childCount++;
		}
		F32 spaceLeft = totalSpace - childrenSpace - F32(childCount - 1) * padding - padding * 2.0F;
		F32 percentageGrowUsableSpace = spaceLeft + percentageGrowMinSpace;
		while (growableCount && spaceLeft > 0.0F) {
			F32 minSize = F32_LARGE;
			F32 secondMinSize = F32_LARGE;
			F32 maxSize = F32_LARGE;
			U32 toGrowCount = 0;
			for (U32 childIdx = 0; childIdx < growableCount; childIdx++) {
				Box* child = growables[childIdx];
				if (child->computedSize.y < minSize) {
					secondMinSize = minSize;
					minSize = child->computedSize.y;
					toGrowCount = 0;
					maxSize = F32_LARGE;
				} else if (child->computedSize.y != minSize && child->computedSize.y < secondMinSize) {
					secondMinSize = child->computedSize.y;
				}
				if (child->computedSize.y == minSize) {
					toGrowCount++;
					if (child->maxSize.y != 0.0F) {
						maxSize = min(maxSize, child->maxSize.y);
					}
					if (child->sizeModeY == SIZE_MODE_PARENT_PERCENT) {
						maxSize = min(maxSize, max(child->computedSize.y, child->parentSizePercent.y * percentageGrowUsableSpace));
					}
				}
			}
			F32 idealGrowth = min(maxSize, secondMinSize) - minSize;
			F32 widthPerBox = min(idealGrowth, spaceLeft / F32(toGrowCount));
			F32 growTo = idealGrowth * F32(toGrowCount) <= spaceLeft ? min(maxSize, secondMinSize) : minSize + widthPerBox;
			for (U32 childIdx = 0; childIdx < growableCount; childIdx++) {
				Box* child = growables[childIdx];
				if (child->computedSize.y == minSize) {
					if (child->maxSize.y != 0.0F && child->maxSize.y == growTo ||
						child->sizeModeY == SIZE_MODE_PARENT_PERCENT && max(child->computedSize.y, child->parentSizePercent.y * percentageGrowUsableSpace) == growTo) {
						growables[childIdx] = growables[--growableCount], childIdx--;
					}
					child->computedSize.y = growTo;
				}

			}
			if (idealGrowth * F32(toGrowCount) > spaceLeft) {
				spaceLeft = 0.0F;
				break;
			}
			spaceLeft -= widthPerBox * F32(toGrowCount);
		}
		if (spaceLeft > 0.0F) {
			F32 alignSpacing = floorf32(spaceLeft * 0.5F * F32(alignMode));
			for (Box* child = box->childFirst; child; child = child->next) {
				child->computedPos.y += alignSpacing;
			}
		}
	}

	F32 current = padding;
	for (Box* child = layoutReversed ? box->childLast : box->childFirst; child; child = layoutReversed ? child->prev : child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_final_sizes_and_positions_y_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				child->computedPos.y += padding;
			} else {
				child->computedPos.y += current;
				current += child->computedSize.y + padding;
			}
		}
	}
}

void layout_box(Box* box) {
	// X must be computed before Y so that text can wrap correctly
	compute_min_sizes_x_recurse(box);
	compute_final_sizes_and_positions_x_recurse(box);
	compute_min_sizes_y_recurse(box);
	compute_final_sizes_and_positions_y_recurse(box);
}

void layout_boxes(U32 rootWidth, U32 rootHeight) {
	root->size = root->computedSize = V2F32{ F32(rootWidth), F32(rootHeight) };
	layout_box(root);
	root->computedSize = V2F32{ F32(rootWidth), F32(rootHeight) };

	for (U32 i = 0; i < contextMenuStack.size; i++) {
		if (Box* contextBox = contextMenuStack.data[i].get()) {
			layout_box(contextBox);
			// Clamp the box inside the render area
			contextBox->computedPos.x -= max(0.0F, contextBox->computedPos.x + contextBox->computedSize.x - F32(rootWidth));
			contextBox->computedPos.x -= min(0.0F, contextBox->computedPos.x);
			contextBox->computedPos.y -= max(0.0F, contextBox->computedPos.y + contextBox->computedSize.y - F32(rootHeight));
			contextBox->computedPos.y -= min(0.0F, contextBox->computedPos.y);
		} else {
			for (U32 j = i; j < contextMenuStack.size; j++) {
				free_box(contextMenuStack.data[j]);
			}
			contextMenuStack.resize(i);
			break;
		}
	}

	if (Box* tooltipBox = tooltip.get()) {
		layout_box(tooltipBox);
		// Clamp the box inside the render area
		tooltipBox->computedPos.x -= max(0.0F, tooltipBox->computedPos.x + tooltipBox->computedSize.x - F32(rootWidth));
		tooltipBox->computedPos.x -= min(0.0F, tooltipBox->computedPos.x);
		tooltipBox->computedPos.y -= max(0.0F, tooltipBox->computedPos.y + tooltipBox->computedSize.y - F32(rootHeight));
		tooltipBox->computedPos.y -= min(0.0F, tooltipBox->computedPos.y);
	}
}

void draw_box(DynamicVertexBuffer::Tessellator& tes, Box* box, V2F mousePos, V2F parentPos, F32 scale, F32 z) {
	if (box->flags & BOX_FLAG_DISABLED) {
		return;
	}
	V2F boxPos = parentPos + box->computedPos;
	Rng2F32 renderArea{ boxPos.x, boxPos.y, boxPos.x + box->computedSize.x, boxPos.y + box->computedSize.y };
	box->renderPos = boxPos;
	U32 prevClipBox = clipBoxIndexStack.back();
	if (box->flags & BOX_FLAG_CLIP_CHILDREN && currentClipBoxCount < MAX_CLIP_BOXES) {
		Rng2F32 boxRange = renderArea.intersected(clipBoxStack.back());
		reinterpret_cast<Rng2F32*>(clipBoxBuffers[VK::currentFrameInFlight].mapping)[currentClipBoxCount] = boxRange;
		clipBoxIndexStack.push_back(currentClipBoxCount);
		clipBoxStack.push_back(boxRange);
		currentClipBoxCount++;
	}
	if (!(box->flags & BOX_FLAG_INVISIBLE)) {
		if (box->borderWidth != 0.0F && box->borderColor.a != 0) {
			V4F32 color = box->borderColor.to_v4f32();
			F32 borderWidth = box->borderWidth * scale;
			tes.ui_rect2d(renderArea.minX - borderWidth, renderArea.minY - borderWidth, renderArea.maxX + borderWidth, renderArea.maxY + borderWidth, z, 0.0F, 0.0F, 1.0F, 1.0F, color, Resources::simpleWhite.index, prevClipBox << 16);
		}
		if (box->backgroundColor.a != 0) {
			V4F32 color = box->backgroundColor.to_v4f32();
			if (box->flags & BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION) {
				if (box == hotBox.get()) {
					color = V4F32{ min(1.0F, color.x + 0.1F), min(1.0F, color.y + 0.1F), min(1.0F, color.z + 0.1F) , color.w };
				}
				if (box == activeBox.get()) {
					color = RGBA8{ 71, 114, 179, 255 }.to_v4f32();
				}
			}
			Resources::Texture& tex = box->backgroundTexture ? *box->backgroundTexture : Resources::simpleWhite;
			U32 flags = clipBoxIndexStack.back() << 16;
			if (tex.flags & Resources::TEXTURE_FLAG_MSDF) {
				flags |= VK::UI_RENDER_FLAG_MSDF;
			}
			tes.ui_rect2d(renderArea.minX, renderArea.minY, renderArea.maxX, renderArea.maxY, z, 0.0F, 0.0F, 1.0F, 1.0F, color, tex.index, flags);
		}
		MEMORY_ARENA_FRAME(scratchArena0) {
			if (!box->text.is_empty() && box->numTypedCharacters == 0) {
				StrA str = box->text;
				StrA* lines = &str;
				U32 zero = 0;
				U32* originalLineOffsets = &zero;
				U32 lineCount = 1;
				if (box->flags & BOX_FLAG_WRAP_TEXT) {
					lines = TextRenderer::wrap_text(scratchArena0, &lineCount, &originalLineOffsets, str, box->computedSize.x - box->padding * 2.0F, box->textSize);
				}
				V4F32 textColor = box->textColor.to_v4f32();
				if (box->typedTextBuffer && box->numTypedCharacters == 0) {
					textColor = themeColor.defaultText.to_v4f32();
				}
				F32 strHeight = TextRenderer::lines_size_y(lines, lineCount, box->textSize * scale);
				F32 textStartX = renderArea.minX + box->padding * scale;
				F32 textStartY = 0.5F * (renderArea.minY + renderArea.maxY) - 0.5F * strHeight;
				TextRenderer::draw_string_batched(tes, str, textStartX, textStartY, z, box->textSize * scale, textColor, clipBoxIndexStack.back() << 16);
			}
			if (box->typedTextBuffer) {
				StrA str{ box->typedTextBuffer, box->numTypedCharacters };
				StrA* lines = &str;
				U32 zero = 0;
				U32* originalLineOffsets = &zero;
				U32 lineCount = 1;
				if (box->flags & BOX_FLAG_WRAP_TEXT) {
					lines = TextRenderer::wrap_text(scratchArena0, &lineCount, &originalLineOffsets, str, box->computedSize.x - box->padding * 2.0F, box->textSize);
				}
				F32 strHeight = TextRenderer::lines_size_y(lines, lineCount, box->textSize * scale);
				F32 textStartX = renderArea.minX + box->padding * scale;
				F32 charWidth = TextRenderer::get_character_width(' ', box->textSize * scale); // Only supporting monospaced for now
				if (activeTextBox.get() == box) {
					if (!(box->flags & BOX_FLAG_WRAP_TEXT)) {
						F32 cursorRenderPos = textStartX + charWidth * textInputHandler.cursor - activeTextBoxTextRenderOffset;
						activeTextBoxTextRenderOffset += cursorRenderPos - clamp(cursorRenderPos, textStartX, renderArea.maxX - box->padding * scale);
					}
					textStartX -= activeTextBoxTextRenderOffset;
				}
				F32 textStartY = 0.5F * (renderArea.minY + renderArea.maxY) - 0.5F * strHeight;
				TextRenderer::draw_lines_batched(tes, lines, lineCount, textStartX, textStartY, z, box->textSize * scale, box->textColor.to_v4f32(), clipBoxIndexStack.back() << 16, false);
				if (activeTextBox.get() == box) {
					V4F highlightColor = themeColor.highlight.to_v4f32();
					F32 cursorOffsetX = 0.0F;
					F32 cursorOffsetY = 0.0F;
					Rng1I32 selected; selected.init(textInputHandler.cursor, textInputHandler.cursorAnchor);
					for (U32 i = 0; i < lineCount; i++) {
						StrA line = lines[i];
						I32 highlightStart = max(0, selected.minX - I32(originalLineOffsets[i]));
						I32 highlightEnd = min(I32(line.length), selected.maxX - I32(originalLineOffsets[i]));
						F32 yOffset = TextRenderer::lines_size_y(lines, i, box->textSize * scale);
						if (highlightEnd > highlightStart) {
							tes.ui_rect2d(textStartX + charWidth * F32(highlightStart), textStartY + yOffset, textStartX + charWidth * F32(highlightEnd), textStartY + yOffset + box->textSize * scale, z, 0.0F, 0.0F, 1.0F, 1.0F, highlightColor, Resources::simpleWhite.index, clipBoxIndexStack.back() << 16);
						}
						if (textInputHandler.cursor >= originalLineOffsets[i] && textInputHandler.cursor < originalLineOffsets[i] + line.length ||
							i == lineCount - 1 && textInputHandler.cursor == originalLineOffsets[i] + line.length) {

							cursorOffsetX = charWidth * (textInputHandler.cursor - originalLineOffsets[i]);
							cursorOffsetY = yOffset;
						}
					}
					if (fractf64(current_time_seconds() - lastKeyTypedSeconds) < 0.5) {
						tes.ui_rect2d(textStartX + cursorOffsetX, textStartY + cursorOffsetY, textStartX + cursorOffsetX + 1.0F, textStartY + cursorOffsetY + box->textSize * scale, z, 0.0F, 0.0F, 1.0F, 1.0F, V4F{ 0.85F, 0.85F, 0.85F, 1.0F }, Resources::simpleWhite.index, clipBoxIndexStack.back() << 16);
					}
				}
			}
		}
	}
	for (Box* child = box->childLast; child; child = child->prev) {
		draw_box(tes, child, mousePos, boxPos, scale * 1.0F, z + child->zOffset);
	}
	if (box->flags & BOX_FLAG_CUSTOM_DRAW) {
		UserCommunication comm{};
		comm.mousePos = mousePos;
		comm.tessellator = &tes;
		comm.renderArea = renderArea;
		comm.scale = scale;
		comm.renderClipBox = clipBoxStack.back();
		comm.clipBoxIndex = clipBoxIndexStack.back();
		comm.renderZ = z;
		box->actionCallback(box, comm);
	}
	if (box->flags & BOX_FLAG_CLIP_CHILDREN && currentClipBoxCount <= MAX_CLIP_BOXES) {
		clipBoxIndexStack.pop_back();
		clipBoxStack.pop_back();
	}
}

void draw() {
	modificationLock.lock_read();
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	tes.begin_draw(VK::uiPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_QUADS);
	tes.set_clip_boxes(clipBoxBuffers[VK::currentFrameInFlight].gpuAddress);
	Rng2F32 infRange = { -F32_LARGE, -F32_LARGE, F32_LARGE, F32_LARGE };
	reinterpret_cast<Rng2F32*>(clipBoxBuffers[VK::currentFrameInFlight].mapping)[0] = infRange;
	currentClipBoxCount = 1;
	clipBoxIndexStack.clear();
	clipBoxIndexStack.push_back(0);
	clipBoxStack.clear();
	clipBoxStack.push_back(infRange);
	V2F32 mousePos = Win32::get_mouse();
	draw_box(tes, root, mousePos, root->computedPos, 1.0F, root->zOffset);
	for (BoxHandle contextMenuHandle : contextMenuStack) {
		if (Box* contextMenuBox = contextMenuHandle.get()) {
			draw_box(tes, contextMenuBox, mousePos, V2F32{}, 1.0F, UI_MAX_Z_OFFSET * 3 / 4);
		}
	}
	if (Box* tooltipBox = tooltip.get()) {
		draw_box(tes, tooltipBox, mousePos, V2F32{}, 1.0F, 1.0F);
	}
	tes.end_draw();
	modificationLock.unlock_read();
}

B32 mouse_input_for_box_recurse(bool* anyContained, Box* box, V2F32 pos, Win32::MouseButton button, Win32::MouseValue state, V2F32 parentPos, F32 scale, bool isContextMenu) {
	if (box->flags & BOX_FLAG_DISABLED) {
		return false;
	}
	V2F32 boxPos = parentPos + box->computedPos;
	Rng2F32 renderArea{ boxPos.x, boxPos.y, boxPos.x + box->computedSize.x, boxPos.y + box->computedSize.y };
	box->renderPos = boxPos;
	bool mouseOutside = !renderArea.contains_point(pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContained && !mouseOutside && box != root) {
		*anyContained = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (mouse_input_for_box_recurse(anyContained, child, pos, button, state, boxPos, scale * 1.0F, isContextMenu)) {
			return false;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr) {
		return false;
	}
	UserCommunication comm{};
	comm.mousePos = pos;
	comm.renderArea = renderArea;
	comm.scale = scale;
	if (button == Win32::MOUSE_BUTTON_WHEEL) {
		comm.scrollInput = state.scroll;
	} else {
		if (activeBox.get() == box) {
			if (state.state == Win32::BUTTON_STATE_UP) {
				comm.leftClicked = button == Win32::MOUSE_BUTTON_LEFT;
				comm.rightClicked = button == Win32::MOUSE_BUTTON_RIGHT;
				comm.middleClicked = button == Win32::MOUSE_BUTTON_MIDDLE;
				comm.mouse4Clicked = button == Win32::MOUSE_BUTTON_3;
				comm.mouse5Clicked = button == Win32::MOUSE_BUTTON_4;
			}
		} else if (Box* active = activeBox.get()) {
			if (active->actionCallback && state.state == Win32::BUTTON_STATE_UP) {
				comm.draggedTo = box;
				if (active->actionCallback(active, comm) == ACTION_HANDLED) {
					return true;
				}
				comm.draggedTo = nullptr;
			}
		} else if (hotBox.get() == box && state.state == Win32::BUTTON_STATE_DOWN) {
			comm.leftClickStart = button == Win32::MOUSE_BUTTON_LEFT;
			comm.rightClickStart = button == Win32::MOUSE_BUTTON_RIGHT;
			comm.middleClickStart = button == Win32::MOUSE_BUTTON_MIDDLE;
			comm.mouse4ClickStart = button == Win32::MOUSE_BUTTON_3;
			comm.mouse5ClickStart = button == Win32::MOUSE_BUTTON_4;
			activeBox = BoxHandle{ box, box->generation };
			if (activeTextBox.get() != box) {
				if (Box* textEntry = activeTextBox.get()) {
					textEntry->borderWidth = 0.0F;
				}
				activeTextBox = BoxHandle{};
			}
			// This is a bit of a hack to avoid having to set parents while also having drag properly scaleed.
			// It won't update correctly if the user scales while dragging
			activeBoxTotalScale = scale;
		}
	}
	ActionResult result = box->actionCallback(box, comm);
	if (result == ACTION_HANDLED && isContextMenu && !(box->flags & BOX_FLAG_DONT_CLOSE_CONTEXT_MENU_ON_INTERACTION)) {
		clear_context_menu();
	}
	return result == ACTION_HANDLED;
}
bool inDialog = false;
bool handle_mouse_action(V2F32 pos, Win32::MouseButton button, Win32::MouseValue state) {
	if (inDialog) return false;
	modificationLock.lock_write();
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_DOWN) {
		activeBox = BoxHandle{};
	}
	bool anyContained = false;
	for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
		if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
			mouse_input_for_box_recurse(&anyContained, contextMenuBox, pos, button, state, root->computedPos, 1.0F, true);
			if (anyContained) {
				goto contextMenuClicked;
			}
		}
	}
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_DOWN) {
		clear_context_menu();
	}
	mouse_input_for_box_recurse(&anyContained, root, pos, button, state, root->computedPos, 1.0F, false);
contextMenuClicked:;
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_DOWN && activeBox.get() && activeBox.get() == activeTextBox.get()) {
		Box* box = activeBox.unsafeBox;
		F32 textStartX = box->renderPos.x + box->padding * activeBoxTotalScale;
		StrA str = textInputHandler.stra();
		F32 wrapWidth = box->computedSize.x - box->padding * 2.0F;
		F32 textStartY = box->renderPos.y + 0.5F * box->computedSize.y - 0.5F * (box->flags & BOX_FLAG_WRAP_TEXT ? TextRenderer::wrapped_size(str, wrapWidth, box->textSize).y : TextRenderer::string_size_y(str, box->textSize));
		textInputHandler.handle_mouse_action(pos - V2F{ textStartX - activeTextBoxTextRenderOffset, textStartY }, false, box->flags & BOX_FLAG_WRAP_TEXT, wrapWidth, box->textSize);
	}
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_UP) {
		activeBox = BoxHandle{};
	}
	modificationLock.unlock_write();
	return anyContained;
}
B32 mouse_update_for_box_recurse(B32* anyContains, Box* box, V2F32 pos, V2F32 delta, V2F32 parentPos, F32 scale) {
	if (box->flags & BOX_FLAG_DISABLED) {
		return false;
	}
	V2F32 boxPos = parentPos + box->computedPos;
	Rng2F32 renderArea{ boxPos.x, boxPos.y, boxPos.x + box->computedSize.x, boxPos.y + box->computedSize.y };
	box->renderPos = boxPos;
	B32 mouseOutside = !renderArea.contains_point(pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContains && !mouseOutside) {
		*anyContains = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (mouse_update_for_box_recurse(anyContains, child, pos, delta, boxPos, scale * 1.0F)) {
			return true;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr) {
		return false;
	}
	if (hotBox.get() != box) {
		hotBoxStartTimeSeconds = current_time_seconds();
	}
	hotBox = BoxHandle{ box, box->generation };
	Win32::set_cursor(box->hoverCursor);
	UserCommunication comm{};
	comm.mouseHovered = true;
	comm.mousePos = pos;
	comm.renderArea = renderArea;
	comm.scale = scale;
	box->actionCallback(box, comm);
	return true;
}

void handle_mouse_update(V2F32 pos, V2F32 delta) {
	modificationLock.lock_write();
	Box* active = activeBox.get();
	if (active && active->flags & BOX_FLAG_DISABLED) {
		activeBox = BoxHandle{};
		active = nullptr;
	}
	if (active && active->actionCallback) {
		if (activeTextBox.get() == active && delta != V2F{ 0.0F, 0.0F }) {
			F32 textStartX = active->renderPos.x + active->padding * activeBoxTotalScale;
			StrA str = textInputHandler.stra();
			F32 wrapWidth = active->computedSize.x - active->padding * 2.0F;
			F32 textStartY = active->renderPos.y + 0.5F * active->computedSize.y - 0.5F * (active->flags & BOX_FLAG_WRAP_TEXT ? TextRenderer::wrapped_size(str, wrapWidth, active->textSize).y : TextRenderer::string_size_y(str, active->textSize));
			textInputHandler.handle_mouse_action(pos - V2F{ textStartX - activeTextBoxTextRenderOffset, textStartY }, true, active->flags & BOX_FLAG_WRAP_TEXT, wrapWidth, active->textSize);
		}
		UserCommunication comm{};
		comm.mousePos = pos;
		comm.drag = delta / activeBoxTotalScale;
		active->actionCallback(active, comm);
	}
	if (!active) {
		hotBox = BoxHandle{};
		for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
			if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
				B32 anyContained = false;
				mouse_update_for_box_recurse(&anyContained, contextMenuBox, pos, delta, V2F32{}, 1.0F);
				if (anyContained) {
					goto contextMenuHovered;
				}
			}
		}
		mouse_update_for_box_recurse(nullptr, root, pos, delta, root->computedPos, 1.0F);
	contextMenuHovered:;
		if (!hotBox.get() && Rng2F32{ 0.0F, 0.0F, F32(Win32::framebufferWidth), F32(Win32::framebufferHeight) }.contains_point(pos)) {
			Win32::set_cursor(Win32::CURSOR_TYPE_POINTER);
		}
	}
	modificationLock.unlock_write();
}
B32 keyboard_input_for_box_recurse(B32* anyContained, Box* box, V2F32 pos, Win32::Key key, Win32::ButtonState state, V2F32 parentPos, F32 scale) {
	if (box->flags & BOX_FLAG_DISABLED) {
		return false;
	}
	V2F32 boxPos = parentPos + box->computedPos;
	Rng2F32 renderArea{ boxPos.x, boxPos.y, boxPos.x + box->computedSize.x, boxPos.y + box->computedSize.y };
	box->renderPos = boxPos;
	B32 mouseOutside = !renderArea.contains_point(pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContained && !mouseOutside) {
		*anyContained = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (keyboard_input_for_box_recurse(anyContained, child, pos, key, state, boxPos, scale * 1.0F)) {
			return false;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr || state != Win32::BUTTON_STATE_DOWN) {
		return false;
	}
	UserCommunication comm{};
	comm.mousePos = pos;
	comm.keyPressed = key;
	comm.charTyped = Win32::key_to_typed_char(key);

	ActionResult result = box->actionCallback(box, comm);
	return result == ACTION_HANDLED;
}
void handle_keyboard_action(V2F32 mousePos, Win32::Key key, Win32::ButtonState state) {
	modificationLock.lock_write();
	if (Box* activeTextInput = activeTextBox.get()) {
		if (state == Win32::BUTTON_STATE_DOWN) {
			if (Box* active = activeTextBox.get()) {
				lastKeyTypedSeconds = current_time_seconds();
				F32 wrapWidth = active->computedSize.x - active->padding * 2.0F;
				textInputHandler.handle_key_press(key, wrapWidth, active->textSize);
			}
			activeTextInput->numTypedCharacters = textInputHandler.textLength;
			if (activeTextInput->boxConsumerCallback) {
				activeTextInput->boxConsumerCallback(activeTextInput); // Notify user that the field changed
			}
		}
	} else {
		for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
			if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
				B32 anyContained = false;
				keyboard_input_for_box_recurse(&anyContained, contextMenuBox, mousePos, key, state, V2F32{}, 1.0F);
				if (anyContained) {
					goto contextMenuTyped;
				}
			}
		}
		keyboard_input_for_box_recurse(nullptr, root, mousePos, key, state, root->pos, 1.0F);
	contextMenuTyped:;
	}
	modificationLock.unlock_write();
}

StrA get_user_selected_file(MemoryArena& arena) {
	inDialog = true;
	StrA result = Win32::open_filename(arena);
	inDialog = false;
	return result;
}



BoxHandle generic_box() {
	BoxHandle box = alloc_box();
	box.unsafeBox->parent = workingBox;
	DLL_INSERT_TAIL(box.unsafeBox, workingBox->childFirst, workingBox->childLast, prev, next);
	return box;
}
void pop_box() {
	if (workingBox != root) {
		workingBox = workingBox->parent;
	}
}
void layout_box(LayoutDirection direction, BoxActionCallback callback) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	box.unsafeBox->layoutDirection = direction;
	box.unsafeBox->actionCallback = callback;
	workingBox = box.unsafeBox;
}
void ubox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_UP, callback);
}
void dbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_DOWN, callback);
}
void lbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_LEFT, callback);
}
void rbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_RIGHT, callback);
}
void background_box() {
	BoxHandle box = generic_box();
	box.unsafeBox->sizeModeX = box.unsafeBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
	workingBox = box.unsafeBox;
}
#define UI_UBOX() DEFER_LOOP(UI::ubox(), UI::pop_box())
#define UI_DBOX() DEFER_LOOP(UI::dbox(), UI::pop_box())
#define UI_LBOX() DEFER_LOOP(UI::lbox(), UI::pop_box())
#define UI_RBOX() DEFER_LOOP(UI::rbox(), UI::pop_box())
#define UI_BACKGROUND() DEFER_LOOP(UI::background_box(), UI::pop_box())
BoxHandle spacer(F32 spacing) {
	BoxHandle box = generic_box();
	if (workingBox->layoutDirection == LAYOUT_DIRECTION_LEFT || workingBox->layoutDirection == LAYOUT_DIRECTION_RIGHT) {
		box.unsafeBox->size.x = spacing;
	} else {
		box.unsafeBox->size.y = spacing;
	}
	box.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	return box;
}
BoxHandle spacer() {
	return spacer(BOX_INF_SIZE);
}
BoxHandle text(StrA str, BoxActionCallback actionCallback = nullptr) {
	BoxHandle box = generic_box();
	box.unsafeBox->text = str;
	box.unsafeBox->backgroundColor = RGBA8{};
	box.unsafeBox->actionCallback = actionCallback;
	return box;
}
// onClick of type BoxConsumer
template<typename Callback>
BoxHandle text_button(StrA text, Callback&& onClick) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags |= BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
	box.unsafeBox->text = text;
	box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
	box.unsafeBox->padding = 2.0F;
	set_box_consumer_box_callback(box.unsafeBox, reinterpret_cast<Callback&&>(onClick));
	box.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
		if (com.leftClicked) {
			box->boxConsumerCallback(box);
			return ACTION_HANDLED;
		}
		return ACTION_PASS;
		};
	return box;
}

// onTextUpdated of type BoxConsumer
template<typename Callback>
BoxHandle text_input(StrA prompt, StrA defaultValue, bool multiline, Callback&& onTextUpdated) {
	BoxHandle boxHandle = generic_box();
	Box* box = boxHandle.unsafeBox;
	box->flags = BOX_FLAG_CLIP_CHILDREN;
	if (multiline) {
		box->flags |= BOX_FLAG_WRAP_TEXT;
	}
	box->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
	box->sizeModeY = SIZE_MODE_FIT_CHILDREN;
	box->size.x = 100.0F;
	box->text = prompt;
	box->borderWidth = 0.0F;
	box->backgroundColor = themeColor.inputField;
	box->borderColor = themeColor.selectionOutline;
	box->padding = 2.0F;
	box->hoverCursor = Win32::CURSOR_TYPE_BEAM;
	box->typedTextBuffer = alloc_text_input();
	defaultValue.length = min<U64>(defaultValue.length, MAX_TEXT_INPUT);
	memcpy(box->typedTextBuffer, defaultValue.str, defaultValue.length);
	box->numTypedCharacters = U32(defaultValue.length);
	set_box_consumer_box_callback(box, reinterpret_cast<Callback&&>(onTextUpdated));
	box->actionCallback = [](Box* box, UserCommunication& comm) {
		if (comm.leftClickStart || comm.rightClickStart) {
			if (activeTextBox.get() != box) {
				box->borderWidth = 1.0F;
				activeTextBox = BoxHandle{ box, box->generation };
				activeTextBoxTextRenderOffset = 0.0F;
				textInputHandler.set_buffer(box->typedTextBuffer, box->numTypedCharacters, MAX_TEXT_INPUT);
				textInputHandler.allowMultiLine = box->flags & BOX_FLAG_WRAP_TEXT;
			}
			return ACTION_HANDLED;
		}
		return ACTION_PASS;
	};
	return boxHandle;
}

// onClick of type BoxConsumer
template<typename Callback>
BoxHandle button(Resources::Texture& tex, Callback&& onClick) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags |= BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
	box.unsafeBox->backgroundTexture = &tex;
	box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
	box.unsafeBox->backgroundColor = themeColor.button;
	set_box_consumer_box_callback(box.unsafeBox, reinterpret_cast<Callback&&>(onClick));
	box.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
		if (com.leftClicked) {
			box->boxConsumerCallback(box);
			return ACTION_HANDLED;
		}
		return ACTION_PASS;
		};
	return box;
}

void path_input(StrA fieldName) {
	UI_RBOX() {
		workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		workingBox->align = ALIGN_MODE_CENTER_LEFT;
		text(fieldName);
		spacer(4.0F);
		BoxHandle textInput = text_input("Enter file path"a, ""a, false, [](Box* box){});
		button(Resources::uiFolder, [textInput](Box* box) mutable {
			if (Box* box = textInput.get()) {
				MemoryArena& arena = get_scratch_arena();
				MEMORY_ARENA_FRAME(arena) {
					StrA path = get_user_selected_file(arena);
					if (path.length <= MAX_TEXT_INPUT) {
						memcpy(box->typedTextBuffer, path.str, path.length);
						box->numTypedCharacters = path.length;
					}
				}
			}
		});
	}
}

/*
// onClick of type BoxConsumer
// Min and max are only hints, they do not guarantee the text value
template<typename Callback>
BoxHandle slider_number(F32 min, F32 max, F32 step, Callback&& onTextUpdated) {
	BoxHandle result;
	UI_RBOX() {
		UI_SIZE((V2F32{ 8.0F, 8.0F })) {
			BoxHandle incrementButton = generic_box();
			incrementButton.unsafeBox->flags |= BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
			incrementButton.unsafeBox->backgroundTexture = &Resources::uiIncrementLeft;
			incrementButton.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
			incrementButton.unsafeBox->flags |= BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
			set_box_consumer_box_callback(incrementButton.unsafeBox, reinterpret_cast<Callback&&>(onTextUpdated));
			incrementButton.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
				if (com.leftClicked) {
					Box* textInput = box->next;
					F32 step = bitcast<F32>(U32(textInput->callbackAltData[1]));
					F32 min = bitcast<F32>(U32(textInput->callbackAltData[2]));
					F32 max = bitcast<F32>(U32(textInput->callbackAltData[2] >> 32));
					F64 num;
					StrA textStr = StrA{ textInput->typedTextBuffer, textInput->numTypedCharacters };
					if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
						num = clamp<F32>(num - step, min, max);
						textInput->numTypedCharacters = MAX_TEXT_INPUT;
						SerializeTools::serialize_f64(textInput->typedTextBuffer, &textInput->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
						BoxConsumer(textInput->callbackAltData[0])(textInput);
					}
					return ACTION_HANDLED;
				}
				return ACTION_PASS;
			};
		}


		BoxHandle box = generic_box();
		result = box;
		box.unsafeBox->flags = BOX_FLAG_CLIP_CHILDREN | BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
		box.unsafeBox->sizeParentPercent.x = 1.0F;
		box.unsafeBox->text = ""a;
		box.unsafeBox->typedTextBuffer = alloc_text_input();
		StrA defaultStr = "0.0"a;
		memcpy(box.unsafeBox->typedTextBuffer, defaultStr.str, defaultStr.length);
		box.unsafeBox->numTypedCharacters = U32(defaultStr.length);
		set_box_consumer_box_callback(box.unsafeBox, reinterpret_cast<Callback&&>(onTextUpdated));
		box.unsafeBox->callbackAltData[1] = bitcast<U32>(step);
		box.unsafeBox->callbackAltData[2] = U64(bitcast<U32>(max)) << 32 | bitcast<U32>(min);
		box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_SIZE_HORIZONTAL;
		box.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
			if (comm.leftClicked) {
				activeTextBox = BoxHandle{ box, box->generation };
				return ACTION_HANDLED;
			}
			F32 step = bitcast<F32>(U32(box->callbackAltData[1]));
			F32 min = bitcast<F32>(U32(box->callbackAltData[2]));
			F32 max = bitcast<F32>(U32(box->callbackAltData[2] >> 32));
			if (comm.keyPressed && activeTextBox.get() == box) {
				if (comm.charTyped && box->numTypedCharacters < MAX_TEXT_INPUT) {
					box->typedTextBuffer[box->numTypedCharacters++] = comm.charTyped;
				} else if (comm.keyPressed == Win32::KEY_BACKSPACE && box->numTypedCharacters > 0) {
					box->numTypedCharacters--;
				}
				BoxConsumer(box->callbackAltData[0])(box);
				return ACTION_HANDLED;
			}
			if (comm.drag.x) {
				F64 num;
				StrA textStr = StrA{ box->typedTextBuffer, box->numTypedCharacters };
				if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
					num += step * comm.drag.x;
					box->numTypedCharacters = MAX_TEXT_INPUT;
					SerializeTools::serialize_f64(box->typedTextBuffer, &box->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
					BoxConsumer(box->callbackAltData[0])(box);
				}
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};

		UI_SIZE((V2F32{ 8.0F, 8.0F })) {
			BoxHandle incrementButton = generic_box();
			incrementButton.unsafeBox->flags |= BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
			incrementButton.unsafeBox->backgroundTexture = &Resources::uiIncrementRight;
			incrementButton.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
			incrementButton.unsafeBox->flags |= BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
			set_box_consumer_box_callback(incrementButton.unsafeBox, reinterpret_cast<Callback&&>(onTextUpdated));
			incrementButton.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
				if (com.leftClicked) {
					Box* textInput = box->prev;
					F32 step = bitcast<F32>(U32(textInput->callbackAltData[1]));
					F32 min = bitcast<F32>(U32(textInput->callbackAltData[2]));
					F32 max = bitcast<F32>(U32(textInput->callbackAltData[2] >> 32));
					F64 num;
					StrA textStr = StrA{ textInput->typedTextBuffer, textInput->numTypedCharacters };
					if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
						num = clamp<F32>(num + step, min, max);
						textInput->numTypedCharacters = MAX_TEXT_INPUT;
						SerializeTools::serialize_f64(textInput->typedTextBuffer, &textInput->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
						BoxConsumer(textInput->callbackAltData[0])(textInput);
					}
					return ACTION_HANDLED;
				}
				return ACTION_PASS;
			};
		}

	}
	return result;
}
template<typename Callback>
BoxHandle slider_number(F32 step, Callback&& onTextUpdated) {
	return slider_number(-F32_INF, F32_INF, step, reinterpret_cast<Callback&&>(onTextUpdated));
}
*/

Box* context_menu_begin_helper() {
	BoxHandle dummyParent = alloc_box();
	dummyParent.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	workingBox = dummyParent.unsafeBox;
	BoxHandle box = generic_box();
	workingBox = box.unsafeBox;
	box.unsafeBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	spacer(2.0F);
	dbox();
	workingBox->padding = 2.0F;
	return dummyParent.unsafeBox;
}
void context_menu_end_helper(BoxHandle parent, V2F32 offset) {
	pop_box();
	spacer(2.0F);
	pop_box();
	context_menu(parent, BoxHandle{ workingBox, workingBox->generation }, offset);
}

#define UI_ADD_CONTEXT_MENU(parent, offset) for (UI::Box* oldWorkingBox = UI::workingBox, * contextMenuBox = UI::context_menu_begin_helper(); oldWorkingBox; UI::context_menu_end_helper(parent, offset), UI::workingBox = oldWorkingBox, oldWorkingBox = nullptr)


}