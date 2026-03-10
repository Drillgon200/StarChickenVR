#pragma once

#pragma once

#include "../src/ShaderCompiler.h"
#include "Testing.h"

namespace ShaderCompilerTests {

void run_shader_compile_test(StrA shaderCode) {
	U32 numCompiledDwords = 0;
	F64 timeTaken = current_time_seconds();
	SPIRV::SpvDword* result = ShaderCompiler::compile_dsl(&numCompiledDwords, shaderCode, "test_code.dsl"a, ""a);
	timeTaken = current_time_seconds() - timeTaken;
	TEST_EXPECT(result);
	U32 validationExitCode = U32_MAX;
	if (result) {
		printf("Compilation succeeded, took % ms\n"a, timeTaken * 1000.0);
		CreateDirectoryA(".\\spv_test_output", NULL);
		write_data_to_file(".\\spv_test_output\\dsl2.spv"a, result, numCompiledDwords * sizeof(SPIRV::SpvDword));
		U32 exitCode = 0;
		if (run_program_and_wait(&exitCode, "spirv-val.exe"a, "--target-env vulkan1.2 --scalar-block-layout .\\spv_test_output\\dsl2.spv"a)) {
			validationExitCode = exitCode;
		} else {
			print("SPIR-V Validator not found, compiler result not validated\n"a);
		}
	} else {
		printf("Compilation failed, took % ms\n"a, timeTaken * 1000.0);
	}
	TEST_EXPECT(validationExitCode == 0);
}

void solo_test() {
	run_shader_compile_test(R"(
#version 2
#shader vertex
#extension multiview

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
};

[set 0, binding 1, uniform_buffer, restrict, nonwritable, block] &struct {
	V2F screenDimensions;
	&V4F uiClipBoxes;
	V2U pUIVertices;
	&M4x3F matrices;
	&V3F positions;
	&V2F texcoords;
	&V3F normals;
	&V3F tangents;
	&U32 boneIndicesAndWeights;
	&V3F skinnedPositions;
	&V3F skinnedNormals;
	&V3F skinnedTangents;
} drawData;
[push_constant, block] &struct {
	U32 transformIdx;
	I32 verticesOffset;
} modelData;

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[output, builtin Position] &V4F outPosition;
[output, location 0] &V3F passPos;
[output, location 1] &V3F passNormal;

[entrypoint] @[][] vert_main{
	I32 vertIdx{ ^inVertexIndex + modelData.verticesOffset };
	V4F pos{ 0.0 };
	V3F norm{ 0.0 };
	if vertIdx > 0 {
		vertIdx = vertIdx - 1;
		pos = V4F(drawData.positions[vertIdx], 1.0);
		norm = drawData.normals[vertIdx];
	} else {
		vertIdx = vertIdx - 1;
		pos = V4F(drawData.positions[vertIdx], 1.0);
		norm = drawData.normals[vertIdx];
		/*
		vertIdx = -vertIdx - 1;
		pos = V4F(drawData.skinnedPositions[vertIdx], 1.0);
		norm = drawData.skinnedNormals[vertIdx];
		*/
	};
	I32 viewIdx{ ^inViewIndex };
	M4x3F modelMat{ drawData.matrices[3/*modelData.transformIdx*/] };
	V4F transformedPos{
		dot(pos, modelMat.row0),
		dot(pos, modelMat.row1),
		dot(pos, modelMat.row2),
		1.0F
	};
	V3F transformedNorm{
		dot(norm, modelMat.row0.xyz),
		dot(norm, modelMat.row1.xyz),
		dot(norm, modelMat.row2.xyz)
	};
	transformedPos = pos;
	M4x3F viewProj{ drawData.matrices[/*viewIdx + */1] };
	F32 x{ dot(transformedPos, viewProj.row0) };
	F32 y{ dot(transformedPos, viewProj.row1) };
	F32 z{ 0.05F }; // Near plane
	// row2 is actually row3 in  this case, since the matrix is a ProjectiveTransformMatrix
	F32 w{ dot(transformedPos, viewProj.row2) };
	^outPosition = V4F(x, -y, z, w);
	^passPos = transformedPos.xyz;
	^passNormal = transformedNorm;
};

//---------------------------------------------------------//
#shader fragment

[output, location 0] &V4F outFragColor;
[input, location 0] &V3F passPosition;
[input, location 1] &V3F passNormal;

[entrypoint] @[][] frag_main{
	//^outFragColor = V4F(1.0, 0.0, 0.0, 1.0);
	V3F lightDir{ 0.57735026919F, 0.57735026919F, 0.57735026919F };
	^outFragColor = V4F(V3F(dot(normalize(^passNormal), lightDir) * 0.5F + 0.5F), 1.0F);
};
)"a);
}




















void general_tests() {
	run_shader_compile_test(R"(
#version 2
#shader vertex

[output, builtin Position] &V4F position;

[entrypoint] @[][] vert_main {
	^position = V4F(1.0F, 1.0F, 1.0F, 1.0F);
};


#shader fragment

[output, location 0] &V4F outColor;

[entrypoint] @[][] frag_main {
	^outColor = V4F(1.0F, 0.5F, 0.25F, 1.0F);
};

)"a);

	run_shader_compile_test(R"(
#version 2
#shader vertex

[output, builtin Position] &V4F position;

[entrypoint] @[][] vert_main {
	^position = V4F(1.0F, 1.0F, 1.0F, 1.0F);
};

#shader fragment

[output, location 0] &V4F outColor;

[entrypoint] @[][] frag_main {
	F32 a{ 1.0F };
	F32 b{ 2.0F };
	if b == 2.0F {
		F32 c{ 5.0F };
		b = 7.0F;
		a = 9.0F;
	} else {
		F32 d{ 6.0F };
		a = 8.0F;
	};
	^outColor = V4F(1.0F, 0.5F, 0.25F, 1.0F);
};
)"a);

	run_shader_compile_test(R"(
#version 2
#shader vertex

[output, builtin Position] &V4F position;

[entrypoint] @[][] vert_main {
	F32 var{ 0.0F };
	var = var + 1.0F;
	var = if var == 2.0F { 2.0F } else { var + 10.0F };
	^position = V4F(1.0F, 1.0F, var, 1.0F);
};


#shader fragment

[output, location 0] &V4F outColor;

[entrypoint] @[][] frag_main {
	^outColor = V4F(1.0F, 0.5F, 0.25F, 1.0F);
};

)"a);
	
	run_shader_compile_test(R"(
#version 2
#shader vertex

[output, builtin Position] &V4F position;

[entrypoint] @[][] vert_main {
	^position = V4F(1.0F, 1.0F, 1.0F, 1.0F);
};


#shader fragment

[output, location 0] &V4F outColor;

[entrypoint] @[][] frag_main {
	^outColor = V4F(1.0F, 0.5F, 0.25F, median(0.2F, 0.1F, 0.3F));
};

@[F32 ret][F32 a, F32 b, F32 c] median{
	return max(min(a, b), min(max(a, b), c));
};
)"a);

	run_shader_compile_test(R"(
#version 2
#shader vertex
#extension multiview

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
};

[block] struct PushConstantModel {
	U32 transformIndex;
};

[block] struct TransformMatrices {
	M4x3F[] matrices;
};

[input, builtin VertexIndex] &I32 inVertexIndex;
[input, builtin ViewIndex] &I32 inViewIndex;

[input, location 0] &V3F inPosition;
[input, location 1] &V2F inTexcoord;
[input, location 2] &V3F inNormal;
[input, location 3] &V3F inTangent;

[output, builtin Position] &V4F outPosition;
[output, location 0] &V3F passPos;
[output, location 1] &V3F passNormal;

[set 0, binding 0, restrict, nonwritable, storage_buffer] &TransformMatrices transformMatrices;
[push_constant] &PushConstantModel modelInfo;

[entrypoint] @[][] vert_main{
	V4F pos{ ^inPosition, 1.0F };
	V3F norm{ ^inNormal };
	I32 viewIdx{ ^inViewIndex };
	U32 transformIndex{ modelInfo.transformIndex };
	M4x3F modelMat{ transformMatrices.matrices[transformIndex] };
	V4F transformedPos{
		dot(pos, modelMat.row0),
		dot(pos, modelMat.row1),
		dot(pos, modelMat.row2),
		1.0F
	};
	V3F transformedNorm{
		dot(norm, modelMat.row0.xyz),
		dot(norm, modelMat.row1.xyz),
		dot(norm, modelMat.row2.xyz)
	};
	F32 x{ dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row0) };
	F32 y{ dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row1) };
	F32 z{ 0.05F }; // Near plane
	// row2 is actually row3 in  this case, since the matrix is a ProjectiveTransformMatrix
	F32 w{ dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row2) };
	^outPosition = V4F(x, -y, z, w);
	^passPos = transformedPos.xyz;
	^passNormal = transformedNorm;
};

//---------------------------------------------------------//
#shader fragment

[output, location 0] &V4F outFragColor;
[input, location 0] &V3F passPosition;
[input, location 1] &V3F passNormal;

[entrypoint] @[][] frag_main{
	V3F lightDir{ 0.57735026919F, 0.57735026919F, 0.57735026919F };
	^outFragColor = V4F(V3F(dot(normalize(^passNormal), lightDir) * 0.5F + 0.5F), 1.0F);
};
)"a);

	run_shader_compile_test(R"(
#version 2
#shader compute

struct SkinnedModel {
	U32 matricesOffset;
	U32 vertexOffset;
	U32 skinnedVerticesOffset;
	U32 skinningDataOffset;
	U32 vertexCount;
};

struct BoneMatrix {
	V4F row0;
	V4F row1;
	V4F row2;
};

[block] struct SkinnedModels {
	U32[] modelMatrixOffsets;
};

[block] struct BoneMatrices {
	BoneMatrix[] matrices;
};

[block] struct F32Data {
	F32[] data;
};

[block] struct U32Data {
	U32[] data;
};

[block] struct PushConstantData {
	SkinnedModel skinnedModel;
};

[input, builtin GlobalInvocationId] &V3U32 globalInvocationId; 

[set 0, storage_buffer, restrict, nonwritable] {
	[nonwritable] {
		[binding 0] &BoneMatrices bones;
		[binding 1] &F32Data inputPositions;
		[binding 2] &F32Data inputNormals;
		[binding 3] &F32Data inputTangents;
		[binding 4] &U32Data boneIndicesAndWeights;
	}
	[nonreadable] {
		[binding 5] &F32Data outputPositions;
		[binding 6] &F32Data outputNormals;
		[binding 7] &F32Data outputTangents;
	}
}
[push_constant] &PushConstantData pushConstants;

[entrypoint, localsize 256 1 1] @[][] compute_main{
	U32 vertexIndex{ globalInvocationId.x };
	SkinnedModel model{ pushConstants.skinnedModel };
	if vertexIndex >= model.vertexCount {
		return;
	};
	Bool a{ vertexIndex >= model.vertexCount };
	if a {
		return;
	};
	U32 vec3Index{ (model.vertexOffset + vertexIndex) * 3u };
	U32 skinningIndex{ (model.skinningDataOffset + vertexIndex) * 2u };
	U32 indices{ boneIndicesAndWeights.data[skinningIndex] };
	V4F weights{ unpack_unorm4x8(boneIndicesAndWeights.data[skinningIndex + 1u]) };
	V4F inPos  { inputPositions.data[vec3Index], inputPositions.data[vec3Index + 1u], inputPositions.data[vec3Index + 2u], 1.0F };
	V3F inNorm { inputNormals.data[vec3Index], inputNormals.data[vec3Index + 1u], inputNormals.data[vec3Index + 2u] };
	V3F inTan  { inputTangents.data[vec3Index], inputTangents.data[vec3Index + 1u], inputTangents.data[vec3Index + 2u] };
	
	BoneMatrix matrix{ bones.matrices[model.matricesOffset + (indices & 0xFFu)] };
	V3F outPos { weights.x * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2)) };
	V3F outNorm{ weights.x * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz)) };
	V3F outTan { weights.x * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz)) };
	
	matrix = bones.matrices[model.matricesOffset + ((indices >> 8u) & 0xFFu)];
	outPos  = outPos  + weights.y * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.y * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.y * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = bones.matrices[model.matricesOffset + ((indices >> 16u) & 0xFFu)];
	outPos  = outPos  + weights.z * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.z * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.z * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = bones.matrices[model.matricesOffset + (indices >> 24u)];
	outPos  = outPos  + weights.w * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.w * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.w * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	U32 outputIndex{ (model.skinnedVerticesOffset + vertexIndex) * 3u };
	outputPositions.data[outputIndex     ] = outPos.x;
	outputPositions.data[outputIndex + 1u] = outPos.y;
	outputPositions.data[outputIndex + 2u] = outPos.z;
	outputNormals.data[outputIndex     ] = outNorm.x;
	outputNormals.data[outputIndex + 1u] = outNorm.y;
	outputNormals.data[outputIndex + 2u] = outNorm.z;
	outputTangents.data[outputIndex     ] = outTan.x;
	outputTangents.data[outputIndex + 1u] = outTan.y;
	outputTangents.data[outputIndex + 2u] = outTan.z;
};
)"a);
	
}

}