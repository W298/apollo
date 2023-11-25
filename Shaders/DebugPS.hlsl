struct VS_OUTPUT
{
    float4 position : SV_Position;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};


PS_OUTPUT PS(VS_OUTPUT input)
{
    PS_OUTPUT output;
    output.color = float4(1, 1, 1, 1);

    return output;
}