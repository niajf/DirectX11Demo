#pragma once
#include "DX11Demo/Common.h"

class Raycast
{
public:
	static void getRayVector(
		float mouseX,
		float mouseY,
		float viewportX,
		float viewportY,
		float viewportWidth,
		float viewportHeight,
		float viewportMinZ,
		float viewportMaxZ,
		XMMATRIX projection,
		XMMATRIX view,
		XMMATRIX world,
		XMVECTOR& origin,
		XMVECTOR& direction
	);
};