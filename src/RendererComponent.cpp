#include "RendererComponent.h"

void RendererComponent::initialize(HWND hwnd)
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

    //Create command list
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

    prepare(modelFilePath);
}

void RendererComponent::render()
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

void RendererComponent::setCommands()
{
    
}

void RendererComponent::waitPreviousFrame()
{
    auto& fence = m_frameFence[m_frameIndex];
    const auto currentValue = ++m_frameFenceValues[m_frameIndex];
    m_commandQueue->Signal(fence.Get(), currentValue);

    auto nextIndex = (m_frameIndex + 1) % FrameBufferCount;
    const auto finishExpected = m_frameFenceValues[nextIndex];
    const auto nextFenceValue = m_frameFence[nextIndex]->GetCompletedValue();
    if (nextFenceValue < finishExpected)
    {
        m_frameFence[nextIndex]->SetEventOnCompletion(finishExpected, m_fenceWaitEvent);
        WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
    }

}
