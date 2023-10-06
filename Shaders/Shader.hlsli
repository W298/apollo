//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ConstantBufferType
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

ConstantBuffer<ConstantBufferType> cb : register(b0);


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};


float nrand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS(PS_INPUT input)
{
    PS_OUTPUT output;
    output.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
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

    return output;
}