//
// FaceTree
//

#pragma once
#include "pch.h"

#include "VertexTypes.h"

class QuadNode
{
public:
	QuadNode(char level, uint32_t indexCount, uint32_t index[4])
	{
		m_level = level;
		m_indexCount = indexCount;
		memcpy(m_index, index, sizeof(uint32_t) * 4);
	}

	void CreateChildren(
		const char limit, 
		const std::vector<DirectX::VertexPosition>& vertices, 
		const std::vector<uint32_t>& indices, uint32_t base)
	{
		if (m_level + 1 > limit)
			return;

		for (int c = 0; c < 4; c++)
		{
			const uint32_t qic = m_indexCount / 4;
			const uint32_t qqic = qic / 4;

			uint32_t index[4];
			index[0] = indices[0 * qqic + c * qic + base];
			index[1] = indices[1 * qqic + c * qic + base];
			index[2] = indices[2 * qqic + c * qic + base];
			index[3] = indices[3 * qqic + c * qic + base];

			const auto child = new QuadNode(m_level + 1, qic, index);
			child->CalcCenter(vertices);
			child->CreateChildren(limit, vertices, indices, c * qic + base);

			m_children[c] = child;
		}
	}

	void CalcCenter(const std::vector<DirectX::VertexPosition>& vertices)
	{
		auto center = DirectX::XMFLOAT3(0, 0, 0);
		for (int j = 0; j < 4; j++)
		{
			center = DirectX::XMFLOAT3(
				center.x + vertices[m_index[j]].position.x,
				center.y + vertices[m_index[j]].position.y,
				center.z + vertices[m_index[j]].position.z
			);
		}
		center = DirectX::XMFLOAT3(center.x / 4.0f, center.y / 4.0f, center.z / 4.0f);
		m_centerPosition = center;
	}

public:
	char									m_level;
	uint32_t								m_indexCount;
	uint32_t								m_index[4];
	DirectX::XMFLOAT3						m_centerPosition;
	QuadNode*								m_children[4];
};

class FaceTree
{
public:
	FaceTree(QuadNode* rootNode, UINT32 faceIndexCount)
	{
		m_rootNode = rootNode;
		m_faceIndexCount = faceIndexCount;
	}
	~FaceTree()
	{
		delete m_rootNode;
	}

public:
	QuadNode*								m_rootNode;
	uint32_t								m_faceIndexCount;
};