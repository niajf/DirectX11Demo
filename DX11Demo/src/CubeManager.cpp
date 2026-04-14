#include "DX11Demo/CubeManager.h"
#include "DX11Demo/Raycast.h"
#include <string>

CubeManager g_cubeManager;

void CubeManager::Update(float deltaTime, XMMATRIX projection, XMMATRIX view)
{
	if (button && !buttonPressed)
	{
		buttonPressed = true;
		XMVECTOR origin, direction;
		Raycast::getRayVector(
			mouseX,
			mouseY,
			0.0f,
			0.0f,
			WINDOW_WIDTH,
			WINDOW_HEIGHT,
			0.0f,
			1.0f,
			projection,
			view,
			XMMatrixIdentity(),
			origin,
			direction
		);

		float t = 0.0f;
		if(fabsf(XMVectorGetY(direction)) > 0.0001f)
			t = (- XMVectorGetY(origin) - 0.5f) / XMVectorGetY(direction);

		if (t > 0.0f)
		{
			XMFLOAT3 position;
			XMVECTOR vec = XMVectorAdd(origin, t * direction);
			XMStoreFloat3(&position, vec);
			position.y = -0.5f + (currentScale.y / 2.0f);
			setCube(position);
		}
	}

	if (keyUp && !upPressed)
	{
		upPressed = true;
		upScale();
	}

	if (keyDown && !downPressed)
	{
		downPressed = true;
		downScale();
	}

	if (!button) buttonPressed = false;
	if (!keyUp) upPressed = false;
	if (!keyDown) downPressed = false;

	for (auto &&cube : cubes)
	{
		cube.angle += deltaTime * 1.0f;
	}
}

void CubeManager::setCube(XMFLOAT3 position)
{
	cubes.push_back(Cube(position, currentScale, 0.0f));
}

Cube& CubeManager::getCube(int index)
{
	return cubes[index];
}

size_t CubeManager::size()
{
	return cubes.size();
}

void CubeManager::setMousePosition(float x, float y)
{
	mouseX = x;
	mouseY = y;
}

void CubeManager::upScale()
{
	float s = currentScale.x;
	s += 1.0f;
	if (s > 10.f)
		s = 10.f;
	currentScale = XMFLOAT3(s, s, s);
}

void CubeManager::downScale()
{
	float s = currentScale.x;
	s -= 1.0f;
	if (s < 1.0f)
		s = 1.0f;
	currentScale = XMFLOAT3(s, s, s);
}

int CubeManager::getScale()
{
	return (int)currentScale.x;
}
