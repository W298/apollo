#pragma once

using namespace DirectX;

class Frustum
{
public:
	Frustum() {}
	~Frustum() {}

	void ConstructFrustum(float screenDepth, const DirectX::XMMATRIX& viewMatrix, DirectX::XMMATRIX projectionMatrix)
	{
        XMFLOAT4X4 pMatrix;
        XMStoreFloat4x4(&pMatrix, projectionMatrix);

        float zMinimum = -pMatrix._43 / pMatrix._33;
        float r = screenDepth / (screenDepth - zMinimum);

        pMatrix._33 = r;
        pMatrix._43 = -r * zMinimum;
        projectionMatrix = XMLoadFloat4x4(&pMatrix);

        XMMATRIX finalMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);

        XMFLOAT4X4 matrix;
        XMStoreFloat4x4(&matrix, finalMatrix);

        float x = (float)(matrix._14 + matrix._13);
        float y = (float)(matrix._24 + matrix._23);
        float z = (float)(matrix._34 + matrix._33);
        float w = (float)(matrix._44 + matrix._43);
        m_planes[0] = XMVectorSet(x, y, z, w);
        m_planes[0] = XMPlaneNormalize(m_planes[0]);

        x = (float)(matrix._14 - matrix._13);
        y = (float)(matrix._24 - matrix._23);
        z = (float)(matrix._34 - matrix._33);
        w = (float)(matrix._44 - matrix._43);
        m_planes[1] = XMVectorSet(x, y, z, w);
        m_planes[1] = XMPlaneNormalize(m_planes[1]);

        x = (float)(matrix._14 + matrix._11);
        y = (float)(matrix._24 + matrix._21);
        z = (float)(matrix._34 + matrix._31);
        w = (float)(matrix._44 + matrix._41);
        m_planes[2] = XMVectorSet(x, y, z, w);
        m_planes[2] = XMPlaneNormalize(m_planes[2]);

        x = (float)(matrix._14 - matrix._11);
        y = (float)(matrix._24 - matrix._21);
        z = (float)(matrix._34 - matrix._31);
        w = (float)(matrix._44 - matrix._41);
        m_planes[3] = XMVectorSet(x, y, z, w);
        m_planes[3] = XMPlaneNormalize(m_planes[3]);

        x = (float)(matrix._14 - matrix._12);
        y = (float)(matrix._24 - matrix._22);
        z = (float)(matrix._34 - matrix._32);
        w = (float)(matrix._44 - matrix._42);
        m_planes[4] = XMVectorSet(x, y, z, w);
        m_planes[4] = XMPlaneNormalize(m_planes[4]);

        x = (float)(matrix._14 + matrix._12);
        y = (float)(matrix._24 + matrix._22);
        z = (float)(matrix._34 + matrix._32);
        w = (float)(matrix._44 + matrix._42);
        m_planes[5] = XMVectorSet(x, y, z, w);
        m_planes[5] = XMPlaneNormalize(m_planes[5]);
	}

	bool CheckPoint(float x, float y, float z)
	{
        for (int i = 0; i < 6; i++)
        {
            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet(x, y, z, 1.0f))) < 0.0f)
                return false;
        }

        return true;
	}

	bool CheckCube(float xCenter, float yCenter, float zCenter, float radius)
	{
        for (int i = 0; i < 6; i++)
        {
            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - radius), (yCenter - radius),
                (zCenter - radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + radius), (yCenter - radius),
                (zCenter - radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - radius), (yCenter + radius),
                (zCenter - radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + radius), (yCenter + radius),
                (zCenter - radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - radius), (yCenter - radius),
                (zCenter + radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + radius), (yCenter - radius),
                (zCenter + radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - radius), (yCenter + radius),
                (zCenter + radius), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + radius), (yCenter + radius),
                (zCenter + radius), 1.0f))) >= 0.0f)
                continue;

            return false;
        }

        return true;
	}

	bool CheckSphere(float xCenter, float yCenter, float zCenter, float radius)
	{
        for (int i = 0; i < 6; i++)
        {
            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet(xCenter, yCenter, zCenter, 1.0f))) < -radius)
                return false;
        }

        return true;
	}

	bool CheckRectangle(float xCenter, float yCenter, float zCenter, float xSize, float ySize, float zSize)
	{
        for (int i = 0; i < 6; i++)
        {
            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter - zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter - zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter - zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter + zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter - zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter + zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter + zSize), 1.0f))) >= 0.0f)
                continue;

            if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter + zSize), 1.0f))) >= 0.0f)
                continue;

            return false;
        }

        return true;
	}

private:
	DirectX::XMVECTOR m_planes[6];
};