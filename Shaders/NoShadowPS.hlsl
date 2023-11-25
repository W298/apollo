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
    float quadWidth;
    uint unitCount;
};

ConstantBuffer<OpaqueCBType> cb : register(b0);


//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
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
SamplerState anisotropicClampMip1 : register(s2);


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float3 GetNormalFromHeight(Texture2D tex, float2 texSize, float2 sTexCoord, float multiplier)
{
    float2 xmOffset = { -1.0f / texSize.x, 0 };
    float2 xpOffset = { +1.0f / texSize.y, 0 };
    float2 ymOffset = { 0, -1.0f / texSize.x };
    float2 ypOffset = { 0, +1.0f / texSize.y };

    float xm = tex.Sample(anisotropicClampMip1, sTexCoord + xmOffset * multiplier).r;
    float xp = tex.Sample(anisotropicClampMip1, sTexCoord + xpOffset * multiplier).r;
    float ym = tex.Sample(anisotropicClampMip1, sTexCoord + ymOffset * multiplier).r;
    float yp = tex.Sample(anisotropicClampMip1, sTexCoord + ypOffset * multiplier).r;

    float3 va = normalize(float3(1.0f, 0, (xp - xm) * 0.4f));
    float3 vb = normalize(float3(0, 1.0f, (yp - ym) * 0.4f));

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

    float lowNoise = lerp(0.95f, 1.0f, noise(sTexCoord * 30000.0f));
    float highNoise = lerp(0.92f, 1.0f, noise(sTexCoord * 80000.0f));

    float h = distance(input.catPos, float3(0, 0, 0));
    h -= 149.0f;

    float4 final = float4(
        saturate((diffuse + ambient)
            * texColor.rgb
            * lowNoise * highNoise
            * lerp(0.98f, 1.0f, h)), 1);

    output.color = final;

    return output;
}