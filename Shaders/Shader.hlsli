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

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};

struct PatchTess
{
    float edgeTess[3] : SV_TessFactor;
    float insideTess : SV_InsideTessFactor;
};

struct HS_OUT
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct DS_OUT
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);


//--------------------------------------------------------------------------------------
// Constant Hull Shader
//--------------------------------------------------------------------------------------
PatchTess ConstantHS(InputPatch<VS_OUTPUT, 3> input, int patchID : SV_PrimitiveID)
{
    PatchTess output;
    output.edgeTess[0] = 16;
    output.edgeTess[1] = 16;
    output.edgeTess[2] = 16;
    output.insideTess = 32;

    return output;
}


//--------------------------------------------------------------------------------------
// Control Point Hull Shader
//--------------------------------------------------------------------------------------
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
HS_OUT HS(InputPatch<VS_OUTPUT, 3> input, int vertexIdx : SV_OutputControlPointID, int patchID : SV_PrimitiveID)
{
    HS_OUT output;
    output.position = input[vertexIdx].position;
    output.texCoord = input[vertexIdx].texCoord;

    return output;
}


//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
[domain("tri")]
DS_OUT DS(const OutputPatch<HS_OUT, 3> input, float3 location : SV_DomainLocation, PatchTess patch)
{
    DS_OUT output;

    float3 localPos = input[0].position * location[0] + input[1].position * location[1] + input[2].position * location[2];
    float2 texCoord = input[0].texCoord * location[0] + input[1].texCoord * location[1] + input[2].texCoord * location[2];

    const float4 spherePos = float4(normalize(localPos.xyz) * 50.0f, 1.0f);

    output.position = mul(float4(localPos, 1.0f), cb.worldMatrix);
    output.position = mul(output.position, cb.viewMatrix);
    output.position = mul(output.position, cb.projectionMatrix);

    output.texCoord = texCoord;

    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS(DS_OUT input)
{
    PS_OUTPUT output;
    output.color = txDiffuse.Sample(samLinear, input.texCoord);
    return output;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    //const float4 sphere_pos = float4(normalize(input.position.xyz) * 50.0f, 1.0f);
    //output.position = mul(sphere_pos, cb.worldMatrix);
    //output.position = mul(output.position, cb.viewMatrix);
    //output.position = mul(output.position, cb.projectionMatrix);

    output.position = input.position;
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}