#include "DX11Demo/Raycast.h"

void Raycast::getRayVector(
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
)
{
	XMVECTOR nViewPos = { mouseX, mouseY, 0.0f };
	XMVECTOR fViewPos = { mouseX, mouseY, 1.0f };

	XMVECTOR nWorldPos = XMVector3Unproject(
		nViewPos,
		viewportX,
		viewportY,
		viewportWidth,
		viewportHeight,
		viewportMinZ,
		viewportMaxZ,
		projection,
		view,
		world
	);

	XMVECTOR fWorldPos = XMVector3Unproject(
		fViewPos,
		viewportX,
		viewportY,
		viewportWidth,
		viewportHeight,
		viewportMinZ,
		viewportMaxZ,
		projection,
		view,
		world
	);

	origin = fWorldPos;
	direction = XMVector3Normalize(XMVectorSubtract(fWorldPos, nWorldPos));
}