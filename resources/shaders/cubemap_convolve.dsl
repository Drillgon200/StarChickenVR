#version 2
#shader compute

[set 0, binding 0, uniform] &Image2DStorageRGBA32F inputImage;
[set 0, binding 1, uniform] &ImageCubeStorageRGBA32F outputImage;

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	V2U inputDimensions;
	V2U outputDimensions;
} pushConstants;


@[V4F color][V3F direction] sample_equirect{
	U32 y{ U32((direction.y + 1.0) * 0.5 * F32(pushConstants.inputDimensions.y)) };
	V2F horizontal{ normalize(direction.xz) };
	U32 x{ (atan2(horizontal.y, horizontal.x) * 0.31830988618 + 1.0) * 0.5 * F32(pushConstants.inputDimensions.x) };
	V2U coord{ clamp(V2U(x, y), V2U(0u, 0u), pushConstants.inputDimensions - V2U(1u, 1u)) };
	return (^inputImage)[coord];
};

@[F32 val][F32 phi, F32 theta, V3F dir] equirect_antiderivative{
	F32 st{ sin(theta) };
	return (dir.x * sin(phi) - dir.z * cos(phi)) * (sin(2.0 * theta) * 0.25 + theta * 0.5) + dir.y * phi * st * st * 0.5;
};

[entrypoint, localsize 16 16 1] @[][] compute_main{
	V2U outputCoord{ globalInvocationId.xy };
	U32 faceIdx{ globalInvocationId.z };
	V2U inputDim{ pushConstants.inputDimensions };
	V2U outputDim{ pushConstants.outputDimensions };
	if outputCoord.x >= outputDim.x || outputCoord.y >= outputDim.y {
		return;
	};

	V2F faceDir{ (V2F(outputCoord) + 0.5) / V2F(outputDim) * 2.0 - 1.0 };
	V3F dir{ 
		if faceIdx == 0u { V3F(1.0, faceDir.yx) } // +X
		else if faceIdx == 1u { V3F(-1.0, faceDir.y, -faceDir.x) } // -X
		else if faceIdx == 2u { V3F(faceDir.x, 1.0, faceDir.y) } // +Y
		else if faceIdx == 3u { V3F(faceDir.x, -1.0, -faceDir.y) } // -Y
		else if faceIdx == 4u { V3F(-faceDir.x, faceDir.y, 1.0) } // +Z
		else { V3F(faceDir.x, faceDir.y, -1.0) } // -Z
	};
	dir = normalize(dir);

	V3F color{ 0.0, 0.0, 0.0 };
	V2F angleStep{ 2.0 * 3.1415926535 / F32(inputDim.x), 3.1415926535 / F32(inputDim.y) };
	for U32 y{ 0u }; y < inputDim.y; y = y + 1u {
		for U32 x{ 0u }; x < inputDim.x; x = x + 1u {
			V2F angles{ angleStep.x * F32(x), angleStep.y * F32(y) - 1.570796327 };
			V2F anglesEnd{ angles + angleStep };
			V3F integratedColor{ (^inputImage)[V2U(x, y)].xyz };
			// Analytical integral of int(angles.y, anglesEnd.y) int(angles.x, anglesEnd.x) dot(dir, V3F(cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi))) * cos(theta) dphi dtheta
			//F32 weight{
			//	equirect_antiderivative(anglesEnd.x, anglesEnd.y, dir) -
			//	equirect_antiderivative(angles.x,    anglesEnd.y, dir) -
			//	equirect_antiderivative(anglesEnd.x, angles.y,    dir) +
			//	equirect_antiderivative(angles.x,    angles.y,    dir)
			//};
			F32 phi{ angles.x + angleStep.x * 0.5 };
			F32 theta{ angles.y + angleStep.y * 0.5 };
			F32 weight{ max(0.0, dot(dir, V3F(cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi)))) * angleStep.x * angleStep.y * cos(theta) / (2.0 * 3.1415926535) };
			color = color + integratedColor * weight;
		};
	};
	write_image(^outputImage, V3U(outputCoord, faceIdx), V4F(color, 1.0));
};