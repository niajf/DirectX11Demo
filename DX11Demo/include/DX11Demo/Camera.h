#pragma once
#include "DX11Demo/Common.h"

// ============================================================
// Camera — フリーカメラの状態管理
// ============================================================
// WASD移動 + マウス視点操作を行うFPSスタイルのカメラ。
// ヨー/ピッチから前方向ベクトルを計算し、ビュー行列を構築する。
class Camera
{
public:
    // 初期化
    void Init(const XMFLOAT3& position, float speed, float sensitivity);

    // 毎フレームの更新（マウス + WASD）
    void Update(float deltaTime);

    // ビュー行列を取得する
    XMMATRIX GetViewMatrix() const;

    // プロジェクション行列を取得する
    XMMATRIX GetProjectionMatrix() const;

    // カメラ位置を取得する
    XMFLOAT3 GetPosition() const { return m_position; }

    // ---- 入力状態（WndProcから直接設定される）----
    bool keyW = false;
    bool keyA = false;
    bool keyS = false;
    bool keyD = false;
    bool mouseCaptured = false;
    POINT lastMousePos = {};

private:
    XMFLOAT3 m_position = { 0, 0, 0 };
    float    m_yaw = 0.0f;
    float    m_pitch = 0.0f;
    float    m_speed = 3.0f;
    float    m_sensitivity = 0.002f;
};

// グローバルカメラインスタンス（main.cpp と WndProc から参照される）
extern Camera g_camera;