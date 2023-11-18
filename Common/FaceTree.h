//
// FaceTree
//

#pragma once
#include "pch.h"

#include "Frustum.h"
#include "VertexTypes.h"

class QuadNode
{
public:
	QuadNode(char level, uint32_t indexCount, uint32_t index[4], uint32_t baseAddress, float width)
	{
		m_level = level;
		m_indexCount = indexCount;
		memcpy(m_cornerIndex, index, sizeof(uint32_t) * 4);
		m_baseAddress = baseAddress;
		m_width = width;
	}
	~QuadNode()
	{
		for (const auto& c : m_children)
		{
			delete c;
		}
	}

	void CreateChildren(
		const char limit, 
		const std::vector<DirectX::VertexPosition>& vertices, 
		const std::vector<uint32_t>& indices)
	{
		if (m_level + 1 > limit)
			return;

		for (int c = 0; c < 4; c++)
		{
			const uint32_t qic = m_indexCount / 4;
			const uint32_t qqic = qic / 4;

			uint32_t index[4];
			index[0] = indices[0 * qqic + c * qic + m_baseAddress];
			index[1] = indices[1 * qqic + c * qic + m_baseAddress];
			index[2] = indices[2 * qqic + c * qic + m_baseAddress];
			index[3] = indices[3 * qqic + c * qic + m_baseAddress];

			const auto child = new QuadNode(m_level + 1, qic, index, c * qic + m_baseAddress, m_width / 4);
			child->CalcCenter(vertices);
			child->CreateChildren(limit, vertices, indices);

			m_children[c] = child;
		}
	}

	void CalcCenter(const std::vector<DirectX::VertexPosition>& vertices)
	{
		auto center = DirectX::XMFLOAT3(0, 0, 0);
		for (const uint32_t& i : m_cornerIndex)
		{
			center = DirectX::XMFLOAT3(
				center.x + vertices[i].position.x,
				center.y + vertices[i].position.y,
				center.z + vertices[i].position.z
			);
		}
		center = DirectX::XMFLOAT3(center.x / 4.0f, center.y / 4.0f, center.z / 4.0f);
		m_centerPosition = center;
	}

	void Render(IN Frustum& frustum, IN const std::vector<uint32_t>& indices, OUT std::vector<uint32_t>& retVec) const
	{
		if (!frustum.CheckCube(m_centerPosition.x, m_centerPosition.y, m_centerPosition.z, m_width / 2.0f))
		{
			return;
		}

		int visibleChildCount = 0;
		for (int i = 0; i < 4; i++)
		{
			if (m_children[i] != nullptr)
			{
				visibleChildCount++;
				m_children[i]->Render(frustum, indices, retVec);
			}
		}

		if (visibleChildCount != 0)
			return;

		retVec.insert(retVec.end(), &indices[m_baseAddress], &indices[m_baseAddress] + m_indexCount);
	}

public:
	char									m_level;
	uint32_t								m_indexCount;
	uint32_t								m_cornerIndex[4];
	uint32_t								m_baseAddress;
	DirectX::XMFLOAT3						m_centerPosition;
	float									m_width;
	QuadNode*								m_children[4] = { nullptr, nullptr, nullptr, nullptr };
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