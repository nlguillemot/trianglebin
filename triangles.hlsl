struct VS_INPUT
{
	uint VertexID : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
#if NUM_EXTRA_FLOATs > 0
	float ExtraFloats[NUM_EXTRA_FLOATs] : EXTRAFLOATS;
#endif
};

struct PS_OUTPUT
{
	float4 Color : SV_Target;
};

RWStructuredBuffer<uint> PixelCounterUAV : register(u1);
cbuffer MaxNumPixelsCBV : register(b0) { uint MaxNumPixels; };

VS_OUTPUT VSmain(VS_INPUT input)
{
	VS_OUTPUT output;

	if (input.VertexID % 3 == 0)
		output.Position = float4(-1, 1, 0, 1);
	else if (input.VertexID % 3 == 1)
		output.Position = float4(1, 1, 0, 1);
	else if (input.VertexID % 3 == 2)
		output.Position = float4(-1, -1, 0, 1);

	const float4 colors[7] = {
		float4(1,0,0,1),
		float4(0,1,0,1),
		float4(0,0,1,1),
		float4(1,1,0,1),
		float4(0,1,1,1),
		float4(1,0,1,1),
		float4(1,1,1,1)
	};
	
	output.Color = colors[(input.VertexID / 3) % 7] * float4(0.4,0.4,0.4,1);

#if NUM_EXTRA_FLOATs > 0
	[unroll]
	for (int i = 0; i < NUM_EXTRA_FLOATs; i++)
	{
		output.ExtraFloats[i] = input.VertexID + i;
	}
#endif

	return output;
}

PS_OUTPUT PSmain(VS_OUTPUT input)
{
	if (PixelCounterUAV.IncrementCounter() > MaxNumPixels)
	{
		discard;
	}

	PS_OUTPUT output;
	output.Color = input.Color;

	// just to force it not to optimize this out
#if NUM_EXTRA_FLOATs > 0
	[unroll]
	for (int i = 0; i < NUM_EXTRA_FLOATs; i++)
	{
		output.Color.r += input.ExtraFloats[i] * 0.00001;
	}
#endif

	return output;
}