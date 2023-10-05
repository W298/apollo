//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ConstantBufferType
{
    float4 colorMultiplier;
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
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
    output.position = mul(input.position, cb.worldMatrix);
    output.position = mul(output.position, cb.viewMatrix);
    output.position = mul(output.position, cb.projectionMatrix);

	output.color = float4(input.color.xyz, 1.0f) * cb.colorMultiplier;

    return output;
}