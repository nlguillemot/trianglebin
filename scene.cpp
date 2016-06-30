#include "scene.h"
#include "imgui/imgui.h"

#include <d3dcompiler.h>
#include "dxutil.h"

#include <vector>

static ID3D11Device* g_Device;
static ID3D11DeviceContext* g_DeviceContext;

static ID3D11Texture2D* g_TrianglesTex2DMS;
static ID3D11Texture2D* g_TrianglesTex2D;
static ID3D11RenderTargetView* g_TrianglesRTV;
static ID3D11ShaderResourceView* g_TrianglesSRV;
static ID3D11SamplerState* g_TrianglesSMP;

static ID3D11RasterizerState* g_TrianglesRasterizerState;
static ID3D11DepthStencilState* g_TrianglesDepthStencilState;
static ID3D11BlendState* g_TrianglesBlendState;
static ID3D11VertexShader* g_TrianglesVS;
static ID3D11PixelShader* g_TrianglesPS;

static ID3D11RasterizerState* g_BlitRasterizerState;
static ID3D11DepthStencilState* g_BlitDepthStencilState;
static ID3D11BlendState* g_BlitBlendState;
static ID3D11VertexShader* g_BlitVS;
static ID3D11PixelShader* g_BlitPS;

static ID3D11Buffer* g_PixelCountBuffer;
static ID3D11UnorderedAccessView* g_PixelCountUAV;

static ID3D11Buffer* g_MaxNumPixelsBuffer;

static D3D11_VIEWPORT g_Viewport;

static int g_NumTris;
static int g_MaxNumPixels;
static int g_NumExtraFloats;
static int g_PixelFormatIndex;
static int g_SampleCountIndex;

static const char* kPixelFormatNames[] = {
	"R8G8B8A8_UNORM",
	"R32G32B32A32_FLOAT"
};

static const DXGI_FORMAT kPixelFormatFormats[] = {
	DXGI_FORMAT_R8G8B8A8_UNORM,
	DXGI_FORMAT_R32G32B32A32_FLOAT
};

static const char* kSampleCountNames[] = {
	"1","2","4","8"
};

static const UINT kSampleCountCounts[] = {
	1, 2, 4, 8
};

void RebuildShaders()
{
	ID3D11Device* dev = g_Device;
	ID3D11DeviceContext* dc = g_DeviceContext;

	ComPtr<ID3DBlob> TrianglesVSBlob;
	ComPtr<ID3DBlob> TrianglesPSBlob;
	ComPtr<ID3DBlob> BlitVSBlob;
	ComPtr<ID3DBlob> BlitPSBlob;

	struct ShaderToCompile
	{
		std::wstring name;
		std::string entry;
		std::string target;
		ID3DBlob** blob;
		std::vector<D3D_SHADER_MACRO> defines;
	};

	std::string numExtraFloatsStr = std::to_string(g_NumExtraFloats);
	D3D_SHADER_MACRO numExtraFloatsMacro{ "NUM_EXTRA_FLOATs", numExtraFloatsStr.c_str() };

	ShaderToCompile shadersToCompile[] = {
		{ L"triangles.hlsl", "VSmain", "vs_5_0", &TrianglesVSBlob, { numExtraFloatsMacro } },
		{ L"triangles.hlsl", "PSmain", "ps_5_0", &TrianglesPSBlob, { numExtraFloatsMacro } },
		{ L"blit.hlsl", "VSmain", "vs_5_0", &BlitVSBlob,{ } },
		{ L"blit.hlsl", "PSmain", "ps_5_0", &BlitPSBlob,{ } },
	};

	for (ShaderToCompile& s2c : shadersToCompile)
	{
		UINT flags = 0;
#if _DEBUG
		flags |= D3DCOMPILE_DEBUG;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		ComPtr<ID3DBlob> ErrBlob;
		s2c.defines.push_back(D3D_SHADER_MACRO{});
		HRESULT hr = D3DCompileFromFile(s2c.name.c_str(), s2c.defines.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, s2c.entry.c_str(), s2c.target.c_str(), flags, 0, s2c.blob, &ErrBlob);
		std::string mbname = MultiByteFromWide(s2c.name);
		if (FAILED(hr))
		{
			std::string hrs = MultiByteFromHR(hr);
			fprintf(stderr,
				"Error (%s):\n%s%s%s\n",
				mbname.c_str(),
				hrs.c_str(),
				ErrBlob ? "\n" : "",
				ErrBlob ? (const char*)ErrBlob->GetBufferPointer() : "");
		}

		if (ErrBlob)
		{
			printf("Warning (%s): %s\n", mbname.c_str(), (const char*)ErrBlob->GetBufferPointer());
		}
	}

	if (g_TrianglesVS) g_TrianglesVS->Release();
	CHECKHR(dev->CreateVertexShader(TrianglesVSBlob->GetBufferPointer(), TrianglesVSBlob->GetBufferSize(), NULL, &g_TrianglesVS));
	
	if (g_TrianglesPS) g_TrianglesPS->Release();
	CHECKHR(dev->CreatePixelShader(TrianglesPSBlob->GetBufferPointer(), TrianglesPSBlob->GetBufferSize(), NULL, &g_TrianglesPS));

	if (g_BlitVS) g_BlitVS->Release();
	CHECKHR(dev->CreateVertexShader(BlitVSBlob->GetBufferPointer(), BlitVSBlob->GetBufferSize(), NULL, &g_BlitVS));

	if (g_BlitPS) g_BlitPS->Release();
	CHECKHR(dev->CreatePixelShader(BlitPSBlob->GetBufferPointer(), BlitPSBlob->GetBufferSize(), NULL, &g_BlitPS));
}

void SceneInit(ID3D11Device* dev, ID3D11DeviceContext* dc)
{
	g_Device = dev;
	g_DeviceContext = dc;

	RebuildShaders();

	// triangles pipeline
	{
		D3D11_RASTERIZER_DESC trianglesRasterizerDesc = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
		CHECKHR(dev->CreateRasterizerState(&trianglesRasterizerDesc, &g_TrianglesRasterizerState));

		D3D11_DEPTH_STENCIL_DESC trianglesDepthStencilDesc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
		trianglesDepthStencilDesc.DepthEnable = FALSE;
		CHECKHR(dev->CreateDepthStencilState(&trianglesDepthStencilDesc, &g_TrianglesDepthStencilState));

		D3D11_BLEND_DESC trianglesBlendDesc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
		CHECKHR(dev->CreateBlendState(&trianglesBlendDesc, &g_TrianglesBlendState));
	}

	// blit pipeline
	{
		D3D11_RASTERIZER_DESC blitRasterizerDesc = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
		CHECKHR(dev->CreateRasterizerState(&blitRasterizerDesc, &g_BlitRasterizerState));

		D3D11_DEPTH_STENCIL_DESC blitDepthStencilDesc = CD3D11_DEPTH_STENCIL_DESC(D3D11_DEFAULT);
		blitDepthStencilDesc.DepthEnable = FALSE;
		CHECKHR(dev->CreateDepthStencilState(&blitDepthStencilDesc, &g_BlitDepthStencilState));

		D3D11_BLEND_DESC blitBlendDesc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
		CHECKHR(dev->CreateBlendState(&blitBlendDesc, &g_BlitBlendState));
	}

	CHECKHR(dev->CreateBuffer(
		&CD3D11_BUFFER_DESC(sizeof(UINT32), D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(UINT32)),
		NULL,
		&g_PixelCountBuffer));

	CHECKHR(dev->CreateUnorderedAccessView(
		g_PixelCountBuffer,
		&CD3D11_UNORDERED_ACCESS_VIEW_DESC(g_PixelCountBuffer, DXGI_FORMAT_UNKNOWN, 0, 1, D3D11_BUFFER_UAV_FLAG_COUNTER),
		&g_PixelCountUAV));

	CHECKHR(dev->CreateBuffer(
		&CD3D11_BUFFER_DESC(16, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, 0, sizeof(UINT32)),
		NULL,
		&g_MaxNumPixelsBuffer));

	CHECKHR(dev->CreateSamplerState(
		&CD3D11_SAMPLER_DESC(D3D11_DEFAULT),
		&g_TrianglesSMP));
}

void SceneResize(int width, int height)
{
	ID3D11Device* dev = g_Device;
	ID3D11DeviceContext* dc = g_DeviceContext;

	g_Viewport.Width = (float)width;
	g_Viewport.Height = (float)height;
	g_Viewport.TopLeftX = 0.0f;
	g_Viewport.TopLeftY = 0.0f;
	g_Viewport.MinDepth = 0.0f;
	g_Viewport.MaxDepth = 1.0f;

	DXGI_FORMAT trianglesFormat = kPixelFormatFormats[g_PixelFormatIndex];
	UINT sampleCount = kSampleCountCounts[g_SampleCountIndex];

	if (g_TrianglesTex2DMS) g_TrianglesTex2DMS->Release();
	CHECKHR(dev->CreateTexture2D(
		&CD3D11_TEXTURE2D_DESC(trianglesFormat, width, height, 1, 1, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, sampleCount, 0, 0),
		NULL,
		&g_TrianglesTex2DMS));

	if (g_TrianglesRTV) g_TrianglesRTV->Release();
	CHECKHR(dev->CreateRenderTargetView(
		g_TrianglesTex2DMS,
		&CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2DMS, trianglesFormat, 0, 1),
		&g_TrianglesRTV));

	if (g_TrianglesTex2D) g_TrianglesTex2D->Release();
	CHECKHR(dev->CreateTexture2D(
		&CD3D11_TEXTURE2D_DESC(trianglesFormat, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, 0),
		NULL,
		&g_TrianglesTex2D));

	if (g_TrianglesSRV) g_TrianglesSRV->Release();
	CHECKHR(dev->CreateShaderResourceView(
		g_TrianglesTex2D,
		&CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, trianglesFormat, 0, 1),
		&g_TrianglesSRV));
}

void ScenePaint(ID3D11RenderTargetView* backbufferRTV)
{
	ID3D11Device* dev = g_Device;
	ID3D11DeviceContext* dc = g_DeviceContext;

	ImGui::SetNextWindowSize(ImVec2(550, 250), ImGuiSetCond_Once);
	if (ImGui::Begin("Toolbox"))
	{
		ImGui::SliderInt("Num triangles", &g_NumTris, 0, 100);
		if (g_NumTris < 0) g_NumTris = 0;
		
		ImGui::SliderInt("Num pixels (in thousands)", &g_MaxNumPixels, 0, 50000);
		if (g_MaxNumPixels < 0) g_MaxNumPixels = 0;
		
		if (ImGui::SliderInt("Num extra vertex floats", &g_NumExtraFloats, 0, 24))
		{
			if (g_NumExtraFloats < 0)
				g_NumExtraFloats = 0;

			RebuildShaders();
		}

		if (ImGui::ListBox("Pixel format", &g_PixelFormatIndex, kPixelFormatNames, _countof(kPixelFormatNames)))
		{
			SceneResize((int)g_Viewport.Width, (int)g_Viewport.Height);
		}

		if (ImGui::ListBox("Sample count", &g_SampleCountIndex, kSampleCountNames, _countof(kSampleCountNames)))
		{
			SceneResize((int)g_Viewport.Width, (int)g_Viewport.Height);
		}
	}
	ImGui::End();

	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		CHECKHR(dc->Map(g_MaxNumPixelsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

		*(UINT32*)mapped.pData = g_MaxNumPixels * 1000;
		dc->Unmap(g_MaxNumPixelsBuffer, 0);
	}

	const float kClearColor[] = { 0, 0, 0, 0 };
	dc->ClearRenderTargetView(g_TrianglesRTV, kClearColor);

	// draw triangles
	{
		ID3D11RenderTargetView* rtvs[] = { g_TrianglesRTV };
		ID3D11UnorderedAccessView* uavs[] = { g_PixelCountUAV };
		UINT uavCounters[_countof(uavs)] = { 0 };
		dc->OMSetRenderTargetsAndUnorderedAccessViews(_countof(rtvs), rtvs, NULL, _countof(rtvs), _countof(uavs), uavs, uavCounters);
		dc->VSSetShader(g_TrianglesVS, NULL, 0);
		dc->PSSetShader(g_TrianglesPS, NULL, 0);
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		dc->IASetInputLayout(NULL);
		dc->RSSetState(g_TrianglesRasterizerState);
		dc->OMSetDepthStencilState(g_TrianglesDepthStencilState, 0);
		dc->OMSetBlendState(g_TrianglesBlendState, NULL, UINT_MAX);
		dc->RSSetViewports(1, &g_Viewport);
		dc->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
		dc->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
		dc->PSSetConstantBuffers(0, 1, &g_MaxNumPixelsBuffer);
		dc->Draw(g_NumTris * 3, 0);
		
		dc->OMSetRenderTargets(0, NULL, NULL);
		dc->VSSetShader(NULL, NULL, 0);
		dc->PSSetShader(NULL, NULL, 0);
	}

	dc->ResolveSubresource(g_TrianglesTex2D, 0, g_TrianglesTex2DMS, 0, kPixelFormatFormats[g_PixelFormatIndex]);

	// blit
	{
		ID3D11RenderTargetView* rtvs[] = { backbufferRTV };
		dc->OMSetRenderTargets(_countof(rtvs), rtvs, NULL);
		dc->VSSetShader(g_BlitVS, NULL, 0);
		dc->PSSetShader(g_BlitPS, NULL, 0);
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		dc->IASetInputLayout(NULL);
		dc->RSSetState(g_BlitRasterizerState);
		dc->OMSetDepthStencilState(g_BlitDepthStencilState, 0);
		dc->OMSetBlendState(g_BlitBlendState, NULL, UINT_MAX);
		dc->RSSetViewports(1, &g_Viewport);
		dc->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
		dc->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
		dc->PSSetShaderResources(0, 1, &g_TrianglesSRV);
		dc->PSSetSamplers(0, 1, &g_TrianglesSMP);
		dc->Draw(3, 0);
		
		ID3D11ShaderResourceView* resetSRV = NULL;
		dc->PSSetShaderResources(0, 1, &resetSRV);
		dc->OMSetRenderTargets(0, NULL, NULL);
		dc->VSSetShader(NULL, NULL, 0);
		dc->PSSetShader(NULL, NULL, 0);
	}
}