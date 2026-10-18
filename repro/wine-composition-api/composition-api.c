#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <stdio.h>

#include "dcomp-compat.h"

static const char *hresult_name(HRESULT hr)
{
    switch ((unsigned long)hr)
    {
        case 0x00000000: return "S_OK";
        case 0x80004001: return "E_NOTIMPL";
        case 0x80004002: return "E_NOINTERFACE";
        case 0x80004005: return "E_FAIL";
        case 0x80070057: return "E_INVALIDARG";
        case 0x887a0001: return "DXGI_ERROR_INVALID_CALL";
        case 0x887a0002: return "DXGI_ERROR_NOT_FOUND";
        case 0x887a0004: return "DXGI_ERROR_UNSUPPORTED";
        default: return "other";
    }
}

static void report_hr(const char *operation, HRESULT hr)
{
    printf("%-58s hr=0x%08lx (%s)\n", operation, (unsigned long)hr, hresult_name(hr));
}

static LRESULT CALLBACK probe_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY)
        PostQuitMessage(0);

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static HWND create_probe_window(void)
{
    WNDCLASSA class = {0};
    HINSTANCE instance = GetModuleHandleA(NULL);

    class.lpfnWndProc = probe_window_proc;
    class.hInstance = instance;
    class.lpszClassName = "WineCompositionApiProbe";
    RegisterClassA(&class);

    return CreateWindowExA(0, class.lpszClassName,
            "Wine D3D11 shared-surface API probe",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 480, 320,
            NULL, NULL, instance, NULL);
}

static void release_unknown(IUnknown **object)
{
    if (*object)
    {
        IUnknown_Release(*object);
        *object = NULL;
    }
}

static HRESULT create_texture(ID3D11Device *device, UINT misc_flags,
        ID3D11Texture2D **texture)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_SUBRESOURCE_DATA data = {0};
    static const DWORD pixels[64 * 64] = {0xffff3b30};

    desc.Width = 64;
    desc.Height = 64;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = misc_flags;

    data.pSysMem = pixels;
    data.SysMemPitch = sizeof(DWORD) * 64;

    return ID3D11Device_CreateTexture2D(device, &desc, &data, texture);
}

static void probe_shared_texture(ID3D11Device *device, UINT misc_flags,
        const char *label)
{
    ID3D11Texture2D *texture = NULL;
    IDXGIResource *resource = NULL;
    IDXGIResource1 *resource1 = NULL;
    IDXGIKeyedMutex *keyed_mutex = NULL;
    ID3D11Device1 *device1 = NULL;
    ID3D11Resource *opened_resource = NULL;
    HANDLE legacy_handle = NULL;
    HANDLE nt_handle = NULL;
    HRESULT hr;

    printf("\n[texture] %s misc=0x%08lx\n", label, (unsigned long)misc_flags);

    hr = create_texture(device, misc_flags, &texture);
    report_hr("ID3D11Device::CreateTexture2D", hr);
    if (FAILED(hr))
        goto done;

    hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGIResource,
            (void **)&resource);
    report_hr("Texture::QueryInterface(IID_IDXGIResource)", hr);

    if (SUCCEEDED(hr))
    {
        hr = IDXGIResource_GetSharedHandle(resource, &legacy_handle);
        report_hr("IDXGIResource::GetSharedHandle", hr);
        printf("  legacy handle=%p\n", legacy_handle);
    }

    hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGIResource1,
            (void **)&resource1);
    report_hr("Texture::QueryInterface(IID_IDXGIResource1)", hr);

    if (SUCCEEDED(hr))
    {
        hr = IDXGIResource1_CreateSharedHandle(resource1, NULL,
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                NULL, &nt_handle);
        report_hr("IDXGIResource1::CreateSharedHandle", hr);
        printf("  NT handle=%p\n", nt_handle);
    }

    hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGIKeyedMutex,
            (void **)&keyed_mutex);
    report_hr("Texture::QueryInterface(IID_IDXGIKeyedMutex)", hr);

    if (legacy_handle)
    {
        hr = ID3D11Device_OpenSharedResource(device, legacy_handle,
                &IID_ID3D11Texture2D, (void **)&opened_resource);
        report_hr("ID3D11Device::OpenSharedResource", hr);
        release_unknown((IUnknown **)&opened_resource);
    }

    hr = ID3D11Device_QueryInterface(device, &IID_ID3D11Device1,
            (void **)&device1);
    report_hr("Device::QueryInterface(IID_ID3D11Device1)", hr);

    if (SUCCEEDED(hr) && nt_handle)
    {
        hr = ID3D11Device1_OpenSharedResource1(device1, nt_handle,
                &IID_ID3D11Texture2D, (void **)&opened_resource);
        report_hr("ID3D11Device1::OpenSharedResource1", hr);
        release_unknown((IUnknown **)&opened_resource);
    }

done:
    if (nt_handle)
        CloseHandle(nt_handle);
    if (device1)
        ID3D11Device1_Release(device1);
    if (keyed_mutex)
        IDXGIKeyedMutex_Release(keyed_mutex);
    if (resource1)
        IDXGIResource1_Release(resource1);
    if (resource)
        IDXGIResource_Release(resource);
    if (texture)
        ID3D11Texture2D_Release(texture);
}

static void probe_composition(ID3D11Device *device, IDXGIDevice *dxgi_device,
        HWND hwnd)
{
    IDXGIFactory2 *factory = NULL;
    IDXGISwapChain1 *swapchain = NULL;
    IDCompositionDevice *dcomp_device = NULL;
    IUnknown *dcomp_device2 = NULL;
    IDCompositionTarget *target = NULL;
    IDCompositionVisual *visual = NULL;
    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    HRESULT hr;

    printf("\n[composition]\n");

    hr = CreateDXGIFactory1(&IID_IDXGIFactory2, (void **)&factory);
    report_hr("CreateDXGIFactory1(IID_IDXGIFactory2)", hr);

    desc.Width = 320;
    desc.Height = 200;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    if (SUCCEEDED(hr))
    {
        hr = IDXGIFactory2_CreateSwapChainForComposition(factory,
                (IUnknown *)device, &desc, NULL, &swapchain);
        report_hr("IDXGIFactory2::CreateSwapChainForComposition", hr);
    }

    hr = DCompositionCreateDevice(dxgi_device, &IID_IDCompositionDevice,
            (void **)&dcomp_device);
    report_hr("DCompositionCreateDevice", hr);

    hr = DCompositionCreateDevice2((IUnknown *)device,
            &IID_IDCompositionDesktopDevice, (void **)&dcomp_device2);
    report_hr("DCompositionCreateDevice2(IID_IDCompositionDesktopDevice)", hr);

    if (dcomp_device)
    {
        hr = dcomp_device->lpVtbl->CreateTargetForHwnd(dcomp_device, hwnd,
                TRUE, &target);
        report_hr("IDCompositionDevice::CreateTargetForHwnd", hr);

        hr = dcomp_device->lpVtbl->CreateVisual(dcomp_device, &visual);
        report_hr("IDCompositionDevice::CreateVisual", hr);

        if (visual && swapchain)
        {
            hr = visual->lpVtbl->SetContent(visual, (IUnknown *)swapchain);
            report_hr("IDCompositionVisual::SetContent(swapchain)", hr);
        }
        else
        {
            printf("IDCompositionVisual::SetContent skipped: no swap chain\n");
        }

        if (target && visual)
        {
            hr = target->lpVtbl->SetRoot(target, visual);
            report_hr("IDCompositionTarget::SetRoot", hr);
        }

        hr = dcomp_device->lpVtbl->Commit(dcomp_device);
        report_hr("IDCompositionDevice::Commit", hr);
    }

    if (visual)
        visual->lpVtbl->Release(visual);
    if (target)
        target->lpVtbl->Release(target);
    if (dcomp_device2)
        IUnknown_Release(dcomp_device2);
    if (dcomp_device)
        dcomp_device->lpVtbl->Release(dcomp_device);
    if (swapchain)
        swapchain->lpVtbl->Release(swapchain);
    if (factory)
        factory->lpVtbl->Release(factory);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL feature_levels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    IDXGIDevice *dxgi_device = NULL;
    D3D_FEATURE_LEVEL feature_level = 0;
    HWND hwnd;
    HRESULT hr;

    printf("wine-composition-api probe\n");
    printf("architecture: 32-bit Windows executable\n");

    hwnd = create_probe_window();
    printf("CreateWindow: hwnd=%p\n", hwnd);

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels, sizeof(feature_levels) / sizeof(feature_levels[0]),
            D3D11_SDK_VERSION, &device, &feature_level, &context);
    report_hr("D3D11CreateDevice(HARDWARE)", hr);
    printf("  feature level=0x%04x\n", (unsigned int)feature_level);
    if (FAILED(hr))
        goto done;

    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice,
            (void **)&dxgi_device);
    report_hr("D3D11Device::QueryInterface(IID_IDXGIDevice)", hr);
    if (FAILED(hr))
        goto done;

    probe_shared_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX,
            "legacy shared + keyed mutex");
    probe_shared_texture(device, D3D11_RESOURCE_MISC_SHARED_NTHANDLE
            | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX,
            "NT shared + keyed mutex");
    probe_composition(device, dxgi_device, hwnd);

done:
    if (context)
        ID3D11DeviceContext_Release(context);
    if (dxgi_device)
        IDXGIDevice_Release(dxgi_device);
    if (device)
        ID3D11Device_Release(device);
    if (hwnd)
        DestroyWindow(hwnd);

    printf("probe complete\n");
    return 0;
}
