#pragma once
#include "DX11Demo/Common.h"

// ============================================================
// 頂点構造体
// ============================================================
// GPU側のHLSL構造体（VSInput）と完全に対応する。
// メモリレイアウト:
//   POSITION  — オフセット 0  (12バイト)
//   NORMAL    — オフセット 12 (12バイト)
//   TEXCOORD  — オフセット 24 (8バイト)
//   合計: 32バイト/頂点
struct Vertex
{
    XMFLOAT3 position;  // 位置 (x, y, z)
    XMFLOAT3 normal;    // 法線ベクトル
    XMFLOAT2 texCoord;  // UV座標
};

// ============================================================
// 定数バッファ構造体（register(b0)に対応）
// ============================================================
struct alignas(16) ConstantBuffer
{
    XMMATRIX wvp;   // ワールド × ビュー × プロジェクション行列
    XMMATRIX world; // ワールド行列（ライティング用）
};

// ============================================================
// ライト用定数バッファ構造体（register(b1)に対応）
// ============================================================
struct alignas(16) LightBuffer
{
    XMFLOAT3 lightDirection;
    float    padding1;
    XMFLOAT3 lightColor;
    float    padding2;
    XMFLOAT3 cameraPosition;
    float    shininess;
};