#define PI 3.1415926538


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct OpaqueCBType
{
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
    float4x4 shadowTransform;
    float4 parameters;
};

ConstantBuffer<OpaqueCBType> cb : register(b0);


//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 position : POSITION;
    nointerpolation float3 quadPos : QUAD;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    nointerpolation float3 quadPos : QUAD;
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
    float3 catPos : POSITION;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};


//--------------------------------------------------------------------------------------
// Texture & Sampler Variables
//--------------------------------------------------------------------------------------
Texture2D texMap[5] : register(t0);
SamplerState samAnisotropic : register(s0);
SamplerComparisonState samShadow : register(s1);
SamplerState anisotropicClampMip1 : register(s2);


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position, 1.0f);
    output.quadPos = input.quadPos;

    return output;
}


//--------------------------------------------------------------------------------------
// Constant Hull Shader
//--------------------------------------------------------------------------------------
static const float near = 10.0f;
static const float far = 150.0f;

// Calc tess factor based on distance between camera.
// It will automatically convert to on sphere position.
float CalcTessFactor(float3 planePos)
{
    float3 spherePos = normalize(planePos) * 150.0f;
    float d = distance(spherePos, cb.cameraPosition.xyz);
    float s = saturate((d - near) / (far - near));

    return pow(2.0f, (int)(-cb.parameters.w * pow(s, 0.8f) + cb.parameters.w));
}

PatchTess ConstantHS(InputPatch<VS_OUTPUT, 4> patch, int patchID : SV_PrimitiveID)
{
    PatchTess output;

    // Calc center position of patch and Get quad position on PLANE (face of cube).
    float3 planeCenterPos = 0.25f * (patch[0].position.xyz + patch[1].position.xyz + patch[2].position.xyz + patch[3].position.xyz);
    float3 planeQuadPos = patch[3].quadPos;

    float tess = CalcTessFactor(planeQuadPos);

    float width = cb.parameters.x;
    uint unitCount = cb.parameters.y;
    float unitWidth = width / unitCount;

    float3 right;
    float3 up;

    // Face detection.
    if (abs(abs(planeQuadPos.z) - 150.0f) <= 0.001f)
    {
        right = float3(-sign(planeQuadPos.z), 0, 0);
        up = float3(0, 1, 0);
    }
    else if (abs(abs(planeQuadPos.x) - 150.0f) <= 0.001f)
    {
        right = float3(0, 0, sign(planeQuadPos.x));
        up = float3(0, 1, 0);
    }
    else
    {
        right = float3(1, 0, 0);
        up = float3(0, 0, sign(planeQuadPos.y));
    }

    // Check patch is on border or not.
    // If not, use same tess factor.
    float planeQuadPosR = dot(planeQuadPos, right);
    float planeQuadPosU = dot(planeQuadPos, up);
    float planeCenterPosR = dot(planeCenterPos, right);
    float planeCenterPosU = dot(planeCenterPos, up);

    if (abs(planeQuadPosR - planeCenterPosR) <= unitWidth * (unitCount / 2 - 1) &&
        abs(planeQuadPosU - planeCenterPosU) <= unitWidth * (unitCount / 2 - 1))
    {
        output.edgeTess[0] = tess;
        output.edgeTess[1] = tess;
        output.edgeTess[2] = tess;
        output.edgeTess[3] = tess;
        output.insideTess[0] = tess;
        output.insideTess[1] = tess;
        return output;
    }

    // Calc rotation of patch.
    float x0 = dot(patch[0].position.xyz, right);
    float y0 = dot(patch[0].position.xyz, up);
    float x1 = dot(patch[1].position.xyz, right);
    float y1 = dot(patch[1].position.xyz, up);
    uint rotation = (x0 == x1) ? (y0 < y1 ? 0 : 2) : (x0 < x1 ? 1 : 3);

    // Check which border is on.
    bool border[4] =
    {
        planeCenterPosU - unitWidth < planeQuadPosU - width / 2, // bottom?
    	planeCenterPosR - unitWidth < planeQuadPosR - width / 2, // left?
    	planeCenterPosU + unitWidth > planeQuadPosU + width / 2, // top?
    	planeCenterPosR + unitWidth > planeQuadPosR + width / 2  // right?
    };

	// Estimate tess factor of adjacent quad.
    float estTess[4] =
    {
        CalcTessFactor(planeQuadPos - up * width),
    	CalcTessFactor(planeQuadPos - right * width),
    	CalcTessFactor(planeQuadPos + up * width),
    	CalcTessFactor(planeQuadPos + right * width)
    };

    // Check quad is on borer or not.
    bool quadBorder[4] =
    {
        border[0] && planeQuadPosU - width < -150.0f,
        border[1] && planeQuadPosR - width < -150.0f,
        border[2] && planeQuadPosU + width > 150.0f,
        border[3] && planeQuadPosR + width > 150.0f
    };

	// Set tess factor.
	[unroll(4)]
    for (int i = 0; i < 4; i++)
    {
        if (quadBorder[(i + rotation) % 4])
            output.edgeTess[i] = CalcTessFactor(patch[0].position);
        else
			output.edgeTess[i] = border[(i + rotation) % 4] ? min(estTess[(i + rotation) % 4], tess) : tess;
    }
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

    return output;
}


//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
// Calc level (mipmap level) based on input tess factor.
// Range is 0 ~ 6 (mipmap has 10 levels but use only 7).
float CalcLevel(float tess)
{
    return max(0, (cb.parameters.w - 2) - (int) log2(tess));
}

[domain("quad")]
DS_OUT DS(const OutputPatch<HS_OUT, 4> input, float2 uv : SV_DomainLocation, PatchTess patch)
{
    DS_OUT output;
	
	// Bilinear interpolation (position).
    float3 v1 = lerp(input[0].position, input[1].position, uv.x);
    float3 v2 = lerp(input[2].position, input[3].position, uv.x);
    float3 position = lerp(v1, v2, uv.y);

    uint level = CalcLevel(patch.insideTess[0]);
    if (distance(uv, float2(0.5f, 0.5f)) >= sqrt(2) / 2.0f) // patch on corner.
    {
        level = 0;
    }
    else // patch on border.
    {
        level = uv.x == 0 ? CalcLevel(patch.edgeTess[0]) : uv.x == 1 ? CalcLevel(patch.edgeTess[2]) : level;
        level = uv.y == 0 ? CalcLevel(patch.edgeTess[1]) : uv.y == 1 ? CalcLevel(patch.edgeTess[3]) : level;
    }
    // both not, just use calculated level.

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
    float2 sTexCoord = float2(round(gTexCoord.x) == 0 ? saturate(gTexCoord.x * 2) : saturate((gTexCoord.x - 0.5f) * 2.0f), gTexCoord.y);

    // Get height from texture.
    float height = texMap[texIndex].SampleLevel(samAnisotropic, sTexCoord, level).r;
    float3 catPos = normCatPos * (150.0f + height * 0.6f);

    // Multiply MVP matrices.
    output.position = mul(float4(catPos, 1.0f), cb.worldMatrix);
    output.position = mul(output.position, cb.viewProjMatrix);

    // Set cartesian position for calc texture coordinates in pixel shader.
    output.catPos = catPos;

    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float3 GetNormalFromHeight(Texture2D tex, float2 texSize, float2 sTexCoord, float multiplier)
{
    float2 xmOffset = { -3.f / texSize.x, 0 };
    float2 xpOffset = { +3.f / texSize.y, 0 };
    float2 ymOffset = { 0, -1.0f / texSize.x };
    float2 ypOffset = { 0, +1.0f / texSize.y };

    float xm = tex.Sample(samAnisotropic, sTexCoord + xmOffset * multiplier).r;
    float xp = tex.Sample(samAnisotropic, sTexCoord + xpOffset * multiplier).r;
    float ym = tex.Sample(samAnisotropic, sTexCoord + ymOffset * multiplier).r;
    float yp = tex.Sample(samAnisotropic, sTexCoord + ypOffset * multiplier).r;

    float3 va = normalize(float3(1.0f, 0, (xp - xm) * 0.1f));
    float3 vb = normalize(float3(0, 1.0f, (yp - ym) * 0.1f));

    return normalize(cross(va, vb));
}

float3 GetTBNNormal(Texture2D tex, float2 sTexCoord, float3x3 TBN)
{
    uint width, height, numMips;
    tex.GetDimensions(0, width, height, numMips);
    float2 texSize = float2(width, height);

	// Calculate local normal from height map.
    float3 localNormal = GetNormalFromHeight(tex, texSize, sTexCoord, 1.0f);
    localNormal = normalize(localNormal);

    return normalize(mul(localNormal, TBN));
}

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z - 0.000773f;

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

    [unroll(9)]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += texMap[4].SampleCmpLevelZero(samShadow, saturate(shadowPosH.xy + offsets[i]), depth).r;
    }
    
    return percentLit / 9.0f;
}

float hash(float2 p)
{
    return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

float noise(float2 x)
{
    float2 i = floor(x);
    float2 f = frac(x);

	// Four corners in 2D of a tile
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

PS_OUTPUT PS(DS_OUT input)
{
    PS_OUTPUT output;

    // Convert cartesian to polar.
    float3 normCatPos = normalize(input.catPos);

    float theta = atan2(normCatPos.z, normCatPos.x);
    theta = sign(theta) == -1 ? 2 * PI + theta : theta;
    float phi = acos(normCatPos.y);

    // Convert polar coordinates to texture coordinates.
    float2 gTexCoord = float2(theta / (2 * PI), phi / PI);

    // Divide texture coordinates into two parts and re-mapping.
    int texIndex = round(gTexCoord.x);
    float2 sTexCoord = float2(texIndex == 0 ? gTexCoord.x * 2 : (gTexCoord.x - 0.5f) * 2.0f, gTexCoord.y);

    // Calculate TBN Matrix.
    float3 N = normCatPos;
    float3 T = float3(-sin(theta), 0, cos(theta));
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(normalize(T), normalize(B), normalize(N));

    // Merge Results.
    float4 texColor = texMap[texIndex].Sample(samAnisotropic, sTexCoord);
    float3 normal = GetTBNNormal(texMap[texIndex + 2], sTexCoord, TBN);
    
    float3 diffuse = saturate(dot(normal, -cb.lightDirection.xyz)) * cb.lightColor.xyz;
    float3 ambient = float3(0.008f, 0.008f, 0.008f) * cb.lightColor.xyz;

    float shadowFactor = CalcShadowFactor(mul(float4(input.catPos, 1.0f), cb.shadowTransform));
    float shadowCorrector = lerp(0.7f, 1.0f, max(dot(normCatPos, -cb.lightDirection.xyz), 0.0f));

    float highNoise = lerp(0.92f, 1.0f, noise(sTexCoord * 60000.0f));

    float h = distance(input.catPos, float3(0, 0, 0));
    h -= 140.0f;
    h /= 20;

    float4 final = float4(
		saturate((diffuse * saturate(shadowFactor + shadowCorrector) + ambient) 
		* texColor.rgb 
		* highNoise 
		* lerp(0.95f, 1.0f, h)), 1);

    output.color = final;

    return output;
}