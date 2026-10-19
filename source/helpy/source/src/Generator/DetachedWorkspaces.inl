// Native presentation surfaces only. Both share the existing map/recipe models
// and authority-checked bridge rather than making copies of game state.
bool HelpyStandaloneWindow::OwnsWindow(HWND window)const{
    if(!window)return false;
    if(window==m_publishedWindow.load()||window==m_publishedBayWindow.load()||window==m_publishedCacheWindow.load())return true;
    for(const auto& published:m_publishedWorkspaces)if(window==published.load())return true;return false;
}
void HelpyStandaloneWindow::DestroyWorkspacePanel(size_t index){
    auto& panel=m_workspaces.at(index);m_publishedWorkspaces.at(index)=nullptr;
    HelpyTaskbar::Current.Forget(WorkspaceTask(index));
    if(m_context)m_context->OMSetRenderTargets(0,nullptr,nullptr);
    if(panel.Window&&GetCapture()==panel.Window)ReleaseCapture();
    if(panel.Target){panel.Target->Release();panel.Target=nullptr;}if(panel.Swap){panel.Swap->Release();panel.Swap=nullptr;}
    if(panel.Window){DestroyWindow(panel.Window);panel.Window=nullptr;}
}
bool HelpyStandaloneWindow::SyncWorkspacePanel(size_t index){
    auto& panel=m_workspaces.at(index);const auto task=WorkspaceTask(index);
    if(!WorkspaceDetached(index)||(index==2&&!RuntimeDetailsPanel::Open)||(index==3&&!MapPanel::Data.ShowParty)||(index==4&&!PlayerInventoryPanel::Open)){if(panel.Window)DestroyWorkspacePanel(index);return false;}
    // A detached party opens only when requested and a current party exists.
    if(index==3&&(!MapPanel::Data.Snapshot.contains("Players")||MapPanel::Data.Snapshot["Players"].empty())){if(panel.Window)DestroyWorkspacePanel(index);return false;}
    if(HelpyTaskbar::Current.Contains(task)){if(panel.Window)ShowWindow(panel.Window,SW_HIDE);return false;}
    if(!panel.Window){
        RECT parent{};GetWindowRect(m_hwnd,&parent);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        const int width=std::min(index==3?640:index==2?680:1000,int(monitor.rcWork.right-monitor.rcWork.left)),height=std::min(index==3?360:index==2?640:760,int(monitor.rcWork.bottom-monitor.rcWork.top));
        const int x=std::clamp(int(parent.left+48+int(index)*32),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
        const int y=std::clamp(int(parent.top+48+int(index)*32),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
        wchar_t windowClass[256]{};GetClassNameW(m_hwnd,windowClass,256);
        panel.Window=CreateWindowExW(WS_EX_TOOLWINDOW,windowClass,WorkspaceCaption(index),WS_OVERLAPPEDWINDOW,x,y,width,height,m_hwnd,nullptr,m_module,this);
        if(panel.Window)DragAcceptFiles(panel.Window,TRUE);
        using Microsoft::WRL::ComPtr;ComPtr<IDXGIDevice> device;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
        DXGI_SWAP_CHAIN_DESC description{};description.BufferCount=2;description.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;description.OutputWindow=panel.Window;description.SampleDesc.Count=1;description.Windowed=TRUE;description.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        bool ready=panel.Window&&SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&device)))&&SUCCEEDED(device->GetAdapter(&adapter))
            &&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateSwapChain(m_device,&description,&panel.Swap));
        if(ready){ComPtr<ID3D11Texture2D> buffer;ready=SUCCEEDED(panel.Swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))&&SUCCEEDED(m_device->CreateRenderTargetView(buffer.Get(),nullptr,&panel.Target));}
        if(!ready){DestroyWorkspacePanel(index);WorkspaceDetached(index)=false;PushToast("Could not detach this workspace; it remains docked.",4);return false;}
        SendMessageW(panel.Window,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,32,32,LR_SHARED)));
        SendMessageW(panel.Window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(LoadImageW(m_module,MAKEINTRESOURCEW(IDI_RSMK),IMAGE_ICON,16,16,LR_SHARED)));
        m_publishedWorkspaces[index]=panel.Window;ApplyCaptionTheme(panel.Window,HelpyImGuiTheme::Current);ShowWindow(panel.Window,SW_SHOWNOACTIVATE);
    }
    if(index==2&&RuntimeDetailsPanel::FocusWanted){ShowWindow(panel.Window,SW_RESTORE);SetForegroundWindow(panel.Window);SetFocus(panel.Window);RuntimeDetailsPanel::FocusWanted=false;}
    ApplyCaptionTheme(panel.Window,HelpyImGuiTheme::Current);return IsWindowVisible(panel.Window)&&!IsIconic(panel.Window)&&panel.Target;
}
