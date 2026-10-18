#ifndef WINE_COMPOSITION_API_DCOMP_COMPAT_H
#define WINE_COMPOSITION_API_DCOMP_COMPAT_H

/*
 * The MinGW dcomp.h shipped by Debian 13 is not C-compilable: it uses
 * interface names before declaring them and contains C++ reference overloads.
 * The probe only needs the initial vtable portions below, so keep this local
 * ABI declaration deliberately small and easy to compare with Microsoft's
 * dcomp.h. It is not a replacement DirectComposition implementation.
 */

#include <windows.h>
#include <dxgi.h>

typedef struct IDCompositionTarget IDCompositionTarget;
typedef struct IDCompositionVisual IDCompositionVisual;
typedef struct IDCompositionDevice IDCompositionDevice;

typedef struct IDCompositionTargetVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionTarget *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDCompositionTarget *);
    ULONG (STDMETHODCALLTYPE *Release)(IDCompositionTarget *);
    HRESULT (STDMETHODCALLTYPE *SetRoot)(IDCompositionTarget *, IDCompositionVisual *);
} IDCompositionTargetVtbl;

struct IDCompositionTarget
{
    const IDCompositionTargetVtbl *lpVtbl;
};

typedef struct IDCompositionVisualVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionVisual *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDCompositionVisual *);
    ULONG (STDMETHODCALLTYPE *Release)(IDCompositionVisual *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetXAnimation)(IDCompositionVisual *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetXFloat)(IDCompositionVisual *, float);
    HRESULT (STDMETHODCALLTYPE *SetOffsetYAnimation)(IDCompositionVisual *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *SetOffsetYFloat)(IDCompositionVisual *, float);
    HRESULT (STDMETHODCALLTYPE *SetTransform)(IDCompositionVisual *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *SetTransformMatrix)(IDCompositionVisual *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetTransformParent)(IDCompositionVisual *, IDCompositionVisual *);
    HRESULT (STDMETHODCALLTYPE *SetEffect)(IDCompositionVisual *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *SetBitmapInterpolationMode)(IDCompositionVisual *, int);
    HRESULT (STDMETHODCALLTYPE *SetBorderMode)(IDCompositionVisual *, int);
    HRESULT (STDMETHODCALLTYPE *SetClip)(IDCompositionVisual *, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *SetClipRect)(IDCompositionVisual *, const void *);
    HRESULT (STDMETHODCALLTYPE *SetContent)(IDCompositionVisual *, IUnknown *);
} IDCompositionVisualVtbl;

struct IDCompositionVisual
{
    const IDCompositionVisualVtbl *lpVtbl;
};

typedef struct IDCompositionDeviceVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDCompositionDevice *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDCompositionDevice *);
    ULONG (STDMETHODCALLTYPE *Release)(IDCompositionDevice *);
    HRESULT (STDMETHODCALLTYPE *Commit)(IDCompositionDevice *);
    HRESULT (STDMETHODCALLTYPE *WaitForCommitCompletion)(IDCompositionDevice *);
    HRESULT (STDMETHODCALLTYPE *GetFrameStatistics)(IDCompositionDevice *, void *);
    HRESULT (STDMETHODCALLTYPE *CreateTargetForHwnd)(IDCompositionDevice *, HWND, BOOL,
            IDCompositionTarget **);
    HRESULT (STDMETHODCALLTYPE *CreateVisual)(IDCompositionDevice *, IDCompositionVisual **);
} IDCompositionDeviceVtbl;

struct IDCompositionDevice
{
    const IDCompositionDeviceVtbl *lpVtbl;
};

static const IID IID_IDCompositionDevice =
    {0xc37ea93a, 0xe7aa, 0x450d, {0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3}};

static const IID IID_IDCompositionDesktopDevice =
    {0x5f4633fe, 0x1e08, 0x4cb8, {0x8c, 0x75, 0xce, 0x24, 0x33, 0x3f, 0x56, 0x02}};

HRESULT WINAPI DCompositionCreateDevice(IDXGIDevice *, REFIID, void **);
HRESULT WINAPI DCompositionCreateDevice2(IUnknown *, REFIID, void **);

#endif
