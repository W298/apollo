#include "pch.h"
#include "FaceTree.h"

using namespace DirectX;

QuadNode::QuadNode(char level, uint32_t indexCount, uint32_t index[4], uint32_t baseAddress, float width)
{
	m_level = level;
	m_indexCount = indexCount;
	memcpy(m_cornerIndex, index, sizeof(uint32_t) * 4);
	m_baseAddress = baseAddress;
	m_width = width;
}

QuadNode::~QuadNode()
{
	for (const auto& c : m_children)
		delete c;
}

void QuadNode::CreateChildren(
	const char limit,
	const std::vector<VertexPosition>& vertices,
	const std::vector<uint32_t>& indices, 
	std::vector<VertexPosition>& debugVertexData, std::vector<uint32_t>& debugIndexData)
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

void QuadNode::CalcCenter(
	const std::vector<VertexPosition>& vertices, 
	std::vector<VertexPosition>& debugVertexData, std::vector<uint32_t>& debugIndexData)
{
	// Calculate center position with corner position
	auto center = XMVectorSet(0, 0, 0, 0);
	for (const uint32_t& i : m_cornerIndex)
	{
		center += XMLoadFloat3(&vertices[i].position);
	}
	center /= 4.0f;

	// Calculate height fit with sphere
	float h = 150.0f * sin(acos(0.5f * m_width / 150.0f));

	// Manipulate center position
	center = XMVector3Normalize(center) * (h);
	XMStoreFloat3(&m_centerPosition, center);

	// Calculate TBN
	auto n = SimpleMath::Vector3(XMVector3Normalize(center));

	const float theta = atan2(n.z, n.x);
	auto t = SimpleMath::Vector3(-sin(theta), 0.0f, cos(theta));
	t.Normalize();

	auto b = n.Cross(t);
	b.Normalize();

	// Calculate quaternion
	const auto q = SimpleMath::Quaternion::CreateFromRotationMatrix(
		SimpleMath::Matrix(
			SimpleMath::Vector4(t), SimpleMath::Vector4(b), 
			SimpleMath::Vector4(n), SimpleMath::Vector4::Zero));

	const auto quaternionVec = XMFLOAT4(q.x, q.y, q.z, q.w);

	// Create OBB with little bigger size
	m_obb = BoundingOrientedBox(
		m_centerPosition,
		XMFLOAT3(m_width * 0.6f, m_width * 0.6f, 0.1f),
		quaternionVec);


	// For Debug
	XMFLOAT3 corners[8];
	m_obb.GetCorners(corners);

	if (m_level != 1) return;

	std::vector<uint32_t> ary = { 0, 1, 2, 2, 3, 0, 4, 0, 3, 3, 7, 4, 5, 4, 7, 7, 6, 5, 1, 5, 6, 6, 2, 1, 2, 6, 7, 7, 3, 2, 5, 1, 0, 0, 4, 5 };
	for (int i = 0; i < ary.size(); i++)
	{
		ary[i] += debugVertexData.size();
	}

	debugIndexData.insert(debugIndexData.end(), ary.begin(), ary.end());

	for (XMFLOAT3 corner : corners)
	{
		debugVertexData.push_back(VertexPosition(corner));
	}
}

void QuadNode::Render(
	IN BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices, 
	OUT std::vector<uint32_t>& retVec, OUT uint32_t& culledQuadCount) const
{
	const ContainmentType result = frustum.Contains(m_obb);

	// Do not cull in level 0, 1
	if (result <= 0 && m_level >= 1)
	{
		culledQuadCount += m_indexCount / 4;
		return;
	}

	// Try to render children
	bool anyChildVisible = false;
	for (const auto c : m_children)
	{
		if (c != nullptr)
		{
			anyChildVisible = true;
			c->Render(frustum, indices, retVec, culledQuadCount);
		}
	}

	// If no child is visible, render this node
	if (!anyChildVisible)
		retVec.insert(retVec.end(), &indices[m_baseAddress], &indices[m_baseAddress] + m_indexCount);
}

void QuadNode::Render(
	IN BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices,
	OUT std::vector<uint32_t>& retVec1, OUT std::vector<uint32_t>& retVec2, OUT uint32_t& culledQuadCount) const
{
	const ContainmentType result = frustum.Contains(m_obb);

	// Do not cull in level 0, 1
	if (result <= 0 && m_level >= 1)
	{
		culledQuadCount += m_indexCount / 4;
		return;
	}

	// Try to render children
	bool anyChildVisible = false;
	for (int c = 0; c < 2; c++)
	{
		if (m_children[c] != nullptr)
		{
			anyChildVisible = true;
			m_children[c]->Render(frustum, indices, retVec1, culledQuadCount);
		}
	}
	for (int c = 2; c < 4; c++)
	{
		if (m_children[c] != nullptr)
		{
			anyChildVisible = true;
			m_children[c]->Render(frustum, indices, retVec2, culledQuadCount);
		}
	}

	// If no child is visible, render this node
	if (!anyChildVisible)
	{
		retVec1.insert(retVec1.end(), &indices[m_baseAddress], &indices[m_baseAddress] + m_indexCount / 2);
		retVec2.insert(retVec2.end(), &indices[m_baseAddress] + m_indexCount / 2, &indices[m_baseAddress] + m_indexCount);
	}
}