#include "DX11Demo/Camera.h"

// グローバルカメラインスタンスの定義
Camera g_camera;

void Camera::Init(const XMFLOAT3& position, float speed, float sensitivity)
{
    m_position = position;
    m_speed = speed;
    m_sensitivity = sensitivity;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

void Camera::Update(float deltaTime)
{
    // ---- マウス入力 → ヨーとピッチの更新 ----
    if (mouseCaptured)
    {
        POINT currentMousePos;
        GetCursorPos(&currentMousePos);

        float deltaX = static_cast<float>(currentMousePos.x - lastMousePos.x);
        float deltaY = static_cast<float>(currentMousePos.y - lastMousePos.y);

        m_yaw += deltaX * m_sensitivity;
        m_pitch += deltaY * m_sensitivity;

        // ピッチを±89度に制限（ジンバルロック防止）
        constexpr float maxPitch = XMConvertToRadians(89.0f);
        if (m_pitch > maxPitch) m_pitch = maxPitch;
        if (m_pitch < -maxPitch) m_pitch = -maxPitch;

        // マウスカーソルを画面中央に戻す
        RECT rect;
        GetWindowRect(GetForegroundWindow(), &rect);
        int centerX = (rect.left + rect.right) / 2;
        int centerY = (rect.top + rect.bottom) / 2;
        SetCursorPos(centerX, centerY);

        lastMousePos.x = centerX;
        lastMousePos.y = centerY;
    }

    // ---- 前方向・右方向ベクトルの計算 ----
    XMFLOAT3 forward;
    forward.x = cosf(m_pitch) * sinf(m_yaw);
    forward.y = -sinf(m_pitch);
    forward.z = cosf(m_pitch) * cosf(m_yaw);

    XMVECTOR forwardVec = XMVector3Normalize(XMLoadFloat3(&forward));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(worldUp, forwardVec));

    // ---- WASD移動 ----
    float speed = m_speed * deltaTime;
    XMVECTOR posVec = XMLoadFloat3(&m_position);

    if (keyW) posVec = XMVectorAdd(posVec, XMVectorScale(forwardVec, speed));
    if (keyS) posVec = XMVectorAdd(posVec, XMVectorScale(forwardVec, -speed));
    if (keyD) posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, speed));
    if (keyA) posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, -speed));

    XMStoreFloat3(&m_position, posVec);
}

XMMATRIX Camera::GetViewMatrix() const
{
    XMFLOAT3 forward;
    forward.x = cosf(m_pitch) * sinf(m_yaw);
    forward.y = -sinf(m_pitch);
    forward.z = cosf(m_pitch) * cosf(m_yaw);

    XMVECTOR eye = XMLoadFloat3(&m_position);
    XMVECTOR forwardVec = XMLoadFloat3(&forward);
    XMVECTOR target = XMVectorAdd(eye, forwardVec);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    return XMMatrixLookAtLH(eye, target, up);
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    constexpr float fov = XMConvertToRadians(60.0f);
    float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    return XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, 100.0f);
}