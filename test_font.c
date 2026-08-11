#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "ext_src/stb/stb_truetype.h"

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *buf;
    long size;
    stbtt_fontinfo fi;
    const char *path;
    unsigned int cps[] = { 0x5317 /*北*/, 0x6781 /*极*/, 0x56DB /*四*/,
                           0x5929 /*天*/, 0x5E73 /*平*/, 0x5BBF /*宿*/,
                           0x0041 /*A*/, 0x30C6 /*テ*/ };
    int i;

    path = argc > 1 ? argv[1] : "apps/web-frontend/public/fonts/NotoSansSC.ttf";
    f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(size);
    fread(buf, 1, size, f);
    fclose(f);

    if (!stbtt_InitFont(&fi, buf, 0)) {
        printf("stbtt_InitFont FAILED for %s (%ld bytes)\n", path, size);
        free(buf);
        return 1;
    }
    printf("stbtt_InitFont OK for %s (%ld bytes)\n", path, size);
    for (i = 0; i < (int)(sizeof(cps)/sizeof(cps[0])); i++) {
        int g = stbtt_FindGlyphIndex(&fi, cps[i]);
        int x0, y0, x1, y1;
        float scale = stbtt_ScaleForPixelHeight(&fi, 20.0f);
        stbtt_GetCodepointBitmapBox(&fi, cps[i], scale, scale, &x0, &y0, &x1, &y1);
        printf("U+%04X: glyph=%d bbox=(%d,%d)-(%d,%d) scale=%.3f\n",
               cps[i], g, x0, y0, x1, y1, scale);
        if (g != 0) {
            // Actually rasterize the glyph and count non-zero pixels.
            int w = x1 - x0, h = y1 - y0;
            if (w > 0 && h > 0 && w < 512 && h < 512) {
                unsigned char *bm = calloc(w * h, 1);
                stbtt_MakeCodepointBitmap(&fi, bm, w, h, w, scale, scale, cps[i]);
                int nz = 0, p;
                for (p = 0; p < w * h; p++) if (bm[p]) nz++;
                printf("  rasterize: %dx%d, non-zero px=%d (%s)\n",
                       w, h, nz, nz > 0 ? "OK" : "EMPTY!");
                free(bm);
            } else {
                printf("  bbox too large/small: w=%d h=%d\n", w, h);
            }
        }
    }
    free(buf);
    return 0;
}
