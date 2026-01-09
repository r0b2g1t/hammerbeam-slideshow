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
    static uint8_t cbuf_tmp[CANVAS_SIZE * CANVAS_SIZE * sizeof(lv_color_t) + sizeof(lv_color32_t) * 2];
    
    uint32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, LV_COLOR_FORMAT_I1);
    uint32_t buf_size = stride * CANVAS_SIZE;
    
    // Prepend palette
    lv_color32_t *palette = (lv_color32_t *)cbuf_tmp;
    palette[0] = LVGL_BACKGROUND_32;
    palette[1] = LVGL_FOREGROUND_32;
    
    // Copy bitmap data after palette
    memcpy(cbuf_tmp + sizeof(lv_color32_t) * 2, cbuf, buf_size);

    lv_image_dsc_t img;
    img.data = (const void *)cbuf_tmp;
    img.header.magic = LV_IMAGE_HEADER_MAGIC;
    img.header.cf = LV_COLOR_FORMAT_I1;
    img.header.flags = 0;
    img.header.w = CANVAS_SIZE;
    img.header.h = CANVAS_SIZE;
    img.header.stride = stride;
    img.data_size = sizeof(lv_color32_t) * 2 + buf_size;

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.src = &img;
    img_dsc.rotation = 900;
    img_dsc.pivot.x = CANVAS_SIZE / 2;
    img_dsc.pivot.y = CANVAS_SIZE / 2;
    
    lv_area_t coords = {0, 0, CANVAS_SIZE, CANVAS_SIZE};
    
    lv_draw_image(&layer, &img_dsc, &coords);
}

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
