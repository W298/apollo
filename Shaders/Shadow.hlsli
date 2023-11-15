#define PI 3.1415926538


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ShadowCBType
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
    float4 cameraPosition;
};

ConstantBuffer<ShadowCBType> cb : register(b1);


//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
};

struct PatchTess
{
    float edgeTess[4] : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

struct HS_OUT
{
    float3 position : POSITION;
};

struct DS_OUT
{
    float4 position : SV_Position;
};


//--------------------------------------------------------------------------------------
// Texture & Sampler Variables
//--------------------------------------------------------------------------------------
Texture2D texMap[5] : register(t0);
SamplerState samLinear : register(s0);

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = input.position;

    return output;
}


//--------------------------------------------------------------------------------------
// Constant Hull Shader
//--------------------------------------------------------------------------------------

static const float near = 20.0f;
static const float far = 150.0f;

float CalcTessFactor(float3 p)
{
    float d = distance(p, cb.cameraPosition.xyz);
    float s = saturate((d - near) / (far - near));
    return pow(2.0f, -8 * pow(s, 0.5f) + 8);
}

PatchTess ConstantHS(InputPatch<VS_OUTPUT, 4> patch, int patchID : SV_PrimitiveID)
{
    PatchTess output;

    float3 localPos = 0.25f * (patch[0].position + patch[1].position + patch[2].position + patch[3].position);
    float3 worldPos = mul(float4(localPos, 1.0f), cb.worldMatrix).xyz;

    float3 p0 = normalize(patch[0].position) * 150.0f;
    float3 p1 = normalize(patch[1].position) * 150.0f;
    float3 p2 = normalize(patch[2].position) * 150.0f;
    float3 p3 = normalize(patch[3].position) * 150.0f;

    float3 e0 = 0.5f * (p0 + p2);
    float3 e1 = 0.5f * (p0 + p1);
    float3 e2 = 0.5f * (p1 + p3);
    float3 e3 = 0.5f * (p2 + p3);
    float3 c = normalize(worldPos) * 150.0f;

    output.edgeTess[0] = CalcTessFactor(e0);
    output.edgeTess[1] = CalcTessFactor(e1);
    output.edgeTess[2] = CalcTessFactor(e2);
    output.edgeTess[3] = CalcTessFactor(e3);

    output.insideTess[0] = CalcTessFactor(c);
    output.insideTess[1] = output.insideTess[0];

    return output;
}


//--------------------------------------------------------------------------------------
// Control Point Hull Shader
//--------------------------------------------------------------------------------------
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
HS_OUT HS(InputPatch<VS_OUTPUT, 4> input, int vertexIdx : SV_OutputControlPointID, int patchID : SV_PrimitiveID)
{
    HS_OUT output;
    output.position = input[vertexIdx].position;

    return output;
}


//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------

[domain("quad")]
DS_OUT DS(const OutputPatch<HS_OUT, 4> input, float2 uv : SV_DomainLocation, PatchTess patch)
{
    DS_OUT output;
	
	// Bilinear interpolation (position).
    float3 v1 = lerp(input[0].position, input[1].position, uv.x);
    float3 v2 = lerp(input[2].position, input[3].position, uv.x);
    float3 position = lerp(v1, v2, uv.y);

    // Get tessellation level.
    float tess = patch.edgeTess[0];

    // Calculate LOD level for height map.
    // range is 0 ~ 5 (mipmap has 10 levels but use only 6).
    float level = max(0, 8 - sqrt(tess) - 3);

    // Get normalized cartesian position.
    float3 normCatPos = normalize(position);

	// Convert cartesian to polar.
    float theta = atan2(normCatPos.z, normCatPos.x);
    theta = sign(theta) == -1 ? 2 * PI + theta : theta;
    float phi = acos(normCatPos.y);

    // Convert polar coordinates to texture coordinates for height map.
    float2 gTexCoord = float2(theta / (2 * PI), phi / PI);

    // Divide texture coordinates into two parts and re-mapping.
    int texIndex = round(gTexCoord.x) + 2;
    float2 sTexCoord = float2(texIndex == 0 ? gTexCoord.x * 2 : (gTexCoord.x - 0.5f) * 2.0f, gTexCoord.y);

    // Get height from texture.
    float height = texMap[texIndex].SampleLevel(samLinear, sTexCoord, level).r;

    // Multiply MVP matrices.
    output.position = mul(float4(normCatPos * (150.0f + height * 0.5f), 1.0f), cb.worldMatrix);
    output.position = mul(output.position, cb.viewProjMatrix);

    return output;
}

void PS(DS_OUT input)
{
    // Nothing to do.
}