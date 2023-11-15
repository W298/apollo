#pragma once

#include <cstdint>
#include <vector>

#include "VertexTypes.h"

class GeometryGenerator
{
public:
	struct MeshData
	{
	public:
		std::vector<DirectX::VertexPosition> Vertices;
        std::vector<std::uint32_t> Indices32;
	};

	static MeshData CreateQuadBox(float width, float height, float depth, std::uint32_t numSubdivisions);
private:
	static void SubdivideQuad(MeshData& meshData);
	static DirectX::VertexPosition MidPoint(const DirectX::VertexPosition& v0, const DirectX::VertexPosition& v1);
};

