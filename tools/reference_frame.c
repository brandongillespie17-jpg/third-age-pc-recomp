/* Separate mGBA diagnostic process. Never linked into the port. */
#include <mgba/core/core.h>
#include <mgba-util/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: reference_frame ROM BIOS FRAMES OUTPUT_PREFIX\n");
        return 2;
    }
    int frames = atoi(argv[3]);
    if (frames < 1 || frames > 100000) return 2;
    struct mCore* core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) return 1;
    mCoreInitConfig(core, NULL);
    mCoreConfigSetIntValue(&core->config, "useBios", 1);
    mCoreConfigSetIntValue(&core->config, "skipBios", 0);
    mCoreConfigSetIntValue(&core->config, "logLevel", 0);
    mCoreLoadForeignConfig(core, &core->config);
    color_t* pixels = calloc(240 * 160, sizeof(color_t));
    if (!pixels || !mCoreLoadFile(core, argv[1])) return 1;
    struct VFile* bios = VFileOpen(argv[2], O_RDONLY);
    if (!bios || !core->loadBIOS(core, bios, 0)) return 1;
    core->setVideoBuffer(core, pixels, 240);
    core->reset(core);
    for (int f = 0; f < frames; ++f) core->runFrame(core);
    char filename[4096];
    if (snprintf(filename, sizeof(filename), "%s.ppm", argv[4]) >= sizeof(filename)) return 1;
    FILE* out = fopen(filename, "wb");
    if (!out) return 1;
    fprintf(out, "P6\n240 160\n255\n");
    for (int i = 0; i < 240 * 160; ++i) {
        unsigned char rgb[3] = {pixels[i] & 255, (pixels[i] >> 8) & 255, (pixels[i] >> 16) & 255};
        fwrite(rgb, 1, 3, out);
    }
    fclose(out);
    snprintf(filename, sizeof(filename), "%s.iwram", argv[4]);
    out = fopen(filename, "wb");
    if (!out) return 1;
    for (unsigned i = 0; i < 0x8000; ++i) fputc(core->rawRead8(core, 0x03000000 + i, -1), out);
    fclose(out);
    printf("reference=mGBA frames=%u output=%s\n", core->frameCounter(core), argv[4]);
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    free(pixels);
    return 0;
}
