/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>
#include "util.h"

LV_IMAGE_DECLARE(bolt);

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
    uint32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, LV_COLOR_FORMAT_I1);
    
    // Copy source to temporary so we can overwrite source (which is `cbuf`)
    static uint8_t src_tmp[CANVAS_SIZE * CANVAS_SIZE]; // Max size just to be safe
    memcpy(src_tmp, cbuf, stride * CANVAS_SIZE);
    
    // Clear destination (cbuf)
    memset(cbuf, 0, stride * CANVAS_SIZE);
    
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            // Get pixel (x,y) from src_tmp
            int src_byte = y * stride + (x / 8);
            int src_bit = 7 - (x % 8);
            uint8_t val = (src_tmp[src_byte] >> src_bit) & 0x1;
            
            if (val) {
                // Rotate 90 degrees clockwise: (x, y) -> (height - 1 - y, x)
                int new_x = CANVAS_SIZE - 1 - y;
                int new_y = x;
                
                // Set pixel (new_x, new_y) in cbuf
                int dst_byte = new_y * stride + (new_x / 8);
                int dst_bit = 7 - (new_x % 8);
                
                ((uint8_t *)cbuf)[dst_byte] |= (1 << dst_bit);
            }
        }
    }
    
    lv_obj_invalidate(canvas);
}

void draw_battery(lv_layer_t *layer, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    lv_area_t rect;

    rect = (lv_area_t){0, 2, 29, 13};
    lv_draw_rect(layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){1, 3, 27, 12};
    lv_draw_rect(layer, &rect_black_dsc, &rect);

    rect = (lv_area_t){2, 4, 2 + (state->battery + 2) / 4, 11};
    lv_draw_rect(layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){30, 5, 32, 10};
    lv_draw_rect(layer, &rect_white_dsc, &rect);

    rect = (lv_area_t){31, 6, 31, 9};
    lv_draw_rect(layer, &rect_black_dsc, &rect);

    if (state->charging) {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        img_dsc.src = &bolt;
        
        lv_area_t img_area = {9, -1, 9 + bolt.header.w - 1, -1 + bolt.header.h - 1};
        lv_draw_image(layer, &img_dsc, &img_area);
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
