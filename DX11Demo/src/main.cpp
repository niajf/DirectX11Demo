#include "DX11Demo/Common.h"
#include "DX11Demo/Renderer.h"
#include "DX11Demo/Camera.h"
#include "DX11Demo/CubeManager.h"

// ---- 前方宣言 ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// WinMain — エントリポイント
// ============================================================
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // COM の初期化（WICTextureLoader が内部で使用する）
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // ---- ウィンドウクラス登録 ----
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassEx(&wc))
    {
        MessageBox(nullptr, L"ウィンドウクラスの登録に失敗", L"エラー", MB_OK);
        return -1;
    }

    // ---- ウィンドウ作成 ----
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0, WINDOW_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
    {
        MessageBox(nullptr, L"ウィンドウの作成に失敗", L"エラー", MB_OK);
        return -1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---- 初期化 ----
    if (!g_renderer.Init(hwnd)) return -1;

    g_camera.Init({ 0.0f, 1.5f, -3.0f }, 3.0f, 0.002f);

    // ---- タイマー初期化 ----
    LARGE_INTEGER frequency, previousTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previousTime);

    // ---- ゲームループ ----
    MSG msg = {};
    bool isRunning = true;
    float fpsTimer = 0.0f;
    int   frameCount = 0;    

    while (isRunning)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { isRunning = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (isRunning)
        {
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            float deltaTime = static_cast<float>(currentTime.QuadPart - previousTime.QuadPart)
                / static_cast<float>(frequency.QuadPart);
            previousTime = currentTime;

            g_cubeManager.Update(deltaTime, g_camera.GetProjectionMatrix(), g_camera.GetViewMatrix());
            g_camera.Update(deltaTime);
            g_renderer.Render(deltaTime);

            // FPS 表示
            fpsTimer += deltaTime;
            frameCount++;
            if (fpsTimer >= 1.0f)
            {
                wchar_t title[64];
                swprintf_s(title, L"%s - FPS: %d, CubeScale : %d", WINDOW_TITLE, frameCount, g_cubeManager.getScale());
                SetWindowText(hwnd, title);
                fpsTimer = 0.0f;
                frameCount = 0;
            }
        }
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

// ============================================================
// WndProc — ウィンドウプロシージャ
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            if (g_camera.mouseCaptured)
            {
                g_camera.mouseCaptured = false;
                ShowCursor(TRUE);
                ClipCursor(nullptr);
            }
            else
            {
                DestroyWindow(hwnd);
            }
            break;
        case 'W': g_camera.keyW = true; break;
        case 'A': g_camera.keyA = true; break;
        case 'S': g_camera.keyS = true; break;
        case 'D': g_camera.keyD = true; break;
        case VK_UP: g_cubeManager.keyUp = true; break;
        case VK_DOWN: g_cubeManager.keyDown = true; break;
        }
        return 0;

    case WM_KEYUP:
        switch (wParam)
        {
        case 'W': g_camera.keyW = false; break;
        case 'A': g_camera.keyA = false; break;
        case 'S': g_camera.keyS = false; break;
        case 'D': g_camera.keyD = false; break;
        case VK_UP: g_cubeManager.keyUp = false; break;
        case VK_DOWN: g_cubeManager.keyDown = false; break;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (!g_camera.mouseCaptured)
        {
            g_camera.mouseCaptured = true;
            ShowCursor(FALSE);

            RECT clipRect;
            GetClientRect(hwnd, &clipRect);
            MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&clipRect), 2);
            ClipCursor(&clipRect);

            GetCursorPos(&g_camera.lastMousePos);
        }
        return 0;

    case WM_RBUTTONDOWN:
        g_cubeManager.button = true;
        g_cubeManager.setMousePosition(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_RBUTTONUP:
        g_cubeManager.button = false;
        return 0;

    case WM_DESTROY:
        ClipCursor(nullptr);
        ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}