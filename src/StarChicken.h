#pragma once
#include "StarChicken_decl.h"
#include "DrillMath.h"
#include "Win32.h"
#include "VK.h"
#include "XR.h"
#include "Resources.h"
#include "SerializeTools.h"
#include "MSDFGenerator.h"
#include "PNG.h"
#include "UI.h"
#include "EditorUI.h"

namespace StarChicken {

enum PlayerFinger : U32 {
	PLAYER_FINGER_INDEX = 0,
	PLAYER_FINGER_MIDDLE = 1,
	PLAYER_FINGER_RING = 2,
	PLAYER_FINGER_PINKY = 3,
	PLAYER_FINGER_THUMB = 4,
	PLAYER_FINGER_Count = 5
};

struct PlayerHand {
	M4x3F32 transform;
	F32 fingerCloseAmounts[PLAYER_FINGER_Count];
	F32 fingerCloseTargets[PLAYER_FINGER_Count];
	B32 fingerBlocked[PLAYER_FINGER_Count];
};

struct PlayerInfo {
	PlayerHand leftHand;
	PlayerHand rightHand;
	V3F32 position;
};

B32 shouldShutDown;
B32 shouldUseDesktopWindow;

B32 isInEditorMode;

U64 frameNumber;
U64 prevFrameTime;
U64 frameTime;
U64 timerFrequency;
F64 deltaTime;
F64 totalTime;

PlayerInfo player;

VKGeometry::StaticModel worldModel;
VKGeometry::SkeletalModel testAnimModel;
VKGeometry::SkeletalModel playerRightHandModel;
VKGeometry::SkeletalModel playerLeftHandModel;

ArenaArrayList<VKGeometry::StaticModel*> staticModelsToRender;
ArenaArrayList<VKGeometry::SkeletalModel*> skeletalModelsToRender;

void setup_scene() {
	player.position = V3F32{ 0.0F, 8.5F, 12.35F };
	player.leftHand.transform.set_identity();
	player.rightHand.transform.set_identity();
	VKGeometry::make_static_model(&worldModel, Resources::testMesh);
	VKGeometry::make_skeletal_model(&testAnimModel, Resources::testAnimMesh);
	testAnimModel.transform.translate(V3F32{ 3.0F, 3.0F, 0.0F });
	VKGeometry::make_skeletal_model(&playerRightHandModel, Resources::rightHandMesh);
	VKGeometry::make_skeletal_model(&playerLeftHandModel, Resources::leftHandMesh);
}

void update_player(XR::OpenXRFrameInfo& openxrFrameInfo) {
	playerRightHandModel.transform = XR::userInput.rightHand.pose;
	playerRightHandModel.transform.add_offset(player.position).rotate_axis_angle(V3F32_NORTH, -0.25F).rotate_axis_angle(V3F32_UP, -0.07F);

	for (U32 finger = PLAYER_FINGER_INDEX; finger < PLAYER_FINGER_PINKY; finger++) {
		player.rightHand.fingerCloseTargets[finger] = XR::userInput.rightHand.grip;
	}
	if (XR::userInput.rightHand.grip > 0.5F) {
		XrHapticActionInfo hapticInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
		hapticInfo.action = XR::rightHandActions.hapticOutput;
		hapticInfo.subactionPath = XR_NULL_PATH;
		XrHapticVibration hapticVibration{ XR_TYPE_HAPTIC_VIBRATION };
		hapticVibration.duration = MILLISECOND_TO_NANOSECOND(100);
		hapticVibration.frequency = XR_FREQUENCY_UNSPECIFIED;
		hapticVibration.amplitude = 0.5F;
		CHK_XR(XR::xrApplyHapticFeedback(XR::session, &hapticInfo, reinterpret_cast<XrHapticBaseHeader*>(&hapticVibration)));
	}

	PlayerHand* hands[2]{ &player.leftHand, &player.rightHand };
	for (U32 hand = 0; hand < ARRAY_COUNT(hands); hand++) {
		for (U32 finger = PLAYER_FINGER_INDEX; finger <= PLAYER_FINGER_PINKY; finger++) {
			if (!hands[hand]->fingerBlocked[finger]) {
				F32 fingerCloseAmount = hands[hand]->fingerCloseAmounts[finger];
				F32 fingerCloseTarget = hands[hand]->fingerCloseTargets[finger];
				hands[hand]->fingerCloseAmounts[finger] = clamp01(fingerCloseAmount + clamp(fingerCloseTarget - fingerCloseAmount, -0.8F, 0.8F) * F32(deltaTime));
			}
		}
	}
}

void draw_static_model(VKGeometry::StaticModel& model) {
	model.gpuMatrixIndex = VK::uniformMatricesHandler.alloc(1);
	staticModelsToRender.push_back(&model);
}

void draw_skeletal_model(VKGeometry::SkeletalModel& model) {
	model.gpuMatrixIndex = VK::uniformMatricesHandler.alloc(1);
	model.skeletonMatrixOffset = VK::uniformMatricesHandler.alloc(model.mesh->skeletonData->boneCount);
	model.skinnedVerticesOffset = VK::geometryHandler.alloc_skinned_result(model.mesh->geometry.verticesCount);
	skeletalModelsToRender.push_back(&model);
}

void set_skeletal_model_to_default_pose(VKGeometry::SkeletalModel& model) {
	U32 boneCount = model.mesh->skeletonData->boneCount;
	model.poseMatrices = frameArena.alloc<M4x3F32>(boneCount);
	for (U32 i = 0; i < boneCount; i++) {
		model.poseMatrices[i] = model.mesh->skeletonData->bones[i].bindTransform;
	}
}

void draw_scene() {
	draw_static_model(worldModel);
	set_skeletal_model_to_default_pose(playerRightHandModel);
	set_skeletal_model_to_default_pose(testAnimModel);
	testAnimModel.poseMatrices[1].rotate_quat(QF32{}.from_axis_angle(AxisAngleF32{ V3F32_EAST, (sinf32(F32(totalTime) * 0.75F) + 1.0F) * 0.125F }));
	draw_skeletal_model(playerRightHandModel);
	draw_skeletal_model(testAnimModel);
}

void draw_frame(XR::OpenXRFrameInfo& openxrFrameBeginInfo) {
	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performance counter");
	}
	frameNumber++;
	prevFrameTime = frameTime;
	frameTime = U64(perfCounter.QuadPart);
	if (!isInEditorMode) {
		deltaTime = F64(openxrFrameBeginInfo.predictedDisplayPeriod) / 1000000000.0;
	} else {
		deltaTime = F64(frameTime - prevFrameTime) / F64(performanceCounterTimerFrequency);
	}
	// Bad things happen to simulations when you give them massive timesteps, clamp it to 30FPS min
	deltaTime = min(deltaTime, 1.0 / 30.0);
	
	totalTime += deltaTime;

	V2F32 mousePos = Win32::get_mouse();
	V2F32 mouseDelta = Win32::get_delta_mouse();
	if (!Win32::mouseCaptured) {
		UI::handle_mouse_update(mousePos, mouseDelta);
	}

	EditorUI::update();

	MemoryArena& stackArena = get_scratch_arena();
	U64 stackArenaFrame0 = stackArena.stackPtr;
	VK::FrameBeginResult beginAction = VK::begin_frame();
	if (beginAction == VK::FRAME_BEGIN_RESULT_TRY_AGAIN) {
		beginAction = VK::begin_frame();
	}
	if (beginAction != VK::FRAME_BEGIN_RESULT_CONTINUE) {
		Sleep(1);
		return;
	}
	UI::layout_boxes(VK::desktopSwapchainData.width, VK::desktopSwapchainData.height);
	if (openxrFrameBeginInfo.shouldRender || isInEditorMode) {
		//draw_scene();
		
		VkCommandBuffer cmdBuf = VK::graphicsCommandBuffer;

		{ // Compute skinning
			VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::skinningPipeline);
			VK::vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::skinningPipelineLayout, 0, 1, &VK::drawDataDescriptorSet.descriptorSet, 0, nullptr);
			for (VKGeometry::SkeletalModel* model : skeletalModelsToRender) {
				VKGeometry::GPUSkinnedModel gpuSkinnedModelData;
				gpuSkinnedModelData.matricesOffset = model->skeletonMatrixOffset;
				gpuSkinnedModelData.vertexOffset = model->mesh->geometry.verticesOffset;
				gpuSkinnedModelData.skinnedVerticesOffset = model->skinnedVerticesOffset;
				gpuSkinnedModelData.skinningDataOffset = model->mesh->skinningDataOffset;
				gpuSkinnedModelData.vertexCount = model->mesh->geometry.verticesCount;
				VK::vkCmdPushConstants(cmdBuf, VK::skinningPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VKGeometry::GPUSkinnedModel), &gpuSkinnedModelData);
				VK::vkCmdDispatch(cmdBuf, (model->mesh->geometry.verticesCount + 255) / 256, 1, 1);
			}

			VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK::graphicsFamily;
			bufferBarrier.dstQueueFamilyIndex = VK::graphicsFamily;
			bufferBarrier.buffer = VK::geometryHandler.buffer;
			bufferBarrier.offset = VK::geometryHandler.skinnedPositionsOffset;
			bufferBarrier.size = VK_WHOLE_SIZE;
			VK::vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
		}

		VkRenderPassBeginInfo renderPassBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBegin.renderPass = VK::mainRenderPass;
		renderPassBegin.framebuffer = VK::mainFramebuffer.framebuffer;
		renderPassBegin.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ VK::mainFramebuffer.framebufferWidth, VK::mainFramebuffer.framebufferHeight } };
		renderPassBegin.clearValueCount = 2;
		VkClearValue clearValues[2]{};
		clearValues[0].color.float32[0] = 0.0F;
		clearValues[0].color.float32[1] = 0.0F;
		clearValues[0].color.float32[2] = 0.0F;
		clearValues[0].color.float32[3] = 0.0F;
		clearValues[1].depthStencil.depth = 0.0F;
		clearValues[1].depthStencil.stencil = 0;
		renderPassBegin.pClearValues = clearValues;
		VK::vkCmdBeginRenderPass(cmdBuf, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = F32(VK::mainFramebuffer.framebufferWidth);
		viewport.height = F32(VK::mainFramebuffer.framebufferHeight);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VK::vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = VK::mainFramebuffer.framebufferWidth;
		scissor.extent.height = VK::mainFramebuffer.framebufferHeight;
		VK::vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
		VK::vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::drawPipelineLayout, 0, 1, &VK::drawDataDescriptorSet.descriptorSet, 0, nullptr);

		if (CubemapGen::hasCubemap) {
			// Fill in background
			VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::tmpBackgroundPipeline);
			VK::vkCmdDraw(cmdBuf, 3, 1, 0, 0);
		}
		
		VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::drawPipeline);

		{ // Draw world geometry
			VK::vkCmdBindIndexBuffer(cmdBuf, VK::geometryHandler.buffer, VK::geometryHandler.indicesOffset, VK_INDEX_TYPE_UINT16);

			for (VKGeometry::StaticModel* model : staticModelsToRender) {
				VK::GPUModelInfo modelInfo{ model->gpuMatrixIndex, I32(model->mesh->verticesOffset + 1) };
				VK::vkCmdPushConstants(cmdBuf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::GPUModelInfo), &modelInfo);
				VK::vkCmdDrawIndexed(cmdBuf, model->mesh->indicesCount, 1, model->mesh->indicesOffset, 0, 0);
			}

			for (VKGeometry::SkeletalModel* model : skeletalModelsToRender) {
				// Negate vertex offset so the shader knows to pull from the skinned vertex arrays
				VK::GPUModelInfo modelInfo{ model->gpuMatrixIndex, -I32(model->skinnedVerticesOffset + 1) };
				VK::vkCmdPushConstants(cmdBuf, VK::drawPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::GPUModelInfo), &modelInfo);
				VK::vkCmdDrawIndexed(cmdBuf, model->mesh->geometry.indicesCount, 1, model->mesh->geometry.indicesOffset, 0, 0);
			}
		}
		
		{ // Draw UI and extras
			EditorUI::debug_render();
			UI::draw();
			DynamicVertexBuffer::get_tessellator().draw();
		}

		VK::vkCmdEndRenderPass(cmdBuf);
	}

	if (!isInEditorMode) {
		XrSwapchainImageWaitInfo swapchainWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		swapchainWaitInfo.timeout = XR_INFINITE_DURATION;
		CHK_XR(XR::xrWaitSwapchainImage(XR::xrColorSwapchain, &swapchainWaitInfo));
		if (XR::depthCompositionLayerSupported) {
			CHK_XR(XR::xrWaitSwapchainImage(XR::xrDepthSwapchain, &swapchainWaitInfo));
		}
	}
	
	CHK_VK(VK::vkWaitForFences(VK::logicalDevice, 1, &VK::geometryCullAndDrawFence, VK_TRUE, U64_MAX));
	CHK_VK(VK::vkResetFences(VK::logicalDevice, 1, &VK::geometryCullAndDrawFence));

	{ // Update GPU model and skeleton matrices
		for (VKGeometry::StaticModel* model : staticModelsToRender) {
			if (model->gpuMatrixIndex != 0) {
				VK::uniformMatricesHandler.matrixMemoryMapping[model->gpuMatrixIndex] = model->transform;
			}
		}
		for (VKGeometry::SkeletalModel* model : skeletalModelsToRender) {
			U64 stackArenaFrame1 = stackArena.stackPtr;
			if (model->gpuMatrixIndex != 0) {
				VK::uniformMatricesHandler.matrixMemoryMapping[model->gpuMatrixIndex] = model->transform;
			}
			if (model->skeletonMatrixOffset != 0) {
				U32 boneCount = model->mesh->skeletonData->boneCount;
				M4x3F32* matrices = stackArena.alloc<M4x3F32>(boneCount);
				if (model->poseMatrices) {
					VKGeometry::Bone* bones = model->mesh->skeletonData->bones;
					for (U32 i = 0; i < boneCount; i++) {
						if (bones[i].parentIdx == VKGeometry::Bone::PARENT_INVALID_IDX) {
							matrices[i] = model->poseMatrices[i];
						} else {
							matrices[i] = matrices[bones[i].parentIdx] * model->poseMatrices[i];
						}
					}
					for (U32 i = 0; i < boneCount; i++) {
						matrices[i] = matrices[i] * model->mesh->skeletonData->bones[i].invBindTransform;
					}
				} else {
					for (U32 i = 0; i < boneCount; i++) {
						matrices[i].set_identity();
					}
				}
				for (U32 i = 0; i < boneCount; i++) {
					VK::uniformMatricesHandler.matrixMemoryMapping[model->skeletonMatrixOffset + i] = matrices[i];
				}
			}
			stackArena.stackPtr = stackArenaFrame1;
		}
	}
	if (!isInEditorMode) {
		// Late latch update eye poses
		XR::update_eye_poses(&openxrFrameBeginInfo);
		{ // Update GPU eye matrices
			PerspectiveProjection leftProjection{};
			leftProjection.project_perspective(PROJECTION_NEAR_PLANE,
											   RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleRight),
											   RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleLeft),
											   RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleUp),
											   RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleDown));
			M4x3F32 leftTransform;
			leftTransform.set_identity();
			leftTransform.set_orientation_from_quat(XR::xr_quat_to_drillmath_quat(openxrFrameBeginInfo.leftEyePose.orientation).conjugate());
			V3F leftCamPos = player.position + XR::xr_vec3_to_drillmath_vec3(openxrFrameBeginInfo.leftEyePose.position);
			leftTransform.translate(-leftCamPos);
			PerspectiveProjection rightProjection{};
			rightProjection.project_perspective(PROJECTION_NEAR_PLANE,
												RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleRight),
												RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleLeft),
												RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleUp),
												RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleDown));
			M4x3F32 rightTransform;
			rightTransform.set_identity();
			rightTransform.set_orientation_from_quat(XR::xr_quat_to_drillmath_quat(openxrFrameBeginInfo.rightEyePose.orientation).conjugate());
			V3F rightCamPos = player.position + XR::xr_vec3_to_drillmath_vec3(openxrFrameBeginInfo.rightEyePose.position);
			rightTransform.translate(-rightCamPos);

			VK::uniformMatricesHandler.set_camera(0, leftTransform, leftProjection, leftCamPos);
			VK::uniformMatricesHandler.set_camera(1, rightTransform, rightProjection, rightCamPos);
		}
	} else {
		PerspectiveProjection proj;
		proj.project_perspective(0.05F, DEG_TO_TURN(120.0F), F32(Win32::framebufferHeight) / F32(Win32::framebufferWidth));
		V3F camPos;
		M4x3F32 view = EditorUI::editor.get_view_transform(&camPos);
		VK::uniformMatricesHandler.set_camera(0, view, proj, camPos);
	}
	VK::uniformMatricesHandler.flush_memory();

	VK::end_frame(openxrFrameBeginInfo.shouldRender | isInEditorMode);
	stackArena.stackPtr = stackArenaFrame0;
}

void keyboard_callback(Win32::Key key, Win32::ButtonState state) {
	if (UI::inDialog) {
		return;
	}
	V2F32 mousePos = Win32::get_mouse();
	UI::handle_keyboard_action(mousePos, key, state);
	if (key == Win32::KEY_ESC && state == Win32::BUTTON_STATE_DOWN) {
		Win32::set_mouse_captured(false);
		EditorUI::editorFocused = false;
	}
	EditorUI::key_input(key, state);
}
void mouse_callback(Win32::MouseButton button, Win32::MouseValue state) {
	if (UI::inDialog) {
		return;
	}
	V2F32 mousePos = Win32::get_mouse();
	if (!EditorUI::editorFocused) {
		if (!UI::handle_mouse_action(mousePos, button, state)) {
			if (button == Win32::MOUSE_BUTTON_LEFT && state.state == Win32::BUTTON_STATE_DOWN) {
				Win32::set_mouse_captured(true);
				EditorUI::editorFocused = true;
			}
		}
	}
	EditorUI::mouse_input(button, state, mousePos);
}

void do_frame() {
	swap(&frameArena, &lastFrameArena);
	frameArena.reset();
	const U32 initialModelToRenderCapacity = 32;
	staticModelsToRender.reset();
	staticModelsToRender.reserve(initialModelToRenderCapacity);
	skeletalModelsToRender.reset();
	skeletalModelsToRender.reserve(initialModelToRenderCapacity);
	if (!isInEditorMode) {
		if (XR::currentSessionState == XR_SESSION_STATE_FOCUSED ||
			XR::currentSessionState == XR_SESSION_STATE_SYNCHRONIZED ||
			XR::currentSessionState == XR_SESSION_STATE_VISIBLE ||
			XR::currentSessionState == XR_SESSION_STATE_READY) {

			XR::OpenXRFrameInfo openxrFrameBeginInfo = XR::begin_frame();
			XR::collect_input(openxrFrameBeginInfo.predictedDisplayTime);
			update_player(openxrFrameBeginInfo);
			// Turns out you're actually supposed to call this right before render work, not right after xrWaitFrame
			// This means XR::begin_frame no longer calls xrBeginFrame, but I'm not sure what else to call it
			XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
			CHK_XR(XR::xrBeginFrame(XR::session, &frameBeginInfo));
			draw_frame(openxrFrameBeginInfo);
			XR::end_frame(openxrFrameBeginInfo);
		}
		XR::poll();
	} else {
		XR::OpenXRFrameInfo dummyFrameBeginInfo{};
		draw_frame(dummyFrameBeginInfo);
	}
	for (VKGeometry::SkeletalModel* model : skeletalModelsToRender) {
		model->poseMatrices = nullptr;
	}
}

U32 run_star_chicken() {
	if (Win32::timeBeginPeriod_ptr) {
		Win32::timeBeginPeriod_ptr(1);
	}
	MemoryArena& stackArena = get_scratch_arena();
	U64 stackArenaFrame0 = stackArena.stackPtr;

	ArenaArrayList<StrA> programArgs{ &stackArena };
	Win32::get_program_args(programArgs);
	for (StrA arg : programArgs) {
		if (arg == "--editor"a) {
			isInEditorMode = true;
		}
	}

	//MSDFGenerator::generate_msdf_image("..\\art\\test_smiley.svg"a, "..\\art\\output"a, 64, 64, 16.0F, 16.0F, 12.0F);
	//MSDFGenerator::generate_msdf_font("..\\art\\font.svg"a, "..\\art\\debug\\font_output"a, 64, 64, PX_TO_MILLIMETER(5.0F), PX_TO_MILLIMETER(12.0F), 12.0F);
	//return 0;

	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performanceCounter");
	}
	frameTime = prevFrameTime = U64(perfCounter.QuadPart);
	LARGE_INTEGER perfFreq;
	if (!QueryPerformanceFrequency(&perfFreq)) {
		abort("Could not get performance counter frequency");
	}
	timerFrequency = U64(perfFreq.QuadPart);

	shouldUseDesktopWindow = Win32::init(1920/2, 1080/2, do_frame, keyboard_callback, mouse_callback);
	if (isInEditorMode && !shouldUseDesktopWindow) {
		abort("Failed to initialize window for editor mode"a);
	}
	if (!isInEditorMode) {
		XR::init_openxr();
		VK::init_vulkan(!isInEditorMode);
		XR::create_gameplay_session();
		VK::create_render_targets_xr();
	} else {
		VK::init_vulkan(!isInEditorMode);
		VK::create_render_targets_editor();
	}
	VK::load_pipelines_and_descriptors();
	ResourceLoading::init_textures();
	Resources::load_resources();
	VK::finish_startup();

	UI::init_ui();
	if (isInEditorMode) {
		CubemapGen::init();
		EditorUI::init();
	}
	Win32::show_window();

	setup_scene();
	stackArena.stackPtr = stackArenaFrame0;
	staticModelsToRender.allocator = &frameArena;
	skeletalModelsToRender.allocator = &frameArena;

	while (!shouldShutDown) {
		if (shouldUseDesktopWindow) {
			Win32::poll_events();
			if (Win32::windowShouldClose) {
				shouldShutDown = true;
			}
		}
		do_frame();
	}

	if (!isInEditorMode) {
		print("Shutting down OpenXR...\n");
		XR::end_openxr();
	}
	
	print("Shutting down Vulkan...\n");
	CHK_VK(VK::vkDeviceWaitIdle(VK::logicalDevice));
	UI::destroy_ui();
	if (isInEditorMode) {
		CubemapGen::destroy();
	}
	VK::end_vulkan();

	if (Win32::timeEndPeriod_ptr) {
		Win32::timeEndPeriod_ptr(1);
	}
	Win32::destroy();
	print("Shutdown complete.\n");
	

	return EXIT_SUCCESS;
}

}