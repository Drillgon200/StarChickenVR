#pragma once
#include "UI.h"
#include "physics/Physics.h"

namespace EditorUI {

UI::BoxHandle growableBox;

U32 middlePoint;

struct EditorPlayer {
	V3F pos;
	F32 pitch, yaw;
	F32 rotateDistance;
	V3F forward, up, right;

	void key_input(Win32::Key key, Win32::ButtonState state) {
		V2F32 mousePos = Win32::get_mouse();
		UI::handle_keyboard_action(mousePos, key, state);
		if (key == Win32::KEY_ESC && state == Win32::BUTTON_STATE_DOWN) {
			Win32::set_mouse_captured(false);
		}
	}
	void mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F pos) {
		if (button == Win32::MOUSE_BUTTON_WHEEL) {
			F32 sensitivity = rotateDistance * (0.2F / 120.0F);
			rotateDistance = clamp(rotateDistance - state.scroll * sensitivity, 1.0F, 1000.0F);
		}
		if (button == Win32::MOUSE_BUTTON_LEFT && state.state == Win32::BUTTON_STATE_DOWN) {
			Physics::points.data[middlePoint].vel += forward * 100.0F;
		}
	}
	void update() {
		V2F deltaMouse = Win32::get_raw_delta_mouse();
		F32 sensitivity = 0.0005F;
		yaw += deltaMouse.x * sensitivity;
		//pitch = clamp(pitch + deltaMouse.y * sensitivity, -0.24999F, 0.24999F);
		pitch += deltaMouse.y * sensitivity;

		QF32 localToWorld = AxisAngleF{ { 0.0F, 1.0F, 0.0F }, -yaw }.to_qf32() * AxisAngleF { { 1.0F, 0.0F, 0.0F }, -pitch }.to_qf32();
		forward = localToWorld * V3F{ 0.0F, 0.0F, -1.0F };
		up      = localToWorld * V3F{ 0.0F, 1.0F,  0.0F };
		right   = localToWorld * V3F{ 1.0F, 0.0F,  0.0F };

		F32 moveDelta = 5.0F * StarChicken::deltaTime;
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
	}

	M4x3F get_view_transform() {
		M4x3F view; view.set_identity();
		view.translate(V3F{ 0.0F, 0.0F, -rotateDistance });
		view.rotate_quat(AxisAngleF{ { 0.0F, 1.0F, 0.0F }, -yaw }.to_qf32() * AxisAngleF { { 1.0F, 0.0F, 0.0F }, -pitch }.to_qf32());
		view.translate(-pos);
		return view;
	}
};

EditorPlayer editor;
B32 editorFocused;

void key_input(Win32::Key key, Win32::ButtonState state) {
	if (editorFocused) {
		editor.key_input(key, state);
	}
}
void mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F pos) {
	if (editorFocused) {
		editor.mouse_input(button, state, pos);
	}
}
void update() {
	if (editorFocused) {
		editor.update();
	}
	using namespace UI;
	if (Box* b7 = growableBox.get()) {
		b7->size.x = max(1.0F, Win32::get_mouse().x);
		b7->size.y = max(1.0F, Win32::get_mouse().y);
	}
	Physics::do_timestep(StarChicken::deltaTime, 10, 0.99F, 100000.0F, 0.99F);
}

void init_physics() {
	const U32 width = 10, height = 10;
	I32 indices[width * height];
	for (U32 y = 0; y < height; y++) {
		for (U32 z = 0; z < width; z++) {
			indices[y * width + z] = Physics::add_point(V3F{ 0.0F, 15.0F - F32(y), F32(z) }, 1.0F);
		}
	}
	middlePoint = indices[(height / 2) * width + width / 2];
	Physics::hard_constrain_point_global(indices[0 * width + 0], V3F{ 0.0F, 16.0F, -1.5F }, 1.0F);
	Physics::hard_constrain_point_global(indices[0 * width + (width - 1)], V3F{ 0.0F, 16.0F, F32(width - 1) }, 1.0F);
	for (U32 y = 0; y < height; y++) {
		for (U32 z = 0; z < width; z++) {
			if (y > 0) {
				Physics::hard_constrain_point_to(indices[y * width + z], indices[(y - 1) * width + z], 1.0F);
			}
			if (y + 1 < height) {
				Physics::hard_constrain_point_to(indices[y * width + z], indices[(y + 1) * width + z], 1.0F);
			}
			if (z > 0) {
				Physics::hard_constrain_point_to(indices[y * width + z], indices[y * width + z - 1], 1.0F);
			}
			if (z + 1 < width) {
				Physics::hard_constrain_point_to(indices[y * width + z], indices[y * width + z + 1], 1.0F);
			}
		}
	}
}

void debug_render() {
	Physics::debug_render();
}

void init() {
	V3F playerEye{ -30.0F, 8.0F, 0.0F };
	editor.pos = playerEye;
	editor.yaw = 0.25F;
	init_physics();
	return;
	using namespace UI;
	Box* b1 = generic_box().unsafeBox;
	b1->size = V2F{ 100.0F, 100.0F };
	b1->backgroundColor = RGBA8{ 255, 255, 255, 255 };
	Box* b2 = generic_box().unsafeBox;
	b2->size = V2F{ 50.0F, 100.0F };
	b2->backgroundColor = RGBA8{ 255, 0, 255, 255 };
	b2->padding = 10.0F;
	UI_SIZE((V2F{ 100.0F, 20.0F }))
	UI_WORKING_BOX(b2) {
		Box* b3 = generic_box().unsafeBox;
		b3->backgroundColor = RGBA8{ 0, 255, 255, 255 };
		Box* b4 = generic_box().unsafeBox;
		b4->backgroundColor = RGBA8{ 0, 0, 255, 255 };
		Box* b5 = generic_box().unsafeBox;
		b5->backgroundColor = RGBA8{ 255, 255, 0, 255 };
		Box* b6 = generic_box().unsafeBox;
		b6->backgroundColor = RGBA8{ 255, 255, 0, 255 };
	}
	Box* b7 = generic_box().unsafeBox;
	b7->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	b7->align = ALIGN_MODE_TOP_LEFT;
	b7->size = V2F{ 200.0F, 100.0F };
	b7->backgroundColor = RGBA8{ 255, 255, 0, 255 };
	b7->padding = 10.0F;
	UI_SIZE((V2F{ 20.0F, 20.0F }))
	UI_WORKING_BOX(b7) {
		Box* b8 = generic_box().unsafeBox;
		b8->flags |= BOX_FLAG_WRAP_TEXT;
		b8->backgroundColor = RGBA8{ 0, 25, 25, 255 };
		b8->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		b8->size.x = 50.0F;
		b8->size = {};
		//b8->text = "\"';:~`?,|\\/=+-_><{}[]()*&^%$#@!.9876543210abcdefghij\nklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"a;
		b8->text = "Ducks are really cool and they live in ponds all around the world"a;
		b8->textSize = 12.0F;
		b8->padding = 2.0F;
		Box* b82 = generic_box().unsafeBox;
		b82->backgroundColor = RGBA8{ 0, 25, 25, 255 };
		b82->sizeModeX = SIZE_MODE_FIT_CHILDREN;
		b82->size.x = 50.0F;
		b82->size = {};
		b82->text = "test@ test test"a;
		b82->textSize = 12.0F;
		b82->padding = 0.0F;
		Box* b9 = generic_box().unsafeBox;
		b9->backgroundColor = RGBA8{ 0, 0, 255, 255 };
		//b9->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		b9->maxSize.x = 200.0F;
		Box* b10 = generic_box().unsafeBox;
		b10->layoutDirection = LAYOUT_DIRECTION_DOWN;
		b10->backgroundColor = RGBA8{ 255, 0, 255, 255 };
		//b10->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
		//b10->sizeModeY = SIZE_MODE_GROW_TO_PARENT;
		b10->size.x = 100.0F;
		b10->padding = 10.0F;
		UI_WORKING_BOX(b10) {
			Box* b11 = generic_box().unsafeBox;
			b11->backgroundColor = RGBA8{ 255, 255, 255, 255 };
			b11->sizeModeX = SIZE_MODE_GROW_TO_PARENT;
			b11->size.y = 100.0F;
		}
		growableBox = BoxHandle{ b7, b7->generation };
	}

	//UI_DBOX() {
	//	workingBox->flags |= BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN | BOX_FLAG_CLIP_CHILDREN;
	//	workingBox->sizeParentPercent = V2F{20.0F, 100.0F};
	//	workingBox->idealSize = V2F{ 20.0F, F32_INF };
	//	UI_BACKGROUND_COLOR((V4F32{ 0.2F, 0.2F, 0.5F, 1.0F })) {
	//		text_button("Ducks are nice"a, [](Box* box) {});
	//		slider_number(0.1F, [=](Box* box) { });
	//		text_input("Input"a, ""a, [](Box* box){});
	//	}
	//}21231wwe
	
}

}