#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "ThirdPartyHeaders/d3dx12.h"
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <wrl.h>
#include <stdexcept>
#include "ThirdPartyHeaders/tiny_gltf.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Renderer
{
public:
    //Initialize common members
    void initialize(HWND hwnd);
    //Prepare for individual GameObject Rendering
    void prepare(UINT modelID);
    void render();
    void terminate();
    float delta = -1.0f;

private:
    const inline static UINT m_gpuWaitTimeout = (10 * 1000);
    const inline static UINT m_frameBufferCount = 2;

    //std::string(modelFilePath);

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

    inline static ComPtr<ID3D12Device> m_device;
    inline static ComPtr<ID3D12CommandQueue> m_commandQueue;
    inline static ComPtr<IDXGISwapChain4> m_swapchain;
    inline static ComPtr<ID3D12DescriptorHeap> m_heapRTV;
    inline static ComPtr<ID3D12DescriptorHeap> m_heapDSV;
    inline static std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
    inline static ComPtr<ID3D12Resource1> m_depthBuffer;
    inline static CD3DX12_VIEWPORT m_viewport;
    inline static CD3DX12_RECT m_scissorRect;
    inline static UINT m_rtvDescriptorSize;
    inline static UINT m_srvCBVDescriptorSize;
    inline static std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
    inline static HANDLE m_fenceWaitEvent;
    inline static std::vector<ComPtr<ID3D12Fence1>> m_frameFences;
    inline static std::vector<UINT64> m_frameFenceValues;
    inline static ComPtr<ID3D12GraphicsCommandList> m_commandList;
    inline static UINT m_frameIndex;

    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT3 Normal;
    };

    struct ShaderParameters
    {
        DirectX::XMFLOAT4X4 mtxWorld;
        DirectX::XMFLOAT4X4 mtxView;
        DirectX::XMFLOAT4X4 mtxProj;
    };

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
        std::string alphaMode;
    };

    struct Model
    {
        std::vector<ModelMesh> meshes;
    };

    enum
    {
        ConstantBufferDescriptorBase = 0,
        // サンプラーは別ヒープなので先頭を使用
        SamplerDescriptorBase = 0,
    };


    void waitGPU();

    ComPtr<ID3D12Resource1> createBuffer(UINT bufferSize, const void* initialData);
    //TextureObject createTextureFromMemory(const std::vector<char>& imageData);
    void createIndividualDescriptorHeaps(UINT materialCount);
    void makeModelGeometry(const std::shared_ptr<tinygltf::Model> model);
    //void makeModelMaterial(const std::shared_ptr<tinygltf::Model> model);
    //TextureObject createTextureFromMemory(const std::vector<const unsigned char>& imageData);
    ComPtr<ID3D12PipelineState> createPipelineState();

    ComPtr<ID3D12DescriptorHeap> m_heapSRVCBV;
    ComPtr<ID3D12DescriptorHeap> m_heapSampler;
    UINT  m_samplerDescriptorSize;
    UINT  m_srvDescriptorBase;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;

    D3D12_GPU_DESCRIPTOR_HANDLE m_sampler;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;

    Model m_model;

    ComPtr<ID3DBlob> m_vs;
    ComPtr<ID3DBlob> m_ps;

    tinygltf::Model* getModel(std::string modelPath);
    void loadModel(std::string path);
    inline static std::unordered_map<std::string, std::shared_ptr<tinygltf::Model>> m_modelList;
    inline static std::vector<std::string> m_modelPathList;

};

