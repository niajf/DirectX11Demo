#pragma once
#include "DX11Demo/Common.h"
#include "DX11Demo/Cube.h"
#include <vector> 

class CubeManager
{
public:
	bool button = false;
	bool keyUp = false;
	bool keyDown = false;

	void Update(float deltaTime, XMMATRIX projection, XMMATRIX view);
	void upScale();
	void downScale();
	void setMousePosition(float x, float y);
	const Cube& getCube(size_t index) const;
	int getScale() const;
	size_t size() const;

private:
	bool buttonPressed = false;
	bool upPressed = false;
	bool downPressed = false;
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	std::vector<Cube> cubes;
	XMFLOAT3 currentScale = { 1.0f, 1.0f, 1.0f }; 

	void setCube(XMFLOAT3 position);
};

extern CubeManager g_cubeManager;