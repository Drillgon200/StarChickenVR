#pragma once
#include "StarChicken_decl.h"
#include "UI.h"
#include "physics/Physics.h"
#include "physics/RigidBody.h"
#include "physics/SAT.h"
#include "Level.h"
#include "CubemapGen.h"
#include "PNG.h"
#include "compression/BC7.h"
#include "compression/LZ.h"
#include "compression/MipGen.h"

namespace EditorUI {

const U32 PREFAB_ICON_SIZE = 256;

UI::BoxHandle growableBox;

ArenaArrayList<U32> constraintsToMove;
ArenaArrayList<U32> pointsToImpulse;
U32 physicsThreadCount;

RigidBody::OrientedBox boxA;
RigidBody::OrientedBox boxB;

struct EditorPlayer {
	V3F pos;
	F32 pitch, yaw;
	F32 rotateDistance;
	V3F forward, up, right;

	void key_input(Win32::Key key, Win32::ButtonState state) {
		if (key == Win32::KEY_P && state == Win32::BUTTON_STATE_DOWN) {
			Win32::keyboardState[Win32::KEY_SHIFT] ? physicsThreadCount-- : physicsThreadCount++;
		}
	}
	void mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F mousePos) {
		if (button == Win32::MOUSE_BUTTON_WHEEL) {
			F32 sensitivity = rotateDistance * (0.2F / 120.0F);
			rotateDistance = clamp(rotateDistance - state.scroll * sensitivity, 1.0F, 1000.0F);
		}
		if (button == Win32::MOUSE_BUTTON_LEFT && state.state == Win32::BUTTON_STATE_DOWN) {
			for (U32 p : pointsToImpulse) {
				Physics::points.data[p].vel += V3F{ 1.0F, 0.0F, 0.0F } * 100.0F;
			}
		}
	}
	void rotation_updated() {
		QF32 localToWorld = AxisAngleF{ { 0.0F, 1.0F, 0.0F }, -yaw }.to_qf32() * AxisAngleF { { 1.0F, 0.0F, 0.0F }, -pitch }.to_qf32();
		forward = localToWorld * V3F{ 0.0F, 0.0F, -1.0F };
		up      = localToWorld * V3F{ 0.0F, 1.0F,  0.0F };
		right   = localToWorld * V3F{ 1.0F, 0.0F,  0.0F };
	}
	void update() {
		V2F deltaMouse = Win32::get_raw_delta_mouse();
		F32 sensitivity = 0.0005F;
		yaw += deltaMouse.x * sensitivity;
		pitch = clamp(pitch + deltaMouse.y * sensitivity, -0.24999F, 0.24999F);
		//pitch += deltaMouse.y * sensitivity;

		rotation_updated();

		F32 moveDelta = 5.0F * F32(StarChicken::deltaTime);
		if (Win32::keyboardState[Win32::KEY_CTRL]) {
			moveDelta *= 4.0F;
		}
		if (Win32::keyboardState[Win32::KEY_W]) {
			pos += forward * moveDelta;
		}
		if (Win32::keyboardState[Win32::KEY_S]) {
			pos -= forward * moveDelta;
		}
		if (Win32::keyboardState[Win32::KEY_D]) {
			pos += right * moveDelta;
		}
		if (Win32::keyboardState[Win32::KEY_A]) {
			pos -= right * moveDelta;
		}
		if (Win32::keyboardState[Win32::KEY_SHIFT]) {
			pos -= V3F{ 0.0F, moveDelta, 0.0F };
		}
		if (Win32::keyboardState[Win32::KEY_SPACE]) {
			pos += V3F{ 0.0F, moveDelta, 0.0F };
		}
		F32 constraintMoveAmount = 5.0F * F32(StarChicken::deltaTime);
		for (U32 c : constraintsToMove) {
			if (Win32::keyboardState[Win32::KEY_UP]) {
				Physics::constraints.data[c].offset.y += constraintMoveAmount;
			}
			if (Win32::keyboardState[Win32::KEY_DOWN]) {
				Physics::constraints.data[c].offset.y -= constraintMoveAmount;
			}
			if (Win32::keyboardState[Win32::KEY_RIGHT]) {
				Physics::constraints.data[c].offset.z += constraintMoveAmount;
			}
			if (Win32::keyboardState[Win32::KEY_LEFT]) {
				Physics::constraints.data[c].offset.z -= constraintMoveAmount;
			}
		}
		F32 boxMoveAmount = 1.0F * F32(StarChicken::deltaTime);
		if (Win32::keyboardState[Win32::KEY_H]) {
			boxB.pos.y += boxMoveAmount;
		}
		if (Win32::keyboardState[Win32::KEY_J]) {
			boxB.pos.y -= boxMoveAmount;
		}
	}

	M4x3F get_view_transform() {
		M4x3F view; view.set_identity();
		view.translate(V3F{ 0.0F, 0.0F, -rotateDistance });
		view.rotate_quat(AxisAngleF{ { 0.0F, 1.0F, 0.0F }, -yaw }.to_qf32() * AxisAngleF { { 1.0F, 0.0F, 0.0F }, -pitch }.to_qf32());
		view.translate(-pos);
		return view;
	}

	V3F get_render_eye_pos() {
		return pos - forward * rotateDistance;
	}
};

enum PanelType : U32 {
	PANEL_TYPE_NONE,
	PANEL_TYPE_EDITOR_3D,
	PANEL_TYPE_TEXTURE_PROCESSING,
	PANEL_TYPE_MATERIAL_EDITOR,
	PANEL_TYPE_MATERIAL_VIEWER,
	PANEL_TYPE_PREFAB_LIST,
	PANEL_TYPE_UI_TEST
};

struct PanelEditor3D;
ArenaArrayList<PanelEditor3D*> renderPanels;

PanelEditor3D* focusedEditor3D;

UI::BoxHandle debugDisplayText;

struct PanelUITest {
	void init() {

	}
	void destroy() {

	}

	void build_ui() {
		using namespace UI;
		UI_BACKGROUND() {
			UI_SCROLL_WINDOW() {
				workingBox->padding = 4.0F;
				spacer(24.0F);
				UI_BACKGROUND_COLOR(themeColor.inputField) {
					text_input("Test text 1"a, ""a, true, [](Box*){  });
					text_input("Another text input"a, ""a, true, [](Box*){  });
					path_input("File 1"a);
					path_input("File 2"a);
					slider_f64();
					slider_f64(nullptr, 0.5, 0.0, 1.0);
					slider_i64();
					slider_i64(nullptr, 5, 0, 10);
					slider_bool();
					color_picker();
					static StrA dropdownNames[]{ "One"a, "Two"a, "Three"a };
					static U32 dropdownIndices[]{ 1, 2, 3 };
					dropdown_selector("Test dropdown"a, 3, dropdownNames, dropdownIndices);
					UI_ACCORDION("Accordion expander"a) {
						workingBox->padding = 4.0F;
						slider_i64();
						slider_i64();
						UI_ACCORDION("Nested expander"a) {
							slider_f64();
						}
						slider_i64();
					}
				}
			}
		}
	}
};

void png_to_dtf_file(StrA outputDtf, StrA inputPng, bool bc7Compress, bool drlzCompress, bool genMips, bool srgb) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		void* compressedData = nullptr;
		U32 compressedSize = 0;
		U32 width, height;
		U32 mipCount = 1;
		RGBA8* imgData;
		PNG::read_image(arena, &imgData, &width, &height, inputPng);
		if (!imgData) {
			printf("Failed to load png for dtf conversion: %\n"a, inputPng);
			goto failed;
		}
		compressedData = imgData;
		compressedSize = width * height * sizeof(RGBA8);
		if (genMips) {
			imgData = MipGen::build_lame_mipmaps(arena, &compressedSize, &mipCount, imgData, width, height, srgb);
			compressedSize *= sizeof(RGBA8);
			compressedData = imgData;
		}
		if (bc7Compress) {
			U32 threadCount = Win32::logicalProcessorCount;
			BC7::BC7Block* blocks = BC7::compress_bc7(arena, imgData, width, height, threadCount, nullptr);
			compressedSize = BC7::block_count(width, height) * sizeof(BC7::BC7Block);
			U32 mipWidth = width, mipHeight = height;
			RGBA8* mipPtr = imgData + mipWidth * mipHeight;
			for (U32 i = 1; i < mipCount; i++) {
				mipWidth = max(mipWidth >> 1, 1u);
				mipHeight = max(mipHeight >> 1, 1u);
				BC7::compress_bc7(arena, mipPtr, mipWidth, mipHeight, threadCount, nullptr);
				compressedSize += BC7::block_count(mipWidth, mipHeight) * sizeof(BC7::BC7Block);
				mipPtr += mipWidth * mipHeight;
			}
			compressedData = blocks;
		}
		if (drlzCompress) {
			compressedData = LZ::encode2(arena, &compressedSize, (Byte*)compressedData, compressedSize);
		}
		if (File file = open_file_for_writing(outputDtf)) {
			ResourceLoading::DTFHeader header{};
			memcpy(header.magic, "DUCK", 4);
			header.version = DRILL_LIB_MAKE_VERSION(2, 1, 0);
			header.flags = Flags16((srgb ? ResourceLoading::TEXTURE_FLAG_SRGB : 0) | (drlzCompress ? ResourceLoading::TEXTURE_FLAG_DRLZ_COMPRESSED : 0));
			header.format = bc7Compress ? ResourceLoading::TEXTURE_FORMAT_RGBA_BC7 : ResourceLoading::TEXTURE_FORMAT_RGBA_U8;
			header.mipCount = U8(mipCount);
			header.width = U16(width);
			header.height = U16(height);
			header.dataSize = compressedSize;
			write_file(file, &header, sizeof(header));
			write_file(file, compressedData, compressedSize);
			close_file(file);
		} else {
			printf("Failed to open DTF file for writing: %\n"a, outputDtf);
		}
	failed:;
	}
}

struct PanelTextureProcessing {
	B8 drlzCompressionEnabled;
	B8 bc7CompressionEnabled;
	UI::BoxHandle baseColorInput;
	UI::BoxHandle normalInput;
	UI::BoxHandle armInput;
	UI::BoxHandle outputFolderInput;

	void init() {
		
	}
	void destroy() {

	}

	void build_ui() {
		using namespace UI;
		UI_BACKGROUND() {
			UI_SCROLL_WINDOW() {
				workingBox->padding = 4.0F;
				spacer(24.0F);
				baseColorInput = path_input("BaseColor"a);
				normalInput = path_input("Normal"a);
				armInput = path_input("ARM"a);
				outputFolderInput = path_input("Output"a);
				set_box_consumer_box_callback(baseColorInput.unsafeBox, [this](Box* box) {
					StrA text{ box->typedTextBuffer, box->numTypedCharacters };
					if (text.ends_with("_BaseColor.png"a)) {
						U64 prefixSize = text.length - "_BaseColor.png"a.length;
						if (normalInput.get() && normalInput.unsafeBox->numTypedCharacters == 0) {
							StrA suffix = "_Normal.png"a;
							memcpy(normalInput.unsafeBox->typedTextBuffer, text.str, prefixSize);
							memcpy(normalInput.unsafeBox->typedTextBuffer + prefixSize, suffix.str, suffix.length);
							normalInput.unsafeBox->numTypedCharacters = U32(prefixSize + suffix.length);
						}
						if (armInput.get() && armInput.unsafeBox->numTypedCharacters == 0) {
							StrA suffix = "_ARM.png"a;
							memcpy(armInput.unsafeBox->typedTextBuffer, text.str, prefixSize);
							memcpy(armInput.unsafeBox->typedTextBuffer + prefixSize, suffix.str, suffix.length);
							armInput.unsafeBox->numTypedCharacters = U32(prefixSize + suffix.length);
						}
						if (outputFolderInput.get() && outputFolderInput.unsafeBox->numTypedCharacters == 0) {
							StrA baseFolder = path_directory(text);
							memcpy(outputFolderInput.unsafeBox->typedTextBuffer, baseFolder.str, baseFolder.length);
							outputFolderInput.unsafeBox->numTypedCharacters = U32(baseFolder.length);
						}
					}
				});
				labeled_slider_bool("BC7 Compress"a, &bc7CompressionEnabled, true);
				labeled_slider_bool("DRLZ Compress"a, &drlzCompressionEnabled, true);
				text_button("Compress"a, [this](Box* box) {
					MemoryArena& arena = get_scratch_arena();
					MEMORY_ARENA_FRAME(arena) {
						if (baseColorInput.get() && baseColorInput.unsafeBox->numTypedCharacters > 0) {
							StrA inputPath{ baseColorInput.unsafeBox->typedTextBuffer, baseColorInput.unsafeBox->numTypedCharacters };
							StrA outputPath = stracat(arena, path_directory(inputPath), path_basename_root(inputPath), ".dtf"a);
							png_to_dtf_file(outputPath, inputPath, bc7CompressionEnabled, drlzCompressionEnabled, true, true);
						}
						if (normalInput.get() && normalInput.unsafeBox->numTypedCharacters > 0) {
							StrA inputPath{ normalInput.unsafeBox->typedTextBuffer, normalInput.unsafeBox->numTypedCharacters };
							StrA outputPath = stracat(arena, path_directory(inputPath), path_basename_root(inputPath), ".dtf"a);
							png_to_dtf_file(outputPath, inputPath, bc7CompressionEnabled, drlzCompressionEnabled, true, false);
						}
						if (armInput.get() && armInput.unsafeBox->numTypedCharacters > 0) {
							StrA inputPath{ armInput.unsafeBox->typedTextBuffer, armInput.unsafeBox->numTypedCharacters };
							StrA outputPath = stracat(arena, path_directory(inputPath), path_basename_root(inputPath), ".dtf"a);
							png_to_dtf_file(outputPath, inputPath, bc7CompressionEnabled, drlzCompressionEnabled, true, false);
						}
					}
				});
			}
		}
	}
};

struct PanelEditor3D;
struct Widget3D;
extern Widget3D* currentInputWidget;

enum TransformOriginMode {
	TRANSFORM_ORIGIN_MODE_OBJECT_AVERAGE,
	TRANSFORM_ORIGIN_MODE_INDIVIDUAL_ORIGINS,
	TRANSFORM_ORIGIN_MODE_ACTIVE_ELEMENT
};

enum TransformOrientationMode {
	TRANSFORM_ORIENTATION_MODE_GLOBAL,
	TRANSFORM_ORIENTATION_MODE_LOCAL
};

enum EditorCmd {
	EDITOR_CMD_TRANSFORM,
	EDITOR_CMD_SELECT_OBJECTS
};

struct EditorCmdTransform {
	U32* affectedObjIds;
	I32 affectedObjIdCount;
	M4x3F* previousTransforms;
	QF32 rotation;
	V3F translation;
	V3F origin;
	TransformOrientationMode orientationMode;
	TransformOriginMode originMode;

	void apply() {
		for (I32 i = 0; i < affectedObjIdCount; i++) {
			if (Level::LevelObject* obj = Level::level.idToLevelObject.find_or_default(affectedObjIds[i], nullptr)) {
				obj->transform.add_offset(translation);
			}
		}
	}
	void revert() {
		for (I32 i = 0; i < affectedObjIdCount; i++) {
			if (Level::LevelObject* obj = Level::level.idToLevelObject.find_or_default(affectedObjIds[i], nullptr)) {
				obj->transform = previousTransforms[i];
			}
		}
	}
};
struct EditorCmdSelectObjects {
	U32* previousSet;
	U32 previousCount;
	U32* nextSet;
	U32 nextCount;

	void apply() {
		Level::level.deselect_all();
		Level::level.select_objects(nextSet, nextCount);
	}
	void revert() {
		Level::level.deselect_all();
		Level::level.select_objects(previousSet, previousCount);
	}
};

struct UndoEntry {
	union {
		EditorCmdTransform cmdTransform;
		EditorCmdSelectObjects cmdSelectObjects;
	};
	EditorCmd cmdType;
	UndoEntry* prev;
	UndoEntry* next;
	U32 allocAmount;

	void apply() {
		switch (cmdType) {
		case EDITOR_CMD_TRANSFORM: cmdTransform.apply(); break;
		case EDITOR_CMD_SELECT_OBJECTS: cmdSelectObjects.apply(); break;
		}
	}
	void revert() {
		switch (cmdType) {
		case EDITOR_CMD_TRANSFORM: cmdTransform.revert(); break;
		case EDITOR_CMD_SELECT_OBJECTS: cmdSelectObjects.revert(); break;
		}
	}
};
struct UndoStack {
	static const U32 UNDO_STACK_SIZE = 16 * MEGABYTE;
	static const U32 UNDO_STACK_MASK = UNDO_STACK_SIZE - 1;
	alignas(32) Byte stack[UNDO_STACK_SIZE];
	UndoEntry* head;
	UndoEntry* tail;
	U32 freeSpace;
	U32 firstUsedPos;
	U32 allocPos;
	// This entry has been applied
	UndoEntry* currentEntry;

	void init() {
		freeSpace = UNDO_STACK_SIZE;
	}

	void undo() {
		if (currentEntry) {
			currentEntry->revert();
			currentEntry = currentEntry->prev;
		}
	}
	void redo() {
		UndoEntry* redoEntry = currentEntry ? currentEntry->next : head;
		if (redoEntry) {
			redoEntry->apply();
			currentEntry = redoEntry;
		}
	}

	void delete_head() {
		firstUsedPos = (firstUsedPos + head->allocAmount) & UNDO_STACK_MASK;
		freeSpace += head->allocAmount;
		UndoEntry* toRemove = head;
		DLL_REMOVE(toRemove, head, tail, prev, next);
	}
	void delete_tail() {
		allocPos = (allocPos - tail->allocAmount) & UNDO_STACK_MASK;
		freeSpace += tail->allocAmount;
		UndoEntry* toRemove = tail;
		DLL_REMOVE(toRemove, head, tail, prev, next);
	}

	// Deletes old entries to make space for the new command
	void make_space_for(U32 byteCount) {
		if (freeSpace == 0) {
			// Guarantee allocPos and firstUsedPos are different
			delete_head();
		}
		while (head && freeSpace < byteCount) {
			delete_head();
		}
	}

	template<typename T>
	T* alloc(U32* usedSpaceOut, U32 count) {
		U32 lastAllocPos = allocPos;
		U32 lastFreeSpace = freeSpace;
		U32 usedSpace = ALIGN_HIGH(allocPos, alignof(T)) - allocPos;
		allocPos += usedSpace, freeSpace -= usedSpace;
		U32 requiredSize = U32(sizeof(T) * count);
		make_space_for(requiredSize + alignof(T));
		if (allocPos > firstUsedPos && UNDO_STACK_SIZE - allocPos < requiredSize) {
			// Two potential free zones, one at the beginning, one at the end
			// Eat the top zone if it's too small
			usedSpace += UNDO_STACK_SIZE - allocPos;
			freeSpace -= UNDO_STACK_SIZE - allocPos;
			allocPos = 0;
			make_space_for(requiredSize);
		}
		if (requiredSize > freeSpace) {
			allocPos = lastAllocPos;
			freeSpace = lastFreeSpace;
			return nullptr;
		}
		T* result = (T*)(stack + allocPos);
		usedSpace += requiredSize;
		freeSpace -= requiredSize;
		allocPos += requiredSize;
		*usedSpaceOut = usedSpace;
		return result;
	}
	UndoEntry* new_entry() {
		while (tail != currentEntry) {
			delete_tail();
		}
		U32 usedSpace;
		UndoEntry* result = alloc<UndoEntry>(&usedSpace, 1);
		*result = UndoEntry{};
		result->allocAmount = usedSpace;
		return result;
	}
	// Call if allocations fail (hopefully won't happen, but I'd like to be robust to that)
	void abort_entry(UndoEntry* entry) {
		allocPos = (allocPos - entry->allocAmount) & UNDO_STACK_MASK;
		freeSpace += entry->allocAmount;
	}
	template<typename T>
	T* alloc_for_entry(UndoEntry* entry, U32 count) {
		U32 usedSpace;
		T* result = alloc<T>(&usedSpace, count);
		entry->allocAmount += usedSpace;
		return result;
	}
	void insert_entry(UndoEntry* entry) {
		currentEntry = entry;
		DLL_INSERT_TAIL(entry, head, tail, prev, next);
	}
} undoStack;

const F32 TRANSFORM_WIDGET_SCALE = 0.125F;
const F32 TRANSFORM_PLANE_HANDLE_ADDITIONAL_SCALE = 0.25F;

enum TranslateWidgetComponent {
	TRANSLATE_WIDGET_COMPONENT_NONE,
	TRANSLATE_WIDGET_COMPONENT_X_AXIS,
	TRANSLATE_WIDGET_COMPONENT_Y_AXIS,
	TRANSLATE_WIDGET_COMPONENT_Z_AXIS,
	TRANSLATE_WIDGET_COMPONENT_XY_PLANE,
	TRANSLATE_WIDGET_COMPONENT_XZ_PLANE,
	TRANSLATE_WIDGET_COMPONENT_YZ_PLANE,
	TRANSLATE_WIDGET_COMPONENT_CAMERA_PLANE
};
struct TranslateWidget {
	M4x3F transform;
	M4x3F preInteractTransform;
	V3F totalTranslationAmount;
	V2F interactStartMousePos;
	TranslateWidgetComponent activeComponent;
	UndoEntry* currentUndoCmd;

	void do_mouse_over(PanelEditor3D* editor3D, V3F eye, V3F look);

	void key_input(PanelEditor3D* editor3D, Win32::Key key, Win32::ButtonState state) {
	}
	void mouse_input(PanelEditor3D* editor3D, Win32::MouseButton button, Win32::MouseValue state, V2F mousePos) {
		if (button == Win32::MOUSE_BUTTON_LEFT && state.state == Win32::BUTTON_STATE_UP) {
			currentInputWidget = nullptr;
			return;
		}
	}

	void update_inactive(PanelEditor3D* editor3D, bool mouseInRange, V3F eye, V3F pickRay) {
		if (mouseInRange) {
			do_mouse_over(editor3D, eye, pickRay);
		} else {
			activeComponent = TRANSLATE_WIDGET_COMPONENT_NONE;
		}
		transform.set_offset(Level::level.get_selection_midpoint());
	}

	void update_active(PanelEditor3D* editor3D);

	bool check_clicked_on(PanelEditor3D* editor3D, V3F eye, V3F pickRay) {
		do_mouse_over(editor3D, eye, pickRay);
		return activeComponent != TRANSLATE_WIDGET_COMPONENT_NONE;
	}

	void on_made_active(PanelEditor3D* editor3D) {
		totalTranslationAmount = {};
		preInteractTransform = transform;
		interactStartMousePos = Win32::get_mouse();
		currentUndoCmd = undoStack.new_entry();
		currentUndoCmd->cmdTransform = EditorCmdTransform{};
		EditorCmdTransform& cmd = currentUndoCmd->cmdTransform;
		cmd.rotation.set_identity();
		transform.set_offset(Level::level.get_selection_midpoint());
		cmd.origin = transform.translation();
		cmd.affectedObjIdCount = Level::level.selectedObjects.size;
		cmd.affectedObjIds = undoStack.alloc_for_entry<U32>(currentUndoCmd, cmd.affectedObjIdCount);
		if (!cmd.affectedObjIds) {
			goto failed;
		}
		cmd.previousTransforms = undoStack.alloc_for_entry<M4x3F>(currentUndoCmd, cmd.affectedObjIdCount);
		if (!cmd.previousTransforms) {
			goto failed;
		}
		for (U32 i = 0; i < Level::level.selectedObjects.size; i++) {
			cmd.affectedObjIds[i] = Level::level.selectedObjects[i]->id;
			cmd.previousTransforms[i] = Level::level.selectedObjects[i]->transform;
		}
		undoStack.insert_entry(currentUndoCmd);
		goto success;
	failed:;
		undoStack.abort_entry(currentUndoCmd);
		currentUndoCmd = nullptr;
	success:;
	}

	void draw(PanelEditor3D* editor3D, DynamicVertexBuffer::Tessellator& tes, V3F eye);
};

enum Widget3DType : U32 {
	WIDGET_3D_NONE,
	WIDGET_3D_TRANSLATE
};
struct Widget3D {
	Widget3DType type;
	PanelEditor3D* parentPanel;
	union {
		TranslateWidget translate;
	};

	void key_input(Win32::Key key, Win32::ButtonState state) {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.key_input(parentPanel, key, state); break;
		}
	}
	void mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F mousePos) {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.mouse_input(parentPanel, button, state, mousePos); break;
		}
	}
	void update_inactive(bool mouseInRange, V3F eye, V3F pickRay) {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.update_inactive(parentPanel, mouseInRange, eye, pickRay); break;
		}
	}
	void update_active() {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.update_active(parentPanel); break;
		}
	}
	void draw(DynamicVertexBuffer::Tessellator& tes, V3F eyePos) {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.draw(parentPanel, tes, eyePos); break;
		}
	}
	void on_made_active() {
		switch (type) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: translate.on_made_active(parentPanel); break;
		}
	}
};

Widget3D* currentInputWidget;

struct PanelEditor3D {
	EditorPlayer editor;
	Rng2F32 viewport;
	F32 fov;
	PerspectiveProjection projection;
	U32 gpuCameraIndex;
	Rng1I32 ui3DDrawSet;
	V2F selectDragStart;
	B8 isDragSelecting;
	B8 panelContainsMouse;
	V3F mousePickRay;
	Widget3D widget3D;

	void switch_widget(Widget3DType newWidget) {
		if (currentInputWidget == &widget3D) {
			currentInputWidget = nullptr;
		}
		switch (newWidget) {
		case WIDGET_3D_NONE: break;
		case WIDGET_3D_TRANSLATE: widget3D.translate = TranslateWidget{}; break;
		}
		widget3D.type = newWidget;
	}

	void init() {
		V3F playerEye{ -30.0F, 8.0F, 0.0F };
		editor.pos = playerEye;
		editor.yaw = 0.25F;
		editor.rotation_updated();
		fov = 120.0F;
		renderPanels.push_back_unique(this);

		widget3D.parentPanel = this;
		switch_widget(WIDGET_3D_TRANSLATE);
		widget3D.translate.transform.set_identity();
	}
	void destroy() {
		I64 idx = renderPanels.idx_of(this);
		if (idx != -1) {
			renderPanels.data[idx] = renderPanels.pop_back();
		}
		if (focusedEditor3D == this) {
			focusedEditor3D = nullptr;
		}
	}
	V3F unproject_vec(V2F screenPixCoords) {
		V2F ndcCoords = (screenPixCoords - V2F{ viewport.minX, viewport.minY }) / V2F{ viewport.width(), viewport.height() } * 2.0F - 1.0F;
		V3F viewSpaceVec = normalize(projection.untransform(V3F{ ndcCoords.x, ndcCoords.y, 0.5F }));
		return viewSpaceVec.x * editor.right - viewSpaceVec.y * editor.up - viewSpaceVec.z * editor.forward;
	}
	V2F project_pos(V3F worldPos) {
		worldPos -= editor.get_render_eye_pos();
		V3F viewSpace{ dot(worldPos, editor.right), -dot(worldPos, editor.up), -dot(worldPos, editor.forward) };
		V3F ndc = projection.transform(viewSpace);
		return V2F{ viewport.minX + (ndc.x * 0.5F + 0.5F) * viewport.width(), viewport.minY + (ndc.y * 0.5F + 0.5F) * viewport.height() };
	}
	V2F vector_in_screen_space(V3F from, V3F to) {
		return project_pos(to) - project_pos(from);
	}
	void update() {
		projection.project_perspective(0.05F, DEG_TO_TURN(fov), viewport.height() / viewport.width());

		if (!Win32::mouseCaptured) {
			V2F mousePos = Win32::get_mouse();
			mousePickRay = unproject_vec(mousePos);
			panelContainsMouse = viewport.contains_point(mousePos);
			V3F eyePos = editor.get_render_eye_pos();

			if (currentInputWidget == &widget3D) {
				widget3D.update_active();
			} else {
				widget3D.update_inactive(panelContainsMouse, eyePos, mousePickRay);
			}
		}
	}
	void debug_render() {
		DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
		V3F eyePos = editor.get_render_eye_pos();
		widget3D.draw(tes, eyePos);
	}
	void build_ui() {
		using namespace UI;
		Box* contentBox = generic_box().unsafeBox;
		contentBox->flags = BOX_FLAG_INVISIBLE | BOX_FLAG_CUSTOM_DRAW;
		contentBox->sizeModeX = contentBox->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
		PanelEditor3D* editor3d = this;
		set_box_callback(contentBox, [=](Box* box, UserCommunication& com){
			if (com.leftClickStart && Win32::mouseButtonState[Win32::MOUSE_BUTTON_MIDDLE]) {
				focusedEditor3D = editor3d;
				Win32::set_mouse_captured(true);
				return ACTION_HANDLED;
			} else if (com.leftClickStart) {
				switch (editor3d->widget3D.type) {
				case WIDGET_3D_TRANSLATE: {
					if (editor3d->widget3D.translate.check_clicked_on(editor3d, editor3d->editor.get_render_eye_pos(), editor3d->mousePickRay)) {
						currentInputWidget = &editor3d->widget3D;
						currentInputWidget->on_made_active();
						return ACTION_HANDLED;
					}
				} break;
				default: break;
				}
				selectDragStart = com.mousePos;
				editor3d->isDragSelecting = true;
			}
			
			if (com.leftClicked) {
				if (!Win32::keyboardState[Win32::KEY_SHIFT]) {
					Level::level.deselect_all();
				}
				Rng2F32 dragArea = make_rng2f(editor3d->selectDragStart, com.mousePos);
				if (editor3d->isDragSelecting && dragArea.area() != 0.0F) {
					dragArea = dragArea.intersected(com.renderArea);
					I32 minX = clamp(I32(dragArea.minX), 0, I32(VK::attachments.mainWidth));
					I32 maxX = clamp(I32(dragArea.maxX) + 1, 0, I32(VK::attachments.mainWidth));
					I32 minY = clamp(I32(dragArea.minY), 0, I32(VK::attachments.mainHeight));
					I32 maxY = clamp(I32(dragArea.maxY) + 1, 0, I32(VK::attachments.mainHeight));
					MemoryArena& arena = get_scratch_arena();
					MEMORY_ARENA_FRAME(arena) {
						ArenaArrayList<U32> selectedSet{ &arena };
						for (I32 y = minY; y <= maxY; y++) {
							for (I32 x = minX; x <= maxX; x++) {
								U32 objId = ((U32*)VK::attachments.objectIdReadbackBuffer.mapping)[y * VK::attachments.mainWidth + x];
								objId &= 0x7FFFFFFF; // Mask off "selected" bit
								if (objId != Level::INVALID_LEVEL_OBJECT_ID) {
									selectedSet.push_back_unique(objId);
								}
							}
						}
						Level::level.select_objects(selectedSet.data, selectedSet.size);
					}
				} else {
					U32 clickPixelIdx = U32(com.mousePos.y) * VK::attachments.mainWidth + U32(com.mousePos.x);
					if (clickPixelIdx * sizeof(U32) < VK::attachments.objectIdReadbackBuffer.capacity) {
						U32 objId = ((U32*)VK::attachments.objectIdReadbackBuffer.mapping)[clickPixelIdx];
						objId &= 0x7FFFFFFF; // Mask off "selected" bit
						if (objId != Level::INVALID_LEVEL_OBJECT_ID) {
							if (Level::level.activeObject && Level::level.activeObject->id == objId) {
								Level::level.deselect_object(objId);
							} else {
								Level::level.select_object(objId);
							}
						}
					}
				}
				return ACTION_HANDLED;
			}
			if (com.keyPressed == Win32::KEY_A) {
				Level::level.select_all();
			}
			if (com.keyPressed == Win32::KEY_Z) {
				if (Win32::keyboardState[Win32::KEY_SHIFT]) {
					undoStack.redo();
				} else {
					undoStack.undo();
				}
			}
			if (com.keyPressed == Win32::KEY_B) {
				VK::currentDebugDisplay = VK::RenderDebugDisplay((U32(VK::currentDebugDisplay) + 1) % U32(VK::RENDER_DEBUG_DISPLAY_Count));
				if (UI::Box* box = debugDisplayText.get()) {
					StrA text{};
					switch (VK::currentDebugDisplay) {
					case VK::RENDER_DEBUG_DISPLAY_PBR_NO_TONEMAP: text = "No Tonemap"a; break;
					case VK::RENDER_DEBUG_DISPLAY_NORMAL: text = "Normal"a; break;
					case VK::RENDER_DEBUG_DISPLAY_AMBIENT_OCCLUSION: text = "Ambient Occlusion"a; break;
					case VK::RENDER_DEBUG_DISPLAY_ROUGHNESS: text = "Roughness"a; break;
					case VK::RENDER_DEBUG_DISPLAY_METALLIC: text = "Metallic"a; break;
					case VK::RENDER_DEBUG_DISPLAY_BASIC_LIGHTING: text = "Basic Lighting"a; break;
					}
					box->text = text;
				}
			}
			if (!Win32::mouseButtonState[Win32::MOUSE_BUTTON_LEFT]) {
				editor3d->isDragSelecting = false;
			}
			if (com.tessellator) {
				editor3d->viewport = com.renderArea;
				Rng2F32 dragArea = make_rng2f(editor3d->selectDragStart, com.mousePos);
				if (editor3d->isDragSelecting && dragArea.area() != 0.0F) {
					com.tessellator->ui_rect2d(dragArea.minX, dragArea.minY, dragArea.maxX, dragArea.maxY, com.renderZ, 0.0F, 0.0F, 0.0F, 0.0F, V4F{ 1.0F, 1.0F, 1.0F, 0.75F }, Resources::simpleWhite.index, 0);
				}
			}
			return ACTION_PASS;
		});
	}
};

void TranslateWidget::update_active(PanelEditor3D* editor3D) {
	if (activeComponent == TRANSLATE_WIDGET_COMPONENT_NONE || StarChicken::frameUIMouseDelta == V2F{}) {
		return;
	}
	V3F eye = editor3D->editor.get_render_eye_pos();
	V3F prevMouseRay = editor3D->unproject_vec(interactStartMousePos);
	V3F mouseRay = editor3D->unproject_vec(Win32::get_mouse());

	V2F drag = Win32::get_mouse() - interactStartMousePos;
	V3F prevWidgetPos = preInteractTransform.translation();
	F32 scale = TRANSFORM_WIDGET_SCALE * distance(editor3D->editor.get_render_eye_pos(), prevWidgetPos);
	if (activeComponent == TRANSLATE_WIDGET_COMPONENT_X_AXIS || activeComponent == TRANSLATE_WIDGET_COMPONENT_Y_AXIS || activeComponent == TRANSLATE_WIDGET_COMPONENT_Z_AXIS) {
		U32 rowIdx =
			activeComponent == TRANSLATE_WIDGET_COMPONENT_X_AXIS ? 0 :
			activeComponent == TRANSLATE_WIDGET_COMPONENT_Y_AXIS ? 1 :
			2;
		V3F translateAxis = normalize(preInteractTransform.get_row(rowIdx));
		V3F planeNormal = eye - prevWidgetPos;
		planeNormal = planeNormal - translateAxis * dot(planeNormal, translateAxis);
		V3F planeTranslation = ray_plane_intersect_point(eye, mouseRay, prevWidgetPos, planeNormal) - ray_plane_intersect_point(eye, prevMouseRay, prevWidgetPos, planeNormal);
		totalTranslationAmount = translateAxis * dot(translateAxis, planeTranslation);
	} else {
		// Translate along plane, not constrained to one axis
		V3F planeNormal{};
		switch (activeComponent) {
		case TRANSLATE_WIDGET_COMPONENT_XY_PLANE: planeNormal = preInteractTransform.get_row(2); break;
		case TRANSLATE_WIDGET_COMPONENT_XZ_PLANE: planeNormal = preInteractTransform.get_row(1); break;
		case TRANSLATE_WIDGET_COMPONENT_YZ_PLANE: planeNormal = preInteractTransform.get_row(0); break;
		case TRANSLATE_WIDGET_COMPONENT_CAMERA_PLANE: planeNormal = editor3D->editor.forward; break;
		}
		totalTranslationAmount = ray_plane_intersect_point(eye, mouseRay, prevWidgetPos, planeNormal) - ray_plane_intersect_point(eye, prevMouseRay, prevWidgetPos, planeNormal);
	}
	transform = preInteractTransform;
	transform.add_offset(totalTranslationAmount);
	if (currentUndoCmd) {
		currentUndoCmd->revert();
		currentUndoCmd->cmdTransform.translation = totalTranslationAmount;
		currentUndoCmd->apply();
	}
}

void TranslateWidget::do_mouse_over(PanelEditor3D* editor3D, V3F eye, V3F look) {
	activeComponent = TRANSLATE_WIDGET_COMPONENT_NONE;
	F32 scale = TRANSFORM_WIDGET_SCALE * distance(eye, V3F{ transform.x, transform.y, transform.z });
	F32 planeScale = scale * TRANSFORM_PLANE_HANDLE_ADDITIONAL_SCALE;
	F32 arrowRadius = 0.05F * scale;
	V3F center{ transform.x, transform.y, transform.z };
	V3F xAxis = transform.get_row(0);
	V3F yAxis = transform.get_row(1);
	V3F zAxis = transform.get_row(2);
	F32 bestTime = F32_INF;
	F32 xAxisTime{};
	if (ray_cylinder_intersect(&xAxisTime, eye, look, center + xAxis * scale * 0.5F, center, arrowRadius * 2.0F) && xAxisTime < bestTime) {
		bestTime = xAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_X_AXIS;
	}
	F32 yAxisTime{};
	if (ray_cylinder_intersect(&yAxisTime, eye, look, center + yAxis * scale * 0.5F, center, arrowRadius * 2.0F) && yAxisTime < bestTime) {
		bestTime = yAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_Y_AXIS;
	}
	F32 zAxisTime{};
	if (ray_cylinder_intersect(&zAxisTime, eye, look, center + zAxis * scale * 0.5F, center, arrowRadius * 2.0F) && zAxisTime < bestTime) {
		bestTime = zAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_Z_AXIS;
	}
	F32 xyAxisTime{};
	if (ray_rect_intersect(&xyAxisTime, eye, look, center + xAxis * planeScale + yAxis * planeScale, xAxis * planeScale, yAxis * planeScale) && xyAxisTime < bestTime) {
		bestTime = xyAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_XY_PLANE;
	}
	F32 xzAxisTime{};
	if (ray_rect_intersect(&xzAxisTime, eye, look, center + xAxis * planeScale + zAxis * planeScale, xAxis * planeScale, zAxis * planeScale) && xzAxisTime < bestTime) {
		bestTime = xzAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_XZ_PLANE;
	}
	F32 yzAxisTime{};
	if (ray_rect_intersect(&yzAxisTime, eye, look, center + yAxis * planeScale + zAxis * planeScale, yAxis * planeScale, zAxis * planeScale) && yzAxisTime < bestTime) {
		bestTime = yzAxisTime;
		activeComponent = TRANSLATE_WIDGET_COMPONENT_YZ_PLANE;
	}
	V3F camX = editor3D->editor.right * planeScale;
	V3F camY = editor3D->editor.up * planeScale;
	if (ray_rect_intersect(nullptr, eye, look, center - camX * 0.5F - camY * 0.5F, camX, camY)) {
		activeComponent = TRANSLATE_WIDGET_COMPONENT_CAMERA_PLANE;
	}
}

void TranslateWidget::draw(PanelEditor3D* editor3D, DynamicVertexBuffer::Tessellator& tes, V3F eye) {
	F32 scale = TRANSFORM_WIDGET_SCALE * distance(eye, V3F{ transform.x, transform.y, transform.z });
	F32 planeScale = scale * TRANSFORM_PLANE_HANDLE_ADDITIONAL_SCALE;
	F32 arrowRadius = 0.05F * scale;
	V3F center{ transform.x, transform.y, transform.z };
	V3F xAxis = transform.get_row(0);
	V3F yAxis = transform.get_row(1);
	V3F zAxis = transform.get_row(2);
	V3F camX = editor3D->editor.right * planeScale;
	V3F camY = editor3D->editor.up * planeScale;
	tes.begin_draw(VK::debugNoDepthPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
	tes.debug_arrow(center, center + xAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_X_AXIS ? V4F{ 1.0F, 0.5F, 0.5F, 1.0F } : V4F{ 1.0F, 0.25F, 0.25F, 1.0F });
	tes.debug_arrow(center, center + yAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_Y_AXIS ? V4F{ 0.5F, 1.0F, 0.5F, 1.0F } : V4F{ 0.25F, 1.0F, 0.25F, 1.0F });
	tes.debug_arrow(center, center + zAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_Z_AXIS ? V4F{ 0.5F, 0.5F, 1.0F, 1.0F } : V4F{ 0.25F, 0.25F, 1.0F, 1.0F });
	tes.debug_quad(center + xAxis * planeScale + yAxis * planeScale, xAxis * planeScale, yAxis * planeScale, activeComponent == TRANSLATE_WIDGET_COMPONENT_XY_PLANE ? V4F{ 0.5F, 0.5F, 1.0F, 1.0F } : V4F{ 0.25F, 0.25F, 1.0F, 1.0F });
	tes.debug_quad(center + xAxis * planeScale + zAxis * planeScale, xAxis * planeScale, zAxis * planeScale, activeComponent == TRANSLATE_WIDGET_COMPONENT_XZ_PLANE ? V4F{ 0.5F, 1.0F, 0.5F, 1.0F } : V4F{ 0.25F, 1.0F, 0.25F, 1.0F });
	tes.debug_quad(center + yAxis * planeScale + zAxis * planeScale, yAxis * planeScale, zAxis * planeScale, activeComponent == TRANSLATE_WIDGET_COMPONENT_YZ_PLANE ? V4F{ 1.0F, 0.5F, 0.5F, 1.0F } : V4F{ 1.0F, 0.25F, 0.25F, 1.0F });
	tes.debug_quad(center - camX * 0.5F - camY * 0.5F, camX, camY, activeComponent == TRANSLATE_WIDGET_COMPONENT_CAMERA_PLANE ? V4F{ 0.75F, 0.75F, 0.75F, 1.0F } : V4F{ 0.5F, 0.5F, 0.5F, 1.0F });
	tes.end_draw();
}

struct PanelMaterialEditor {
	void init() {
	}
	void destroy() {
	}
	void build_ui() {
		using namespace UI;
		UI_BACKGROUND() {
			UI_SCROLL_WINDOW() {
				workingBox->padding = 4.0F;
				spacer(24.0F);
				Box* picker = color_picker().unsafeBox;
				set_color_consumer_box_callback(picker, [](V4F color) {
					Level::LevelObject* obj = Level::level.activeObject;
					if (obj && (obj->type == Level::LEVEL_OBJECT_STATIC_MODEL || obj->type == Level::LEVEL_OBJECT_SKELETAL_MODEL)) {
						ResourceLoading::Material* mat = obj->type == Level::LEVEL_OBJECT_STATIC_MODEL ? ((Level::StaticModel*)obj)->material : ((Level::SkeletalModel*)obj)->material;
						mat->color = color;
						mat->invalidate();
					}
				});
			}
		}
	}
};

struct PanelMaterialViewer {
	void init() {
	}
	void destroy() {
	}
	void build_ui() {
		using namespace UI;
		UI_BACKGROUND() {
			UI_SCROLL_WINDOW() {
				workingBox->padding = 4.0F;
				spacer(24.0F);
				UI_RBOX() {
					workingBox->padding = 8.0F;
					workingBox->flags |= BOX_FLAG_WRAP_CHILDREN;
					workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
					workingBox->borderWidth = 2.0F;
					UI_BACKGROUND_COLOR(themeColor.foreground)
					UI_SIZE((V2F{ 128.0F, 128.0F })) {
						for (ResourceLoading::Material* mat : ResourceLoading::allMaterials) {
							Box* box = generic_box().unsafeBox;
							box->backgroundColor = mat->color.to_rgba8();
							box->backgroundTexture = mat->baseColor ? mat->baseColor : &Resources::simpleWhite;
						}
					}
				}
			}
		}
	}
};

struct PanelPrefabList {
	void init() {
	}
	void destroy() {
	}
	void build_ui() {
		using namespace UI;
		UI_BACKGROUND() {
			UI_SCROLL_WINDOW() {
				workingBox->padding = 4.0F;
				spacer(24.0F);
				UI_RBOX() {
					workingBox->padding = 8.0F;
					workingBox->flags |= BOX_FLAG_WRAP_CHILDREN;
					workingBox->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
					workingBox->borderWidth = 2.0F;
					UI_BACKGROUND_COLOR(themeColor.foreground)
						UI_SIZE((V2F{ 128.0F, 128.0F })) {
						for (Level::Prefab* prefab : Level::allPrefabs) {
							Box* box = generic_box().unsafeBox;
							box->backgroundColor = RGBA8{ 255, 255, 255, 255 };
							box->backgroundTexture = prefab->icon;
						}
					}
				}
			}
		}
	}
};

struct Panel;
Panel* alloc_panel();
void free_panel(Panel* panel);

Panel* rootPanel;

struct Panel {
	Panel* parent;
	Panel* childA;
	Panel* childB;
	UI::BoxHandle uiBox;
	UI::BoxHandle content;

	PanelType panelType;
	union {
		PanelEditor3D editor3D;
		PanelTextureProcessing textureProcessing;
		PanelMaterialEditor materialEditor;
		PanelMaterialViewer materialViewer;
		PanelPrefabList prefabList;
		PanelUITest uiTest;
	};

	void build_ui() {
		using namespace UI;
		UI::free_box(content);
		Box* panelBox = uiBox.get();
		if (!panelBox) {
			return;
		}
		UI_WORKING_BOX(panelBox) {
			content = generic_box();
			Box* contentBox = content.unsafeBox;
			contentBox->flags = BOX_FLAG_INVISIBLE | BOX_FLAG_CUSTOM_DRAW;
			contentBox->sizeModeX = contentBox->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
			Panel* panel = this;
			set_box_callback(contentBox, [=](Box* box, UserCommunication& com){
				if (com.keyPressed == Win32::KEY_V) {
					panel->split(AXIS2_X);
					return ACTION_HANDLED;
				}
				if (com.keyPressed == Win32::KEY_H) {
					panel->split(AXIS2_Y);
					return ACTION_HANDLED;
				}
				return ACTION_PASS;
			});

			UI_WORKING_BOX(contentBox) {
				Panel* panel = this;
				Box* panelSwitcher = button(Resources::uiWindowSwitch, [panel](Box* box) {
					UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F{ box->renderPos.x, box->renderPos.y + box->computedSize.y })) {
						text_button("3D Editor"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_EDITOR_3D); });
						text_button("Texture Processing"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_TEXTURE_PROCESSING); });
						text_button("Material Editor"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_MATERIAL_EDITOR); });
						text_button("Material Viewer"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_MATERIAL_VIEWER); });
						text_button("Prefab List"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_PREFAB_LIST); });
						text_button("UI Test"a, [panel](Box* box) { panel->set_type(PANEL_TYPE_UI_TEST); });
					}
				}).unsafeBox;
				panelSwitcher->flags |= BOX_FLAG_FLOATING;
				panelSwitcher->sizeModeX = panelSwitcher->sizeModeY = SIZE_MODE_ABSOLUTE;
				panelSwitcher->pos = V2F{ 8.0F, 8.0F };
				panelSwitcher->size = V2F{ 16.0F, 16.0F };
				switch (panelType) {
				case PANEL_TYPE_NONE: break;
				case PANEL_TYPE_EDITOR_3D: editor3D.build_ui(); break;
				case PANEL_TYPE_TEXTURE_PROCESSING: textureProcessing.build_ui(); break;
				case PANEL_TYPE_MATERIAL_EDITOR: materialEditor.build_ui(); break;
				case PANEL_TYPE_MATERIAL_VIEWER: materialViewer.build_ui(); break;
				case PANEL_TYPE_PREFAB_LIST: prefabList.build_ui(); break;
				case PANEL_TYPE_UI_TEST: uiTest.build_ui(); break;
				}
			}
		}
	}

	void set_type(PanelType type) {
		switch (panelType) {
		case PANEL_TYPE_NONE: break;
		case PANEL_TYPE_EDITOR_3D: editor3D.destroy(); break;
		case PANEL_TYPE_TEXTURE_PROCESSING: textureProcessing.destroy(); break;
		case PANEL_TYPE_MATERIAL_EDITOR: materialEditor.destroy(); break;
		case PANEL_TYPE_MATERIAL_VIEWER: materialViewer.destroy(); break;
		case PANEL_TYPE_PREFAB_LIST: prefabList.destroy(); break;
		case PANEL_TYPE_UI_TEST: uiTest.destroy(); break;
		}
		switch (type) {
		case PANEL_TYPE_NONE: break;
		case PANEL_TYPE_EDITOR_3D: editor3D = PanelEditor3D{}; editor3D.init(); break;
		case PANEL_TYPE_TEXTURE_PROCESSING: textureProcessing = PanelTextureProcessing{}; textureProcessing.init(); break;
		case PANEL_TYPE_MATERIAL_EDITOR: materialEditor = PanelMaterialEditor{}; materialEditor.init(); break;
		case PANEL_TYPE_MATERIAL_VIEWER: materialViewer = PanelMaterialViewer{}; materialViewer.init(); break;
		case PANEL_TYPE_PREFAB_LIST: prefabList = PanelPrefabList{}; prefabList.init(); break;
		case PANEL_TYPE_UI_TEST: uiTest = PanelUITest{}; uiTest.init(); break;

		}
		panelType = type;
		build_ui();
	}

	bool split(Axis2 axis) {
		Panel* newParent = alloc_panel();
		Panel* a = this;
		Panel* b = alloc_panel();
		newParent->uiBox = UI::alloc_box();
		newParent->uiBox.unsafeBox->parentSizePercent = a->uiBox.unsafeBox->parentSizePercent;
		newParent->uiBox.unsafeBox->sizeModeX = a->uiBox.unsafeBox->sizeModeX;
		newParent->uiBox.unsafeBox->sizeModeY = a->uiBox.unsafeBox->sizeModeY;
		newParent->uiBox.unsafeBox->flags = UI::BOX_FLAG_INVISIBLE;
		b->uiBox = UI::alloc_box();
		b->uiBox.unsafeBox->flags = UI::BOX_FLAG_DONT_FIT_CHILDREN | UI::BOX_FLAG_CLIP_CHILDREN | UI::BOX_FLAG_INVISIBLE;
		if (axis == AXIS2_X) {
			b->uiBox.unsafeBox->sizeModeX = UI::SIZE_MODE_PARENT_PERCENT;
			a->uiBox.unsafeBox->sizeModeX = UI::SIZE_MODE_PARENT_PERCENT;
			b->uiBox.unsafeBox->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
			a->uiBox.unsafeBox->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
			a->uiBox.unsafeBox->parentSizePercent.x = b->uiBox.unsafeBox->parentSizePercent.x = 0.5F;
		} else {
			b->uiBox.unsafeBox->sizeModeX = UI::SIZE_MODE_GROW_TO_PARENT;
			a->uiBox.unsafeBox->sizeModeX = UI::SIZE_MODE_GROW_TO_PARENT;
			b->uiBox.unsafeBox->sizeModeY = UI::SIZE_MODE_PARENT_PERCENT;
			a->uiBox.unsafeBox->sizeModeY = UI::SIZE_MODE_PARENT_PERCENT;
			a->uiBox.unsafeBox->parentSizePercent.y = b->uiBox.unsafeBox->parentSizePercent.y = 0.5F;
		}

		DLL_REPLACE(uiBox.unsafeBox, newParent->uiBox.unsafeBox, uiBox.unsafeBox->parent->childFirst, uiBox.unsafeBox->parent->childLast, prev, next);

		if (parent) {
			*parent->child_ref(this) = newParent;
		} else {
			rootPanel = newParent;
		}

		newParent->uiBox.unsafeBox->parent = uiBox.unsafeBox->parent;
		newParent->parent = parent;
		a->parent = newParent;
		b->parent = newParent;
		newParent->childA = a;
		newParent->childB = b;

		a->uiBox.unsafeBox->parent = newParent->uiBox.unsafeBox;
		b->uiBox.unsafeBox->parent = newParent->uiBox.unsafeBox;

		b->set_type(a->panelType);
		b->build_ui();

		UI::Box* draggableCenter = UI::alloc_box().unsafeBox;
		draggableCenter->parent = newParent->uiBox.unsafeBox;
		constexpr F32 CENTER_WIDTH = 2.0F;
		draggableCenter->size = V2F{ CENTER_WIDTH, CENTER_WIDTH };
		if (axis == AXIS2_X) {
			draggableCenter->sizeModeX = UI::SIZE_MODE_ABSOLUTE;
			draggableCenter->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
		} else {
			draggableCenter->sizeModeX = UI::SIZE_MODE_GROW_TO_PARENT;
			draggableCenter->sizeModeY = UI::SIZE_MODE_ABSOLUTE;
		}
		draggableCenter->hoverCursor = axis == AXIS2_X ? Win32::CURSOR_TYPE_SIZE_HORIZONTAL : Win32::CURSOR_TYPE_SIZE_VERTICAL;
		draggableCenter->backgroundColor = V4F32{ 0.7F, 0.7F, 0.7F, 1.0F }.to_rgba8();
		draggableCenter->callbackData[0] = char(axis);
		draggableCenter->actionCallback = [](UI::Box* box, UI::UserCommunication& com) {
			Axis2 splitAxis = Axis2(box->callbackData[0]);
			F32 dragAmount = splitAxis == AXIS2_X ? com.drag.x : com.drag.y;
			F32* prevSizePercent = splitAxis == AXIS2_X ? &box->prev->parentSizePercent.x : &box->prev->parentSizePercent.y;
			F32* nextSizePercent = splitAxis == AXIS2_X ? &box->next->parentSizePercent.x : &box->next->parentSizePercent.y;
			if (dragAmount != 0.0F) {
				F32 percentA = *prevSizePercent;
				F32 percentB = *nextSizePercent;
				F32 normalizedDistance = percentA / (percentA + percentB);
				F32 parentRange = (splitAxis == AXIS2_X ? box->parent->computedSize.x : box->parent->computedSize.y) - CENTER_WIDTH;
				F32 currentSplitPos = parentRange * normalizedDistance;
				F32 nextSplitPos = clamp(currentSplitPos + dragAmount, 0.0F, parentRange);
				*prevSizePercent = nextSplitPos / parentRange;
				*nextSizePercent = 1.0F - *prevSizePercent;
				return UI::ACTION_HANDLED;
			}
			return UI::ACTION_PASS;
		};
		UI::Box* parentBox = newParent->uiBox.unsafeBox;
		parentBox->layoutDirection = axis == AXIS2_X ? UI::LAYOUT_DIRECTION_RIGHT : UI::LAYOUT_DIRECTION_DOWN;
		DLL_INSERT_TAIL(a->uiBox.unsafeBox, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_INSERT_TAIL(draggableCenter, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_INSERT_TAIL(b->uiBox.unsafeBox, parentBox->childFirst, parentBox->childLast, prev, next);
		return true;
	}

	void destroy() {
		if (!parent || childA || childB) {
			return;
		}
		// Destroy this panel, then replace the parent with its sibling
		UI::Box* parentBox = parent->uiBox.unsafeBox;
		UI::free_box(UI::BoxHandle{ parentBox->childFirst->next, parentBox->childFirst->next->generation }); // Panel divider
		UI::free_box(uiBox);
		Panel* sibling = *parent->sibling_ref(this);
		UI::Box* siblingBox = sibling->uiBox.unsafeBox;
		siblingBox->parentSizePercent = parentBox->parentSizePercent;
		DLL_REMOVE(siblingBox, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_REPLACE(parentBox, siblingBox, parentBox->parent->childFirst, parentBox->parent->childLast, prev, next);
		siblingBox->parent = parentBox->parent;
		UI::free_box(parent->uiBox);
		if (parent->parent) {
			*parent->parent->child_ref(parent) = sibling;
		} else {
			rootPanel = sibling;
		}
		sibling->parent = parent->parent;
		free_panel(parent);
		free_panel(this);
	}

	Panel** sibling_ref(Panel* child) {
		return child == childA ? &childB :
			child == childB ? &childA :
			nullptr;
	}
	Panel** child_ref(Panel* child) {
		return child == childA ? &childA :
			child == childB ? &childB :
			nullptr;
	}
};

Panel* panelFreeList;

Panel* alloc_panel() {
	if (!panelFreeList) {
		panelFreeList = globalArena.alloc<Panel>(1);
		panelFreeList->childA = nullptr;
	}
	Panel* panel = panelFreeList;
	panelFreeList = panel->childA;
	*panel = Panel{};
	return panel;
}

void free_panel(Panel* panel) {
	panel->childA = panelFreeList;
	panelFreeList = panel;
}

bool key_input(Win32::Key key, Win32::ButtonState state) {
	if (focusedEditor3D) {
		focusedEditor3D->editor.key_input(key, state);
		return true;
	}
	if (currentInputWidget) {
		currentInputWidget->key_input(key, state);
		return true;
	}
	return false;
}
bool mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F pos) {
	if (focusedEditor3D) {
		focusedEditor3D->editor.mouse_input(button, state, pos);
		return true;
	}
	if (currentInputWidget) {
		currentInputWidget->mouse_input(button, state, pos);
		return true;
	}
	return false;
}
void update() {
	if (focusedEditor3D) {
		focusedEditor3D->editor.update();
	}
	for (EditorUI::PanelEditor3D* editor3d : EditorUI::renderPanels) {
		editor3d->update();
	};
	using namespace UI;
	if (Box* b7 = growableBox.get()) {
		b7->size.x = max(1.0F, Win32::get_mouse().x);
		b7->size.y = max(1.0F, Win32::get_mouse().y);
	}
	Physics::do_timestep(F32(StarChicken::deltaTime), 8, 0.95F, 300.0F, 0.98F, physicsThreadCount);
}

void add_point_grid(U32 width, U32 height, F32 scale, V3F pos) {
	MemoryArena& arena = get_scratch_arena();
	MEMORY_ARENA_FRAME(arena) {
		I32* indices = arena.alloc<I32>(width * height);
		for (U32 y = 0; y < height; y++) {
			for (U32 z = 0; z < width; z++) {
				indices[y * width + z] = Physics::add_point(V3F{ pos.x, (F32(height) - F32(y)) * scale + pos.y, F32(z) * scale + pos.z}, 0.5F);
			}
		}
		U32 middlePoint = U32(indices[(height / 2) * width + width / 2]);
		pointsToImpulse.push_back(middlePoint);
		U32 c1 = Physics::constraints.size;
		Physics::hard_constrain_point_global(indices[0 * width + 0], V3F{ pos.x, pos.y + (F32(height) + 1.0F) * scale, pos.z - 1.5F * scale }, scale);
		U32 c2 = Physics::constraints.size;
		Physics::hard_constrain_point_global(indices[0 * width + (width - 1)], V3F{ pos.x, pos.y + (F32(height) + 1.0F) * scale, F32(width - 1) * scale + pos.z }, scale);
		constraintsToMove.push_back(c1, c2);
		for (U32 y = 0; y < height; y++) {
			for (U32 z = 0; z < width; z++) {
				if (y > 0) {
					Physics::hard_constrain_point_to(indices[y * width + z], indices[(y - 1) * width + z], scale);
				}
				if (y + 1 < height) {
					Physics::hard_constrain_point_to(indices[y * width + z], indices[(y + 1) * width + z], scale);
				}
				if (z > 0) {
					Physics::hard_constrain_point_to(indices[y * width + z], indices[y * width + z - 1], scale);
				}
				if (z + 1 < width) {
					Physics::hard_constrain_point_to(indices[y * width + z], indices[y * width + z + 1], scale);
				}
			}
		}
	}
}

void init_physics() {
	add_point_grid(10, 10, 1.0F, V3F{ 0.0F, 5.0F, 0.0F });
	/*add_point_grid(20, 20, 2.0F, V3F{ 0.0F, 30.0F, 0.0F });
	add_point_grid(20, 20, 2.0F, V3F{ 0.0F, 30.0F, 50.0F });
	add_point_grid(20, 20, 2.0F, V3F{ 0.0F, 30.0F, -50.0F });
	add_point_grid(10, 10, 0.2F, V3F{ 0.0F, -10.0F, 0.0F });
	add_point_grid(20, 20, 2.0F, V3F{ 40.0F, 30.0F, 0.0F });*/

	boxA.maxX = 1.0F;
	boxA.maxY = 1.0F;
	boxA.maxZ = 1.0F;
	boxA.minX = -1.0F;
	boxA.minY = -1.0F;
	boxA.minZ = -1.0F;
	boxA.localToGlobalOrientation.set_identity();
	boxA.pos = V3F{ -10.0F, 10.0F, 0.0F };
	boxB = boxA;
	boxB.pos = V3F{ -10.0F, 13.0F, 0.0F };
}

void debug_render() {
	Physics::debug_render();
	bool intersect = SAT::is_intersecting(boxA, boxB);
	V3F color = intersect ? V3F{ 0.0F, 1.0F, 0.0F } : V3F{ 1.0F, 0.0F, 0.0F };
	boxA.debug_render(color);
	boxB.debug_render(color);
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	if (focusedEditor3D) {
		F32 t{};
		V3F o = focusedEditor3D->editor.get_render_eye_pos();
		V3F d = focusedEditor3D->editor.forward;
		bool hit = ray_cylinder_intersect(&t, o, d, V3F{ 0.0F, 2.0F, 0.0F }, V3F{ 0.0F, 3.0F, 0.0F }, 0.2F);
		if (hit) {
			V3F hitPos = o + t * d;
			tes.begin_draw(VK::debugPointsPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
			tes.pos3(hitPos.x, hitPos.y, hitPos.z).color(1.0F, 1.0F, 0.0F).end_vert();
			tes.end_draw();
		}
	}
	tes.begin_draw(VK::debugPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
	tes.debug_arrow(V3F{ 0.0F, 1.0F, 0.0F }, V3F{ 0.0F, 3.0F, 0.0F }, 0.1F, V4F{ 0.5F, 0.5F, 1.0F, 1.0F });
	tes.end_draw();
}

void render_prefab_icons(Level::Prefab** prefabs, U32 prefabCount) {
	VK::TmpCmdBuf cmdBuf = VK::begin_tmp_cmd_buf();
	VK::update_draw_data_buffer(cmdBuf.buf);
	VK::DedicatedImage iconDepthBuffer = VK::create_dedicated_image(VK_FORMAT_D32_SFLOAT, PREFAB_ICON_SIZE, PREFAB_ICON_SIZE, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
	VK::img_barrier(cmdBuf.buf, iconDepthBuffer.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VK::uniformMatricesHandler.set_camera(0, M4x3F{}.set_identity(), PerspectiveProjection{}.project_perspective(PROJECTION_NEAR_PLANE, 0.25F, 1.0F), V3F{});

	for (U32 i = 0; i < prefabCount; i++) {
		ResourceLoading::Texture* tex = globalArena.alloc<ResourceLoading::Texture>(1);
		ResourceLoading::create_texture(tex, nullptr, PREFAB_ICON_SIZE, PREFAB_ICON_SIZE, 1, ResourceLoading::TEXTURE_FORMAT_RGBA_U8_RENDER_TARGET, true, false);
		VK::img_barrier(cmdBuf.buf, tex->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		VkRenderingInfo iconRenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		iconRenderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ PREFAB_ICON_SIZE, PREFAB_ICON_SIZE } };
		iconRenderInfo.layerCount = 1;
		iconRenderInfo.colorAttachmentCount = 1;
		VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		colorAttachment.imageView = tex->imageView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = VkClearColorValue{};
		iconRenderInfo.pColorAttachments = &colorAttachment;
		VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		depthAttachment.imageView = iconDepthBuffer.imgView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue.depthStencil.depth = 0.0F;
		iconRenderInfo.pDepthAttachment = &depthAttachment;
		VK::vkCmdBeginRenderingKHR(cmdBuf.buf, &iconRenderInfo);

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = F32(iconRenderInfo.renderArea.extent.width);
		viewport.height = F32(iconRenderInfo.renderArea.extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VK::vkCmdSetViewport(cmdBuf.buf, 0, 1, &viewport);
		VK::vkCmdSetScissor(cmdBuf.buf, 0, 1, &iconRenderInfo.renderArea);

		VK::vkCmdBindPipeline(cmdBuf.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::prefabIconPipeline);
		VK::vkCmdBindDescriptorSets(cmdBuf.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::drawPipelineLayout, 0, 1, &VK::drawDataDescriptorSet.descriptorSet, 0, nullptr);
		VK::vkCmdBindIndexBuffer(cmdBuf.buf, VK::geometryHandler.buffer, VK::geometryHandler.indicesOffset, VK_INDEX_TYPE_UINT16);

		Level::Prefab* prefab = prefabs[i];
		V3F target = prefab->boundingBox.midpoint();
		F32 scale = prefab->boundingBox.diag_length() * 0.6F;
		V3F camPos = target - V3F{ 0.0F, 0.0F, -scale };
		for (U32 objIdx = 0; objIdx < prefab->objectCount; objIdx++) {
			Level::LevelObject* obj = prefab->objects[objIdx];
			switch (obj->type) {
			case Level::LEVEL_OBJECT_STATIC_MODEL: {
				Level::StaticModel* model = (Level::StaticModel*)obj;
				M4x3F transform = model->obj.transform;
				transform.add_offset(-camPos);
				U32 matrixIdx = VK::uniformMatricesHandler.alloc_and_set(1, &transform);
				VK::WorldDrawPushConstants pushConstants{};
				pushConstants.transformIdx = matrixIdx;
				pushConstants.verticesOffset = model->mesh->verticesOffset;
				pushConstants.materialId = model->material->gpuIdx;
				VK_PUSH_STRUCT(cmdBuf.buf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConstants, 0);
				VK::vkCmdDrawIndexed(cmdBuf.buf, model->mesh->indicesCount, 1, model->mesh->indicesOffset, 0, 1);
			} break;
			}
		}

		VK::vkCmdEndRenderingKHR(cmdBuf.buf);
	
		VK::img_barrier(cmdBuf.buf, iconDepthBuffer.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		VK::img_barrier(cmdBuf.buf, tex->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		prefab->icon = tex;
	}

	VK::uniformMatricesHandler.flush_memory();
	VK::end_tmp_cmd_buf(cmdBuf);

	VK::destroy_dedicated_image(iconDepthBuffer);
}

void init() {
	using namespace UI;
	UI_WORKING_BOX(root) {
		Box* toolbar = generic_box().unsafeBox;
		toolbar->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		toolbar->size.y = 16.0F;
		toolbar->layoutDirection = LAYOUT_DIRECTION_RIGHT;
		toolbar->backgroundColor = themeColor.header;
		UI_WORKING_BOX(toolbar) {
			UI_PADDING(2.0F)
			text_button("File"a, [](Box* box) {
				UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F{ 0.0F, 16.0F })) {
					text_button("Cubemap Gen"a, [](Box* box) {
						print("Convolving...");
						CubemapGen::equirectangular2convolved_cubemap("cubemap_test/cube"a, get_user_selected_file(globalArena), true);
						print(" complete\n");
					});
				}
			});

			spacer();
			debugDisplayText = text(""a);
		}
	}
	Panel* panel = alloc_panel();
	panel->uiBox = UI::alloc_box();
	panel->uiBox.unsafeBox->flags = UI::BOX_FLAG_DONT_FIT_CHILDREN | UI::BOX_FLAG_CLIP_CHILDREN | UI::BOX_FLAG_INVISIBLE;
	panel->uiBox.unsafeBox->sizeModeX = panel->uiBox.unsafeBox->sizeModeY = UI::SIZE_MODE_GROW_TO_PARENT;
	rootPanel = panel;
	panel->set_type(PANEL_TYPE_EDITOR_3D);
	panel->uiBox.unsafeBox->parent = UI::root;
	DLL_INSERT_TAIL(panel->uiBox.unsafeBox, UI::root->childFirst, UI::root->childLast, prev, next);
	
	undoStack.init();
	
	render_prefab_icons(Level::allPrefabs.data, Level::allPrefabs.size);
	init_physics();
}

}