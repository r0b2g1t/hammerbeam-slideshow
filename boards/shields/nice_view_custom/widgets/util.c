/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>
#include "util.h"

LV_IMAGE_DECLARE(bolt);

    // For LVGL 9 with 1-bit displays, direct rotation of indexed images might be tricky.
    // Instead of rotating the image, let's rotate the layer/draw task if possible, or just copy 1:1 for now to verify content.
    // However, the nice!view is mounted 90 degrees, so we DO need rotation.
    
    // Let's try using the simple canvas transform API which might handle this better if supported
    // But since we are drawing into the layer, we use lv_draw_image.
    
    // If we are strictly 1-bit, LVGL's software rotator might not support I1 rotation well.
    // Let's try to do it manually or verify if we can just draw it unrotated first to prove pixels exist.
    // But user says "screen is blank".
    
    // Let's verify the buffer size calc.
    // Stride for 68px width is likely 9 bytes (ceil(68/8)) or 4-byte aligned?
    // lv_draw_buf_width_to_stride(68, LV_COLOR_FORMAT_I1) -> likely 12 bytes (4-byte aligned) or 9.
    
    // Let's try to force the image to be treated as a simple bitmap if possible, or use the dedicated
    // 1-bit rotation if available.
    
    // Actually, simply using the canvas's own buffer might be issue if it's being read while written.
    // We already use a tmp buffer.
    
    // Let's try ensuring the image is drawn without rotation first to see if ANYTHING appears.
    // If that works, then rotation is the issue.
    // But I can't ask the user to check repeatedly.
    
    // Let's try a different approach: Rotate the drawing itself? No, we are drawing onto a canvas.
    // The widget logic draws UPRIGHT into cbuf, then `rotate_canvas` rotates it 90 deg onto the display canvas.
    
    // Wait, the `rotate_canvas` function takes `cbuf` (source, upright) and draws it onto `canvas` (destination).
    // The destination `canvas` is associated with the display buffer?
    // In `zmk_widget_status_init`:
    // lv_canvas_set_buffer(top, widget->cbuf, ...);
    
    // So `widget->cbuf` is the buffer of the `top` canvas.
    // But `rotate_canvas` receives `widget->cbuf` as the `cbuf` argument?
    // No, `zmk_widget_status_init` sets buffer to `widget->cbuf`.
    // `draw_top` calls `rotate_canvas(canvas, cbuf)`.
    // `cbuf` in `draw_top` comes from `widget->cbuf`.
    // So we are reading from `cbuf` (which is the canvas's own buffer!) and writing back to the SAME canvas?
    // That's definitely problematic if we are overwriting it.
    
    // In `rotate_canvas`:
    // memcpy(cbuf_tmp, cbuf, ...); // Copy CURRENT canvas content to tmp
    // lv_canvas_fill_bg(canvas, ...); // Clear canvas
    // lv_draw_image(... src=&img ...); // Draw tmp (which maps to old cbuf) onto canvas
    
    // The issue: The `draw_top` functions draw UPRIGHT text/rects onto `cbuf` (via the canvas API).
    // THEN `rotate_canvas` takes that content, copies it to tmp, clears `cbuf`, and draws the rotated version back.
    // This seems convoluted.
    
    // CRITICAL: LVGL 9 `lv_canvas_set_buffer` expects a buffer that is aligned and sized correctly.
    // `lv_color_t cbuf[CANVAS_SIZE * CANVAS_SIZE]` is just an array of `lv_color_t` (which is likely uint8_t or uint32_t depending on config).
    // But for `LV_COLOR_FORMAT_NATIVE` (which is 1-bit `I1` effectively on nice!view), it needs to be packed bitmap.
    // If `lv_color_t` is 16-bit or 32-bit, but we treat it as I1, the size calc is wrong.
    
    // In `Kconfig.defconfig`:
    // config LV_Z_BITS_PER_PIXEL default 1
    // choice LV_COLOR_DEPTH default LV_COLOR_DEPTH_1
    
    // So `lv_color_t` is likely `uint8_t` (mapped to 1-bit?).
    // No, `lv_color_t` structure depends on `LV_COLOR_DEPTH`.
    // If depth is 1, `lv_color_t` is likely a union with `full` being 1 bit? No, usually 8-bit min.
    
    // The issue might be that we are treating `cbuf` as a pixel array `CANVAS_SIZE * CANVAS_SIZE`,
    // but initializing the canvas as `LV_COLOR_FORMAT_NATIVE`.
    // If NATIVE is 1-bit (I1), then the buffer should be `stride * height` bytes, which is much smaller than `width * height`.
    // But we allocated `CANVAS_SIZE * CANVAS_SIZE` lv_color_t's. That's plenty of space, so safety is fine.
    
    // BUT `rotate_canvas` copies `buf_size` which is `stride * height`.
    // If the drawing functions (draw_rect, draw_label) wrote to `cbuf` assuming it's a bitmap, then fine.
    
    // IMPORTANT: The `rotate_canvas` function clears the background then draws the image.
    // If the image `img.header.cf` is `LV_COLOR_FORMAT_I1`, it expects the data to be packed bits.
    // The `cbuf` (source) was drawn into using `lv_canvas` functions.
    // If the canvas was initialized with `LV_COLOR_FORMAT_NATIVE` (and native is I1), then `cbuf` contains packed bits.
    // So copying `buf_size` bytes is correct.
    
    // HOWEVER, the rotation might be failing or drawing black-on-black if palette isn't 100% right.
    
    // Let's try to SIMPLIFY `rotate_canvas` by just doing a pixel-by-pixel rotation manually.
    // This avoids LVGL's complex image rotation pipeline for 1-bit images which might be buggy or unsupported.
    // Since it's just 68x68, it's fast enough.
    
    // We need to read pixels from the unrotated buffer and write them rotated to the canvas.
    // But we can't easily write pixels to canvas in LVGL 9 without `lv_canvas_set_px` which might be slow or complex with buffers.
    // Actually `lv_canvas_set_px` exists.
    
    // Let's try this:
    // 1. Copy current canvas buffer to tmp.
    // 2. Clear canvas.
    // 3. Loop x,y and set pixels on canvas based on tmp.
    
    // To read pixel from packed buffer:
    // val = (buf[y * stride + x / 8] >> (7 - (x % 8))) & 0x1;
    
    // But wait, `cbuf` is `lv_color_t` array in the struct?
    // struct zmk_widget_status { lv_color_t cbuf[...]; }
    // If `LV_COLOR_DEPTH` is 1, `lv_color_t` is likely 8 bits.
    // But the canvas treats it as a buffer.
    
    // Let's just reimplement `rotate_canvas` to do manual pixel rotation. It's robust.
    
    uint8_t *src_buf = (uint8_t *)cbuf; // Source is the current canvas content
    static uint8_t dst_buf[CANVAS_SIZE * CANVAS_SIZE / 8 + 100]; // Destination buffer (packed)
    memset(dst_buf, 0, sizeof(dst_buf));
    
    uint32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, LV_COLOR_FORMAT_I1);
    
    // Copy source to temporary so we can overwrite source (which is `cbuf`)
    // Actually `cbuf` is the buffer we want to FINAL update.
    // The drawing functions drew UPRIGHT into `cbuf`.
    // We need to rotate `cbuf` in place effectively.
    // So:
    // 1. Copy `cbuf` (upright) to `src_tmp`.
    // 2. Rotate from `src_tmp` back into `cbuf`.
    
    static uint8_t src_tmp[CANVAS_SIZE * CANVAS_SIZE]; // Max size just to be safe
    memcpy(src_tmp, cbuf, stride * CANVAS_SIZE);
    
    // Clear destination (cbuf)
    memset(cbuf, IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_INVERTED) ? 0x00 : 0xFF, stride * CANVAS_SIZE); 
    // Wait, 0 is index 0 (Background). 1 is index 1 (Foreground).
    // We should clear to Background (Index 0). So memset 0.
    memset(cbuf, 0, stride * CANVAS_SIZE);
    
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            // Get pixel (x,y) from src_tmp
            // Assuming I1 format: MSB first? LVGL usually MSB first for I1.
            int src_byte = y * stride + (x / 8);
            int src_bit = 7 - (x % 8);
            uint8_t val = (src_tmp[src_byte] >> src_bit) & 0x1;
            
            if (val) {
                // Pixel is set (Foreground)
                // We want to rotate 90 degrees clockwise?
                // old (0,0) -> new (width-1, 0) ? 
                // Or (0,0) -> (0, height-1)?
                // `img_dsc.rotation = 900` (90 deg).
                // Standard 90 deg CW: (x, y) -> (height - 1 - y, x)
                // Let's try that.
                
                int new_x = CANVAS_SIZE - 1 - y;
                int new_y = x;
                
                // Set pixel (new_x, new_y) in cbuf
                int dst_byte = new_y * stride + (new_x / 8);
                int dst_bit = 7 - (new_x % 8);
                
                ((uint8_t *)cbuf)[dst_byte] |= (1 << dst_bit);
            }
        }
    }
    
    // No need to call lv_draw_image or flush, as the canvas buffer is directly modified.
    // But we might need to invalidate the object to trigger redraw.
    lv_obj_invalidate(canvas);

void draw_battery(lv_obj_t *canvas, const struct status_state *state) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    lv_area_t rect;

    rect = (lv_area_t){0, 2, 29, 13};
    lv_draw_rect(&layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){1, 3, 27, 12};
    lv_draw_rect(&layer, &rect_black_dsc, &rect);

    rect = (lv_area_t){2, 4, 2 + (state->battery + 2) / 4, 11};
    lv_draw_rect(&layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){30, 5, 32, 10};
    lv_draw_rect(&layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){31, 6, 31, 9};
    lv_draw_rect(&layer, &rect_black_dsc, &rect);

    if (state->charging) {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        img_dsc.src = &bolt;
        
        lv_area_t img_area = {9, -1, 9 + bolt.header.w - 1, -1 + bolt.header.h - 1};
        lv_draw_image(&layer, &img_dsc, &img_area);
    }
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align) {
    lv_draw_label_dsc_init(label_dsc);
    label_dsc->color = color;
    label_dsc->font = font;
    label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
    lv_draw_rect_dsc_init(rect_dsc);
    rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width) {
    lv_draw_line_dsc_init(line_dsc);
    line_dsc->color = color;
    line_dsc->width = width;
}

void init_arc_dsc(lv_draw_arc_dsc_t *arc_dsc, lv_color_t color, uint8_t width) {
    lv_draw_arc_dsc_init(arc_dsc);
    arc_dsc->color = color;
    arc_dsc->width = width;
}
