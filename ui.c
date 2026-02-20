/**
 * @file ui.c
 * @brief Implementation of Medical Mission ID Scanner UI (LVGL 8.x)
 * 
 * INTEGRATION NOTES for Raspberry Pi:
 * 1. Initialize LVGL (lv_init())
 * 2. Configure display and input drivers (e.g., DRM/KMS or Framebuffer for Pi)
 * 3. Call ui_init(lv_scr_act()) to build the UI on the active screen.
 * 4. Run LVGL timer handler (lv_timer_handler()) in a loop.
 */

#include "ui.h"
#include <stdio.h>

/* --- STATICS / GLOBALS --- */
static lv_style_t style_app_bg;
static lv_style_t style_card;
static lv_style_t style_btn_primary;
static lv_style_t style_btn_secondary;
static lv_style_t style_chip;
static lv_style_t style_title;
static lv_style_t style_label_small;

static lv_obj_t * ui_root;
static lv_obj_t * scr_scan;
static lv_obj_t * scr_fields;
static lv_obj_t * scr_settings;
static lv_obj_t * current_screen;

static lv_obj_t * chip_net;
static lv_obj_t * label_time;
static lv_obj_t * chip_cam;
static lv_obj_t * chip_ocr;

/* OCR Simulation Widgets */
static lv_obj_t * btn_capture;
static lv_obj_t * sw_cam_ready;
static lv_obj_t * spinner_ocr;
static lv_obj_t * box_overlay;
static lv_obj_t * boxes[3];

/* Extracted Data Labels */
static lv_obj_t * lbl_name;
static lv_obj_t * lbl_phid;
static lv_obj_t * lbl_dob;
static lv_obj_t * lbl_sex;
static lv_obj_t * bar_conf;
static lv_obj_t * lbl_conf_pct;

/* Form Fields */
static lv_obj_t * kb;
static lv_obj_t * ta_last_name;
static lv_obj_t * ta_first_name;

typedef enum {
    IDLE,
    CAMERA_READY,
    CAPTURING,
    OCR_RUNNING,
    OCR_DONE,
    VAL_ERROR
} ui_state_t;

static ui_state_t app_state = IDLE;

/* --- HELPERS --- */

static void ui_show_toast(const char * msg) {
    lv_obj_t * toast = lv_label_create(lv_layer_top());
    lv_label_set_text(toast, msg);
    lv_obj_set_style_bg_color(toast, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(toast, 200, 0);
    lv_obj_set_style_text_color(toast, lv_color_white(), 0);
    lv_obj_set_style_pad_all(toast, 12, 0);
    lv_obj_set_style_radius(toast, 8, 0);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -100);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_cb_t)lv_obj_set_style_opa);
    lv_anim_start(&a);

    /* Auto-hide after 2s */
    lv_obj_del_delayed(toast, 2000);
}

static void update_state_ui() {
    switch(app_state) {
        case IDLE:
            lv_obj_add_state(btn_capture, LV_STATE_DISABLED);
            lv_label_set_text(chip_cam, "CAM: IDLE");
            lv_obj_set_style_bg_color(chip_cam, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_add_flag(spinner_ocr, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(box_overlay, LV_OBJ_FLAG_HIDDEN);
            break;
        case CAMERA_READY:
            lv_obj_clear_state(btn_capture, LV_STATE_DISABLED);
            lv_label_set_text(chip_cam, "CAM: READY");
            lv_obj_set_style_bg_color(chip_cam, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_add_flag(spinner_ocr, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(box_overlay, LV_OBJ_FLAG_HIDDEN);
            break;
        case OCR_RUNNING:
            lv_label_set_text(chip_ocr, "OCR: RUNNING");
            lv_obj_set_style_bg_color(chip_ocr, lv_palette_main(LV_PALETTE_ORANGE), 0);
            lv_obj_clear_flag(spinner_ocr, LV_OBJ_FLAG_HIDDEN);
            break;
        case OCR_DONE:
            lv_label_set_text(chip_ocr, "OCR: DONE");
            lv_obj_set_style_bg_color(chip_ocr, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_add_flag(spinner_ocr, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(box_overlay, LV_OBJ_FLAG_HIDDEN);
            
            /* Fill dummy data */
            lv_label_set_text(lbl_name, "DELA CRUZ, JUAN P.");
            lv_label_set_text(lbl_phid, "12-345678901-2");
            lv_label_set_text(lbl_dob, "1985-05-20");
            lv_label_set_text(lbl_sex, "MALE");
            lv_bar_set_value(bar_conf, 92, LV_ANIM_ON);
            lv_label_set_text(lbl_conf_pct, "92%");
            break;
        default: break;
    }
}

/* --- EVENT HANDLERS --- */

static void ocr_timer_cb(lv_timer_t * timer) {
    app_state = OCR_DONE;
    update_state_ui();
    lv_timer_del(timer);
}

static void btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * user_data = lv_event_get_user_data(e);

    if(btn == btn_capture) {
        app_state = OCR_RUNNING;
        update_state_ui();
        lv_timer_create(ocr_timer_cb, 1500, NULL);
    } else if (lv_obj_has_flag(btn, LV_OBJ_FLAG_USER_1)) { /* Retake */
        app_state = CAMERA_READY;
        update_state_ui();
        lv_label_set_text(lbl_name, "-");
        lv_label_set_text(lbl_phid, "-");
        lv_bar_set_value(bar_conf, 0, LV_ANIM_OFF);
        lv_label_set_text(lbl_conf_pct, "0%");
    } else if (lv_obj_has_flag(btn, LV_OBJ_FLAG_USER_2)) { /* Clear */
        app_state = IDLE;
        lv_btn_set_state(sw_cam_ready, LV_STATE_DEFAULT); /* Wait, it's a switch */
        lv_obj_clear_state(sw_cam_ready, LV_STATE_CHECKED);
        update_state_ui();
    }
}

static void nav_event_cb(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_user_data(e);
    if(current_screen) lv_obj_add_flag(current_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
    current_screen = target;
}

static void sw_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    if(lv_obj_has_state(sw, LV_STATE_CHECKED)) app_state = CAMERA_READY;
    else app_state = IDLE;
    update_state_ui();
}

/* --- UI BUILDERS --- */

static void ui_style_init() {
    lv_style_init(&style_app_bg);
    lv_style_set_bg_color(&style_app_bg, lv_color_hex(0xF4F7FA));

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_white());
    lv_style_set_radius(&style_card, 12);
    lv_style_set_shadow_width(&style_card, 20);
    lv_style_set_shadow_color(&style_card, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_opa(&style_card, 40);
    lv_style_set_pad_all(&style_card, 16);

    lv_style_init(&style_btn_primary);
    lv_style_set_bg_color(&style_btn_primary, lv_color_hex(0x2196F3));
    lv_style_set_radius(&style_btn_primary, 8);
    lv_style_set_height(&style_btn_primary, 72);
    lv_style_set_text_font(&style_btn_primary, &lv_font_montserrat_18);

    lv_style_init(&style_chip);
    lv_style_set_radius(&style_chip, 20);
    lv_style_set_bg_color(&style_chip, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_text_color(&style_chip, lv_color_white());
    lv_style_set_pad_hor(&style_chip, 12);
    lv_style_set_pad_ver(&style_chip, 4);
}

void ui_init(lv_obj_t * root) {
    ui_root = root;
    ui_style_init();
    lv_obj_add_style(ui_root, &style_app_bg, 0);

    /* TOP BAR */
    lv_obj_t * topbar = lv_obj_create(ui_root);
    lv_obj_set_size(topbar, 1024, 64);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(0x1A237E), 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    
    lv_obj_t * lbl_mission = lv_label_create(topbar);
    lv_label_set_text(lbl_mission, "MEDICAL MISSION");
    lv_obj_set_style_text_color(lbl_mission, lv_color_white(), 0);
    lv_obj_align(lbl_mission, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t * lbl_station = lv_label_create(topbar);
    lv_label_set_text(lbl_station, "ID SCAN STATION");
    lv_obj_set_style_text_color(lbl_station, lv_color_white(), 0);
    lv_obj_align(lbl_station, LV_ALIGN_CENTER, 0, 0);

    chip_cam = lv_label_create(topbar);
    lv_obj_add_style(chip_cam, &style_chip, 0);
    lv_label_set_text(chip_cam, "CAM: IDLE");
    lv_obj_align(chip_cam, LV_ALIGN_RIGHT_MID, -280, 0);

    chip_ocr = lv_label_create(topbar);
    lv_obj_add_style(chip_ocr, &style_chip, 0);
    lv_label_set_text(chip_ocr, "OCR: IDLE");
    lv_obj_align(chip_ocr, LV_ALIGN_RIGHT_MID, -170, 0);

    chip_net = lv_label_create(topbar);
    lv_obj_add_style(chip_net, &style_chip, 0);
    lv_label_set_text(chip_net, "NET: LAN");
    lv_obj_align(chip_net, LV_ALIGN_RIGHT_MID, -80, 0);

    label_time = lv_label_create(topbar);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_label_set_text(label_time, "12:00");
    lv_obj_align(label_time, LV_ALIGN_RIGHT_MID, -20, 0);

    /* NAVIGATION BAR (BOTTOM) */
    lv_obj_t * nav = lv_obj_create(ui_root);
    lv_obj_set_size(nav, 1024, 80);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(nav, 40, 0);

    /* SCREENS SETUP */
    scr_scan = lv_obj_create(ui_root);
    lv_obj_set_size(scr_scan, 1024, 456); /* 600 - 64 - 80 */
    lv_obj_align(scr_scan, LV_ALIGN_TOP_MID, 0, 64);
    
    scr_fields = lv_obj_create(ui_root);
    lv_obj_set_size(scr_fields, 1024, 456);
    lv_obj_align(scr_fields, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_add_flag(scr_fields, LV_OBJ_FLAG_HIDDEN);

    scr_settings = lv_obj_create(ui_root);
    lv_obj_set_size(scr_settings, 1024, 456);
    lv_obj_align(scr_settings, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_add_flag(scr_settings, LV_OBJ_FLAG_HIDDEN);

    /* NAV BUTTONS */
    const char * nav_labels[] = {"SCAN", "FIELDS", "SETTINGS"};
    lv_obj_t * nav_scrs[] = {scr_scan, scr_fields, scr_settings};
    for(int i=0; i<3; i++) {
        lv_obj_t * b = lv_btn_create(nav);
        lv_obj_set_size(b, 200, 60);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_text(l, nav_labels[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, nav_event_cb, LV_EVENT_CLICKED, nav_scrs[i]);
    }

    /* SCAN SCREEN CONTENT */
    lv_obj_set_flex_flow(scr_scan, LV_FLEX_FLOW_ROW);
    
    /* LEFT: Camera Preview */
    lv_obj_t * cam_card = lv_obj_create(scr_scan);
    lv_obj_set_size(cam_card, 580, 420);
    lv_obj_add_style(cam_card, &style_card, 0);
    
    lv_obj_t * cam_placeholder = lv_obj_create(cam_card);
    lv_obj_set_size(cam_placeholder, 540, 320);
    lv_obj_set_style_bg_color(cam_placeholder, lv_color_black(), 0);
    lv_obj_center(cam_placeholder);
    
    lv_obj_t * lbl_preview = lv_label_create(cam_placeholder);
    lv_label_set_text(lbl_preview, "CAMERA PREVIEW");
    lv_obj_set_style_text_color(lbl_preview, lv_color_white(), 0);
    lv_obj_center(lbl_preview);

    box_overlay = lv_obj_create(cam_placeholder);
    lv_obj_set_size(box_overlay, 540, 320);
    lv_obj_set_style_bg_opa(box_overlay, 0, 0);
    lv_obj_set_style_border_width(box_overlay, 0, 0);
    lv_obj_add_flag(box_overlay, LV_OBJ_FLAG_HIDDEN);

    /* Sample boxes */
    for(int i=0; i<3; i++) {
        boxes[i] = lv_obj_create(box_overlay);
        lv_obj_set_size(boxes[i], 100, 30);
        lv_obj_set_style_border_color(boxes[i], lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
        lv_obj_set_style_border_width(boxes[i], 2, 0);
        lv_obj_set_style_bg_opa(boxes[i], 0, 0);
        lv_obj_set_pos(boxes[i], 50 + (i*120), 100);
    }

    spinner_ocr = lv_spinner_create(cam_placeholder, 1000, 60);
    lv_obj_set_size(spinner_ocr, 80, 80);
    lv_obj_center(spinner_ocr);
    lv_obj_add_flag(spinner_ocr, LV_OBJ_FLAG_HIDDEN);

    /* RIGHT: Controls */
    lv_obj_t * right_col = lv_obj_create(scr_scan);
    lv_obj_set_size(right_col, 380, 420);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(right_col, 16, 0);
    lv_obj_set_style_bg_opa(right_col, 0, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);

    lv_obj_t * ctrl_card = lv_obj_create(right_col);
    lv_obj_set_size(ctrl_card, 360, 200);
    lv_obj_add_style(ctrl_card, &style_card, 0);
    lv_obj_set_flex_flow(ctrl_card, LV_FLEX_FLOW_COLUMN);

    sw_cam_ready = lv_switch_create(ctrl_card);
    lv_obj_add_event_cb(sw_cam_ready, sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t * lbl_sw = lv_label_create(ctrl_card);
    lv_label_set_text(lbl_sw, "Ready Camera");

    btn_capture = lv_btn_create(ctrl_card);
    lv_obj_add_style(btn_capture, &style_btn_primary, 0);
    lv_obj_set_width(btn_capture, lv_pct(100));
    lv_obj_t * lbl_cap = lv_label_create(btn_capture);
    lv_label_set_text(lbl_cap, "Capture & OCR");
    lv_obj_center(lbl_cap);
    lv_obj_add_event_cb(btn_capture, btn_event_cb, LV_EVENT_CLICKED, NULL);

    /* OCR Results Card */
    lv_obj_t * res_card = lv_obj_create(right_col);
    lv_obj_set_size(res_card, 360, 180);
    lv_obj_add_style(res_card, &style_card, 0);
    
    lbl_name = lv_label_create(res_card);
    lv_label_set_text(lbl_name, "-");
    lv_obj_set_pos(lbl_name, 0, 0);

    bar_conf = lv_bar_create(res_card);
    lv_obj_set_size(bar_conf, 200, 15);
    lv_obj_align(bar_conf, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    lbl_conf_pct = lv_label_create(res_card);
    lv_label_set_text(lbl_conf_pct, "0%");
    lv_obj_align(lbl_conf_pct, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* FIELDS SCREEN CONTENT */
    lv_obj_set_flex_flow(scr_fields, LV_FLEX_FLOW_ROW);
    
    /* Left: Form */
    lv_obj_t * form_card = lv_obj_create(scr_fields);
    lv_obj_set_size(form_card, 600, 420);
    lv_obj_add_style(form_card, &style_card, 0);
    lv_obj_set_flex_flow(form_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(form_card, 12, 0);

    const char * field_names[] = {"LAST NAME", "FIRST NAME", "PHILHEALTH NO", "BIRTHDATE"};
    for(int i=0; i<4; i++) {
        lv_obj_t * l = lv_label_create(form_card);
        lv_label_set_text(l, field_names[i]);
        lv_obj_t * ta = lv_textarea_create(form_card);
        lv_obj_set_width(ta, lv_pct(100));
        lv_textarea_set_one_line(ta, true);
        lv_obj_add_event_cb(ta, lv_keyboard_set_textarea, LV_EVENT_FOCUSED, kb);
    }

    /* Right: Preview */
    lv_obj_t * field_preview = lv_obj_create(scr_fields);
    lv_obj_set_size(field_preview, 360, 420);
    lv_obj_add_style(field_preview, &style_card, 0);
    
    lv_obj_t * img_placeholder = lv_obj_create(field_preview);
    lv_obj_set_size(img_placeholder, 320, 200);
    lv_obj_set_style_bg_color(img_placeholder, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t * l_zoom = lv_label_create(img_placeholder);
    lv_label_set_text(l_zoom, "Tap to zoom");
    lv_obj_center(l_zoom);

    /* SETTINGS SCREEN CONTENT */
    lv_obj_set_flex_flow(scr_settings, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scr_settings, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(scr_settings, 20, 0);

    /* Device Card */
    lv_obj_t * dev_card = lv_obj_create(scr_settings);
    lv_obj_set_size(dev_card, 300, 380);
    lv_obj_add_style(dev_card, &style_card, 0);
    lv_obj_t * l_dev = lv_label_create(dev_card);
    lv_label_set_text(l_dev, "DEVICE");
    
    lv_obj_t * slider = lv_slider_create(dev_card);
    lv_obj_set_width(slider, 240);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t * btn_reboot = lv_btn_create(dev_card);
    lv_obj_align(btn_reboot, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_label_set_text(lv_label_create(btn_reboot), "RESTART");
    lv_obj_add_event_cb(btn_reboot, (lv_event_cb_t)ui_show_toast, LV_EVENT_CLICKED, "Restarting...");

    /* KEYBOARD (global on layer_top) */
    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    current_screen = scr_scan;
    update_state_ui();
}

void ui_set_net_status(bool lan) {
    lv_label_set_text(chip_net, lan ? "NET: LAN" : "NET: OFFLINE");
    lv_obj_set_style_bg_color(chip_net, lan ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_RED), 0);
}

void ui_set_time(const char * hhmm) {
    lv_label_set_text(label_time, hhmm);
}
