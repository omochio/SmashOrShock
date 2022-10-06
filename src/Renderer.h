#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "ThirdPartyHeaders/d3dx12.h"
#include <DirectXMath.h>
#include <wrl.h>
//#include "ThirdPartyHeaders//tiny_gltf.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Renderer
{
public:
    //Initialize static members
    void initialize(HWND hwnd);
    //Prepare individual GameObject Rendering
    void prepare();
    void render();
    void Terminate();

private:
    const UINT GpuWaitTimeout = (10 * 1000);
    const UINT FrameBufferCount = 2;

    std::string(modelFilePath);

    void createCommonDescriptorHeaps();
    void createRenderTargetView();
    void createDepthBuffer(int width, int height);
    void createCommandAllocators();
    void createFrameFences();
    void waitPreviousFrame();
    HRESULT compileShaderFromFile(
        const std::wstring& fileName,
        const std::wstring& profile,
        ComPtr<ID3DBlob>& shaderBlob,
        ComPtr<ID3DBlob>& errorBlob);
    void setCommands();

    static ComPtr<ID3D12Device> m_device;
    static ComPtr<ID3D12CommandQueue> m_commandQueue;
    static ComPtr<IDXGISwapChain4> m_swapchain;
    static ComPtr<ID3D12DescriptorHeap> m_heapRTV;
    static ComPtr<ID3D12DescriptorHeap> m_heapDSV;
    static std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
    static ComPtr<ID3D12Resource1> m_depthBuffer;
    static CD3DX12_VIEWPORT m_viewport;
    static CD3DX12_RECT m_scissorRect;
    static UINT m_rtvDescriptorSize;
    static UINT m_SRVCBVDescriptorSize;
    static std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
    static HANDLE m_fenceWaitEvent;
    static std::vector<ComPtr<ID3D12Fence1>> m_frameFence;
    static std::vector<UINT64> m_frameFenceValues;
    static ComPtr<ID3D12GraphicsCommandList> m_commandList;
    static UINT m_frameIndex;
    const static UINT m_frameBufferCount = 2;


    struct BufferObject
    {
        ComPtr<ID3D12Resource1> buffer;
        union
        {
            D3D12_VERTEX_BUFFER_VIEW vertexView;
            D3D12_INDEX_BUFFER_VIEW indexView;
        };
    };

    struct TextureObject
    {
        ComPtr<ID3D12Resource1> texture;
        DXGI_FORMAT format;
    };

    struct ModelMesh
    {
        BufferObject vertexBuffer;
        BufferObject indexBuffer;
        uint32_t vertexCount;
        uint32_t indexCount;

        int materialIndex;
    };

    struct Material
    {
        ComPtr<ID3D12Resource1> texture;
        CD3DX12_GPU_DESCRIPTOR_HANDLE shaderResourceView;
    };

    struct Model
    {
        std::vector<ModelMesh> meshes;
        std::vector<Material>  materials;
    };

    void waitGPU();

    ComPtr<ID3D12Resource1> createBuffer(UINT bufferSize, const void* initialData);
    TextureObject createTextureFromMemory(const std::vector<char>& imageData);
    void createIndividualDescriptorHeaps(UINT materialCount);
    //void makeModelGeometry(const tinygltf::Model& model);
    //void makeModelMaterial(const tinygltf::Model& model);
    ComPtr<ID3D12PipelineState> createOpaquePSO();
    ComPtr<ID3D12PipelineState> createAlphaPSO();

    ComPtr<ID3D12DescriptorHeap> m_heapSRVCBV;
    ComPtr<ID3D12DescriptorHeap> m_heapSampler;
    UINT  m_samplerDescriptorSize;
    UINT  m_SRVDescriptorBase;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineOpaque, m_pipelineAlpha;
    std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;

    D3D12_GPU_DESCRIPTOR_HANDLE m_sampler;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;

    Model m_model;

    ComPtr<ID3DBlob> m_vs;
    ComPtr<ID3DBlob> m_psOpaque, m_psAlpha;

};

