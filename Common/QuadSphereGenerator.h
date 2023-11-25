#pragma once

#include <cstdint>
#include <vector>

#include "FaceTree.h"

class QuadSphereGenerator
{
public:
	struct MeshData
	{
		std::vector<VertexTess> vertices;
        std::vector<std::uint32_t> indices;
	};

	struct QuadSphereInfo
	{
		std::vector<VertexTess> vertices;
		std::vector<uint32_t> indices;
		std::vector<FaceTree*> faceTrees;

		QuadSphereInfo(
			const std::vector<VertexTess>& vertices,
			const std::vector<uint32_t>& indices,
			const std::vector<FaceTree*>& faceTrees)
		{
			this->vertices = vertices;
			this->indices = indices;
			this->faceTrees = faceTrees;
		}
	};

	static QuadSphereInfo* CreateQuadSphere(
		float width, float height, float depth,
		std::uint32_t numSubdivisions);
private:
	static void SubdivideQuad(MeshData& meshData);
	static VertexTess MidPoint(const VertexTess& v0, const VertexTess& v1);
};