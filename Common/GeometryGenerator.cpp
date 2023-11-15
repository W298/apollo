#include "pch.h"
#include "GeometryGenerator.h"

using namespace std;
using namespace DirectX;

GeometryGenerator::MeshData GeometryGenerator::CreateQuadBox(float width, float height, float depth, std::uint32_t numSubdivisions)
{
	MeshData meshData;

	// Create the vertices.
	VertexPosition v[8];

	const float w2 = 0.5f * width;
	const float h2 = 0.5f * height;
	const float d2 = 0.5f * depth;

	// Fill in the front face vertex data.
	v[0] = VertexPosition(XMFLOAT3(-w2, -h2, -d2));
	v[1] = VertexPosition(XMFLOAT3(-w2, +h2, -d2));
	v[2] = VertexPosition(XMFLOAT3(+w2, +h2, -d2));
	v[3] = VertexPosition(XMFLOAT3(+w2, -h2, -d2));

	// Fill in the back face vertex data.
	v[4] = VertexPosition(XMFLOAT3(-w2, -h2, +d2));
	v[5] = VertexPosition(XMFLOAT3(+w2, -h2, +d2));
	v[6] = VertexPosition(XMFLOAT3(+w2, +h2, +d2));
	v[7] = VertexPosition(XMFLOAT3(-w2, +h2, +d2));

	meshData.Vertices.assign(&v[0], &v[8]);

	// Create the indices.
	uint32_t i[24];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 3; i[3] = 2;

	// Fill in the back face index data
	i[4] = 4; i[5] = 5; i[6] = 7; i[7] = 6;

	// Fill in the top face index data
	i[8] = 1; i[9] = 7; i[10] = 2; i[11] = 6;

	// Fill in the bottom face index data
	i[12] = 3; i[13] = 5; i[14] = 0; i[15] = 4;

	// Fill in the left face index data
	i[16] = 4; i[17] = 7; i[18] = 0; i[19] = 1;

	// Fill in the right face index data
	i[20] = 3; i[21] = 2; i[22] = 5; i[23] = 6;

	meshData.Indices32.assign(&i[0], &i[24]);

	for (uint32_t idx = 0; idx < numSubdivisions; ++idx)
		SubdivideQuad(meshData);

	return meshData;
}

void GeometryGenerator::SubdivideQuad(MeshData& meshData)
{
	// Save a copy of the input geometry.
	const MeshData inputCopy = meshData;

	// Reset Only indices.
	meshData.Indices32.resize(0);

	const uint32_t numQuads = static_cast<uint32_t>(inputCopy.Indices32.size()) / 4;
	for (uint32_t i = 0; i < numQuads; ++i)
	{
		const int v0i = inputCopy.Indices32[i*4+0];
		const int v1i = inputCopy.Indices32[i*4+1];
		const int v2i = inputCopy.Indices32[i*4+3];
		const int v3i = inputCopy.Indices32[i*4+2];

		VertexPosition v0 = inputCopy.Vertices[v0i];
		VertexPosition v1 = inputCopy.Vertices[v1i];
		VertexPosition v2 = inputCopy.Vertices[v2i];
		VertexPosition v3 = inputCopy.Vertices[v3i];

		// Generate the midpoints.
		VertexPosition m0 = MidPoint(v0, v1);
		VertexPosition m1 = MidPoint(v1, v2);
		VertexPosition m2 = MidPoint(v2, v3);
		VertexPosition m3 = MidPoint(v3, v0);
		VertexPosition m4 = MidPoint(m0, m2);

		// Add midpoints to vector.
		const int m0i = static_cast<int>(meshData.Vertices.size());
		const int m1i = m0i + 1;
		const int m2i = m1i + 1;
		const int m3i = m2i + 1;
		const int m4i = m3i + 1;

		meshData.Vertices.push_back(m0);
		meshData.Vertices.push_back(m1);
		meshData.Vertices.push_back(m2);
		meshData.Vertices.push_back(m3);
		meshData.Vertices.push_back(m4);

		// Add indices to vector.
		meshData.Indices32.push_back(v0i);
		meshData.Indices32.push_back(m0i);
		meshData.Indices32.push_back(m3i);
		meshData.Indices32.push_back(m4i);

		meshData.Indices32.push_back(m0i);
		meshData.Indices32.push_back(v1i);
		meshData.Indices32.push_back(m4i);
		meshData.Indices32.push_back(m1i);

		meshData.Indices32.push_back(m4i);
		meshData.Indices32.push_back(m1i);
		meshData.Indices32.push_back(m2i);
		meshData.Indices32.push_back(v2i);

		meshData.Indices32.push_back(m3i);
		meshData.Indices32.push_back(m4i);
		meshData.Indices32.push_back(v3i);
		meshData.Indices32.push_back(m2i);
	}
}

VertexPosition GeometryGenerator::MidPoint(const VertexPosition& v0, const VertexPosition& v1)
{
	const XMVECTOR p0 = XMLoadFloat3(&v0.position);
	const XMVECTOR p1 = XMLoadFloat3(&v1.position);
	const XMVECTOR pos = 0.5f * (p0 + p1);

	VertexPosition v;
    XMStoreFloat3(&v.position, pos);

    return v;
}
