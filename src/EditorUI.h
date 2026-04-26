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
	TranslateWidgetComponent activeComponent;

	void do_mouse_over(V3F eye, V3F look) {
		activeComponent = TRANSLATE_WIDGET_COMPONENT_NONE;
		F32 scale = 0.125F * distance(eye, V3F{ transform.x, transform.y, transform.z });
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
	}

	void draw(DynamicVertexBuffer::Tessellator& tes, V3F eye) {
		F32 scale = 0.125F * distance(eye, V3F{ transform.x, transform.y, transform.z });
		F32 arrowRadius = 0.05F * scale;
		V3F center{ transform.x, transform.y, transform.z };
		V3F xAxis = transform.get_row(0);
		V3F yAxis = transform.get_row(1);
		V3F zAxis = transform.get_row(2);
		tes.begin_draw(VK::debugNoDepthPipeline, VK::drawPipelineLayout, DynamicVertexBuffer::DRAW_MODE_PRIMITIVES);
		tes.debug_arrow(center, center + xAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_X_AXIS ? V4F{ 1.0F, 0.5F, 0.5F, 1.0F } : V4F{ 1.0F, 0.25F, 0.25F, 1.0F });
		tes.debug_arrow(center, center + yAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_Y_AXIS ? V4F{ 0.5F, 1.0F, 0.5F, 1.0F } : V4F{ 0.25F, 1.0F, 0.25F, 1.0F });
		tes.debug_arrow(center, center + zAxis * scale, arrowRadius, activeComponent == TRANSLATE_WIDGET_COMPONENT_Z_AXIS ? V4F{ 0.5F, 0.5F, 1.0F, 1.0F } : V4F{ 0.25F, 0.25F, 1.0F, 1.0F });
		tes.end_draw();
	}
};


enum PanelType {
	PANEL_TYPE_NONE,
	PANEL_TYPE_EDITOR_3D,
	PANEL_TYPE_TEXTURE_PROCESSING,
	PANEL_TYPE_UI_TEST
};

struct PanelEditor3D;
ArenaArrayList<PanelEditor3D*> renderPanels;

PanelEditor3D* focusedEditor3D;

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
	TranslateWidget translateWidget;

	void init() {
		V3F playerEye{ -30.0F, 8.0F, 0.0F };
		editor.pos = playerEye;
		editor.yaw = 0.25F;
		editor.rotation_updated();
		fov = 120.0F;
		renderPanels.push_back_unique(this);

		translateWidget.transform.set_identity();
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
	V2F vector_in_screen_space(V3F origin, V3F direction) {
		return project_pos(origin + direction) - project_pos(origin);
	}
	void update() {
		projection.project_perspective(0.05F, DEG_TO_TURN(fov), viewport.height() / viewport.width());

		if (!Win32::mouseCaptured) {
			V2F mousePos = Win32::get_mouse();
			mousePickRay = unproject_vec(mousePos);
			panelContainsMouse = viewport.contains_point(mousePos);
			V3F eyePos = editor.get_render_eye_pos();
			if (panelContainsMouse) {
				translateWidget.do_mouse_over(eyePos, mousePickRay);
				//F32 scale = 0.125F * distance(eyePos, translateWidget.transform.translation());
				//printf("%\n"a, vector_in_screen_space(translateWidget.transform.translation(), translateWidget.transform.get_row(1) * scale));
			} else {
				translateWidget.activeComponent = TRANSLATE_WIDGET_COMPONENT_NONE;
			}
		}
	}
	void debug_render() {
		DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
		V3F eyePos = editor.get_render_eye_pos();
		translateWidget.draw(tes, eyePos);
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
						Level::level.select_all(selectedSet.data, selectedSet.size);
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
		case PANEL_TYPE_UI_TEST: uiTest.destroy(); break;
		}
		switch (type) {
		case PANEL_TYPE_NONE: break;
		case PANEL_TYPE_EDITOR_3D: editor3D = PanelEditor3D{}; editor3D.init(); break;
		case PANEL_TYPE_TEXTURE_PROCESSING: textureProcessing = PanelTextureProcessing{}; textureProcessing.init(); break;
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

void key_input(Win32::Key key, Win32::ButtonState state) {
	if (focusedEditor3D) {
		focusedEditor3D->editor.key_input(key, state);
	}
}
void mouse_input(Win32::MouseButton button, Win32::MouseValue state, V2F pos) {
	if (focusedEditor3D) {
		focusedEditor3D->editor.mouse_input(button, state, pos);
	}
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

	init_physics();
}

}