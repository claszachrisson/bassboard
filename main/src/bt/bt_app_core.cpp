//
// Created by Clas Zachrisson on 2025-04-19.
//

#include <string.h>
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "a2dp_source.h"

constexpr char BT_APP_CORE_TAG[] = "bt_app_core";

QueueHandle_t A2DPSource::s_bt_app_task_queue = nullptr;
TaskHandle_t A2DPSource::s_bt_app_task_handle = nullptr;

bool A2DPSource::bt_app_send_msg(bt_app_msg_t *msg)
{
    if (msg == nullptr) {
        return false;
    }

    if (pdTRUE != xQueueSend(s_bt_app_task_queue, msg, 10 / portTICK_PERIOD_MS)) {
        ESP_LOGE(BT_APP_CORE_TAG, "%s xQueue send failed", __func__);
        return false;
    }

    return true;
}

void A2DPSource::bt_app_task_handler(void *arg)
{
    bt_app_msg_t msg;

    while (true) {
        /* receive message from work queue and handle it */
        if (pdTRUE == xQueueReceive(s_bt_app_task_queue, &msg, portMAX_DELAY)) {
            //ESP_LOGD(BT_APP_CORE_TAG, "%s, signal: 0x%x, event: 0x%x", __func__, msg.sig, msg.event);

            switch (msg.sig) {
            case BT_APP_SIG_WORK_DISPATCH:
                if (msg.cb) {
                    msg.cb(msg.event, msg.param);
                }
                break;
            default:
                ESP_LOGW(BT_APP_CORE_TAG, "%s, unhandled signal: %d", __func__, msg.sig);
                break;
            }

            if (msg.param) {
                free(msg.param);
            }
        }
    }
}

/*********************************
 * EXTERN FUNCTION DEFINITIONS
 ********************************/

bool A2DPSource::bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len)
{
    ESP_LOGD(BT_APP_CORE_TAG, "%s event: 0x%x, param len: %d", __func__, event, param_len);

    bt_app_msg_t msg = {};

    msg.sig = BT_APP_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = p_cback;

    if (param_len == 0) {
        return bt_app_send_msg(&msg);
    } else if (p_params && param_len > 0) {
        if ((msg.param = malloc(param_len)) != nullptr) {
            memcpy(msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy */
            return bt_app_send_msg(&msg);
        }
    }

    return false;
}

void A2DPSource::bt_app_task_start_up(void)
{
    s_bt_app_task_queue = xQueueCreate(10, sizeof(bt_app_msg_t));
    xTaskCreate(bt_app_task_handler, "BtAppTask", 3072, nullptr, 10, &s_bt_app_task_handle);
}

void A2DPSource::bt_app_task_shut_down(void)
{
    if (s_bt_app_task_handle) {
        vTaskDelete(s_bt_app_task_handle);
        s_bt_app_task_handle = nullptr;
    }
    if (s_bt_app_task_queue) {
        vQueueDelete(s_bt_app_task_queue);
        s_bt_app_task_queue = nullptr;
    }
}