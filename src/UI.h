#pragma once
#include "DrillLib.h"
#include "Resources.h"
#include "DynamicVertexBuffer.h"
#include "TextRenderer.h"
#include "SerializeTools.h"
#include "compression/Oklab.h"

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
	Flags32 textBoxDeselected : 1;
	Box* draggedTo;
	V2F32 drag;
	V2F32 totalDrag;
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
	BOX_FLAG_FLOATING = 1 << 8,
	BOX_FLAG_SLIDER_MIN_MAX_ENFORCED = 1 << 9,
	BOX_FLAG_WRAP_CHILDREN = 1 << 10
};
typedef Flags32 BoxFlags;
const F32 BOX_INF_SIZE = 100000.0F;

typedef ActionResult (*BoxActionCallback)(Box* box, UserCommunication& com);
typedef void (*BoxConsumer)(Box* box);
typedef void (*ColorConsumer)(V4F color);
typedef void (*ColorConsumerAdapted)(Box* box, V4F color);

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
	Rng2F32 clippedRenderArea; // The area this box was allowed to draw to last render pass

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
	U32 backgroundRenderFlags;
	Rng2F32 backgroundUV;

	union {
		struct {
			F64 minVal;
			F64 maxVal;
			F64 val;
		} f64;
		struct {
			I64 minVal;
			I64 maxVal;
			I64 val;
		} i64;
		B8 b8;
		struct {
			V3F oklabLrCH;
			V3F srgb;
		} color;
	} value;

	union {
		StrA* strA;
		F64* f64;
		I64* i64;
		B8* b8;
	} updatePtr;

	BoxActionCallback actionCallback;
	union {
		BoxConsumer boxConsumerCallback;
		ColorConsumerAdapted colorConsumerCallback;
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
template<typename Callback>
void color_consumer_adapter(Box* box, V4F color) {
	void* thisPtr = box->callbackData;
	(*reinterpret_cast<Callback*>(thisPtr))(color);
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
// Callback of type ColorConsumer (void callback(V4F))
template<typename Callback>
void set_color_consumer_box_callback(Box* box, Callback&& cb) {
	static_assert(sizeof(Callback) <= sizeof(box->callbackData));
	//*reinterpret_cast<Callback*>(callbackData) = std::move(cb);
	new (&box->callbackData[0]) Callback(static_cast<Callback&&>(cb));
	box->colorConsumerCallback = color_consumer_adapter<Callback>;
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
	I32 bufferCap;
	I32 textLength;
	// cursorAnchor and cursor represent a highlighted range, both values are the same if nothing is selected
	I32 cursorAnchor;
	I32 cursor;
	F64 lastCursorClickedTime; // For double/triple click
	B8 allowMultiLine;

	void set_buffer(char* data, U32 length, U32 cap) {
		buffer = data;
		bufferCap = I32(cap);
		textLength = I32(length);
		cursor = cursorAnchor = I32(length);
	}

	StrA stra() {
		return StrA{ buffer, U64(textLength) };
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
		memmove(buffer + selected.minX, buffer + selected.maxX, U32(textLength - selected.maxX));
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
				Win32::set_clipboard(buffer + selected.minX, U32(selected.area()));
			} break;
			case Win32::KEY_X: {
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				Win32::set_clipboard(buffer + selected.minX, U32(selected.area()));
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
				if (textLength + I32(clipLength) <= bufferCap) {
					memmove(buffer + cursor + clipLength, buffer + cursor, U32(textLength - cursor));
					memcpy(buffer + cursor, clipData, clipLength);
					textLength += I32(clipLength);
					cursor = cursorAnchor = cursor + I32(clipLength);
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
							if (cursor >= I32(originalOffsets[i]) && cursor < I32(originalOffsets[i]) + I32(lines[i].length)) {
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
					memmove(buffer + selected.minX + 1, buffer + selected.maxX, U32(textLength - selected.maxX));
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
			StrA baseStr = stra();
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
			if (selectedLine >= I32(lineCount)) {
				selectedLine = I32(lineCount) - 1;
				selectedColumn = I32_MAX;
			}
			selectedColumn = clamp(selectedColumn, 0, I32(lines[selectedLine].length));
			if (drag) {
				cursor = I32(originalLineOffsets[selectedLine]) + selectedColumn;
			} else {
				Rng1I32 selected; selected.init(cursor, cursorAnchor);
				cursorAnchor = cursor = I32(originalLineOffsets[selectedLine]) + selectedColumn;
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
V2F totalActiveDrag;
// The box currently selected for typing
BoxHandle activeTextBox;
TypedTextBuffer textInputHandler;
// For single line 
F32 activeTextBoxTextRenderOffset;
F64 lastKeyTypedSeconds;

void set_active_text_box(Box* box) {
	DEBUG_ASSERT(box->typedTextBuffer != nullptr, "Tried to set a non text box to active"a);
	if (activeTextBox.get() != box) {
		box->borderWidth = 1.0F;
		activeTextBox = BoxHandle{ box, box->generation };
		activeTextBoxTextRenderOffset = 0.0F;
		textInputHandler.set_buffer(box->typedTextBuffer, box->numTypedCharacters, MAX_TEXT_INPUT);
		textInputHandler.allowMultiLine = box->flags & BOX_FLAG_WRAP_TEXT;
	}
}

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
	box->backgroundUV = Rng2F32{ 0.0F, 0.0F, 1.0F, 1.0F };
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
	textColorStack.push_back(themeColor.text);
	borderColorStack.reserve(16);
	borderColorStack.push_back(themeColor.selectionOutline);
	backgroundColorStack.reserve(16);
	backgroundColorStack.push_back(themeColor.background);
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
			if (layoutAxis == AXIS2_X && !(box->flags & BOX_FLAG_WRAP_CHILDREN)) {
				current += child->computedSize.x + padding;
			} else {
				current = max(current, padding + child->computedPos.x + child->computedSize.x + padding);
			}
		}
	}
	current = max(padding, current - padding);
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
	bool layoutReversed = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_LEFT;

	F32 current = padding;
	F32 wrapCounterX = padding;
	F32 lastMaxYSize = 0.0F;
	for (Box* child = layoutReversed ? box->childLast : box->childFirst; child; child = layoutReversed ? child->prev : child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_min_sizes_y_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				if (box->flags & BOX_FLAG_WRAP_CHILDREN && wrapCounterX + child->computedSize.x > box->computedSize.x && wrapCounterX != padding) {
					wrapCounterX = padding;
					current += lastMaxYSize + padding;
					lastMaxYSize = 0.0F;
				}
				wrapCounterX += child->computedSize.x + padding;
				lastMaxYSize = max(lastMaxYSize, child->computedPos.y + child->computedSize.y);
			} else {
				current += child->computedSize.y + padding;
			}
		}
	}
	if (lastMaxYSize != 0.0F) {
		current += lastMaxYSize + padding;
	}
	current = max(padding, current - padding);
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
			if (child->flags & BOX_FLAG_DISABLED) {
				continue;
			}
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
				if (!(child->flags & BOX_FLAG_DISABLED)) {
					child->computedPos.x += alignSpacing;
				}
			}
		}
	} else {
		F32 growSizeX = box->computedSize.x - padding * 2.0F;
		for (Box* child = box->childFirst; child; child = child->next) {
			if (child->flags & BOX_FLAG_DISABLED) {
				continue;
			}
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
				if (box->flags & BOX_FLAG_WRAP_CHILDREN && current + child->computedSize.x > box->computedSize.x && current != padding) {
					current = padding;
				}
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
			if (child->flags & BOX_FLAG_DISABLED) {
				continue;
			}
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
			if (child->flags & BOX_FLAG_DISABLED) {
				continue;
			}
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
				if (!(child->flags & BOX_FLAG_DISABLED)) {
					child->computedPos.y += alignSpacing;
				}
			}
		}
	}

	F32 current = padding;
	F32 wrapCounterX = padding;
	F32 lastMaxYSize = 0.0F;
	for (Box* child = layoutReversed ? box->childLast : box->childFirst; child; child = layoutReversed ? child->prev : child->next) {
		if (child->flags & BOX_FLAG_DISABLED) {
			continue;
		}
		compute_final_sizes_and_positions_y_recurse(child);
		if (!(child->flags & BOX_FLAG_FLOATING)) {
			if (layoutAxis == AXIS2_X) {
				if (box->flags & BOX_FLAG_WRAP_CHILDREN && wrapCounterX + child->computedSize.x > box->computedSize.x && wrapCounterX != padding) {
					wrapCounterX = padding;
					current += lastMaxYSize + padding;
					lastMaxYSize = 0.0F;
				}
				child->computedPos.y += current;
				wrapCounterX += child->computedSize.x + padding;
				lastMaxYSize = max(lastMaxYSize, child->computedSize.y);
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
	Rng2F32 renderAreaClipped = renderArea.intersected(clipBoxStack.back());
	box->renderPos = boxPos;
	box->clippedRenderArea = renderAreaClipped;
	U32 prevClipBox = clipBoxIndexStack.back();
	if (box->flags & BOX_FLAG_CLIP_CHILDREN && currentClipBoxCount < MAX_CLIP_BOXES) {
		reinterpret_cast<Rng2F32*>(clipBoxBuffers[VK::currentFrameInFlight].mapping)[currentClipBoxCount] = renderAreaClipped;
		clipBoxIndexStack.push_back(currentClipBoxCount);
		clipBoxStack.push_back(renderAreaClipped);
		currentClipBoxCount++;
	}
	if (!(box->flags & BOX_FLAG_INVISIBLE)) {
		if (box->borderWidth != 0.0F && box->borderColor.a != 0) {
			V4F32 color = box->borderColor.to_v4f32();
			F32 borderWidth = box->borderWidth * scale;
			tes.ui_rect2d(renderArea.minX - borderWidth, renderArea.minY - borderWidth, renderArea.maxX + borderWidth, renderArea.maxY + borderWidth, z, 0.0F, 0.0F, 0.0F, 0.0F, color, Resources::simpleWhite.index, prevClipBox << 16);
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
			U32 flags = clipBoxIndexStack.back() << 16 | box->backgroundRenderFlags;
			if (tex.flags & Resources::TEXTURE_FLAG_MSDF) {
				flags |= VK::UI_RENDER_FLAG_MSDF;
			}
			Rng2F32 uv = box->backgroundUV;
			tes.ui_rect2d(renderArea.minX, renderArea.minY, renderArea.maxX, renderArea.maxY, z, uv.minX, uv.minY, uv.maxX, uv.maxY, color, tex.index, flags);
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
						F32 cursorRenderPos = textStartX + charWidth * F32(textInputHandler.cursor) - activeTextBoxTextRenderOffset;
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
						if (textInputHandler.cursor >= I32(originalLineOffsets[i]) && textInputHandler.cursor < I32(originalLineOffsets[i]) + I32(line.length) ||
							i == lineCount - 1 && U64(textInputHandler.cursor) == originalLineOffsets[i] + line.length) {

							cursorOffsetX = charWidth * F32(textInputHandler.cursor - originalLineOffsets[i]);
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
					if (textEntry->actionCallback) {
						UserCommunication deselectComm{};
						deselectComm.textBoxDeselected = true;
						textEntry->actionCallback(textEntry, deselectComm);
					}
				}
				activeTextBox = BoxHandle{};
			}
			totalActiveDrag = V2F{};
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
		comm.renderArea = Rng2F32{ active->renderPos.x, active->renderPos.y, active->renderPos.x + active->computedSize.x, active->renderPos.y + active->computedSize.y };
		totalActiveDrag += comm.drag;
		comm.totalDrag = totalActiveDrag;
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
			activeTextInput->numTypedCharacters = U32(textInputHandler.textLength);
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
BoxHandle background_box() {
	BoxHandle box = generic_box();
	box.unsafeBox->sizeModeX = box.unsafeBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
	workingBox = box.unsafeBox;
	return box;
}
#define UI_UBOX() DEFER_LOOP(UI::ubox(), UI::pop_box())
#define UI_DBOX() DEFER_LOOP(UI::dbox(), UI::pop_box())
#define UI_LBOX() DEFER_LOOP(UI::lbox(), UI::pop_box())
#define UI_RBOX() DEFER_LOOP(UI::rbox(), UI::pop_box())
#define UI_BACKGROUND() DEFER_LOOP(UI::background_box(), UI::pop_box())
BoxHandle spacer(F32 spacing) {
	BoxHandle box = generic_box();
	box.unsafeBox->sizeModeX = box.unsafeBox->sizeModeY = SIZE_MODE_FIT_CHILDREN;
	if (workingBox->layoutDirection == LAYOUT_DIRECTION_LEFT || workingBox->layoutDirection == LAYOUT_DIRECTION_RIGHT) {
		box.unsafeBox->size = V2F{ spacing, 0.0F };
	} else {
		box.unsafeBox->size = V2F{ 0.0F, spacing };
	}
	box.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	return box;
}
BoxHandle spacer() {
	BoxHandle box = spacer(0.0F);
	if (workingBox->layoutDirection == LAYOUT_DIRECTION_LEFT || workingBox->layoutDirection == LAYOUT_DIRECTION_RIGHT) {
		box.unsafeBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
	} else {
		box.unsafeBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
	}
	return box;
}
BoxHandle icon(Resources::Texture& tex) {
	BoxHandle box = generic_box();
	box.unsafeBox->sizeModeX = box.unsafeBox->sizeModeY = SIZE_MODE_FIT_CHILDREN;
	box.unsafeBox->backgroundTexture = &tex;
	return box;
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
	box.unsafeBox->backgroundColor = themeColor.inputField;
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
			set_active_text_box(box);
			return ACTION_HANDLED;
		}
		if (comm.textBoxDeselected) {
			box->boxConsumerCallback(box);
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

BoxHandle path_input(StrA fieldName) {
	BoxHandle result{};
	UI_RBOX() {
		workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		workingBox->align = ALIGN_MODE_CENTER_LEFT;
		text(fieldName);
		spacer(4.0F);
		BoxHandle textInput = text_input("Enter file path"a, ""a, false, [](Box* box){});
		button(Resources::uiFolder, [textInput](Box* buttonBox) mutable {
			if (Box* box = textInput.get()) {
				MemoryArena& arena = get_scratch_arena();
				MEMORY_ARENA_FRAME(arena) {
					StrA path = get_user_selected_file(arena);
					if (path.length <= MAX_TEXT_INPUT) {
						memcpy(box->typedTextBuffer, path.str, path.length);
						box->numTypedCharacters = U32(path.length);
						box->boxConsumerCallback(box);
					}
				}
			}
		});
		result = textInput;
	}
	return result;
}

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

// Suppress "hides previous local declaration" and "local variable is initialized but not referenced", intended behavior for this construct
#define UI_ADD_CONTEXT_MENU(parent, offset) __pragma(warning(suppress : 4456 4189))\
	for (UI::Box* oldWorkingBox = UI::workingBox, * contextMenuBox = UI::context_menu_begin_helper(); oldWorkingBox; UI::context_menu_end_helper(parent, offset), UI::workingBox = oldWorkingBox, oldWorkingBox = nullptr)

void set_box_f64_val(Box* box, F64 newVal) {
	if (box->flags & BOX_FLAG_SLIDER_MIN_MAX_ENFORCED) {
		newVal = clamp(newVal, box->value.f64.minVal, box->value.f64.maxVal);
	}
	*box->updatePtr.f64 = newVal;
	if (box->typedTextBuffer) {
		U32 bufferSize = MAX_TEXT_INPUT;
		SerializeTools::serialize_f64(box->typedTextBuffer, &bufferSize, newVal);
		box->numTypedCharacters = bufferSize;
	}
}

BoxHandle slider_f64(F64* toUpdate = nullptr, F64 defaultVal = 0.0, F64 minVal = -F64_INF, F64 maxVal = F64_INF, F64 incrementAmount = 1.0, bool minMaxEnforced = false) {
	if (maxVal < minVal) {
		maxVal = minVal;
	}
	Box* textBox = nullptr;
	UI_RBOX() {
		workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		button(Resources::uiArrowLeft, [incrementAmount](Box* box){
			Box* slider = box->next;
			set_box_f64_val(slider, *slider->updatePtr.f64 - incrementAmount);
		});
		textBox = text_input(""a, "0.0"a, false, [](Box* box){
			F64 newVal = *box->updatePtr.f64;
			F64 parsed;
			if (SerializeTools::parse_f64(&parsed, StrA{ box->typedTextBuffer, box->numTypedCharacters })) {
				newVal = parsed;
			}
			if (box->numTypedCharacters == 0) {
				newVal = 0.0;
			}
			set_box_f64_val(box, newVal);
		}).unsafeBox;
		textBox->actionCallback = [](Box* box, UserCommunication& com) {
			if (com.leftClicked || com.rightClicked) {
				set_active_text_box(box);
				return ACTION_HANDLED;
			}
			if (com.textBoxDeselected) {
				box->boxConsumerCallback(box);
				return ACTION_HANDLED;
			}
			if (com.drag.x) {
				F64 dragAmount = com.drag.x / box->computedSize.x * (box->value.f64.maxVal - box->value.f64.minVal);
				if (box->value.f64.minVal == -F64_INF || box->value.f64.maxVal == F64_INF) {
					dragAmount = com.drag.x * 0.125;
				}
				set_box_f64_val(box, clamp(*box->updatePtr.f64 + dragAmount, box->value.f64.minVal, box->value.f64.maxVal));
				return ACTION_HANDLED;
			}
			if (com.tessellator) {
				F32 percentUsed = F32(clamp01((*box->updatePtr.f64 - box->value.f64.minVal) / (box->value.f64.maxVal - box->value.f64.minVal)));
				F32 maxX = com.renderArea.minX + com.renderArea.width() * percentUsed;
				V4F color = themeColor.button.to_v4f32();
				color.w = 0.3F;
				com.tessellator->ui_rect2d(com.renderArea.minX, com.renderArea.minY, maxX, com.renderArea.maxY, com.renderZ, 0.0F, 0.0F, 1.0F, 1.0F, color, Resources::simpleWhite.index, com.clipBoxIndex << 16);
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};
		if (!toUpdate) {
			toUpdate = &textBox->value.f64.val;
		}
		*toUpdate = defaultVal;
		textBox->value.f64.minVal = minVal;
		textBox->value.f64.maxVal = maxVal;
		textBox->updatePtr.f64 = toUpdate;
		textBox->hoverCursor = Win32::CURSOR_TYPE_SIZE_HORIZONTAL;
		bool usesMinMax = minVal != -F64_INF || maxVal != F64_INF;
		if (usesMinMax) {
			textBox->flags |= BOX_FLAG_CUSTOM_DRAW;
		}
		if (minMaxEnforced) {
			textBox->flags |= BOX_FLAG_SLIDER_MIN_MAX_ENFORCED;
		}
		set_box_f64_val(textBox, defaultVal);
		button(Resources::uiArrowRight, [incrementAmount](Box* box) {
			Box* slider = box->prev;
			set_box_f64_val(slider, *slider->updatePtr.f64 + incrementAmount);
		});
	}
	return BoxHandle{ textBox, textBox->generation };
}

void set_box_i64_val(Box* box, I64 newVal) {
	if (box->flags & BOX_FLAG_SLIDER_MIN_MAX_ENFORCED) {
		newVal = clamp(newVal, box->value.i64.minVal, box->value.i64.maxVal);
	}
	*box->updatePtr.i64 = newVal;
	if (box->typedTextBuffer) {
		U32 bufferSize = MAX_TEXT_INPUT;
		SerializeTools::serialize_i64(box->typedTextBuffer, &bufferSize, newVal);
		box->numTypedCharacters = bufferSize;
	}
}

BoxHandle slider_i64(I64* toUpdate = nullptr, I64 defaultVal = 0, I64 minVal = I64_MIN, I64 maxVal = I64_MAX, I64 incrementAmount = 1, bool minMaxEnforced = false) {
	// Mostly the same code as the F64 slider, slightly different drag code because it's quantized larger than a pixel
	if (maxVal < minVal) {
		maxVal = minVal;
	}
	Box* textBox = nullptr;
	UI_RBOX() {
		workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		button(Resources::uiArrowLeft, [incrementAmount](Box* box){
			Box* slider = box->next;
			set_box_i64_val(slider, *slider->updatePtr.i64 - incrementAmount);
		});
		textBox = text_input(""a, "0"a, false, [](Box* box){
			I64 newVal = *box->updatePtr.i64;
			I64 parsed;
			SerializeTools::IntParseError err = SerializeTools::parse_i64(&parsed, StrA{ box->typedTextBuffer, box->numTypedCharacters });
			if (err == SerializeTools::INT_PARSE_OVERFLOW) {
				newVal = I64_MAX;
			} else if (err == SerializeTools::INT_PARSE_UNDERFLOW) {
				newVal = I64_MIN;
			} else if (err == SerializeTools::INT_PARSE_SUCCESS) {
				newVal = parsed;
			}
			if (box->numTypedCharacters == 0) {
				newVal = 0;
			}
			set_box_i64_val(box, newVal);
		}).unsafeBox;
		textBox->actionCallback = [](Box* box, UserCommunication& com) {
			if (com.leftClicked || com.rightClicked) {
				set_active_text_box(box);
				return ACTION_HANDLED;
			}
			if (com.textBoxDeselected) {
				box->boxConsumerCallback(box);
				return ACTION_HANDLED;
			}
			if (com.drag.x) {
				F64 dragPerUnit = box->computedSize.x / F64(box->value.i64.maxVal - box->value.i64.minVal);
				I64 incrementBy = 0;
				if (box->value.i64.minVal == I64_MIN || box->value.i64.maxVal == I64_MAX) {
					incrementBy = I64(com.drag.x);
				} else {
					incrementBy = I64(com.totalDrag.x / dragPerUnit);
					totalActiveDrag.x -= F32(F64(incrementBy) * dragPerUnit);
				}
				set_box_i64_val(box, clamp(*box->updatePtr.i64 + incrementBy, box->value.i64.minVal, box->value.i64.maxVal));
				return ACTION_HANDLED;
			}
			if (com.tessellator) {
				F32 percentUsed = F32(clamp01(F64(*box->updatePtr.i64 - box->value.i64.minVal) / F64(box->value.i64.maxVal - box->value.i64.minVal)));
				F32 maxX = com.renderArea.minX + com.renderArea.width() * percentUsed;
				V4F color = themeColor.button.to_v4f32();
				color.w = 0.3F;
				com.tessellator->ui_rect2d(com.renderArea.minX, com.renderArea.minY, maxX, com.renderArea.maxY, com.renderZ, 0.0F, 0.0F, 1.0F, 1.0F, color, Resources::simpleWhite.index, com.clipBoxIndex << 16);
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};
		if (!toUpdate) {
			toUpdate = &textBox->value.i64.val;
		}
		*toUpdate = defaultVal;
		textBox->value.i64.minVal = minVal;
		textBox->value.i64.maxVal = maxVal;
		textBox->updatePtr.i64 = toUpdate;
		textBox->hoverCursor = Win32::CURSOR_TYPE_SIZE_HORIZONTAL;
		bool usesMinMax = minVal != I64_MIN || maxVal != I64_MAX;
		if (usesMinMax) {
			textBox->flags |= BOX_FLAG_CUSTOM_DRAW;
		}
		if (minMaxEnforced) {
			textBox->flags |= BOX_FLAG_SLIDER_MIN_MAX_ENFORCED;
		}
		set_box_i64_val(textBox, defaultVal);
		button(Resources::uiArrowRight, [incrementAmount](Box* box) {
			Box* slider = box->prev;
			set_box_i64_val(slider, *slider->updatePtr.i64 + incrementAmount);
		});
	}
	return BoxHandle{ textBox, textBox->generation };
}

BoxHandle slider_bool(B8* toUpdate = nullptr, B8 defaultVal = false) {
	BoxHandle result{};
	UI_RBOX() {
		result = BoxHandle{ workingBox, workingBox->generation };
		F32 size = sizeStack.back().x;
		Box* spacerBefore = spacer(size).unsafeBox;
		Box* iconBox = generic_box().unsafeBox;
		Box* spacerAfter = spacer(size).unsafeBox;
		(defaultVal ? spacerAfter : spacerBefore)->flags |= BOX_FLAG_DISABLED;
		iconBox->backgroundTexture = defaultVal ? &Resources::uiToggleOn : &Resources::uiToggleOff;
		iconBox->backgroundColor = themeColor.button;

		set_box_callback(workingBox, [spacerBefore, spacerAfter, iconBox](Box* box, UserCommunication& com) {
			if (com.leftClicked) {
				*box->updatePtr.b8 = B8(!bool(*box->updatePtr.b8));
				iconBox->backgroundTexture = *box->updatePtr.b8 ? &Resources::uiToggleOn : &Resources::uiToggleOff;
				box->backgroundColor = *box->updatePtr.b8 ? themeColor.subheader : themeColor.inputField;
				spacerBefore->flags ^= BOX_FLAG_DISABLED;
				spacerAfter->flags ^= BOX_FLAG_DISABLED;
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		});
		workingBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
		if (!toUpdate) {
			toUpdate = &workingBox->value.b8;
		}
		*toUpdate = defaultVal;
		workingBox->updatePtr.b8 = toUpdate;
		workingBox->backgroundColor = defaultVal ? themeColor.subheader : themeColor.inputField;
		workingBox->padding = 2.0F;
		workingBox->flags &= ~BOX_FLAG_INVISIBLE;
	}
	return result;
}

BoxHandle labeled_slider_bool(StrA label, B8* toUpdate = nullptr, B8 defaultVal = false) {
	BoxHandle result{};
	UI_RBOX() {
		workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		workingBox->align = ALIGN_MODE_CENTER_LEFT;
		text(label);
		spacer(4.0F);
		result = slider_bool(toUpdate, defaultVal);
	}
	return result;
}

void accordion_begin(StrA name) {
	dbox();
	workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
	Box* collapseButton = button(Resources::simpleWhite, [](Box* box) {}).unsafeBox;
	collapseButton->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	collapseButton->sizeModeY = SIZE_MODE_FIT_CHILDREN;
	collapseButton->align = ALIGN_MODE_CENTER_LEFT;
	collapseButton->padding = 2.0F;
	collapseButton->backgroundColor = RGBA8{ 0, 0, 0, 0 };
	Box* collapseIcon = nullptr;
	UI_WORKING_BOX(collapseButton) {
		collapseIcon = icon(Resources::uiAccordionClosed).unsafeBox;
		collapseIcon->backgroundColor = themeColor.button;
		collapseIcon->size *= 0.5F;
		text(name);
	}
	rbox();
	workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
	Box* userContainer = workingBox;
	set_box_consumer_box_callback(collapseButton, [userContainer, collapseIcon](Box* box) {
		bool previouslyDisabled = (userContainer->flags & BOX_FLAG_DISABLED) != 0;
		userContainer->flags ^= BOX_FLAG_DISABLED;
		collapseIcon->backgroundTexture = previouslyDisabled ? &Resources::uiAccordionOpen : &Resources::uiAccordionClosed;
	});
	userContainer->flags |= BOX_FLAG_DISABLED;
	spacer(10.0F);
	dbox();
	workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
}

void accordion_end() {
	pop_box(); // User container
	pop_box(); // Indentation
	pop_box(); // Accordion layout
}

#define UI_ACCORDION(name) DEFER_LOOP(UI::accordion_begin(name), UI::accordion_end())

void dropdown_selector(StrA name, U32 count, StrA* modes, U32* indices) {
	Box* layoutButton = button(Resources::simpleWhite, [count, modes](Box* box) {
		UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F{ box->renderPos.x, box->renderPos.y + box->computedSize.y })) {
			for (U32 i = 0; i < count; i++) {
				text_button(modes[i], [](Box* box){}).unsafeBox->value.i64.val = i;
			}
		}
	}).unsafeBox;
	layoutButton->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	layoutButton->align = ALIGN_MODE_CENTER_CENTER;
	layoutButton->padding = 2.0F;
	layoutButton->backgroundColor = themeColor.inputField;
	UI_WORKING_BOX(layoutButton) {
		Box* dropdownIcon = icon(Resources::uiArrowDown).unsafeBox;
		dropdownIcon->backgroundColor = themeColor.button;
		dropdownIcon->size *= 0.5F;
		text(name);
	}
}

void color_picker_set_lrch(Box* clPicker, Box* colorBox, Box* callbackBox, V3F LrCH) {
	clPicker->value.color.oklabLrCH = Oklab::clip_lrch_to_srgb_gamut(LrCH);
	V3F srgb = clamp01(Oklab::lrch_to_srgb(clPicker->value.color.oklabLrCH));
	clPicker->value.color.srgb = srgb;
	colorBox->backgroundColor = srgb.to_rgba8(1.0F);
	if (callbackBox->colorConsumerCallback) {
		callbackBox->colorConsumerCallback(callbackBox, V4F{ srgb.x, srgb.y, srgb.z, 1.0F });
	}
}

BoxHandle color_picker() {
	Box* colorCallbackBox;
	UI_DBOX() {
		Box* collapseButton = nullptr;
		Box* colorBox = nullptr;
		UI_RBOX() {
			workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
			collapseButton = button(Resources::uiAccordionClosed, [](Box* box) {}).unsafeBox;
			collapseButton->sizeModeX = collapseButton->sizeModeY = SIZE_MODE_FIT_CHILDREN;
			collapseButton->size = V2F{ 16.0F, 16.0F };
			colorBox = generic_box().unsafeBox;
			colorBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
			colorBox->sizeModeY = SIZE_MODE_FIT_CHILDREN;
			colorBox->size = V2F{ 16.0F, 16.0F };
		}
		Box* boxToCollapse = nullptr;
		UI_DBOX() {
			boxToCollapse = workingBox;
			spacer(10.0F);
			UI_RBOX() {
				colorCallbackBox = workingBox;
				Box* clPicker = generic_box().unsafeBox;
				clPicker->flags |= BOX_FLAG_CUSTOM_DRAW;
				clPicker->sizeModeX = clPicker->sizeModeY = SIZE_MODE_FIT_CHILDREN;
				clPicker->size = V2F{ 256.0F, 256.0F };
				clPicker->backgroundColor = themeColor.inputField;

				spacer(10.0F);

				Box* huePicker = generic_box().unsafeBox;
				huePicker->flags |= BOX_FLAG_CUSTOM_DRAW;
				huePicker->sizeModeX = SIZE_MODE_FIT_CHILDREN;
				huePicker->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
				huePicker->backgroundColor = RGBA8{ 127, 0, 0, 255 };
				huePicker->backgroundRenderFlags = VK::UI_RENDER_FLAG_OKLrCH | VK::UI_RENDER_FLAG_OKLrCH_USE_UV_CH;
				huePicker->backgroundUV = Rng2F32{ 0.25F, 0.0F, 0.25F, 1.0F };
				huePicker->value.f64.val = 0.0F;
				set_box_callback(huePicker, [clPicker, colorBox, colorCallbackBox](Box* box, UserCommunication& com) {
					if (com.leftClickStart || com.drag.y != 0.0F) {
						F32 newHue = clamp01((com.mousePos.y - com.renderArea.minY) / com.renderArea.height());
						V3F LrCH = clPicker->value.color.oklabLrCH;
						LrCH.z = newHue;
						color_picker_set_lrch(clPicker, colorBox, colorCallbackBox, LrCH);
						return ACTION_HANDLED;
					}
					if (com.tessellator) {
						V2F pos = box->renderPos;
						F32 size = 8.0F;
						F32 hue = clPicker->value.color.oklabLrCH.z;
						F32 startY = com.renderArea.minY + hue * com.renderArea.height() - size * 0.5F;
						com.tessellator->ui_rect2d(com.renderArea.minX - size, startY, com.renderArea.minX, startY + size, com.renderZ, 0.0F, 0.0F, 1.0F, 1.0F, V4F{ 1.0F, 1.0F, 1.0F, 1.0F }, Resources::uiAccordionClosed.index, com.clipBoxIndex << 16);
						return ACTION_HANDLED;
					}
					return ACTION_PASS;
				});

				set_box_callback(clPicker, [colorBox, colorCallbackBox](Box* box, UserCommunication& com){
					if (com.leftClickStart || com.drag != V2F{ 0.0F, 0.0F }) {
						V2F newLrC = clamp01((com.mousePos - V2F{ com.renderArea.minX, com.renderArea.minY }) / V2F{ com.renderArea.width(), com.renderArea.height() });
						V3F LrCH = box->value.color.oklabLrCH;
						LrCH.x = 1.0F - newLrC.y;
						LrCH.y = newLrC.x * Oklab::SRGB_PICKER_CHROMA_END;
						color_picker_set_lrch(box, colorBox, colorCallbackBox, LrCH);
						return ACTION_HANDLED;
					}
					if (com.tessellator) {
						Rng2F32 area = com.renderArea;
						V3F LrCH = box->value.color.oklabLrCH;
						com.tessellator->ui_rect2d(area.minX, area.minY, area.maxX, area.maxY, com.renderZ, 0.0F, 1.0F, 1.0f, 0.0F, U32(LrCH.z * 65535.0F + 0.49F), Resources::simpleWhite.index, com.clipBoxIndex << 16 | VK::UI_RENDER_FLAG_OKLrCH | VK::UI_RENDER_FLAG_OKLrCH_USE_UV_CL);
						F32 size = 8.0F;
						V2F start = box->renderPos - V2F{ size, size } * 0.5F + V2F{ LrCH.y / Oklab::SRGB_PICKER_CHROMA_END, 1.0F - LrCH.x } * box->computedSize;
						com.tessellator->ui_rect2d(start.x, start.y, start.x + size, start.y + size, com.renderZ, 0.0F, 0.0F, 1.0F, 1.0F, V4F{ 1.0F, 1.0F, 1.0F, 1.0F }, Resources::uiToggleOff.index, com.clipBoxIndex << 16);
						return ACTION_HANDLED;
					}
					return ACTION_PASS;
				});
			}
		}
		boxToCollapse->flags |= BOX_FLAG_DISABLED;
		set_box_consumer_box_callback(collapseButton, [boxToCollapse](Box* box) {
			bool previouslyDisabled = (boxToCollapse->flags & BOX_FLAG_DISABLED) != 0;
			boxToCollapse->flags ^= BOX_FLAG_DISABLED;
			box->backgroundTexture = previouslyDisabled ? &Resources::uiAccordionOpen : &Resources::uiAccordionClosed;
		});
	}
	return BoxHandle{ colorCallbackBox, colorCallbackBox->generation };
}

void do_scroll(Box* scrollHandler, Box* scrolled, F32 amount) {
	F32 maxScroll = max(scrolled->computedSize.y - scrollHandler->clippedRenderArea.height(), 0.0F);
	scrolled->pos.y = clamp(scrolled->pos.y + roundf32(amount), -maxScroll, 0.0F);
}

void scroll_window_begin() {
	Box* containerBox = generic_box().unsafeBox;
	containerBox->flags |= BOX_FLAG_INVISIBLE;
	containerBox->sizeModeX = containerBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
	containerBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	workingBox = containerBox;

	Box* scrollBox = generic_box().unsafeBox;
	scrollBox->flags |= BOX_FLAG_INVISIBLE | BOX_FLAG_DONT_FIT_CHILDREN | BOX_FLAG_CUSTOM_DRAW;
	scrollBox->sizeModeX = scrollBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;

	F32 scrollBarWidth = 8.0F;
	Box* scrollBar = generic_box().unsafeBox;
	scrollBar->size = V2F{};
	scrollBar->sizeModeX = SIZE_MODE_FIT_CHILDREN;
	scrollBar->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
	scrollBar->backgroundColor = themeColor.background;
	workingBox = scrollBar;

	UI_SIZE((V2F{ scrollBarWidth, scrollBarWidth }))
	button(Resources::uiArrowUp, [scrollBox](Box* box){
		do_scroll(scrollBox, scrollBox->childFirst, 10.0F);
	});

	Box* spacerBefore = spacer().unsafeBox;
	spacerBefore->sizeModeY = SIZE_MODE_PARENT_PERCENT;
	spacerBefore->parentSizePercent.y = 0.95F;

	Box* scrollHandle = button(Resources::simpleWhite, [](Box* box) {}).unsafeBox;
	set_box_callback(scrollHandle, [scrollBox](Box* box, UserCommunication& com) {
		if (com.drag.y != 0.0F) {
			Box* scrollHandler = scrollBox;
			Box* scrolled = scrollBox->childFirst;
			F32 visibleHeight = scrollHandler->clippedRenderArea.height();
			F32 maxScroll = max(scrolled->computedSize.y - visibleHeight, 0.0F);
			F32 scrollBarEmptySpace = box->next->computedSize.y + box->prev->computedSize.y;
			if (scrollBarEmptySpace > 0.001F) {
				do_scroll(scrollHandler, scrolled, -com.drag.y / scrollBarEmptySpace * maxScroll);
			}
			return ACTION_HANDLED;
		}
		return ACTION_PASS;
	});
	scrollHandle->size = V2F{ scrollBarWidth, 10.0F };

	Box* spacerAfter = spacer().unsafeBox;
	spacerAfter->sizeModeY = SIZE_MODE_PARENT_PERCENT;
	spacerAfter->parentSizePercent.y = 0.05F;

	UI_SIZE((V2F{ scrollBarWidth, scrollBarWidth }))
	button(Resources::uiArrowDown, [scrollBox](Box* box){
		do_scroll(scrollBox, scrollBox->childFirst, -10.0F);
	});

	set_box_callback(scrollBox, [scrollHandle, scrollBar](Box* box, UserCommunication& com) {
		if (com.scrollInput != 0.0F) {
			do_scroll(box, box->childFirst, com.scrollInput * 0.1F);
			return ACTION_HANDLED;
		}
		if (com.tessellator) {
			// The render step will count as our layout update. This means we'll be one frame behind, but that's ok.
			Box* scrollHandler = box;
			Box* scrolled = box->childFirst;
			F32 visibleHeight = scrollHandler->clippedRenderArea.height();
			F32 maxScroll = max(scrolled->computedSize.y - visibleHeight, 0.0F);
			if (maxScroll < 0.001F) {
				scrollBar->flags |= BOX_FLAG_DISABLED;
			} else {
				scrollBar->flags &= ~BOX_FLAG_DISABLED;
				F32 visibleToTotalRatio = visibleHeight / scrolled->computedSize.y;
				F32 scrolledAmountPercent = clamp01(-scrolled->pos.y / (scrolled->computedSize.y - visibleHeight));
				// Scroll bar height minus the two buttons
				F32 totalScrollBarHeight = scrollBar->computedSize.y - scrollBar->childFirst->computedSize.y - scrollBar->childLast->computedSize.y;
				scrollHandle->size.y = max(10.0F, floorf32(visibleToTotalRatio * totalScrollBarHeight));
				scrollHandle->prev->parentSizePercent.y = scrolledAmountPercent;
				scrollHandle->next->parentSizePercent.y = 1.0F - scrolledAmountPercent;
				scrolled->pos.y = clamp(scrolled->pos.y, -maxScroll, 0.0F);
			}
		}
		return ACTION_PASS;
	});

	workingBox = scrollBox;
	dbox();
	workingBox->sizeModeX = workingBox->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
}
void scroll_window_end() {
	pop_box(); // Pop scrolled container
	pop_box(); // Pop scroll handler
	pop_box(); // Pop container
}

#define UI_SCROLL_WINDOW() DEFER_LOOP(UI::scroll_window_begin(), UI::scroll_window_end())


}