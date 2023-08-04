#version 1
#shader vertex
#extension multiview

struct ProjectiveMatrix {
	vec4f row0;
	vec4f row1;
	vec4f row3;
}

(block) struct PushConstantMatrices {
	ProjectiveMatrix[2] eyeMatrices;
}

(input, builtin VertexIndex) i32& inVertexIndex;
(input, builtin ViewIndex) i32& inViewIndex;

(input, location 0) vec3f& inPosition;
(input, location 1) vec2f& inTexcoord;
(input, location 2) vec3f& inNormal;
(input, location 3) vec3f& inTangent;

(output, builtin Position) vec4f& outPosition;
(output, location 0) vec3f& passPos;
(output, location 1) vec3f& passColor;

(push_constant) PushConstantMatrices& pushConstantMatrices;

(entrypoint) void vert_main(){
	//vec3f pos = vec3f(f32((inVertexIndex >>> 1) * 4 - 1), f32((inVertexIndex & 1) * 4 - 1), 0.0F);
	/*
	i32 vertexIdx = inVertexIndex;
	vec4f pos;
	if(vertexIdx == 0){
		pos = vec4f(0.0F, 1.5F, 0.0F, 1.0F);
		passColor = vec3f(1.0F, 0.0F, 0.0F);
	} else if(vertexIdx == 1){
		pos = vec4f(-0.5F, 0.5F, 0.0F, 1.0F);
		passColor = vec3f(0.0F, 1.0F, 0.0F);
	} else if(vertexIdx == 2){
		pos = vec4f(0.5F, 0.5F, 0.0F, 1.0F);
		passColor = vec3f(0.0F, 0.0F, 1.0F);
	}
	*/
	vec4f pos = vec4f(inPosition - vec3f(0.0F, 2.0F, 0.0F), 1.0F);
	i32 viewIdx = inViewIndex;
	f32 x = dot(pos, pushConstantMatrices.eyeMatrices[viewIdx].row0);
	f32 y = dot(pos, pushConstantMatrices.eyeMatrices[viewIdx].row1);
	f32 z = 0.05F; // Near plane
	f32 w = dot(pos, pushConstantMatrices.eyeMatrices[viewIdx].row3);
	outPosition = vec4f(x, -y, z, w);
	passPos = pos.xyz;
	passColor = vec3f(0.5F, 0.5F, 0.5F);
}

//---------------------------------------------------------//
#shader fragment

(output, location 0) vec4f& outFragColor;
(input, location 0) vec3f& passPosition;
(input, location 1) vec3f& passColor;

(entrypoint) void frag_main(){
	outFragColor = vec4f(passColor, 1.0F);
}
