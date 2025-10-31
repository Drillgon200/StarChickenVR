#version 2
#shader vertex

struct M4x3F {
	V4F row0;
	V4F row1;
	V4F row2;
};

struct Vertex {
	V3F pos;
	V2F tex;
	U32 color;
	U32 textureIndex;
	U32 flags;
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

[input, builtin VertexIndex] &I32 vertexIndex;
[output, builtin Position] &V4F outPosition;

[entrypoint] @[][] vert_main {
	&Vertex vertexBuffer{ (&Vertex)(v2u_u64_add(drawData.pUIVertices, U32(modelData.verticesOffset))) };
	Vertex vert{ vertexBuffer[^vertexIndex] };
	^pos = vert.pos.xy;
	^texcoord = vert.tex;
	^color = unpack_unorm4x8(vert.color);
	^texidx = vert.textureIndex;
	^flags = vert.flags;
	if drawData.uiClipBoxes {
		V4F clip{ drawData.uiClipBoxes[vert.flags >> 16u] };
		^clipBox = clip;
	};
	^outPosition = V4F((vert.pos.xy / drawData.screenDimensions) * 2.0 - 1.0, 0.01 / vert.pos.z + 0.99, 1.0);
};

#interface

&V2F pos;
&V2F texcoord;
&V4F color;
[flat] &U32 texidx;
[flat] &U32 flags;
[flat] &V4F clipBox;

#shader fragment

[uniform, set 0, binding 0] &Sampler bilinearSampler;
[uniform, set 0, binding 3] &Image2DSampled[] textures;

[output, location 0] &V4F fragColor;

@[F32 result][F32 a, F32 b, F32 c] median{
	return max(min(a, b), min(max(a, b), c));
};

@[F32 result][V2F fragWidth] screen_px_range{
	F32 sdfRadius{ (12.0 / 64.0) / 16.0 };
	V2F v{ fragWidth / (sdfRadius * 2.0) };
	return 0.5 * (v.x + v.y);
};

@[V4F result][F32 screenPxRange, V2F coord] sample_color{
	V4F msdf{ (^textures)[nonuniform ^texidx][^bilinearSampler, coord] };
	F32 val{ median(msdf.r, msdf.g, msdf.b) };
	// Setting the cutoff to something a bit smaller makes the font look more bold,
	// but it also takes care of some artifacts I don't feel like fixing
	// (letter B has a hole and bad things happen at smaller font sizes)
	F32 cutoff{ 0.46 };
	return V4F(1.0, 1.0, 1.0, min(max((val - cutoff) / min(screenPxRange, 1.0 - cutoff), 0.0), 1.0));
};

[entrypoint] @[][] frag_main{
	V2F dxdy{ fwidth(^texcoord) };
	if ^flags >> 16u {
		if pos.x < clipBox.x || pos.x >= clipBox.z || pos.y < clipBox.y || pos.y >= clipBox.w {
			terminate;
		};
	};
	U32 UI_RENDER_FLAG_MSDF{ 1 };
	if ^flags & UI_RENDER_FLAG_MSDF {
		F32 screenPxRange{ screen_px_range(dxdy) };
		V4F val{ sample_color(screenPxRange, ^texcoord) * ^color };
		if !val.a {
			terminate;
		};
		^fragColor = val;
	} else {
		^fragColor = (^textures)[nonuniform ^texidx][^bilinearSampler, ^texcoord] * ^color;
	};
};