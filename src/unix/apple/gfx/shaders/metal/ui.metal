// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include <metal_stdlib>

using namespace metal;

struct Uniforms {
	float4x4 proj;
};

struct UniformsFS {
	uint hdr;
	float hdr_brighten_factor;
	uint pad[2];
};

struct VertexIn {
	float2 pos   [[attribute(0)]];
	float2 uv	 [[attribute(1)]];
	uchar4 color [[attribute(2)]];
};

struct VertexOut {
	float4 pos [[position]];
	float2 uv;
	float4 color;
};

// Courtesy of https://github.com/obsproject/obs-studio/pull/6157/files#diff-81ee756f47c3a2fbb9f9fa0a858d79c4da89db97d8ae79fbd643c9533fba177b
static constant float3x3 REC709_TO_REC2020 =
{
	{0.6274040f, 0.3292820f, 0.0433136f},
	{0.0690970f, 0.9195400f, 0.0113612f},
	{0.0163916f, 0.0880132f, 0.8955950f}
};

float3 srgb_to_linear(float3 color)
{
	// Fast approximation of sRGB's transfer function
	return pow(abs(saturate(color)), 2.2f);
}

float3 srgb_linear_to_rec2020_linear(float3 color)
{
	return color * REC709_TO_REC2020;
}

/////// HDR10 ///////

float spow(float x, float p)
{
	return sign(x) * pow(abs(x), p);
}

float3 spow3(float3 v, float p)
{
	return float3(spow(v.x, p), spow(v.y, p), spow(v.z, p));
}

static constant float PQ_m_1 = 2610.0f / 4096.0f / 4.0f;
static constant float PQ_m_1_d = 1.0f / PQ_m_1;
static constant float PQ_m_2 = 2523.0f / 4096.0f * 128.0f;
static constant float PQ_m_2_d = 1.0f / PQ_m_2;
static constant float PQ_c_1 = 3424.0f / 4096.0f;
static constant float PQ_c_2 = 2413.0f / 4096.0f * 32.0f;
static constant float PQ_c_3 = 2392.0f / 4096.0f * 32.0f;

static constant float HDR10_MAX_NITS = 10000.0f;
static constant float SDR_MAX_NITS = 80.0f; // the reference sRGB luminance is 80 nits (aka the brightness of paper white)

float3 rec2020_pq_to_rec2020_linear(float3 color)
{
	// Apply the PQ EOTF (SMPTE ST 2084-2014) in order to linearize it
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L126

	float3 V_p = spow3(color, PQ_m_2_d);

	float3 n = max(0, V_p - PQ_c_1);

	float3 L = spow3(n / (PQ_c_2 - PQ_c_3 * V_p), PQ_m_1_d);
	float3 C = L * HDR10_MAX_NITS / SDR_MAX_NITS;

	return C;
}

float3 rec2020_linear_to_rec2020_pq(float3 color)
{
	// Apply the inverse of the PQ EOTF (SMPTE ST 2084-2014) in order to encode the signal as PQ
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L56

	float3 Y_p = spow3(max(0.0f, (color / HDR10_MAX_NITS) * SDR_MAX_NITS), PQ_m_1);

	float3 N = spow3((PQ_c_1 + PQ_c_2 * Y_p) / (PQ_c_3 * Y_p + 1.0f), PQ_m_2);

	return N;
}

/////////////////////

vertex VertexOut vs(VertexIn in [[stage_in]],
	constant Uniforms &uniforms [[buffer(1)]])
{
	VertexOut out;
	out.pos = uniforms.proj * float4(in.pos, 0, 1);
	out.uv = in.uv;
	out.color = float4(in.color) / float4(255.0);

	return out;
}

fragment float4 fs(VertexOut in [[stage_in]],
	constant UniformsFS &cb [[buffer(0)]],
	texture2d<float, access::sample> texture [[texture(0)]])
{
	constexpr sampler linearSampler(coord::normalized,
		min_filter::linear, mag_filter::linear, mip_filter::linear);

	float4 ui = float4(in.color) * texture.sample(linearSampler, in.uv);

	if (cb.hdr) {
		float3 ui_rgb = ui.rgb;
		ui_rgb = srgb_to_linear(ui_rgb); // UI texture is encoded non-linearly in sRGB, so we need to first linearize it
		ui_rgb = srgb_linear_to_rec2020_linear(ui_rgb);
		// TODO: It looks like for EDR we do NOT want to do any brightening because EDR value 1.0 is display-referred and NOT scene-referred (which was the case in Windows)
		// ui_rgb *= cb.hdr_brighten_factor; // 1.0 in sRGB is 80 nits which is the reference SDR luminance but most SDR displays will actually render 1.0 at around 200-300 nits for improved viewing; we mimic this by brightening the UI texture by a configurable constant
		// ui_rgb = rec2020_linear_to_rec2020_pq(ui_rgb);
		ui.rgb = ui_rgb;
	}

	return ui;
}
