//--------------------------------------------------------------------------------------
// VertexShader.hlsl
//
// Simple vertex shader for rendering a triangle
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct Vertex
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct Interpolants
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

cbuffer ConstantBuffer : register(b0)
{
    float4 colorMultiplier;
};

Interpolants main(Vertex In)
{
    Interpolants Out;
    Out.position = In.position;
    Out.color = In.color * colorMultiplier;

    return Out;
}