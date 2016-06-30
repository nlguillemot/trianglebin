struct VS_INPUT
{
	uint VertexID : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD;
};

struct PS_OUTPUT
{
	float4 Color : SV_Target;
};

Texture2D TrianglesSRV : register(t0);
SamplerState TrianglesSMP : register(s0);

VS_OUTPUT VSmain(VS_INPUT input)
{
	VS_OUTPUT output;

	output.Position.x = (float)(input.VertexID / 2) * 4.0 - 1.0;
	output.Position.y = (float)(input.VertexID % 2) * 4.0 - 1.0;
	output.Position.z = 0.0;
	output.Position.w = 1.0;

	output.TexCoord.x = (float)(input.VertexID / 2) * 2.0;
	output.TexCoord.y = 1.0 - (float)(input.VertexID % 2) * 2.0;

	return output;
}

PS_OUTPUT PSmain(VS_OUTPUT input)
{
	PS_OUTPUT output;
	output.Color = TrianglesSRV.Sample(TrianglesSMP, input.TexCoord);
	return output;
}