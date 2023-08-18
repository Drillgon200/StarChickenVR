#pragma once
#include "StarChicken_decl.h"
#include "DrillMath.h"
#include "VK.h"
#include "XR.h"
namespace StarChicken {

b32 shouldShutDown;

//typedef WINUSERAPI
//int
//(WINAPI *MessageBoxA2_t)(
//    _In_opt_ HWND hWnd,
//    _In_opt_ LPCSTR lpText,
//    _In_opt_ LPCSTR lpCaption,
//    _In_ UINT uType);
//MessageBoxA2_t MessageBoxA2;

u64 prevFrameTime;
u64 frameTime;
u64 timerFrequency;
f64 deltaTime;
f64 totalTime;

VKGeometry::StaticModel worldModel;
VKGeometry::SkeletalModel handModel;

void setup_scene() {
	VKGeometry::make_static_model(&worldModel, VK::testMesh);
	worldModel.transform.translate(-Vector3f{ 0.0F, 8.5F, 12.35F });
	VKGeometry::make_skeletal_model(&handModel, VK::testAnimMesh);
	handModel.transform.translate(-Vector3f{ -3.0F, 5.0F, 12.35F });
}

void draw_frame() {
	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performance counter");
	}
	prevFrameTime = frameTime;
	frameTime = u64(perfCounter.QuadPart);
	deltaTime = f64(frameTime - prevFrameTime) / f64(timerFrequency);
	totalTime += deltaTime;

	stackArena.push_frame();
	XR::OpenXRFrameInfo openxrFrameBeginInfo = XR::begin_frame();
	VK::begin_frame();
	if (openxrFrameBeginInfo.shouldRender) {
		PerspectiveProjection leftProjection{};
		leftProjection.project_perspective(PROJECTION_NEAR_PLANE,
			RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleRight),
			RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleLeft),
			RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleUp),
			RAD_TO_TURN(openxrFrameBeginInfo.leftEyeFov.angleDown));
		Matrix4x3f leftTransform;
		leftTransform.set_identity();
		leftTransform.set_rotation_from_quat(XR::xr_quat_to_drillmath_quat(openxrFrameBeginInfo.leftEyePose.orientation).conjugate());
		leftTransform.translate(-XR::xr_vec3_to_drillmath_vec3(openxrFrameBeginInfo.leftEyePose.position));
		PerspectiveProjection rightProjection{};
		rightProjection.project_perspective(PROJECTION_NEAR_PLANE,
			RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleRight),
			RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleLeft),
			RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleUp),
			RAD_TO_TURN(openxrFrameBeginInfo.rightEyeFov.angleDown));
		Matrix4x3f rightTransform;
		rightTransform.set_identity();
		rightTransform.set_rotation_from_quat(XR::xr_quat_to_drillmath_quat(openxrFrameBeginInfo.rightEyePose.orientation).conjugate());
		rightTransform.translate(-XR::xr_vec3_to_drillmath_vec3(openxrFrameBeginInfo.rightEyePose.position));

		u32 boneCount = handModel.mesh->skeletonData->boneCount;
		Matrix4x3f* matrices = stackArena.alloc<Matrix4x3f>(boneCount);
		for (u32 i = 0; i < boneCount; i++) {
			matrices[i].set_identity();
		}
		matrices[1].translate(Vector3f{ sinf(totalTime * 0.75F + 0.5F), sinf(totalTime * 0.25F) * 0.75F, sinf(totalTime * 0.75F + 0.25F) });
		handModel.skeletonMatrixOffset = VK::skinningHandler.alloc_and_set(boneCount, matrices);
		VK::skinningHandler.flush_memory();
		handModel.skinnedVerticesOffset = VK::geometryHandler.alloc_skinned_result(handModel.mesh->geometry.verticesCount);


		VkCommandBuffer cmdBuf = VK::graphicsCommandBuffer;

		VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::skinningPipeline);
		VK::vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VK::skinningPipelineLayout, 0, 1, &VK::skinningDescriptorSet, 0, nullptr);
		VKGeometry::GPUSkinnedModel skinnedHand;
		skinnedHand.matricesOffset = handModel.skeletonMatrixOffset;
		skinnedHand.vertexOffset = handModel.mesh->geometry.verticesOffset;
		skinnedHand.skinningVertexOffset = handModel.skinnedVerticesOffset;
		skinnedHand.vertexCount = handModel.mesh->geometry.verticesCount;
		VK::vkCmdPushConstants(cmdBuf, VK::skinningPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VKGeometry::GPUSkinnedModel), &skinnedHand);
		VK::vkCmdDispatch(cmdBuf, (VK::testMesh.verticesCount + 255) / 256, 1, 1);

		VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		bufferBarrier.srcQueueFamilyIndex = VK::graphicsFamily;
		bufferBarrier.dstQueueFamilyIndex = VK::graphicsFamily;
		bufferBarrier.buffer = VK::geometryHandler.buffer;
		bufferBarrier.offset = VK::geometryHandler.skinnedPositionsOffset;
		bufferBarrier.size = VK_WHOLE_SIZE;
		VK::vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);


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
		viewport.width = f32(XR::xrRenderWidth);
		viewport.height = f32(XR::xrRenderHeight);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VK::vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = XR::xrRenderWidth;
		scissor.extent.height = XR::xrRenderHeight;
		VK::vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
		VK::vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::testPipeline);

		VK::PushConstantMatrices testMatrices{};
		ProjectiveTransformMatrix leftPerspectiveTransform;
		ProjectiveTransformMatrix rightPerspectiveTransform;
		
		VkBuffer geoBuffer = VK::geometryHandler.buffer;
		VkBuffer buffers[4]{
			geoBuffer,
			geoBuffer,
			geoBuffer,
			geoBuffer
		};
		VkDeviceSize offsets[4]{
			VK::geometryHandler.positionsOffset,
			VK::geometryHandler.texcoordsOffset,
			VK::geometryHandler.normalsOffset,
			VK::geometryHandler.tangentsOffset
		};
		VK::vkCmdBindVertexBuffers(cmdBuf, 0, 4, buffers, offsets);
		VK::vkCmdBindIndexBuffer(cmdBuf, geoBuffer, VK::geometryHandler.indicesOffset, VK_INDEX_TYPE_UINT16);

		leftPerspectiveTransform.generate(leftProjection, leftTransform * worldModel.transform);
		rightPerspectiveTransform.generate(rightProjection, rightTransform * worldModel.transform);
		testMatrices.set_eye_transforms(leftPerspectiveTransform, rightPerspectiveTransform);
		VK::vkCmdPushConstants(cmdBuf, VK::testPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::PushConstantMatrices), &testMatrices);
		VK::vkCmdDrawIndexed(cmdBuf, worldModel.mesh->indicesCount, 1, worldModel.mesh->indicesOffset, i32(worldModel.mesh->verticesOffset), 0);

		offsets[0] = VK::geometryHandler.skinnedPositionsOffset;
		offsets[2] = VK::geometryHandler.skinnedNormalsOffset;
		offsets[3] = VK::geometryHandler.skinnedTangentsOffset;
		VK::vkCmdBindVertexBuffers(cmdBuf, 0, 4, buffers, offsets);

		Matrix4x3f handTransform = handModel.transform;
		leftPerspectiveTransform.generate(leftProjection, leftTransform * handTransform);
		rightPerspectiveTransform.generate(rightProjection, rightTransform * handTransform);
		testMatrices.set_eye_transforms(leftPerspectiveTransform, rightPerspectiveTransform);
		VK::vkCmdPushConstants(cmdBuf, VK::testPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::PushConstantMatrices), &testMatrices);
		VK::vkCmdDrawIndexed(cmdBuf, handModel.mesh->geometry.indicesCount, 1, handModel.mesh->geometry.indicesOffset, i32(handModel.skinnedVerticesOffset), 0);

		VK::vkCmdEndRenderPass(cmdBuf);
	}
	XrSwapchainImageWaitInfo swapchainWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	swapchainWaitInfo.timeout = XR_INFINITE_DURATION;
	CHK_XR(XR::xrWaitSwapchainImage(XR::xrSwapchain, &swapchainWaitInfo));
	VK::end_frame(openxrFrameBeginInfo.shouldRender);
	XR::end_frame(openxrFrameBeginInfo);
	stackArena.pop_frame();
}

u32 run_star_chicken() {
	//HMODULE user32 = LoadLibraryA("user32.dll");
	//MessageBoxA2 = reinterpret_cast<MessageBoxA2_t>(GetProcAddress(user32, "MessageBoxA"));
	//MessageBoxA2(0, "Test bpx", "Test box caption", MB_OK | MB_ICONINFORMATION);

	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performanceCounter");
	}
	frameTime = prevFrameTime = u64(perfCounter.QuadPart);
	LARGE_INTEGER perfFreq;
	if (!QueryPerformanceFrequency(&perfFreq)) {
		abort("Could not get performance counter frequency");
	}
	timerFrequency = u64(perfFreq.QuadPart);

	stackArena.push_frame();
	XR::init_openxr();
	VK::init_vulkan();
	XR::create_session_and_vk_swapchain();
	VK::create_render_targets();
	VK::load_resources();
	setup_scene();
	stackArena.pop_frame();

	while (!shouldShutDown) {
		frameArena.push_frame();
		if (XR::currentSessionState == XR_SESSION_STATE_FOCUSED ||
			XR::currentSessionState == XR_SESSION_STATE_SYNCHRONIZED ||
			XR::currentSessionState == XR_SESSION_STATE_VISIBLE ||
			XR::currentSessionState == XR_SESSION_STATE_READY) {
			draw_frame();
		}
		XR::poll();
		frameArena.pop_frame();
	}

	print("Shutting down...\n");
	VK::end_vulkan();
	XR::end_openxr();
	return EXIT_SUCCESS;
}

}