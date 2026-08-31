#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <linux/fb.h>

#include "fbink.h"

int main(void) {
    FBInkConfig config;
    memset(&config, 0, sizeof(config));
    config.is_quiet = true;
    config.no_viewport = true;

    const int fbfd = fbink_open();
    if (fbfd < 0) {
        fprintf(stderr, "fbink_open=%d\n", fbfd);
        return 1;
    }

    const int init_rc = fbink_init(fbfd, &config);
    if (init_rc < 0) {
        fprintf(stderr, "fbink_init=%d\n", init_rc);
        (void)fbink_close(fbfd);
        return 1;
    }

    FBInkState state;
    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;
    memset(&state, 0, sizeof(state));
    memset(&var_info, 0, sizeof(var_info));
    memset(&fix_info, 0, sizeof(fix_info));

    fbink_get_state(&config, &state);
    fbink_get_fb_info(&var_info, &fix_info);

    size_t buffer_size = 0;
    const unsigned char *const buffer = fbink_get_fb_pointer(fbfd, &buffer_size);

    printf("device=%s codename=%s platform=%s id=%hu\n",
           state.device_name,
           state.device_codename,
           state.device_platform,
           state.device_id);
    printf("state view=%ux%u screen=%ux%u origin=%u,%u offset=%u\n",
           state.view_width,
           state.view_height,
           state.screen_width,
           state.screen_height,
           state.view_hori_origin,
           state.view_vert_origin,
           state.view_vert_offset);
    printf("state stride=%u bpp=%u pixel_format=%d rotation=%u mtk=%d inverted=%d\n",
           state.scanline_stride,
           state.bpp,
           (int)state.pixel_format,
           state.current_rota,
           state.is_mtk,
           state.inverted_grayscale);
    printf("linux id=%.16s visible=%ux%u virtual=%ux%u offset=%u,%u\n",
           fix_info.id,
           var_info.xres,
           var_info.yres,
           var_info.xres_virtual,
           var_info.yres_virtual,
           var_info.xoffset,
           var_info.yoffset);
    printf("linux stride=%u smem_len=%u bpp=%u grayscale=%u rotation=%u type=%u visual=%u\n",
           fix_info.line_length,
           fix_info.smem_len,
           var_info.bits_per_pixel,
           var_info.grayscale,
           var_info.rotate,
           fix_info.type,
           fix_info.visual);
    printf("mapping available=%d size=%zu\n", buffer != NULL, buffer_size);

    return fbink_close(fbfd) < 0 ? 1 : 0;
}
