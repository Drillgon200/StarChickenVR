#version 2
#shader compute
#include "lib.dsi"

struct SkinnedModel {
	U32 matricesOffset;
	U32 vertexOffset;
	U32 skinnedVerticesOffset;
	U32 skinningDataOffset;
	U32 vertexCount;
};

[input, builtin GlobalInvocationId] &V3U globalInvocationId;
[push_constant, block] &struct {
	SkinnedModel skinnedModel;
	U32 drawDataUniformBuffer;
} pushData;

[entrypoint, localsize 256 1 1] @[][] compute_main{
	U32 vertexIndex{ globalInvocationId.x };
	SkinnedModel model{ pushData.skinnedModel };
	if vertexIndex >= model.vertexCount {
		return;
	};
	U32 vertIndex{ model.vertexOffset + vertexIndex };
	U32 skinningIndex{ (model.skinningDataOffset + vertexIndex) * 2u };
	&DrawData drawData{ UniformBuffer(DrawData)(pushData.drawDataUniformBuffer) };
	U32 indices{ drawData.boneIndicesAndWeights[skinningIndex] };
	V4F weights{ unpack_unorm4x8(drawData.boneIndicesAndWeights[skinningIndex + 1u]) };
	V4F inPos  { drawData.positions[vertIndex], 1.0 };
	V3F inNorm { drawData.normals[vertIndex] };
	V3F inTan  { drawData.tangents[vertIndex] };
	
	M4x3F matrix{ drawData.matrices[model.matricesOffset + (indices & 0xFFu)] };
	V3F outPos { weights.x * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2)) };
	V3F outNorm{ weights.x * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz)) };
	V3F outTan { weights.x * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz)) };
	
	matrix = drawData.matrices[model.matricesOffset + ((indices >> 8u) & 0xFFu)];
	outPos  = outPos  + weights.y * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.y * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.y * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = drawData.matrices[model.matricesOffset + ((indices >> 16u) & 0xFFu)];
	outPos  = outPos  + weights.z * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.z * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.z * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = drawData.matrices[model.matricesOffset + (indices >> 24u)];
	outPos  = outPos  + weights.w * V3F(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.w * V3F(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.w * V3F(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	U32 outputIndex{ model.skinnedVerticesOffset + vertexIndex };
	drawData.skinnedPositions[outputIndex] = outPos;
	drawData.skinnedNormals[outputIndex] = outNorm;
	drawData.skinnedTangents[outputIndex] = outTan;
};