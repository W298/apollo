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
		std::vector<DirectX::VertexPosition> vertices;
        std::vector<std::uint32_t> indices;
	};

	struct GeometryInfo
	{
		std::vector<DirectX::VertexPosition> vertices;
		std::vector<uint32_t> indices;
		std::vector<FaceTree*> faceTrees;

		GeometryInfo(
			const std::vector<DirectX::VertexPosition>& vertices,
			const std::vector<uint32_t>& indices,
			const std::vector<FaceTree*>& faceTrees)
		{
			this->vertices = vertices;
			this->indices = indices;
			this->faceTrees = faceTrees;
		}
	};

	static GeometryInfo* CreateQuadBox(float width, float height, float depth, std::uint32_t numSubdivisions);
private:
	static void SubdivideQuad(MeshData& meshData);
	static DirectX::VertexPosition MidPoint(const DirectX::VertexPosition& v0, const DirectX::VertexPosition& v1);
};