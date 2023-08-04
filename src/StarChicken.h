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

void draw_frame() {
	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performance counter");
	}
	prevFrameTime = frameTime;
	frameTime = perfCounter.QuadPart;
	deltaTime = f64(frameTime - prevFrameTime) / f64(timerFrequency);
	totalTime += deltaTime;
	//println_float(totalTime);

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

		ProjectiveTransformMatrix leftPerspectiveTransform;
		leftPerspectiveTransform.generate(leftProjection, leftTransform);
		ProjectiveTransformMatrix rightPerspectiveTransform;
		rightPerspectiveTransform.generate(rightProjection, rightTransform);

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = XR::xrRenderWidth;
		viewport.height = XR::xrRenderHeight;
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VK::vkCmdSetViewport(VK::graphicsCommandBuffer, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = XR::xrRenderWidth;
		scissor.extent.height = XR::xrRenderHeight;
		VK::vkCmdSetScissor(VK::graphicsCommandBuffer, 0, 1, &scissor);
		VK::vkCmdBindPipeline(VK::graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VK::testPipeline);

		VK::PushConstantMatrices testMatrices{};
		testMatrices.leftMatrixRow0 = Vector4f{ leftPerspectiveTransform.m00, leftPerspectiveTransform.m01, leftPerspectiveTransform.m02, leftPerspectiveTransform.m03 };
		testMatrices.leftMatrixRow1 = Vector4f{ leftPerspectiveTransform.m10, leftPerspectiveTransform.m11, leftPerspectiveTransform.m12, leftPerspectiveTransform.m13 };
		testMatrices.leftMatrixRow3 = Vector4f{ leftPerspectiveTransform.m30, leftPerspectiveTransform.m31, leftPerspectiveTransform.m32, leftPerspectiveTransform.m33 };
		testMatrices.rightMatrixRow0 = Vector4f{ rightPerspectiveTransform.m00, rightPerspectiveTransform.m01, rightPerspectiveTransform.m02, rightPerspectiveTransform.m03 };
		testMatrices.rightMatrixRow1 = Vector4f{ rightPerspectiveTransform.m10, rightPerspectiveTransform.m11, rightPerspectiveTransform.m12, rightPerspectiveTransform.m13 };
		testMatrices.rightMatrixRow3 = Vector4f{ rightPerspectiveTransform.m30, rightPerspectiveTransform.m31, rightPerspectiveTransform.m32, rightPerspectiveTransform.m33 };
		VK::vkCmdPushConstants(VK::graphicsCommandBuffer, VK::testPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::PushConstantMatrices), &testMatrices);
		
		VKGeometry::Mesh& mesh = VK::testMesh;
		VkBuffer buffers[4]{
			mesh.gpuGeometry.buffer,
			mesh.gpuGeometry.buffer,
			mesh.gpuGeometry.buffer,
			mesh.gpuGeometry.buffer
		};
		VkDeviceSize offsets[4]{
			mesh.gpuGeometry.offset + mesh.positionsOffset,
			mesh.gpuGeometry.offset + mesh.texcoordsOffset,
			mesh.gpuGeometry.offset + mesh.normalsOffset,
			mesh.gpuGeometry.offset + mesh.tangentsOffset
		};
		VK::vkCmdBindVertexBuffers(VK::graphicsCommandBuffer, 0, 4, buffers, offsets);
		VK::vkCmdBindIndexBuffer(VK::graphicsCommandBuffer, mesh.gpuGeometry.buffer, mesh.gpuGeometry.offset + mesh.indicesOffset, VK_INDEX_TYPE_UINT16);
		VK::vkCmdDrawIndexed(VK::graphicsCommandBuffer, VK::testMesh.indexCount, 1, 0, 0, 0);
		
	}
	XrSwapchainImageWaitInfo swapchainWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	swapchainWaitInfo.timeout = XR_INFINITE_DURATION;
	CHK_XR(XR::xrWaitSwapchainImage(XR::xrSwapchain, &swapchainWaitInfo));
	VK::end_frame();
	XR::end_frame(openxrFrameBeginInfo);
	stackArena.pop_frame();
}

int run_star_chicken() {
	//HMODULE user32 = LoadLibraryA("user32.dll");
	//MessageBoxA2 = reinterpret_cast<MessageBoxA2_t>(GetProcAddress(user32, "MessageBoxA"));
	//MessageBoxA2(0, "Test bpx", "Test box caption", MB_OK | MB_ICONINFORMATION);

	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performanceCounter");
	}
	frameTime = prevFrameTime = perfCounter.QuadPart;
	LARGE_INTEGER perfFreq;
	if (!QueryPerformanceFrequency(&perfFreq)) {
		abort("Could not get performance counter frequency");
	}
	timerFrequency = perfFreq.QuadPart;

	stackArena.push_frame();
	XR::init_openxr();
	VK::init_vulkan();
	XR::create_session_and_vk_swapchain();
	VK::create_render_targets();
	VK::load_resources();
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