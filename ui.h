/**
 * @file ui.h
 * @brief Header for Medical Mission ID Scanner UI (LVGL 8.x)
 */

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* UI Initialization */
void ui_init(lv_obj_t * root);

/* Status Updates */
void ui_set_net_status(bool lan);
void ui_set_time(const char * hhmm);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_H*/
