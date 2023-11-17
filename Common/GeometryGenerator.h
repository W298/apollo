#pragma once

#include <cstdint>
#include <vector>

#include "FaceTree.h"
#include "VertexTypes.h"

class GeometryGenerator
{
public:
	struct MeshData
	{
	public:
		std::vector<DirectX::VertexPosition> vertices;
        std::vector<std::uint32_t> indices;
	};

	struct GeometryInfo
	{
		std::vector<DirectX::VertexPosition> vertices;
		std::vector<FaceTree*> faceTrees;
		uint32_t totalIndexCount;

		GeometryInfo(
			const std::vector<DirectX::VertexPosition>& vertices, 
			const std::vector<FaceTree*>& faceTrees, uint32_t totalIndexCount)
		{
			this->vertices = vertices;
			this->faceTrees = faceTrees;
			this->totalIndexCount = totalIndexCount;
		}
	};

	static GeometryInfo* CreateQuadBox(float width, float height, float depth, std::uint32_t numSubdivisions);
private:
	static void SubdivideQuad(MeshData& meshData);
	static DirectX::VertexPosition MidPoint(const DirectX::VertexPosition& v0, const DirectX::VertexPosition& v1);
};