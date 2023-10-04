//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ConstantBufferType
{
    float4 colorMultiplier;
};

ConstantBuffer<ConstantBufferType> cb : register(b0);


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR0;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET0;
};


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS(PS_INPUT input)
{
    PS_OUTPUT output;
	output.color = input.color;
    return output;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position.xyz, 1.0f);
    output.color = float4(input.color.xyz, 1.0f) * cb.colorMultiplier;

    return output;
}