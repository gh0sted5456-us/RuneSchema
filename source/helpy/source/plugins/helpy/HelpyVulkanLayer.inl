// Included by the bridge so Vulkan presentation shares the same input/session state as DXGI and WGL.
namespace VulkanLayer {
struct Instance {
    VkInstance Handle{};PFN_vkGetInstanceProcAddr Get{};
};
struct Swap {
    VkSwapchainKHR Handle{};HWND Window=nullptr;VkExtent2D Extent{};VkFormat Format{};std::vector<VkImage> Images;
};
struct Device {
    VkDevice Handle{};VkPhysicalDevice Physical{};std::shared_ptr<Instance> Parent;PFN_vkGetDeviceProcAddr Get{};PFN_vkQueuePresentKHR Present{};bool Sharing=false;
    std::map<VkQueue,uint32_t> Queues;std::map<VkSwapchainKHR,Swap> Swaps;
};
std::mutex Gate;
std::map<void*,std::shared_ptr<Instance>> Instances;
std::map<void*,std::shared_ptr<Device>> Devices;
std::map<VkSurfaceKHR,HWND> Windows;
template<class T> void* Key(T value){return value?*reinterpret_cast<void**>(value):nullptr;}
template<class T> std::shared_ptr<Instance> Parent(T value){std::lock_guard lock(Gate);const auto found=Instances.find(Key(value));return found==Instances.end()?nullptr:found->second;}
template<class T> std::shared_ptr<Device> Context(T value){std::lock_guard lock(Gate);const auto found=Devices.find(Key(value));return found==Devices.end()?nullptr:found->second;}
template<class F> F Function(const std::shared_ptr<Device>& device,const char* name){return reinterpret_cast<F>(device->Get(device->Handle,name));}
PFN_vkVoidFunction VKAPI_CALL GetInstance(VkInstance,const char*);
PFN_vkVoidFunction VKAPI_CALL GetDevice(VkDevice,const char*);
VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocation,VkInstance* result){
    auto* link=reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(info->pNext));while(link&&(link->sType!=VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO||link->function!=VK_LAYER_LINK_INFO))link=reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(link->pNext));if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    const auto next=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;const auto create=reinterpret_cast<PFN_vkCreateInstance>(next(nullptr,"vkCreateInstance"));link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto status=create(info,allocation,result);if(status==VK_SUCCESS){auto entry=std::make_shared<Instance>();entry->Handle=*result;entry->Get=next;std::lock_guard lock(Gate);Instances[Key(*result)]=entry;}return status;
}
void VKAPI_CALL DestroyInstance(VkInstance instance,const VkAllocationCallbacks* allocation){const auto parent=Parent(instance);if(!parent)return;{std::lock_guard lock(Gate);Instances.erase(Key(instance));}reinterpret_cast<PFN_vkDestroyInstance>(parent->Get(instance,"vkDestroyInstance"))(instance,allocation);}
VkResult VKAPI_CALL CreateSurface(VkInstance instance,const VkWin32SurfaceCreateInfoKHR* info,const VkAllocationCallbacks* allocation,VkSurfaceKHR* result){const auto parent=Parent(instance);if(!parent)return VK_ERROR_INITIALIZATION_FAILED;const auto status=reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(parent->Get(instance,"vkCreateWin32SurfaceKHR"))(instance,info,allocation,result);if(status==VK_SUCCESS){std::lock_guard lock(Gate);Windows[*result]=info->hwnd;}return status;}
void VKAPI_CALL DestroySurface(VkInstance instance,VkSurfaceKHR surface,const VkAllocationCallbacks* allocation){const auto parent=Parent(instance);if(!parent)return;{std::lock_guard lock(Gate);Windows.erase(surface);}reinterpret_cast<PFN_vkDestroySurfaceKHR>(parent->Get(instance,"vkDestroySurfaceKHR"))(instance,surface,allocation);}
VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* allocation,VkDevice* result){
    const auto parent=Parent(physical);if(!parent)return VK_ERROR_INITIALIZATION_FAILED;auto* link=reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(info->pNext));while(link&&(link->sType!=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO||link->function!=VK_LAYER_LINK_INFO))link=reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(link->pNext));if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    const auto instanceGet=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;const auto deviceGet=link->u.pLayerInfo->pfnNextGetDeviceProcAddr;const auto create=reinterpret_cast<PFN_vkCreateDevice>(instanceGet(parent->Handle,"vkCreateDevice"));link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto enumerate=reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(parent->Get(parent->Handle,"vkEnumerateDeviceExtensionProperties"));uint32_t count=0;bool sharing=enumerate&&enumerate(physical,nullptr,&count,nullptr)==VK_SUCCESS&&count<65536;std::vector<VkExtensionProperties> supported(count);if(sharing)sharing=enumerate(physical,nullptr,&count,supported.data())==VK_SUCCESS;
    std::vector<const char*> extensions;if(info->enabledExtensionCount)extensions.assign(info->ppEnabledExtensionNames,info->ppEnabledExtensionNames+info->enabledExtensionCount);
    for(const auto* name:{VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME}){
        if(std::none_of(supported.begin(),supported.end(),[&](const auto& value){return std::strcmp(name,value.extensionName)==0;})){sharing=false;break;}
        if(std::none_of(extensions.begin(),extensions.end(),[&](const auto* value){return std::strcmp(name,value)==0;}))extensions.push_back(name);
    }
    auto amended=*info;if(sharing){amended.enabledExtensionCount=uint32_t(extensions.size());amended.ppEnabledExtensionNames=extensions.data();}const auto status=create(physical,&amended,allocation,result);
    if(status==VK_SUCCESS){auto entry=std::make_shared<Device>();entry->Handle=*result;entry->Physical=physical;entry->Parent=parent;entry->Get=deviceGet;entry->Present=reinterpret_cast<PFN_vkQueuePresentKHR>(deviceGet(*result,"vkQueuePresentKHR"));entry->Sharing=sharing;std::lock_guard lock(Gate);Devices[Key(*result)]=entry;}return status;
}
void Release(const std::shared_ptr<Device>& device,VkSwapchainKHR swap=VK_NULL_HANDLE){
    auto& state=Shared();HWND window=nullptr;{
        std::lock_guard lock(state.Gate);if(!state.Vulkan||state.Vulkan->Device!=device->Handle||(swap&&state.Vulkan->Swap!=swap))return;
        state.Hide();state.Capturing=false;state.Drawing=false;window=state.Window;
        if(state.RendererVk.Active())Function<PFN_vkQueueWaitIdle>(device,"vkQueueWaitIdle")(state.Vulkan->Queue);
        state.RendererVk.Reset();state.Vulkan.reset();state.VulkanVerifier.Reset();state.Ready=false;state.Api.clear();
    }if(window)PostMessageW(window,WM_APP+0x5a1,0,0);
}
void VKAPI_CALL DestroyDevice(VkDevice handle,const VkAllocationCallbacks* allocation){const auto device=Context(handle);if(!device)return;Release(device);{std::lock_guard lock(Gate);Devices.erase(Key(handle));}Function<PFN_vkDestroyDevice>(device,"vkDestroyDevice")(handle,allocation);}
void VKAPI_CALL GetQueue(VkDevice handle,uint32_t family,uint32_t index,VkQueue* output){const auto device=Context(handle);if(!device)return;Function<PFN_vkGetDeviceQueue>(device,"vkGetDeviceQueue")(handle,family,index,output);std::lock_guard lock(Gate);device->Queues[*output]=family;}
void VKAPI_CALL GetQueue2(VkDevice handle,const VkDeviceQueueInfo2* info,VkQueue* output){const auto device=Context(handle);if(!device)return;Function<PFN_vkGetDeviceQueue2>(device,"vkGetDeviceQueue2")(handle,info,output);std::lock_guard lock(Gate);device->Queues[*output]=info->queueFamilyIndex;}
VkResult VKAPI_CALL CreateSwap(VkDevice handle,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* allocation,VkSwapchainKHR* output){
    const auto device=Context(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;const auto status=Function<PFN_vkCreateSwapchainKHR>(device,"vkCreateSwapchainKHR")(handle,info,allocation,output);if(status!=VK_SUCCESS)return status;
    HWND window=nullptr;{std::lock_guard lock(Gate);if(const auto found=Windows.find(info->surface);found!=Windows.end())window=found->second;}
    if(window&&GameWindow(window)&&(info->imageUsage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)&&info->imageArrayLayers==1){Swap entry{*output,window,info->imageExtent,info->imageFormat};uint32_t count=0;const auto get=Function<PFN_vkGetSwapchainImagesKHR>(device,"vkGetSwapchainImagesKHR");if(get(handle,*output,&count,nullptr)==VK_SUCCESS&&count&&count<=8){entry.Images.resize(count);if(get(handle,*output,&count,entry.Images.data())==VK_SUCCESS){std::lock_guard lock(Gate);device->Swaps[*output]=std::move(entry);}}}return status;
}
void VKAPI_CALL DestroySwap(VkDevice handle,VkSwapchainKHR swap,const VkAllocationCallbacks* allocation){const auto device=Context(handle);if(!device)return;Release(device,swap);{std::lock_guard lock(Gate);device->Swaps.erase(swap);}Function<PFN_vkDestroySwapchainKHR>(device,"vkDestroySwapchainKHR")(handle,swap,allocation);}
VkResult VKAPI_CALL Present(VkQueue queue,const VkPresentInfoKHR* info){
    const auto device=Context(queue);if(!device||!device->Present)return VK_ERROR_INITIALIZATION_FAILED;auto& state=Shared();const auto next=device->Present;
    VkPresentInfoKHR amended{};VkSemaphore finished{};bool rendered=false;
    if(state.Started&&!state.Stopping&&(!state.Ready||state.Drawing)){
        std::unique_lock lock(state.Gate,std::defer_lock);if(state.Drawing)lock.lock();else if(!lock.try_lock())return next(queue,info);try{
            if(!state.Vulkan){Swap swap;uint32_t family=UINT32_MAX;{std::lock_guard metadata(Gate);const auto familyAt=device->Queues.find(queue);if(familyAt!=device->Queues.end())family=familyAt->second;for(uint32_t i=0;i<info->swapchainCount;++i)if(const auto found=device->Swaps.find(info->pSwapchains[i]);found!=device->Swaps.end()){swap=found->second;break;}}
                if(swap.Handle&&family!=UINT32_MAX){
                    if(!device->Sharing)throw std::runtime_error("Vulkan driver does not provide the required Win32 external-memory/semaphore extensions");
                    const auto getFamilies=reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(device->Parent->Get(device->Parent->Handle,"vkGetPhysicalDeviceQueueFamilyProperties"));uint32_t count=0;getFamilies(device->Physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);getFamilies(device->Physical,&count,families.data());if(family>=count||!(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))throw std::runtime_error("Vulkan presentation uses a separate non-graphics queue; native overlay requires a verified graphics/present queue");
                    auto connection=std::make_shared<VulkanConnection>();connection->Device=device->Handle;connection->Queue=queue;connection->Family=family;connection->Get=device->Get;connection->Swap=swap.Handle;connection->Extent=swap.Extent;connection->Format=swap.Format;connection->Images=swap.Images;const auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(device->Parent->Get(device->Parent->Handle,"vkGetPhysicalDeviceMemoryProperties"));getMemory(device->Physical,&connection->Memory);
                    const auto getProperties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(device->Parent->Get(device->Parent->Handle,"vkGetPhysicalDeviceProperties2"));if(!getProperties)throw std::runtime_error("Vulkan 1.1 GPU identity support unavailable");auto identity=VkInfo<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);auto properties=VkInfo<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);properties.pNext=&identity;getProperties(device->Physical,&properties);if(!identity.deviceLUIDValid)throw std::runtime_error("Vulkan GPU LUID unavailable");std::memcpy(&connection->Adapter,identity.deviceLUID,sizeof(LUID));
                    state.Vulkan=connection;state.Window=swap.Window;state.Width=swap.Extent.width;state.Height=swap.Extent.height;state.Api="Vulkan";state.Error.clear();state.Ready=true;
                }
            }
            if(state.Vulkan&&state.Vulkan->Queue==queue&&state.RendererVk.Active())for(uint32_t i=0;i<info->swapchainCount;++i)if(info->pSwapchains[i]==state.Vulkan->Swap){auto* page=state.Page();if(GetForegroundWindow()!=state.Window)InterlockedExchange(&page->Visible,0);rendered=state.RendererVk.Draw(info->pImageIndices[i],*info,amended,finished);const bool active=HelpyOverlayFrames::Alive(*page,GetTickCount64())&&GetForegroundWindow()==state.Window;if(state.Capturing.exchange(active)!=active)PostMessageW(state.Window,WM_APP+0x5a1,0,0);if(!active)state.Drawing=false;break;}
        }catch(const std::exception& error){state.Error=error.what();state.Stopping=true;state.Hide();state.Capturing=false;state.Drawing=false;if(state.Window)PostMessageW(state.Window,WM_APP+0x5a1,0,0);}
    }
    return next(queue,rendered?&amended:info);
}
PFN_vkVoidFunction Intercept(const char* name){
    #define INTERCEPT(vk,local) if(std::strcmp(name,"vk" #vk)==0)return reinterpret_cast<PFN_vkVoidFunction>(local)
    INTERCEPT(GetInstanceProcAddr,GetInstance);INTERCEPT(GetDeviceProcAddr,GetDevice);INTERCEPT(CreateInstance,CreateInstance);INTERCEPT(DestroyInstance,DestroyInstance);INTERCEPT(CreateDevice,CreateDevice);INTERCEPT(DestroyDevice,DestroyDevice);INTERCEPT(CreateWin32SurfaceKHR,CreateSurface);INTERCEPT(DestroySurfaceKHR,DestroySurface);INTERCEPT(GetDeviceQueue,GetQueue);INTERCEPT(GetDeviceQueue2,GetQueue2);INTERCEPT(CreateSwapchainKHR,CreateSwap);INTERCEPT(DestroySwapchainKHR,DestroySwap);INTERCEPT(QueuePresentKHR,Present);
    #undef INTERCEPT
    return nullptr;
}
PFN_vkVoidFunction VKAPI_CALL GetInstance(VkInstance instance,const char* name){if(!name)return nullptr;if(auto intercepted=Intercept(name))return intercepted;const auto parent=Parent(instance);return parent?parent->Get(instance,name):nullptr;}
PFN_vkVoidFunction VKAPI_CALL GetDevice(VkDevice device,const char* name){if(!name)return nullptr;if(auto intercepted=Intercept(name))return intercepted;const auto context=Context(device);return context?context->Get(device,name):nullptr;}
}
