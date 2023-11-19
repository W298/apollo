//
// FaceTree
//

#pragma once
#include "pch.h"

#include <SimpleMath.h>

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
		const std::vector<uint32_t>& indices, std::vector<VertexPosition>& debugVertexData, std::vector<uint32_t>& debugIndexData)
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

			const auto child = new QuadNode(m_level + 1, qic, index, c * qic + m_baseAddress, m_width / 2);
			child->CalcCenter(vertices, debugVertexData, debugIndexData);
			child->CreateChildren(limit, vertices, indices, debugVertexData, debugIndexData);

			m_children[c] = child;
		}
	}

	void CalcCenter(const std::vector<DirectX::VertexPosition>& vertices, std::vector<VertexPosition>& debugVertexData, std::vector<uint32_t>& debugIndexData)
	{
		auto center = DirectX::XMVectorSet(0, 0, 0, 0);
		for (const uint32_t& i : m_cornerIndex)
		{
			center += XMLoadFloat3(&vertices[i].position);
		}
		center /= 4.0f;

		float h = 150.0f * sin(acos(0.5f * m_width / 150.0f));

		center = XMVector3Normalize(center) * (h);
		XMStoreFloat3(&m_centerPosition, center);

		auto n = SimpleMath::Vector3(XMVector3Normalize(center));

		float theta = atan2(n.z, n.x);
		auto t = SimpleMath::Vector3(-sin(theta), 0.0f, cos(theta));
		t.Normalize();

		auto b = n.Cross(t);
		b.Normalize();

		SimpleMath::Quaternion q = SimpleMath::Quaternion::CreateFromRotationMatrix(SimpleMath::Matrix(SimpleMath::Vector4(t), SimpleMath::Vector4(b), SimpleMath::Vector4(n), SimpleMath::Vector4::Zero));

		XMFLOAT4 qv = XMFLOAT4(q.x, q.y, q.z, q.w);

		XMFLOAT3 centerf4;
		XMStoreFloat3(&centerf4, center);

		m_obb = BoundingOrientedBox(
			centerf4,
			XMFLOAT3(m_width * 0.5f, m_width * 0.5f, 0.1f),
			qv);

		XMFLOAT3 corners[8];
		m_obb.GetCorners(corners);

		if (m_level != 3) return;

		std::vector<uint32_t> tary = { 0, 1, 2, 2, 3, 0, 4, 0, 3, 3, 7, 4, 5, 4, 7, 7, 6, 5, 1, 5, 6, 6, 2, 1, 2, 6, 7, 7, 3, 2, 5, 1, 0, 0, 4, 5 };
		for (int i = 0; i < tary.size(); i++)
		{
			tary[i] += debugVertexData.size();
		}

		debugIndexData.insert(debugIndexData.end(), tary.begin(), tary.end());

		for (XMFLOAT3 corner : corners)
		{
			debugVertexData.push_back(VertexPosition(corner));
		}
	}

	void Render(IN BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices, OUT std::vector<uint32_t>& retVec, OUT uint32_t& culledQuadCount) const
	{
		const ContainmentType result = frustum.Contains(m_obb);
		if (result <= 0 && m_level >= 2)
		{
			culledQuadCount += m_indexCount / 4;
			return;
		}

		int visibleChildCount = 0;
		for (int i = 0; i < 4; i++)
		{
			if (m_children[i] != nullptr)
			{
				visibleChildCount++;
				m_children[i]->Render(frustum, indices, retVec, culledQuadCount);
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
	BoundingOrientedBox						m_obb;
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