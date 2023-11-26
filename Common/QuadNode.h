#pragma once

#define QUAD_NODE_MAX_LEVEL 4u
#define TESS_GROUP_QUAD_LEVEL 5u	// DO NOT CHANGE THIS VALUE

#include <DirectXCollision.h>
#include <SimpleMath.h>

struct VertexTess
{
	DirectX::XMFLOAT3	position;
	DirectX::XMFLOAT3	quadPos;

	explicit VertexTess(
		const DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0, 0, 0),
		const DirectX::XMFLOAT3 quadPos = DirectX::XMFLOAT3(0, 0, 0)) : position(position), quadPos(quadPos) {}
};

class QuadNode
{
public:
	QuadNode(char level, uint32_t indexCount, uint32_t index[4], uint32_t baseAddress, float width);
	~QuadNode();

	void CreateChildren(
		const char limit,
		std::vector<VertexTess>& vertices,
		const std::vector<uint32_t>& indices);

	void CalcCenter(
		std::vector<VertexTess>& vertices,
		const std::vector<uint32_t>& indices);

	void Render(
		IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices,
		OUT std::vector<uint32_t>& retVec, OUT uint32_t& culledQuadCount) const;

	uint32_t	GetIndexCount() const { return m_indexCount; }
	char		GetLevel() const { return m_level; }
	float		GetWidth() const { return m_width; }

private:
	char									m_level;
	uint32_t								m_indexCount;
	uint32_t								m_cornerIndex[4];
	uint32_t								m_baseAddress;
	DirectX::XMFLOAT3						m_centerPosition;
	DirectX::BoundingOrientedBox			m_obb;
	float									m_width;
	QuadNode* m_children[4] = { nullptr, nullptr, nullptr, nullptr };
};
