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
#include "Level.h"
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

void update_player(XR::OpenXRFrameInfo& openxrFrameInfo) {
	//playerRightHandModel.transform = XR::userInput.rightHand.pose;
	//playerRightHandModel.transform.add_offset(player.position).rotate_axis_angle(V3F32_NORTH, -0.25F).rotate_axis_angle(V3F32_UP, -0.07F);

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
	Level::level.update(F32(deltaTime));

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
		Level::level.prepare_render_transforms();

		DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
		EditorUI::debug_render();
		Rng1I32 tesWorldDebugDrawSet = tes.end_draw_set();
		for (EditorUI::PanelEditor3D* editor3d : EditorUI::renderPanels) {
			editor3d->debug_render();
			editor3d->ui3DDrawSet = tes.end_draw_set();
		};

		VkCommandBuffer cmdBuf = VK::graphicsCommandBuffer;

		{ // Compute skinning
			VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::skinningPipeline);
			for (Level::SkeletalModel* model : Level::level.skeletalModels) {
				VKGeometry::GPUSkinningPushData skinningPushData{
					.model = VKGeometry::GPUSkinnedModel{
						.matricesOffset = model->skeletonMatrixOffset,
						.vertexOffset = model->mesh->geometry.verticesOffset,
						.skinnedVerticesOffset = model->skinnedVerticesOffset,
						.skinningDataOffset = model->mesh->skinningDataOffset,
						.vertexCount = model->mesh->geometry.verticesCount
					},
					.drawDataUniformBuffer = VK::defaultDrawDescriptorSet.drawDataUniformBuffer
				};
				VK_PUSH_STRUCT(cmdBuf, skinningPushData, 0);
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

		VK::img_init_barrier(cmdBuf, VK::attachments.color.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		VK::img_init_barrier(cmdBuf, VK::attachments.objectId.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		VK::img_init_barrier(cmdBuf, VK::attachments.depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		VkRenderingInfoKHR renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
		renderingInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ VK::attachments.mainWidth, VK::attachments.mainHeight } };
		renderingInfo.layerCount = 1;
		renderingInfo.viewMask = isInEditorMode ? 0u : 0b11u; // Two eye rendering when in VR mode
		renderingInfo.colorAttachmentCount = 2;
		VkRenderingAttachmentInfo colorAttachments[2];
		VkRenderingAttachmentInfo& colorAttachment = colorAttachments[0];
		colorAttachment = VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		colorAttachment.imageView = VK::attachments.color.imgView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = VkClearColorValue{};
		VkRenderingAttachmentInfo& idAttachment = colorAttachments[1];
		idAttachment = VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		idAttachment.imageView = VK::attachments.objectId.imgView;
		idAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		idAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
		idAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		idAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		idAttachment.clearValue.color = VkClearColorValue{};
		renderingInfo.pColorAttachments = colorAttachments;
		VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		depthAttachment.imageView = VK::attachments.depth.imgView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue.depthStencil.depth = 0.0F;
		renderingInfo.pDepthAttachment = &depthAttachment;
		VK::vkCmdBeginRenderingKHR(cmdBuf, &renderingInfo);

		VK::vkCmdSetViewport(cmdBuf, 0, 1, ptr(VkViewport{
			.x = 0.0F,
			.y = 0.0F,
			.width = F32(VK::attachments.mainWidth),
			.height = F32(VK::attachments.mainHeight),
			.minDepth = 0.0F,
			.maxDepth = 1.0F
		}));
		VK::vkCmdSetScissor(cmdBuf, 0, 1, ptr(VkRect2D{ .offset = { 0, 0 }, .extent = { VK::attachments.mainWidth, VK::attachments.mainHeight } }));

		{ // Draw world geometry
			if (isInEditorMode) {
				U32 camIdx = 0;
				for (EditorUI::PanelEditor3D* editor3d : EditorUI::renderPanels) {
					if (rng_area(editor3d->viewport) == 0.0F) {
						editor3d->gpuCameraIndex = U32_MAX;
						continue;
					}
					if (camIdx < VK::uniformMatricesHandler.maxCameras) {
						editor3d->gpuCameraIndex = camIdx;
						VK::vkCmdSetViewport(cmdBuf, 0, 1, ptr(VkViewport{
							.x = editor3d->viewport.minX,
							.y = editor3d->viewport.minY,
							.width = editor3d->viewport.width(),
							.height = editor3d->viewport.height(),
							.minDepth = 0.0F,
							.maxDepth = 1.0F
						}));
						if (VK::hasCubemap) {
							// Fill in background
							VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::tmpBackgroundPipeline);
							VK::BackgroundPushData backgroundPushData{
								.drawSet = VK::defaultDrawDescriptorSet,
								.camIdx = editor3d->gpuCameraIndex
							};
							VK_PUSH_STRUCT(cmdBuf, backgroundPushData, 0);
							VK::vkCmdDraw(cmdBuf, 3, 1, 0, 0);
						}
						VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::drawPipeline);
						VK::vkCmdBindIndexBuffer(cmdBuf, VK::geometryHandler.buffer, VK::geometryHandler.indicesOffset, VK_INDEX_TYPE_UINT16);
						Level::level.draw_models(cmdBuf, camIdx);
						tes.draw(tesWorldDebugDrawSet, camIdx);
						tes.draw(editor3d->ui3DDrawSet, camIdx);
					} else {
						editor3d->gpuCameraIndex = U32_MAX;
					}
					camIdx++;
				}
			} else {
				if (VK::hasCubemap) {
					// Fill in background
					VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::tmpBackgroundPipeline);
					VK::BackgroundPushData backgroundPushData{
						.drawSet = VK::defaultDrawDescriptorSet,
						.camIdx = 0
					};
					VK_PUSH_STRUCT(cmdBuf, backgroundPushData, 0);
					VK::vkCmdDraw(cmdBuf, 3, 1, 0, 0);
				}
				VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::drawPipeline);
				VK::vkCmdBindIndexBuffer(cmdBuf, VK::geometryHandler.buffer, VK::geometryHandler.indicesOffset, VK_INDEX_TYPE_UINT16);
				Level::level.draw_models(cmdBuf, 0);
				tes.draw(tesWorldDebugDrawSet, 0);
			}
		}

		VK::vkCmdEndRenderingKHR(cmdBuf); // world render pass

		if (!isInEditorMode) { // Final composite (XR)
			VK::img_barrier(cmdBuf, VK::attachments.color.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			VK::img_init_barrier(cmdBuf, VK::attachments.composite.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::finalCompositePipeline);
			VK::FinalCompositePushData compositeConstants{};
			compositeConstants.sceneColor = VK::attachments.color.descriptorIdx;
			compositeConstants.sceneIds = VK::attachments.objectId.descriptorIdx;
			compositeConstants.uiColor = VK::attachments.uiColor.descriptorIdx;
			compositeConstants.compositeOutput = VK::attachments.compositeLinearDescriptor;
			compositeConstants.activeObjectId = Level::level.activeObject ? Level::level.activeObject->id : Level::INVALID_LEVEL_OBJECT_ID;
			compositeConstants.outputDimensions = V2U{ VK::attachments.mainWidth, VK::attachments.mainHeight };
			VK_PUSH_STRUCT(cmdBuf, compositeConstants, 0);
			VK::vkCmdDispatch(cmdBuf, (VK::attachments.mainWidth + 15) / 16, (VK::attachments.mainHeight + 15) / 16, 1);
		}

		// 2D UI render pass
		if (isInEditorMode) {
			VK::img_init_barrier(cmdBuf, VK::attachments.uiColor.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
		} else {
			// The UI image will be presented, so we blit a VR view into it as a background image
			VK::img_barrier(cmdBuf, VK::attachments.composite.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			VK::img_init_barrier(cmdBuf, VK::attachments.uiColor.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
			// Try to fit XR dimensions to desktop dimensions
			V2F middle{ F32(VK::attachments.mainWidth) * 0.5F, F32(VK::attachments.mainHeight) * 0.5F };
			V2F blitSourceExtent = V2F{ F32(VK::attachments.uiWidth) * 0.5F, F32(VK::attachments.uiHeight) * 0.5F };
			blitSourceExtent *= min(middle.x / blitSourceExtent.x, middle.y / blitSourceExtent.y);
			VK::img_blit2d(cmdBuf,
						   VK::attachments.composite.img, Rng2I32{ max(I32(middle.x - blitSourceExtent.x), 0), max(I32(middle.y - blitSourceExtent.y), 0), min(I32(middle.x + blitSourceExtent.x), I32(VK::attachments.mainWidth)), min(I32(middle.y + blitSourceExtent.y), I32(VK::attachments.mainHeight)) },
						   VK::attachments.uiColor.img, Rng2I32{ 0, 0, I32(VK::attachments.uiWidth), I32(VK::attachments.uiHeight) },
						   1, VK_FILTER_NEAREST, VK_IMAGE_ASPECT_COLOR_BIT);
			VK::img_barrier(cmdBuf, VK::attachments.uiColor.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
		}
		VK::img_init_barrier(cmdBuf, VK::attachments.uiDepth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		renderingInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ VK::attachments.uiWidth, VK::attachments.uiHeight } };
		renderingInfo.layerCount = 1;
		renderingInfo.viewMask = 0u;
		renderingInfo.colorAttachmentCount = 1;
		VkRenderingAttachmentInfo uiColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		uiColorAttachment.imageView = VK::attachments.uiColor.imgView;
		uiColorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		uiColorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
		uiColorAttachment.loadOp = isInEditorMode ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		uiColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		uiColorAttachment.clearValue.color = VkClearColorValue{};
		renderingInfo.pColorAttachments = &uiColorAttachment;
		VkRenderingAttachmentInfo uiDepthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		uiDepthAttachment.imageView = VK::attachments.uiDepth.imgView;
		uiDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		uiDepthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
		uiDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		uiDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		uiDepthAttachment.clearValue.depthStencil.depth = 0.0F;
		renderingInfo.pDepthAttachment = &uiDepthAttachment;
		VK::vkCmdBeginRenderingKHR(cmdBuf, &renderingInfo);
		
		VK::vkCmdSetViewport(cmdBuf, 0, 1, ptr(VkViewport{
			.x = 0.0F,
			.y = 0.0F,
			.width = F32(VK::attachments.uiWidth),
			.height = F32(VK::attachments.uiHeight),
			.minDepth = 0.0F,
			.maxDepth = 1.0F
		}));
		VK::vkCmdSetScissor(cmdBuf, 0, 1, ptr(VkRect2D{ .offset = { 0, 0 }, .extent = { VK::attachments.uiWidth, VK::attachments.uiHeight } }));
		UI::draw();
		Rng1I32 uiTessellatorPass = tes.end_draw_set();
		tes.draw(uiTessellatorPass, 0);

		VK::vkCmdEndRenderingKHR(cmdBuf); // 2D UI render pass

		tes.end_frame();

		if (isInEditorMode) { // Final composite (editor)
			VK::img_barrier(cmdBuf, VK::attachments.color.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			VK::img_barrier(cmdBuf, VK::attachments.objectId.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			VK::img_barrier(cmdBuf, VK::attachments.uiColor.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::finalCompositePipeline);
			VK::FinalCompositePushData compositeConstants{};
			compositeConstants.sceneColor = VK::attachments.color.descriptorIdx;
			compositeConstants.sceneIds = VK::attachments.objectId.descriptorIdx;
			compositeConstants.uiColor = VK::attachments.uiColor.descriptorIdx;
			compositeConstants.compositeOutput = VK::attachments.uiColorLinearDescriptor;
			compositeConstants.activeObjectId = Level::level.activeObject ? Level::level.activeObject->id : Level::INVALID_LEVEL_OBJECT_ID;
			compositeConstants.outputDimensions = V2U{ VK::attachments.uiWidth, VK::attachments.uiHeight };
			VK_PUSH_STRUCT(cmdBuf, compositeConstants, 0);
			VK::vkCmdDispatch(cmdBuf, (VK::attachments.uiWidth + 15) / 16, (VK::attachments.uiHeight + 15) / 16, 1);
		}

		if (isInEditorMode) { // Object ID readback
			VK::img_barrier(cmdBuf, VK::attachments.objectId.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0; // Tightly packed
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = VkOffset3D{ 0, 0, 0 };
			region.imageExtent = VkExtent3D{ VK::attachments.mainWidth, VK::attachments.mainHeight, 1 };
			VK::vkCmdCopyImageToBuffer(cmdBuf, VK::attachments.objectId.img, VK_IMAGE_LAYOUT_GENERAL, VK::attachments.objectIdReadbackBuffer.buffer, 1, &region);
			VK::buffer_barrier(cmdBuf, VK::attachments.objectIdReadbackBuffer.buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		}
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
		for (EditorUI::PanelEditor3D* editor3d : EditorUI::renderPanels) {
			if (editor3d->gpuCameraIndex != U32_MAX) {
				V3F camPos = editor3d->editor.get_render_eye_pos();
				M4x3F32 view = editor3d->editor.get_view_transform();
				VK::uniformMatricesHandler.set_camera(editor3d->gpuCameraIndex, view, editor3d->projection, camPos);
			}
		}
		
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
		EditorUI::focusedPanel = nullptr;
	}
	EditorUI::key_input(key, state);
}
void mouse_callback(Win32::MouseButton button, Win32::MouseValue state) {
	if (UI::inDialog) {
		return;
	}
	V2F mousePos = Win32::get_mouse();
	if (!EditorUI::focusedPanel) {
		UI::handle_mouse_action(mousePos, button, state);
	}
	if (EditorUI::focusedPanel) {
		UI::activeBox = UI::hotBox = UI::BoxHandle{};
	}
	EditorUI::mouse_input(button, state, mousePos);
}

void do_frame() {
	swap(&frameArena, &lastFrameArena);
	frameArena.reset();
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
#ifdef SC_FORCE_EDITOR_MODE
	// This is so that the DebugEditor configuration worked when other machines pull from git
	// Run configuration command line arguments are stored in vcxproj.user for some reason, so they don't get pushed to github
	isInEditorMode = true;
#endif

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
		VK::load_pipelines_and_descriptors();
		VK::create_render_targets(XR::xrRenderWidth, XR::xrRenderHeight, 2);
	} else {
		VK::init_vulkan(!isInEditorMode);
		VK::load_pipelines_and_descriptors();
		VK::create_render_targets(U32(Win32::framebufferWidth), U32(Win32::framebufferHeight), 1);
	}
	ResourceLoading::init_textures();
	ResourceLoading::init_materials();
	Resources::load_resources();
	VK::finish_startup();

	UI::init_ui();
	if (isInEditorMode) {
		CubemapGen::init();
		EditorUI::init();
	}
	Win32::show_window();

	Level::build_test_level();
	stackArena.stackPtr = stackArenaFrame0;

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