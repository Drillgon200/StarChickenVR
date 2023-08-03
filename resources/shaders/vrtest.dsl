#version 1
#shader vertex
#extension multiview

(block) struct PushConstantMatrices {
	vec4f leftMatrixRow0;
	vec4f leftMatrixRow1;
	vec4f leftMatrixRow3;
	vec4f rightMatrixRow0;
	vec4f rightMatrixRow1;
	vec4f rightMatrixRow3;
}

//(input, location 0) vec3f& inPosition;
(input, builtin VertexIndex) i32& inVertexIndex;
(input, builtin ViewIndex) i32& inViewIndex;

(output, builtin Position) vec4f& outPosition;
(output, location 0) vec3f& passPos;
(output, location 1) vec3f& passColor;

(push_constant) PushConstantMatrices& pushConstantMatrices;

(entrypoint) void vert_main(){
	//vec3f pos = vec3f(f32((inVertexIndex >>> 1) * 4 - 1), f32((inVertexIndex & 1) * 4 - 1), 0.0F);
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
	if(inViewIndex == 0){
		f32 x = dot(pos, pushConstantMatrices.leftMatrixRow0);
		f32 y = dot(pos, pushConstantMatrices.leftMatrixRow1);
		f32 z = 0.05F; // Near plane
		f32 w = dot(pos, pushConstantMatrices.leftMatrixRow3);
		outPosition = vec4f(x, -y, z, w);
	} else {
		f32 x = dot(pos, pushConstantMatrices.rightMatrixRow0);
		f32 y = dot(pos, pushConstantMatrices.rightMatrixRow1);
		f32 z = 0.05F; // Near plane
		f32 w = dot(pos, pushConstantMatrices.rightMatrixRow3);
		outPosition = vec4f(x, -y, z, w);
	}
	passPos = pos.xyz;
}

//---------------------------------------------------------//
#shader fragment

(output, location 0) vec4f& outFragColor;
(input, location 0) vec3f& passPosition;
(input, location 1) vec3f& passColor;

(entrypoint) void frag_main(){
	outFragColor = vec4f(passColor, 1.0F);
}
