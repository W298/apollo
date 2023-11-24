#define PI 3.1415926538


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct ShadowCBType
{
    float4x4 lightWorldMatrix;
    float4x4 lightViewProjMatrix;
    float4 cameraPosition;
    float quadWidth;
    uint unitCount;
};

ConstantBuffer<ShadowCBType> cb : register(b1);


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
};


//--------------------------------------------------------------------------------------
// Texture & Sampler Variables
//--------------------------------------------------------------------------------------
Texture2D texMap[5] : register(t0);
SamplerState samAnisotropic : register(s0);


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
static const float near = 20.0f;
static const float far = 150.0f;

// Calc tess factor based on distance between camera.
// It will automatically convert to on sphere position.
float CalcTessFactor(float3 planePos)
{
    float3 spherePos = normalize(planePos) * 150.0f;
    float d = distance(spherePos, cb.cameraPosition.xyz);
    float s = saturate((d - near) / (far - near));

    return pow(2.0f, (int) (-8 * pow(s, 0.25f) + 8));
}

PatchTess ConstantHS(InputPatch<VS_OUTPUT, 4> patch, int patchID : SV_PrimitiveID)
{
    PatchTess output;

    // Calc center position of patch and Get quad position on PLANE (face of cube).
    float3 planeCenterPos = 0.25f * (patch[0].position + patch[1].position + patch[2].position + patch[3].position);
    float3 planeQuadPos = patch[3].quadPos;

    float tess = CalcTessFactor(planeQuadPos);

    float width = cb.quadWidth;
    uint unitCount = cb.unitCount;
    float unitWidth = width / unitCount;

    float3 right;
    float3 up;

    if (abs(planeQuadPos.z + 150.0f) <= 0.001f)
    {
        right = float3(1, 0, 0);
        up = float3(0, 1, 0);
    }
    else if (abs(planeQuadPos.z - 150.0f) <= 0.001f)
    {
        right = float3(-1, 0, 0);
        up = float3(0, 1, 0);
    }
    else if (abs(planeQuadPos.x + 150.0f) <= 0.001f)
    {
        right = float3(0, 0, -1);
        up = float3(0, 1, 0);
    }
    else if (abs(planeQuadPos.x - 150.0f) <= 0.001f)
    {
        right = float3(0, 0, 1);
        up = float3(0, 1, 0);
    }
    else if (abs(planeQuadPos.y + 150.0f) <= 0.001f)
    {
        right = float3(1, 0, 0);
        up = float3(0, 0, -1);
    }
    else
    {
        right = float3(1, 0, 0);
        up = float3(0, 0, 1);
    }

    // Check patch is on border or not.
    // If not, use same tess factor.
    if (distance(planeQuadPos * right, planeCenterPos * right) <= unitWidth * (unitCount / 2 - 1) &&
        distance(planeQuadPos * up, planeCenterPos * up) <= unitWidth * (unitCount / 2 - 1))
    {
        output.edgeTess[0] = tess;
        output.edgeTess[1] = tess;
        output.edgeTess[2] = tess;
        output.edgeTess[3] = tess;
        output.insideTess[0] = tess;
        output.insideTess[1] = tess;
    }
    else
    {
        // Check which border is on.
        bool border[4] = { false, false, false, false };

        if (dot(planeCenterPos, right) + unitWidth > dot(planeQuadPos, right) + width / 2)      // right?
        {
            border[3] = true;
        }
        else if (dot(planeCenterPos, right) - unitWidth < dot(planeQuadPos, right) - width / 2) // left?
        {
            border[1] = true;
        }
        if (dot(planeCenterPos, up) + unitWidth > dot(planeQuadPos, up) + width / 2)            // top?
        {
            border[2] = true;
        }
        else if (dot(planeCenterPos, up) - unitWidth < dot(planeQuadPos, up) - width / 2)       // bottom?
        {
            border[0] = true;
        }

        // Estimate tess factor of adjacent quad.
        float estTess[4] =
        {
            CalcTessFactor(planeQuadPos - up * width),
        	CalcTessFactor(planeQuadPos - right * width),
        	CalcTessFactor(planeQuadPos + up * width),
        	CalcTessFactor(planeQuadPos + right * width)
        };

        // Calc rotation of patch.
        float x0 = dot(patch[0].position, right);
        float y0 = dot(patch[0].position, up);
        float x1 = dot(patch[1].position, right);
        float y1 = dot(patch[1].position, up);
        uint rotation = (x0 == x1) ? (y0 < y1 ? 0 : 2) : (x0 < x1 ? 1 : 3);

        // Rotate border and estTess based on rotation factor.
        bool borderTmp[4] = border;
        float estTessTemp[4] = estTess;

    	[unroll(4)]
        for (int j = 0; j < 4; ++j)
        {
            border[j] = borderTmp[(j + rotation) % 4];
            estTess[j] = estTessTemp[(j + rotation) % 4];
        }

        // Set tess factor.
        [unroll(4)]
        for (int i = 0; i < 4; i++)
        {
            output.edgeTess[i] = border[i] ? min(estTess[i], tess) : tess;
        }
        output.insideTess[0] = tess;
        output.insideTess[1] = tess;
    }

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
    return max(0, 6 - (int) log2(tess));
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

    // If patch is on corner, use level 0.
    // If patch is on border, use edge tess factor.
    if ((uv.x == 0 && (uv.y == 0 || uv.y == 1)) || (uv.x == 1 && (uv.y == 0 || uv.y == 1)))
    {
        level = 0;
    }
    else if (uv.x == 0)
    {
        level = CalcLevel(patch.edgeTess[0]);
    }
    else if (uv.x == 1)
    {
        level = CalcLevel(patch.edgeTess[2]);
    }
    else if (uv.y == 0)
    {
        level = CalcLevel(patch.edgeTess[1]);
    }
    else if (uv.y == 1)
    {
        level = CalcLevel(patch.edgeTess[3]);
    }

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
    float3 catPos = normCatPos * (150.0f + height * 0.4f);

    // Multiply MVP matrices.
    output.position = mul(float4(catPos, 1.0f), cb.lightWorldMatrix);
    output.position = mul(output.position, cb.lightViewProjMatrix);

    return output;
}

void PS(DS_OUT input)
{
    // Nothing to do.
}