#include "Renderer.h"
#include <fstream>
#include <filesystem>
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")


void Renderer::initialize(HWND hwnd)
{
    HRESULT hr;
    UINT dxgiFlags = 0;

    m_renderTargets.resize(m_frameBufferCount);
    m_frameFenceValues.resize(m_frameBufferCount);

    m_fenceWaitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef _DEBUG
    m_modelPathList = { "../Resources/Field.glb", "../Resources/Player.glb", "../Resources/Enemy.glb" };
#else
    m_modelPathList = { "Resources/Field.glb", "Resources/Player.glb", "Resources/Enemy.glb" };
#endif

//Enable debugLayer
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) 
    {
        debug->EnableDebugLayer();
        dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    //Create factory
    ComPtr<IDXGIFactory3> factory;
    hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateDXGIFactory2 failed.");
    }

    //Create device
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapterIndex = 0;
    while (DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter))
    {
        DXGI_ADAPTER_DESC1 desc1{};
        adapter->GetDesc1(&desc1);
        ++adapterIndex;
        if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }
        hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            __uuidof(ID3D12Device),
            nullptr);
        if (SUCCEEDED(hr))
        {
            break;
        }
    }
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D12CreateDevice failed.");
    }

    //Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc{
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        0,
        D3D12_COMMAND_QUEUE_FLAG_NONE,
        0
    };
    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateCommandQueue failed.");
    }

    //Client rect size
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    //Create swapchain
    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.BufferCount = m_frameBufferCount;
    scDesc.Width = width;
    scDesc.Height = height;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapchain;
    hr = factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &scDesc,
        nullptr,
        nullptr,
        &swapchain
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateSwapChainForHwnd failed.");
    }
    swapchain.As(&m_swapchain);    //Convert to IDXGISwapChain4

    //Create RTV and DSV
    createCommonDescriptorHeaps();

    //Create RTV
    createRenderTargetView();

    //Create depth buffer
    createDepthBuffer(width, height);

    //Create command allocators
    createCommandAllocators();

    //Create frame fences
    createFrameFences();

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    m_commandList->Close();

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(width), float(height));
    m_scissorRect = CD3DX12_RECT(0, 0, LONG(width), LONG(height));

    for (std::string i : m_modelPathList)
    {
        loadModel(i);
    }

}

void Renderer::terminate()
{
    waitGPU();
}

void Renderer::prepare(UINT modelID)
{
    //Fetch model from list
    const std::shared_ptr<tinygltf::Model> model = m_modelList[m_modelPathList[modelID]];
    
    createIndividualDescriptorHeaps(model->materials.size());
    
    makeModelGeometry(model);
    //makeModelMaterial(model);

    HRESULT hr;
    ComPtr<ID3DBlob> errBlob;
    hr = compileShaderFromFile(L"shaderVS.hlsl", L"vs_6_0", m_vs, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }
    hr = compileShaderFromFile(L"shaderPS.hlsl", L"ps_6_0", m_ps, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }

    m_srvDescriptorBase = m_frameBufferCount;

    CD3DX12_DESCRIPTOR_RANGE cbv, srv, sampler;
    cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    sampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParams[2];
    rootParams[0].InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParams[1].InitAsDescriptorTable(1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Init(
        _countof(rootParams), 
        rootParams,  
        0, 
        nullptr,   
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> signature;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D12SerializeRootSignature faild.");
    }
    // RootSignature の生成
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("CrateRootSignature failed.");
    }

    m_pipelineState = createPipelineState();


    // 定数バッファ/定数バッファビューの生成
    m_constantBuffers.resize(m_frameBufferCount);
    m_cbViews.resize(m_frameBufferCount);
    for (UINT i = 0; i < m_frameBufferCount; ++i)
    {
        UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
        m_constantBuffers[i] = createBuffer(bufferSize, nullptr);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
        cbDesc.BufferLocation = m_constantBuffers[i]->GetGPUVirtualAddress();
        cbDesc.SizeInBytes = bufferSize;
        CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSRVCBV->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvCBVDescriptorSize);
        m_device->CreateConstantBufferView(&cbDesc, handleCBV);

        m_cbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSRVCBV->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvCBVDescriptorSize);
    }

    // サンプラーの生成
    D3D12_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(
        D3D12_FILTER_TYPE_LINEAR, // min
        D3D12_FILTER_TYPE_LINEAR, // mag
        D3D12_FILTER_TYPE_LINEAR, // mip
        D3D12_FILTER_REDUCTION_TYPE_STANDARD);
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MaxLOD = FLT_MAX;
    samplerDesc.MinLOD = -FLT_MAX;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

    // サンプラー用ディスクリプタヒープの0番目を使用する
    auto descriptorSampler = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heapSampler->GetCPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);
    m_device->CreateSampler(&samplerDesc, descriptorSampler);
    m_sampler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSampler->GetGPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);
    
}

void Renderer::render()
{
    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    //Clear commands
    m_commandAllocators[m_frameIndex]->Reset();
    m_commandList->Reset(
        m_commandAllocators[m_frameIndex].Get(),
        nullptr
    );
    
    // Barrier transition
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierToRT);

    //Handles
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
        m_heapRTV->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex,
        m_rtvDescriptorSize
    );
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv(
        m_heapDSV->GetCPUDescriptorHandleForHeapStart()
    );

    //Clear color buffer
    const float clearColor[] = { 0.2f, 0.2f, 0.2f, 0.0f };
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    //Clear depth buffer
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    //Set renderd targets
    m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    //Set commands include draw command
    setCommands();

    //Barrier transition
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &barrierToPresent);

    m_commandList->Close();

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapchain->Present(1, 0);

    waitPreviousFrame();
}

void Renderer::setCommands()
{
    ShaderParameters shaderParams;
    XMStoreFloat4x4(&shaderParams.mtxWorld, DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XMConvertToRadians(0.0f)));
    auto eye = DirectX::XMVectorSet(-4.0, 5.0f, -5.0f, 0.0f);
    eye = DirectX::XMVector4Transform(eye, DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4 * delta));
    auto mtxView = DirectX::XMMatrixLookAtLH(
        eye,
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    auto mtxProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 100.0f);
    XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mtxView));
    XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mtxProj));

    // 定数バッファの更新.
    auto& constantBuffer = m_constantBuffers[m_frameIndex];
    {
        void* p;
        CD3DX12_RANGE range(0, 0);
        constantBuffer->Map(0, &range, &p);
        memcpy(p, &shaderParams, sizeof(shaderParams));
        constantBuffer->Unmap(0, nullptr);
    }

    // ルートシグネチャのセット
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    // ビューポートとシザーのセット
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // ディスクリプタヒープをセット.
    ID3D12DescriptorHeap* heaps[] = {
      m_heapSRVCBV.Get(), m_heapSampler.Get()
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    for (const auto& mesh : m_model.meshes)
    {
        m_commandList->SetPipelineState(m_pipelineState.Get());

        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexBuffer.vertexView);
        m_commandList->IASetIndexBuffer(&mesh.indexBuffer.indexView);

        m_commandList->SetGraphicsRootDescriptorTable(0, m_cbViews[m_frameIndex]);
        m_commandList->SetGraphicsRootDescriptorTable(1, m_sampler);

        // このメッシュを描画
        m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        
    }

}

ComPtr<ID3D12Resource1> Renderer::createBuffer(UINT bufferSize, const void* initialData)
{
    HRESULT hr;
    ComPtr<ID3D12Resource1> buffer;
    const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (SUCCEEDED(hr) && initialData != nullptr)
    {
        void* mapped;
        CD3DX12_RANGE range(0, 0);
        hr = buffer->Map(0, &range, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped, initialData, bufferSize);
            buffer->Unmap(0, nullptr);
        }
    }

    return buffer;

}

void Renderer::createIndividualDescriptorHeaps(UINT materialCount)
{
    m_srvDescriptorBase = m_frameBufferCount;
    UINT countCBVSRVDescriptors = materialCount + m_frameBufferCount;

    //Create descriptor heap for CBV and SRV
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      countCBVSRVDescriptors,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapSRVCBV));
    m_srvCBVDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create descriptor heap for sampler
    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
      1,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_heapSampler));
    m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

}



void Renderer::createCommonDescriptorHeaps()
{  
    //RTV descriptorHeap
    HRESULT hr;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
      m_frameBufferCount,
      D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      0
    };
    hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_heapRTV));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed CreateDescriptorHeap(RTV)");
    }
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //DSV descriptorHeap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
      1,
      D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      0
    };
    hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_heapDSV));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed CreateDescriptorHeap(DSv)");
    }

}

void Renderer::createRenderTargetView()
{
    //Create RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_heapRTV->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < m_frameBufferCount; ++i)
    {
        m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void Renderer::createDepthBuffer(int width, int height)
{
    //Create depth buffer
    auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        width,
        height,
        1, 
        0,
        1, 
        0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = depthBufferDesc.Format;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    HRESULT hr;
    const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthBufferDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&m_depthBuffer)
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed CreateCommittedResource(DepthBuffer)");
    }

    //Create DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
    {
      DXGI_FORMAT_D32_FLOAT,
      D3D12_DSV_DIMENSION_TEXTURE2D,
      D3D12_DSV_FLAG_NONE,
      0
    };
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_heapDSV->GetCPUDescriptorHandleForHeapStart());
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, dsvHandle);
}

void Renderer::createCommandAllocators()
{
    HRESULT hr;
    m_commandAllocators.resize(m_frameBufferCount);
    for (UINT i = 0; i < m_frameBufferCount; ++i)
    {
        hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])
        );
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed CreateCommandAllocator");
        }
    }
}

void Renderer::createFrameFences()
{
    HRESULT hr;
    m_frameFences.resize(m_frameBufferCount);
    for (UINT i = 0; i < m_frameBufferCount; ++i)
    {
        hr = m_device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_frameFences[i]));
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed CreateFence");
        }
    }

}

void Renderer::waitPreviousFrame()
{
    auto& fence = m_frameFences[m_frameIndex];
    const auto currentValue = ++m_frameFenceValues[m_frameIndex];
    m_commandQueue->Signal(fence.Get(), currentValue);

    auto nextIndex = (m_frameIndex + 1) % m_frameBufferCount;
    const auto finishExpected = m_frameFenceValues[nextIndex];
    const auto nextFenceValue = m_frameFences[nextIndex]->GetCompletedValue();
    if (nextFenceValue < finishExpected)
    {
        m_frameFences[nextIndex]->SetEventOnCompletion(finishExpected, m_fenceWaitEvent);
        WaitForSingleObject(m_fenceWaitEvent, m_gpuWaitTimeout);
    }

}

HRESULT Renderer::compileShaderFromFile(const std::wstring& fileName, const std::wstring& profile, ComPtr<ID3DBlob>& shaderBlob, ComPtr<ID3DBlob>& errorBlob)
{
    std::filesystem::path filePath(fileName);
    std::ifstream infile(filePath);
    std::vector<char> srcData;
    if (!infile)
        throw std::runtime_error("shader not found");
    srcData.resize(uint32_t(infile.seekg(0, infile.end).tellg()));
    infile.seekg(0, infile.beg).read(srcData.data(), srcData.size());

    ComPtr<IDxcLibrary> library;
    ComPtr<IDxcCompiler> compiler;
    ComPtr<IDxcBlobEncoding> source;
    ComPtr<IDxcOperationResult> dxcResult;

    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    library->CreateBlobWithEncodingFromPinned(srcData.data(), UINT(srcData.size()), CP_ACP, &source);
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    LPCWSTR compilerFlags[] = {
  #if _DEBUG
      L"/Zi", L"/O0",
  #else
      L"/O2" 
  #endif
    };
    compiler->Compile(source.Get(), filePath.wstring().c_str(),
        L"main", profile.c_str(),
        compilerFlags, _countof(compilerFlags),
        nullptr, 0, 
        nullptr,
        &dxcResult);

    HRESULT hr;
    dxcResult->GetStatus(&hr);
    if (SUCCEEDED(hr))
    {
        dxcResult->GetResult(
            reinterpret_cast<IDxcBlob**>(shaderBlob.GetAddressOf()));
    }
    else
    {
        dxcResult->GetErrorBuffer(
            reinterpret_cast<IDxcBlobEncoding**>(errorBlob.GetAddressOf()));
    }
    return hr;

}

void Renderer::makeModelGeometry(const std::shared_ptr<tinygltf::Model> model)
{
    for (const auto &mesh : model->meshes)
    {
        for (const auto &meshPrimitive : mesh.primitives)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            
            //Fetch accessors
            const auto& accPos = model->accessors[meshPrimitive.attributes.at("POSITION")];
            const auto& accNrm = model->accessors[meshPrimitive.attributes.at("NORMAL")];
            const auto& accIdx = model->accessors[meshPrimitive.indices];
            
            //Fetch buffer views
            const auto& bvPos = model->bufferViews[accPos.bufferView];
            const auto& bvNrm = model->bufferViews[accNrm.bufferView];
            const auto& bvIdx = model->bufferViews[accIdx.bufferView];

            //Fetch buffers
            const auto& bPos = model->buffers[bvPos.buffer];
            const auto& bNrm = model->buffers[bvNrm.buffer];
            const auto& bIdx = model->buffers[bvIdx.buffer];
            
            //Fetch vertex data
            const float* vertPos = reinterpret_cast<const float*>(&bPos.data[bvPos.byteOffset + accPos.byteOffset]);
            const float* vertNrm = reinterpret_cast<const float*>(&bNrm.data[bvNrm.byteOffset + accPos.byteOffset]);

            //Assemble vertex data
            auto vertCount = accPos.count;
            for (uint32_t i = 0; i < vertCount; ++i)
            {
                int vid0 = 3 * i, vid1 = 3 * i + 1, vid2 = 3 * i + 2;
                vertices.emplace_back(
                    Vertex
                    {
                      DirectX::XMFLOAT3(vertPos[vid0], vertPos[vid1],vertPos[vid2]),
                      DirectX::XMFLOAT3(vertNrm[vid0], vertNrm[vid1],vertNrm[vid2]),
                    }
                );
            }

            //Fetch index data
            const uint16_t* idc = reinterpret_cast<const uint16_t*>(&bIdx.data[bvIdx.byteOffset + accPos.byteOffset]);
            for (size_t i = 0; i < accIdx.count; ++i)
            {
                indices.emplace_back(idc[i]);
            }

            auto vbSize = UINT(sizeof(Vertex) * vertices.size());
            auto ibSize = UINT(sizeof(uint32_t) * indices.size());
            ModelMesh modelMesh;
            auto vb = createBuffer(vbSize, vertices.data());
            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = vb->GetGPUVirtualAddress();
            vbView.SizeInBytes = vbSize;
            vbView.StrideInBytes = sizeof(Vertex);
            modelMesh.vertexBuffer.buffer = vb;
            modelMesh.vertexBuffer.vertexView = vbView;

            auto ib = createBuffer(ibSize, indices.data());
            D3D12_INDEX_BUFFER_VIEW ibView;
            ibView.BufferLocation = ib->GetGPUVirtualAddress();
            ibView.Format = DXGI_FORMAT_R32_UINT;
            ibView.SizeInBytes = ibSize;
            modelMesh.indexBuffer.buffer = ib;
            modelMesh.indexBuffer.indexView = ibView;

            modelMesh.vertexCount = UINT(vertices.size());
            modelMesh.indexCount = UINT(indices.size());
            modelMesh.materialIndex = meshPrimitive.material;
            m_model.meshes.push_back(modelMesh);
        }
    }
}

/*
void Renderer::makeModelMaterial(const std::shared_ptr<tinygltf::Model> model)
{
    int texIdx = 0;
    for (auto& mat: model->materials)
    {

        tinygltf::Accessor accTex = model->accessors[mat.pbrMetallicRoughness.baseColorTexture.index];
        if (accTex.count == 0)
        {
            accTex = model->accessors[mat.normalTexture.index];
        }

        const auto& bvTex = model->bufferViews[accTex.bufferView];
        const auto& bTex = model->buffers[bvTex.buffer];
        const unsigned char* texData = reinterpret_cast<const unsigned char*>(&bTex.data[bvTex.byteOffset + accTex.byteOffset]);
        std::vector<const unsigned char> texture(texData, texData + sizeof(unsigned char*) * (accTex.count - 1));

        Material material{};
        material.alphaMode = mat.alphaMode;
        auto texObj = createTextureFromMemory(texture);
        material.texture = texObj.texture;

        auto descriptorIndex = m_srvDescriptorBase + texIdx;
        auto srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_heapSRVCBV->GetCPUDescriptorHandleForHeapStart(),
            descriptorIndex,
            m_srvCBVDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = texObj.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_device->CreateShaderResourceView(
            texObj.texture.Get(), &srvDesc, srvHandle);
        material.shaderResourceView =
            CD3DX12_GPU_DESCRIPTOR_HANDLE(
                m_heapSRVCBV->GetGPUDescriptorHandleForHeapStart(),
                descriptorIndex,
                m_srvCBVDescriptorSize);

        m_model.materials.push_back(material);

        texIdx++;
    }
}

Renderer::TextureObject Renderer::createTextureFromMemory(const std::vector<const unsigned char>& imageData)
{
    ComPtr<ID3D12Resource1> staging;
    HRESULT hr;
    DirectX::ScratchImage image;
    hr = LoadFromWICMemory(
        imageData.data(),
        imageData.size(),
        DirectX::WIC_FLAGS_NONE,
        nullptr,
        image);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed LoadFromWICMemory");
    }
    const auto& metadata = image.GetMetadata();
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    DirectX::PrepareUpload(
        m_device.Get(), image.GetImages(), image.GetImageCount(),
        metadata, subresources);

    ComPtr<ID3D12Resource> texture;
    DirectX::CreateTexture(m_device.Get(), metadata, &texture);

    const auto totalBytes = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
    const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);
    m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&staging)
    );

    ComPtr<ID3D12GraphicsCommandList> command;
    m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get(),
        nullptr, IID_PPV_ARGS(&command));

    UpdateSubresources(
        command.Get(),
        texture.Get(), staging.Get(),
        0, 0, uint32_t(subresources.size()), subresources.data()
    );

    auto barrierTex = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    command->ResourceBarrier(1, &barrierTex);

    command->Close();
    ID3D12CommandList* cmds[] = { command.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmds);
    m_frameFenceValues[m_frameIndex]++;
    waitGPU();

    TextureObject ret;
    texture.As(&ret.texture);
    ret.format = metadata.format;

    return ret;
}
*/

ComPtr<ID3D12PipelineState> Renderer::createPipelineState()
{
    // インプットレイアウト
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    };

    // パイプラインステートオブジェクトの生成.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    // シェーダーのセット
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.Get());
    // ブレンドステート設定
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // ラスタライザーステート
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // 出力先は1ターゲット
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // デプスバッファのフォーマットを設定
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

    // ルートシグネチャのセット
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // マルチサンプル設定
    psoDesc.SampleDesc = { 1,0 };
    psoDesc.SampleMask = UINT_MAX; // これを忘れると絵が出ない＆警告も出ないので注意.

    ComPtr<ID3D12PipelineState> pipeline;
    HRESULT hr;
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateGraphicsPipelineState failed");
    }
    return pipeline;

};

void Renderer::waitGPU()
{
    HRESULT hr;
    const auto finishExpected = m_frameFenceValues[m_frameIndex];
    hr = m_commandQueue->Signal(m_frameFences[m_frameIndex].Get(), finishExpected);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed Signal(WaitGPU)");
    }
    m_frameFences[m_frameIndex]->SetEventOnCompletion(finishExpected, m_fenceWaitEvent);
    WaitForSingleObject(m_fenceWaitEvent, m_gpuWaitTimeout);
    m_frameFenceValues[m_frameIndex] = finishExpected + 1;
}


tinygltf::Model* Renderer::getModel(std::string modelPath)
{
    return m_modelList[modelPath].get();
}

void Renderer::loadModel(std::string path)
{
    tinygltf::TinyGLTF loader;
    auto model = std::make_unique<tinygltf::Model>();
    std::string err;
    std::string warn;
    bool ret = loader.LoadBinaryFromFile(model.get(), &err, &warn, path);

    if (!warn.empty()) {
        throw std::runtime_error(warn.c_str());
    }

    if (!err.empty()) {
        throw std::runtime_error(err.c_str());
    }

    if (!ret) {
        throw std::runtime_error("Failed to parse glTF");
    }

    m_modelList.insert(std::make_pair(path, std::move(model)));
}