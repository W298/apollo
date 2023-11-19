#pragma once

#include <SimpleMath.h>

#include "VertexTypes.h"

class QuadNode
{
public:
	QuadNode(char level, uint32_t indexCount, uint32_t index[4], uint32_t baseAddress, float width);
	~QuadNode();

	void CreateChildren(
		const char limit,
		const std::vector<DirectX::VertexPosition>& vertices,
		const std::vector<uint32_t>& indices, 
		std::vector<DirectX::VertexPosition>& debugVertexData, 
		std::vector<uint32_t>& debugIndexData);

	void CalcCenter(
		const std::vector<DirectX::VertexPosition>& vertices, 
		std::vector<DirectX::VertexPosition>& debugVertexData, 
		std::vector<uint32_t>& debugIndexData);

	void Render(
		IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices, 
		OUT std::vector<uint32_t>& retVec, OUT uint32_t& culledQuadCount) const;

private:
	char									m_level;
	uint32_t								m_indexCount;
	uint32_t								m_cornerIndex[4];
	uint32_t								m_baseAddress;
	DirectX::XMFLOAT3						m_centerPosition;
	DirectX::BoundingOrientedBox			m_obb;
	float									m_width;
	QuadNode*								m_children[4] = { nullptr, nullptr, nullptr, nullptr };
};

class FaceTree
{
public:
	FaceTree(QuadNode* rootNode, UINT32 faceIndexCount) : m_rootNode(rootNode), m_faceIndexCount(faceIndexCount) {}
	~FaceTree() { delete m_rootNode; }

	QuadNode*								GetRootNode() const { return m_rootNode; }
private:
	QuadNode*								m_rootNode;
	uint32_t								m_faceIndexCount;
};