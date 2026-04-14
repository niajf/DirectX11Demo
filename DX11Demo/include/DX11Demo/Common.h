#pragma once

// ---- Windows ----
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

// ---- DirectX 11 ----
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "WICTextureLoader.h"

// ---- ライブラリリンク ----
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// ---- 共通の using 宣言 ----
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ---- 定数 ----
constexpr int     WINDOW_WIDTH = 1280;
constexpr int     WINDOW_HEIGHT = 720;
constexpr LPCWSTR WINDOW_CLASS = L"DX11DemoWindowClass";
constexpr LPCWSTR WINDOW_TITLE = L"DirectX 11 Demo";