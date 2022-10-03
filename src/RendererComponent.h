#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include "ThirdPartyHeaders/d3dx12.h"
#include <DirectXMath.h>
#include <wrl.h>
#include "Component.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class RendererComponent :
    public Component
{
public:
    //Initialize static members
    void initialize(HWND hwnd);
    //Prepare individual GameObject Rendering
    void prepare(std::string);
    void render();
    void Terminate();

protected:
    void prepareDescriptorHeaps();
    void prepareRenderTargetView();
    void createDepthBuffer(int width, int height);
    void createCommandAllocators();
    void createFrameFences();
    void waitPreviousFrame();
    HRESULT compileShaderFromFile(
        const std::wstring& fileName,
        const std::wstring& profile,
        ComPtr<ID3DBlob>& shaderBlob,
        ComPtr<ID3DBlob>& errorBlob);
    void makeCommand();

    static ComPtr<ID3D12Device> m_device;
    static ComPtr<ID3D12CommandQueue> m_commandQueue;
    static ComPtr<IDXGISwapChain4> m_swapchain;
    static ComPtr<ID3D12DescriptorHeap> m_heapRTV;
    static ComPtr<ID3D12DescriptorHeap> m_heapDSV;
    static std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
    static ComPtr<ID3D12Resource1> m_depthBuffer;
    static CD3DX12_VIEWPORT m_viewport;
    static CD3DX12_RECT m_scissorRect;
    static UINT m_RTVDescriptorSize;
    static UINT m_SRVCBVDescriptorSize;
    static std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
    static HANDLE m_fenceWaitEvent;
    static std::vector<ComPtr<ID3D12Fence1>> m_frameFence;
    static std::vector<UINT64> m_frameFenceValues;
    static ComPtr<ID3D12GraphicsCommandList> m_commandList;
    static UINT m_frameIndex;

    struct Vertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 normal;
    };

    struct ShaderParameters
    {
        DirectX::XMFLOAT4X4 mtxWorld;
        DirectX::XMFLOAT4X4 mtxView;
        DirectX::XMFLOAT4X4 mtxProj;
    };

private:
    struct BufferObject
    {
        ComPtr<ID3D12Resource1> buffer;
        union
        {
            D3D12_VERTEX_BUFFER_VIEW vertexView;
            D3D12_INDEX_BUFFER_VIEW indexView;
        };
    };

    struct ModelMesh
    {
        BufferObject vertexBuffer;
        BufferObject indexBuffer;
        uint32_t vertexCount;
        uint32_t indexCount;
        int materialIndex;
    };


};

