#define PI 3.1415926538


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ConstantBufferType
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
    float lightNearZ;
    float lightFarZ;
    float3 lightPosW;
    float4x4 shadowTransform;
    float4x4 invViewMatrix;
    float4x4 invProjMatrix;
};

ConstantBuffer<ConstantBufferType> cb : register(b0);


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

struct PS_OUTPUT
{
    float4 color : SV_Target;
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
    float3 normCatPos : POSITION;
};


//--------------------------------------------------------------------------------------
// Texture & Sampler Variables
//--------------------------------------------------------------------------------------
Texture2D texMap[5] : register(t0);
SamplerState samLinear : register(s0);
SamplerComparisonState samShadow : register(s1);

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
    return pow(2, -8 * pow(s, 2) + 8);
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

static const float2 size = { 2.0, 0.0 };
static const float3 off = { -1.0, 0.0, 1.0 };
static const float2 nTex = { 11520, 11520 };

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
    output.position = mul(output.position, cb.viewMatrix);
    output.position = mul(output.position, cb.projectionMatrix);

    // Set normalized cartesian position for calc texture coordinates in pixel shader.
    output.normCatPos = normCatPos;

    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float3 GetNormalFromHeight(Texture2D tex, float2 sTexCoord, float multiplier)
{
    float2 offxy = { off.x / nTex.x, off.y / nTex.y };
    float2 offzy = { off.z / nTex.x, off.y / nTex.y };
    float2 offyx = { off.y / nTex.x, off.x / nTex.y };
    float2 offyz = { off.y / nTex.x, off.z / nTex.y };

    float a = tex.Sample(samLinear, sTexCoord + offxy * multiplier).r;
    float b = tex.Sample(samLinear, sTexCoord + offzy * multiplier).r;
    float c = tex.Sample(samLinear, sTexCoord + offyx * multiplier).r;
    float d = tex.Sample(samLinear, sTexCoord + offyz * multiplier).r;

    float3 va = normalize(float3(size.xy, (b - a) * 0.5f));
    float3 vb = normalize(float3(size.yx, (d - c) * 0.5f));

    return normalize(cross(va, vb));
}

float3 GetNormalFromHeightCross(Texture2D tex, float2 sTexCoord, float multiplier)
{
    float2 offxy = { -1 / nTex.x, -1 / nTex.y };
    float2 offzy = { 1 / nTex.x, 1 / nTex.y };
    float2 offyx = { 1 / nTex.x, -1 / nTex.y };
    float2 offyz = { -1 / nTex.x, 1 / nTex.y };

    float a = tex.Sample(samLinear, sTexCoord + offxy * multiplier).r;
    float b = tex.Sample(samLinear, sTexCoord + offzy * multiplier).r;
    float c = tex.Sample(samLinear, sTexCoord + offyx * multiplier).r;
    float d = tex.Sample(samLinear, sTexCoord + offyz * multiplier).r;

    float3 va = normalize(float3(size.xy, (b - a) * 0.5f));
    float3 vb = normalize(float3(size.yx, (d - c) * 0.5f));

    return normalize(cross(va, vb));
}

bool CalcShadowFactor(float4 shadowPosH)
{
    float x = shadowPosH.x / shadowPosH.w / 2.0f + 0.5f;
    float y = -shadowPosH.y / shadowPosH.w / 2.0f + 0.5f;

    // Depth in NDC space.
    float realDepth = shadowPosH.z / shadowPosH.w;
    float shadowMapDepth = texMap[4].SampleLevel(samLinear, float2(x, y), 0).r;

    return shadowMapDepth > realDepth;

    uint width, height, numMips;
    texMap[4].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += texMap[4].SampleCmpLevelZero(samShadow, shadowPosH.xy + offsets[i], realDepth).r;
    }
    
    return percentLit / 9.0f;
}

float CalcShadowFactor2(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    texMap[4].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += texMap[4].SampleCmpLevelZero(samShadow, shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

PS_OUTPUT PS(DS_OUT input)
{
    PS_OUTPUT output;

    // Convert cartesian to polar.
    float theta = atan2(input.normCatPos.z, input.normCatPos.x);
    theta = sign(theta) == -1 ? 2 * PI + theta : theta;
    float phi = acos(input.normCatPos.y);

    // Convert polar coordinates to texture coordinates.
    float2 gTexCoord = float2(theta / (2 * PI), phi / PI);

    // Divide texture coordinates into two parts and re-mapping.
	int texIndex = round(gTexCoord.x);
    float2 sTexCoord = float2(texIndex == 0 ? gTexCoord.x * 2 : (gTexCoord.x - 0.5f) * 2.0f, gTexCoord.y);

    // Calculate local normal from height map.
    float3 n1 = GetNormalFromHeight(texMap[texIndex + 2], sTexCoord, 1.0f);
    float3 n2 = GetNormalFromHeight(texMap[texIndex + 2], sTexCoord, 2.0f);
	float3 n3 = GetNormalFromHeight(texMap[texIndex + 2], sTexCoord, 3.0f);

    float3 localNormal = (n1 * 3.0f + n2 * 2.0f + n3 * 1.0f) / 6.0f;
    localNormal = normalize(localNormal);

    // Calculate TBN Matrix.
    float3 N = input.normCatPos;
    float3 T = float3(-sin(theta), 0, cos(theta));
    float3 B = cross(N, T);

    // Calculate Normal.
    float3x3 TBN = float3x3(normalize(T), normalize(B), normalize(N));
    float3 normal = normalize(mul(localNormal, TBN));

    // [Diffuse color]
    float4 texColor = texMap[texIndex].Sample(samLinear, sTexCoord);
    float3 diffuse = max(dot(normal, cb.lightDirection.xyz), 0.0f) * cb.lightColor.xyz;
    float3 ambient = float3(0.008f, 0.008f, 0.008f) * cb.lightColor.xyz;

    float height = texMap[texIndex + 2].Sample(samLinear, sTexCoord).r;
    float shadowFactor = CalcShadowFactor2(mul(float4(input.normCatPos * (150.0f + height * 0.5f), 1.0f), cb.shadowTransform));

    float4 final = float4(saturate((diffuse * (1 - shadowFactor) + ambient) * texColor.rgb), texColor.a);
	final.a = 1;
    output.color = final;

	// [LOD level]
    // float lod = texMap[0].CalculateLevelOfDetail(samLinear, input.texCoord);
    // output.color = float4(lod, 1 - lod, 0.0f, 1.0f);

	// [Just gray color]
	// output.color = float4(0.5f, 0.5f, 0.5f, 1.0f);
    
    return output;
}