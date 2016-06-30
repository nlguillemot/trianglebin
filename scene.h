#pragma once

#include <d3d11.h>

void SceneInit(ID3D11Device* dev, ID3D11DeviceContext* dc);
void SceneResize(int width, int height);
void ScenePaint(ID3D11RenderTargetView* backbufferRTV);