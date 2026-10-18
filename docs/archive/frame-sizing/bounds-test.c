#define COBJMACROS
#include "initguid.h"
#include "/home/octavio/Downloads/wine-upstream/dlls/dcomp/target.c"
#include <stdio.h>

static unsigned int failures;

static void check(BOOL condition, const char *name)
{
    printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

static void bounds(struct composition_target *target, HWND parent, LONG x, LONG y,
        UINT width, UINT height, const char *name)
{
    RECT rect;
    check(target_present_surface_frame(target, parent), "frame publication");
    check(target->surface_frame_x == x && target->surface_frame_y == y
            && target->surface_width == width && target->surface_height == height, name);
    GetWindowRect(target->surface, &rect);
    check(rect.right - rect.left == width && rect.bottom - rect.top == height,
            "native surface matches current frame size");
}

static DWORD pixel(struct composition_target *target, LONG x, LONG y)
{
    x -= target->surface_frame_x;
    y -= target->surface_frame_y;
    if (x < 0 || y < 0 || x >= target->surface_width || y >= target->surface_height)
        return 0xdeadbeef;
    return ((DWORD *)target->surface_bits)[(SIZE_T)y * target->surface_width + x];
}

int main(void)
{
    struct composition_target target = {0};
    DWORD red[160 * 120], blue[16], half_blue[16];
    HWND parent;
    POINT first, second;
    unsigned int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 0; i < ARRAY_SIZE(red); ++i) red[i] = 0xffff0000;
    for (i = 0; i < ARRAY_SIZE(blue); ++i) blue[i] = 0xff0000ff;
    for (i = 0; i < ARRAY_SIZE(half_blue); ++i) half_blue[i] = 0x80000080;
    parent = CreateWindowW(L"static", L"Composition frame bounds test",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 320, 240,
            NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!parent) return 2;

    first.x = first.y = 0;
    target_begin_surface_frame(&target, parent);
    check(target_composite_surface(&target, parent, &first, 160, 120, red, 160 * 4),
            "initial large visual");
    bounds(&target, parent, 0, 0, 160, 120, "initial bounds");

    first.x = -4; first.y = 8;
    second.x = 12; second.y = 8;
    target_begin_surface_frame(&target, parent);
    check(target_composite_surface(&target, parent, &first, 4, 4, red, 4 * 4), "first small visual");
    check(target_composite_surface(&target, parent, &second, 4, 4, blue, 4 * 4), "second small visual");
    bounds(&target, parent, -4, 8, 20, 4, "shrink to current union with negative origin");
    check(pixel(&target, -4, 8) == 0xffff0000 && pixel(&target, 15, 11) == 0xff0000ff,
            "both visuals survive cropping");
    check(!pixel(&target, 5, 8), "gap remains transparent");

    target_begin_surface_frame(&target, parent);
    check(target_composite_surface(&target, parent, &second, 4, 4, blue, 4 * 4), "reverse first visual");
    check(target_composite_surface(&target, parent, &first, 4, 4, red, 4 * 4), "reverse second visual");
    bounds(&target, parent, -4, 8, 20, 4, "stable bounds in reverse traversal");
    check(pixel(&target, -4, 8) == 0xffff0000 && pixel(&target, 15, 11) == 0xff0000ff,
            "reverse traversal preserves both visuals");

    first.x = 25; first.y = 16;
    second.x = 41; second.y = 16;
    target_begin_surface_frame(&target, parent);
    target_composite_surface(&target, parent, &first, 4, 4, red, 4 * 4);
    target_composite_surface(&target, parent, &second, 4, 4, blue, 4 * 4);
    bounds(&target, parent, 25, 16, 20, 4, "movement does not retain old area");

    second.x = 27;
    target_begin_surface_frame(&target, parent);
    target_composite_surface(&target, parent, &first, 4, 4, red, 4 * 4);
    target_composite_surface(&target, parent, &second, 4, 4, half_blue, 4 * 4);
    bounds(&target, parent, 25, 16, 6, 4, "overlapping current union");
    check(pixel(&target, 27, 16) == 0xff7f0080, "premultiplied alpha survives cropping");

    target_begin_surface_frame(&target, parent);
    target_composite_surface(&target, parent, &first, 4, 4, red, 4 * 4);
    bounds(&target, parent, 25, 16, 4, 4, "removed visual no longer enlarges frame");
    check(pixel(&target, 28, 19) == 0xffff0000, "remaining visual is intact");

    target_destroy_surface(&target);
    DestroyWindow(parent);
    printf("RESULT failures=%u\n", failures);
    return !!failures;
}
