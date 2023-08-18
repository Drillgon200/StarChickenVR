#version 1
#shader compute

struct SkinnedModel {
	u32 matricesOffset;
	u32 vertexOffset;
	u32 skinningVertexOffset;
	u32 vertexCount;
}

struct BoneMatrix {
	vec4f row0;
	vec4f row1;
	vec4f row2;
}

(block) struct SkinnedModels {
	u32[] modelMatrixOffsets;
}

(block) struct BoneMatrices {
	BoneMatrix[] matrices;
}

(block) struct F32Data {
	f32[] data;
}

(block) struct U32Data {
	u32[] data;
}

(block) struct PushConstantData {
	SkinnedModel skinnedModel;
}

(input, builtin GlobalInvocationId) vec3u32& globalInvocationId; 

(descriptor_set 0, storage_buffer, restrict, nonwritable) {
	(nonwritable) {
		(binding 0) BoneMatrices& bones;
		(binding 1) F32Data& inputPositions;
		(binding 2) F32Data& inputNormals;
		(binding 3) F32Data& inputTangents;
		(binding 4) U32Data& boneIndicesAndWeights;
	}
	(nonreadable) {
		(binding 5) F32Data& outputPositions;
		(binding 6) F32Data& outputNormals;
		(binding 7) F32Data& outputTangents;
	}
}
(push_constant) PushConstantData& pushConstants;


(entrypoint, localsize 256 1 1) void compute_main(){
	u32 vertexIndex = globalInvocationId.x;
	SkinnedModel model = pushConstants.skinnedModel;
	if(vertexIndex >= model.vertexCount){
		return;
	}
	u32 vec3Index = (model.vertexOffset + vertexIndex) * 3u;
	u32 skinningIndex = model.skinningVertexOffset + vertexIndex * 2u;
	u32 indices = boneIndicesAndWeights.data[skinningIndex];
	vec4f weights = unpack_unorm4x8(boneIndicesAndWeights.data[skinningIndex + 1u]);
	vec4f inPos  = vec4f(inputPositions.data[vec3Index], inputPositions.data[vec3Index + 1u], inputPositions.data[vec3Index + 2u], 1.0F);
	vec3f inNorm = vec3f(inputNormals.data[vec3Index], inputNormals.data[vec3Index + 1u], inputNormals.data[vec3Index + 2u]);
	vec3f inTan  = vec3f(inputTangents.data[vec3Index], inputTangents.data[vec3Index + 1u], inputTangents.data[vec3Index + 2u]);
	
	BoneMatrix matrix = bones.matrices[model.matricesOffset + (indices & 0xFFu)];
	vec3f outPos  = weights.x * vec3f(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	vec3f outNorm = weights.x * vec3f(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	vec3f outTan  = weights.x * vec3f(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = bones.matrices[model.matricesOffset + ((indices >> 8u) & 0xFFu)];
	outPos  = outPos  + weights.y * vec3f(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.y * vec3f(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.y * vec3f(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = bones.matrices[model.matricesOffset + ((indices >> 16u) & 0xFFu)];
	outPos  = outPos  + weights.z * vec3f(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.z * vec3f(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.z * vec3f(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	matrix = bones.matrices[model.matricesOffset + (indices >> 24u)];
	outPos  = outPos  + weights.w * vec3f(dot(inPos,  matrix.row0),     dot(inPos,  matrix.row1),     dot(inPos,  matrix.row2));
	outNorm = outNorm + weights.w * vec3f(dot(inNorm, matrix.row0.xyz), dot(inNorm, matrix.row1.xyz), dot(inNorm, matrix.row2.xyz));
	outTan  = outTan  + weights.w * vec3f(dot(inTan,  matrix.row0.xyz), dot(inTan,  matrix.row1.xyz), dot(inTan,  matrix.row2.xyz));
	
	u32 outputIndex = (model.skinningVertexOffset + vertexIndex) * 3u;
	outputPositions.data[outputIndex     ] = outPos.x;
	outputPositions.data[outputIndex + 1u] = outPos.y;
	outputPositions.data[outputIndex + 2u] = outPos.z;
	outputNormals.data[outputIndex     ] = outNorm.x;
	outputNormals.data[outputIndex + 1u] = outNorm.y;
	outputNormals.data[outputIndex + 2u] = outNorm.z;
	outputTangents.data[outputIndex     ] = outTan.x;
	outputTangents.data[outputIndex + 1u] = outTan.y;
	outputTangents.data[outputIndex + 2u] = outTan.z;
}