#version 1
#shader vertex
#extension multiview

struct Matrix4x3f {
	vec4f row0;
	vec4f row1;
	vec4f row2;
}

(block) struct PushConstantModel {
	u32 transformIndex;
}

(block) struct TransformMatrices {
	Matrix4x3f[] matrices;
}

(input, builtin VertexIndex) i32& inVertexIndex;
(input, builtin ViewIndex) i32& inViewIndex;

(input, location 0) vec3f& inPosition;
(input, location 1) vec2f& inTexcoord;
(input, location 2) vec3f& inNormal;
(input, location 3) vec3f& inTangent;

(output, builtin Position) vec4f& outPosition;
(output, location 0) vec3f& passPos;
(output, location 1) vec3f& passNormal;

(descriptor_set 0, binding 0, restrict, nonwritable, storage_buffer) TransformMatrices& transformMatrices;
(push_constant) PushConstantModel& modelInfo;

(entrypoint) void vert_main(){
	vec4f pos = vec4f(inPosition, 1.0F);
	vec3f norm = inNormal;
	i32 viewIdx = inViewIndex;
	u32 transformIndex = modelInfo.transformIndex;
	Matrix4x3f modelMat = transformMatrices.matrices[transformIndex];
	vec4f transformedPos = vec4f(
		dot(pos, modelMat.row0),
		dot(pos, modelMat.row1),
		dot(pos, modelMat.row2),
		1.0F
	);
	vec3f transformedNorm = vec3f(
		dot(norm, modelMat.row0.xyz),
		dot(norm, modelMat.row1.xyz),
		dot(norm, modelMat.row2.xyz)
	);
	f32 x = dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row0);
	f32 y = dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row1);
	f32 z = 0.05F; // Near plane
	// row2 is actually row3 in  this case, since the matrix is a ProjectiveTransformMatrix
	f32 w = dot(transformedPos, transformMatrices.matrices[viewIdx + 1].row2);
	outPosition = vec4f(x, -y, z, w);
	passPos = transformedPos.xyz;
	passNormal = transformedNorm;
}

//---------------------------------------------------------//
#shader fragment

(output, location 0) vec4f& outFragColor;
(input, location 0) vec3f& passPosition;
(input, location 1) vec3f& passNormal;

(entrypoint) void frag_main(){
	vec3f lightDir = vec3f(0.57735026919F, 0.57735026919F, 0.57735026919F);
	outFragColor = vec4f(vec3f(dot(normalize(passNormal), lightDir) * 0.5F + 0.5F), 1.0F);
}
