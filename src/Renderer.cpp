#include "Renderer.h"

Renderer::Renderer()
{
    m_renderTargets.resize(m_frameBufferCount);
    m_frameFenceValues.resize(m_frameBufferCount);
    m_frameIndex = 0;

    m_fenceWaitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef _DEBUG
    m_modelPathList = { "../Resources/Field.glb", "../Resources/Player.glb", "../Resources/Enemy.glb" };
#else
    m_modelPathList = { "Resources/Field.glb", "Resources/Player.glb", "Resources/Enemy.glb" };
#endif

}

void Renderer::initialize(HWND hwnd)
{
    HRESULT hr;
    UINT dxgiFlags = 0;

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

void Renderer::prepare(UINT modelID)
{
    //Fetch model from list
    const std::shared_ptr<tinygltf::Model> model = m_modelList[m_modelPathList[modelID]];
    
    // マテリアル数分のSRVディスクリプタが必要になるのでここで準備.
    createIndividualDescriptorHeaps(model->materials.size());
    
    makeModelGeometry(model);
    makeModelMaterial(model);

    // シェーダーをコンパイル.
    HRESULT hr;
    ComPtr<ID3DBlob> errBlob;
    hr = CompileShaderFromFile(L"shaderVS.hlsl", L"vs_6_0", m_vs, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }
    hr = CompileShaderFromFile(L"shaderOpaquePS.hlsl", L"ps_6_0", m_psOpaque, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }
    hr = CompileShaderFromFile(L"shaderAlphaPS.hlsl", L"ps_6_0", m_psAlpha, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }

    m_srvDescriptorBase = FrameBufferCount;

    CD3DX12_DESCRIPTOR_RANGE cbv, srv, sampler;
    cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 レジスタ
    srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 レジスタ
    sampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // s0 レジスタ

    CD3DX12_ROOT_PARAMETER rootParams[3];
    rootParams[0].InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParams[1].InitAsDescriptorTable(1, &srv, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[2].InitAsDescriptorTable(1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);

    // ルートシグネチャの構築
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Init(
        _countof(rootParams), rootParams,   //pParameters
        0, nullptr,   //pStaticSamplers
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );
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

    m_pipelineOpaque = CreateOpaquePSO();
    m_pipelineAlpha = CreateAlphaPSO();


    // 定数バッファ/定数バッファビューの生成
    m_constantBuffers.resize(FrameBufferCount);
    m_cbViews.resize(FrameBufferCount);
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
        m_constantBuffers[i] = CreateBuffer(bufferSize, nullptr);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
        cbDesc.BufferLocation = m_constantBuffers[i]->GetGPUVirtualAddress();
        cbDesc.SizeInBytes = bufferSize;
        CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvcbvDescriptorSize);
        m_device->CreateConstantBufferView(&cbDesc, handleCBV);

        m_cbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvcbvDescriptorSize);
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
    const float clearColor[] = { 0.1f, 0.25f, 0.5f, 0.0f };
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
    
}

ComPtr<ID3D12Resource1> Renderer::createBuffer(UINT bufferSize, const void* initialData)
{
    return ComPtr<ID3D12Resource1>();
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
    m_SRVCBVDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
    return E_NOTIMPL;
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
            
            //Fetch buffer views
            const auto& bvPos = model->bufferViews[accPos.bufferView];
            const auto& bvNrm = model->bufferViews[accNrm.bufferView];

            //Fetch buffer
            auto vertPos = bPos = model
            
            auto vertexCount = model->accessors.;
            for (uint32_t i = 0; i < vertexCount; ++i)
            {
                // 頂点データの構築
                int vid0 = 3 * i, vid1 = 3 * i + 1, vid2 = 3 * i + 2;
                int tid0 = 2 * i, tid1 = 2 * i + 1;
                vertices.emplace_back(
                    Vertex{
                      XMFLOAT3(vertPos[vid0], vertPos[vid1],vertPos[vid2]),
                      XMFLOAT3(vertNrm[vid0], vertNrm[vid1],vertNrm[vid2]),
                      XMFLOAT2(vertUV[tid0],vertUV[tid1])
                    }
                );
            }
            // インデックスデータ
            indices = reader->ReadBinaryData<uint32_t>(doc, accIndex);

            auto vbSize = UINT(sizeof(Vertex) * vertices.size());
            auto ibSize = UINT(sizeof(uint32_t) * indices.size());
            ModelMesh modelMesh;
            auto vb = CreateBuffer(vbSize, vertices.data());
            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = vb->GetGPUVirtualAddress();
            vbView.SizeInBytes = vbSize;
            vbView.StrideInBytes = sizeof(Vertex);
            modelMesh.vertexBuffer.buffer = vb;
            modelMesh.vertexBuffer.vertexView = vbView;

            auto ib = CreateBuffer(ibSize, indices.data());
            D3D12_INDEX_BUFFER_VIEW ibView;
            ibView.BufferLocation = ib->GetGPUVirtualAddress();
            ibView.Format = DXGI_FORMAT_R32_UINT;
            ibView.SizeInBytes = ibSize;
            modelMesh.indexBuffer.buffer = ib;
            modelMesh.indexBuffer.indexView = ibView;

            modelMesh.vertexCount = UINT(vertices.size());
            modelMesh.indexCount = UINT(indices.size());
            modelMesh.materialIndex = int(doc.materials.GetIndex(meshPrimitive.materialId));
            m_model.meshes.push_back(modelMesh);
        }
    }
}

void Renderer::makeModelMaterial(const std::shared_ptr<tinygltf::Model> model)
{
    int textureIndex = 0;
    for (auto& m : doc.materials.Elements())
    {
        auto textureId = m.metallicRoughness.baseColorTexture.textureId;
        if (textureId.empty())
        {
            textureId = m.normalTexture.textureId;
        }
        auto& texture = doc.textures.Get(textureId);
        auto& image = doc.images.Get(texture.imageId);
        auto imageBufferView = doc.bufferViews.Get(image.bufferViewId);
        auto imageData = reader->ReadBinaryData<char>(doc, imageBufferView);

        // imageData が画像データ
        Material material{};
        material.alphaMode = m.alphaMode;
        auto texObj = CreateTextureFromMemory(imageData);
        material.texture = texObj.texture;

        // シェーダーリソースビューの生成.
        auto descriptorIndex = m_srvDescriptorBase + textureIndex;
        auto srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(),
            descriptorIndex,
            m_srvcbvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = texObj.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_device->CreateShaderResourceView(
            texObj.texture.Get(), &srvDesc, srvHandle);
        material.shaderResourceView =
            CD3DX12_GPU_DESCRIPTOR_HANDLE(
                m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(),
                descriptorIndex,
                m_srvcbvDescriptorSize);

        m_model.materials.push_back(material);

        textureIndex++;
    }
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