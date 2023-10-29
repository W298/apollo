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
    float edgeTess[4] : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

struct HS_OUT
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct DS_OUT
{
    float4 position : SV_Position;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

Texture2D texMap[2] : register(t0);
SamplerState samLinear : register(s0);


//--------------------------------------------------------------------------------------
// Constant Hull Shader
//--------------------------------------------------------------------------------------
PatchTess ConstantHS(InputPatch<VS_OUTPUT, 4> patch, int patchID : SV_PrimitiveID)
{
    PatchTess output;

    float3 localPos = 0.25f * (patch[0].position + patch[1].position + patch[2].position + patch[3].position);
    float3 worldPos = mul(float4(localPos, 1.0f), cb.worldMatrix).xyz;
    float3 spherePos = normalize(worldPos) * 150.0f;

    float dist = distance(spherePos, cb.cameraPosition);

    // Tessellate the patch based on distance from the eye such that
	// the tessellation is 0 if d >= d1 and 64 if d <= d0.  The interval
	// [d0, d1] defines the range we tessellate in.
	
    const float near = 2.0f;
    const float far = 50.0f;

    float tess = max(pow(2, floor(8 * saturate((far - dist) / (far - near)))), 1);

    output.edgeTess[0] = tess;
    output.edgeTess[1] = tess;
    output.edgeTess[2] = tess;
    output.edgeTess[3] = tess;

    output.insideTess[0] = tess;
    output.insideTess[1] = tess;

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
    output.normal = input[vertexIdx].normal;
    output.texCoord = input[vertexIdx].texCoord;

    return output;
}

static const float2 size = { 2.0, 0.0 };
static const float3 off = { -1.0, 0.0, 1.0 };
static const float2 nTex = { 5760, 2880 };

//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
[domain("quad")]
DS_OUT DS(const OutputPatch<HS_OUT, 4> input, float2 uv : SV_DomainLocation, PatchTess patch)
{
    DS_OUT output;
	
	// Bilinear interpolation.
    float3 v1 = lerp(input[0].position, input[1].position, uv.x);
    float3 v2 = lerp(input[2].position, input[3].position, uv.x);

    float3 n1 = lerp(input[0].normal, input[1].normal, uv.x);
    float3 n2 = lerp(input[2].normal, input[3].normal, uv.x);

    float2 t1 = lerp(input[0].texCoord, input[1].texCoord, uv.x);
    float2 t2 = lerp(input[2].texCoord, input[3].texCoord, uv.x);

    float3 position = lerp(v1, v2, uv.y);
    float3 normal = lerp(n1, n2, uv.y);
    float2 _dummyTexCoord = lerp(t1, t2, uv.y);

    // Get tessellation level.
    float tess = patch.edgeTess[0];
    float level = 8 - sqrt(tess);

    // Convert positions to polar coordinates.
    float3 pointOnSphere = normalize(position);

    float theta = atan2(pointOnSphere.z, pointOnSphere.x);
    theta = theta <= 0.0f ? 2 * PI - abs(theta) : theta;
    float phi = acos(pointOnSphere.y);

    // Convert polar coordinates to texture coordinates.
    float2 texCoord = float2(theta / (2 * PI), phi / PI);

    // Get height from texture.
    const float height = texMap[1].SampleLevel(samLinear, texCoord, level).r;

    output.position = mul(float4(pointOnSphere * (150.0f + height * 0.7f), 1.0f), cb.worldMatrix);
    output.position = mul(output.position, cb.viewMatrix);
    output.position = mul(output.position, cb.projectionMatrix);

    float2 offxy = { off.x / nTex.x, off.y / nTex.y };
    float2 offzy = { off.z / nTex.x, off.y / nTex.y };
    float2 offyx = { off.y / nTex.x, off.x / nTex.y };
    float2 offyz = { off.y / nTex.x, off.z / nTex.y };

    float s11 = height;
    float s01 = texMap[1].SampleLevel(samLinear, texCoord + offxy, level).r;
    float s21 = texMap[1].SampleLevel(samLinear, texCoord + offzy, level).r;
    float s10 = texMap[1].SampleLevel(samLinear, texCoord + offyx, level).r;
    float s12 = texMap[1].SampleLevel(samLinear, texCoord + offyz, level).r;
    float3 va = { size.x, size.y, s21 - s01 };
    float3 vb = { size.y, size.x, s12 - s10 };
    va = normalize(va);
    vb = normalize(vb);

    output.normal = float4(cross(va, vb) / 2 + 0.5, 1.0f);
    output.texCoord = texCoord;

    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS(DS_OUT input)
{
    PS_OUTPUT output;
    // output.color = texMap[0].Sample(samLinear, input.texCoord);
    // output.color = float4(0.5f, 0.5f, 0.5f, 1.0f);
    output.color = input.normal;

	//float lod = texMap[0].CalculateLevelOfDetail(samLinear, input.texCoord);
    //output.color = float4(lod, 1 - lod, 0.0f, 1.0f);
    
    return output;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    output.position = input.position;
    output.normal = input.normal;
    output.texCoord = input.texCoord;

    return output;
}