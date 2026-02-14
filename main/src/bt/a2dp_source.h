//
// Created by Clas Zachrisson on 2025-04-18.
//

#ifndef A2DP_SOURCE_H
#define A2DP_SOURCE_H

#include <stdint.h>
#include "esp_a2dp_api.h"
#include "esp_gap_bt_api.h"
#include "esp_bt.h"

/* signal for dispatcher */
static constexpr int BT_APP_SIG_WORK_DISPATCH = 0x01;

/**
 * @brief    handler for the dispatched work
 *
 * @param [in] event  message event id
 * @param [in] param  pointer to the parameter
 */
typedef void (* bt_app_cb_t) (uint16_t event, void *param);

/* message to be sent */
typedef struct {
    uint16_t             sig;      /*!< signal to bt_app_task */
    uint16_t             event;    /*!< message event id */
    bt_app_cb_t          cb;       /*!< context switch callback */
    void                 *param;   /*!< parameter area needs to be last */
} bt_app_msg_t;

class A2DPSource {
public:
    A2DPSource();
    static void register_a2d_source_data_cb(esp_a2d_source_data_cb_t a2d_source_data_cb);
    void init();
    void start();
    int get_state();
private:
    static QueueHandle_t s_bt_app_task_queue;
    static TaskHandle_t s_bt_app_task_handle;
    static esp_a2d_source_data_cb_t bt_app_a2d_data_cb;
    static int s_media_state;
    static esp_bd_addr_t s_peer_bda;
    static uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    static int s_a2d_state;
    static TimerHandle_t s_tmr;
    static int s_intv_cnt;
    static int s_connecting_intv;
    static uint32_t s_pkt_cnt;
    static uint8_t reconnects;
    static esp_bd_addr_t last_connection;

    static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
    static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);
    static void bt_app_a2d_heart_beat(TimerHandle_t arg);
    static void bt_app_av_sm_hdlr(uint16_t event, void *param);
    static void bt_app_av_state_unconnected_hdlr(uint16_t event, void *param);
    static void bt_app_av_state_connecting_hdlr(uint16_t event, void *param);
    static void bt_app_av_state_connected_hdlr(uint16_t event, void *param);
    static void bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param);
    static void bt_app_av_media_proc(uint16_t event, void *param);


    /*********************************
     * HELP METHODS
     ********************************/
    static char* bda2str(esp_bd_addr_t bda, char *str, size_t size);
    static esp_err_t write_bda_to_nvs(const char* name, esp_bd_addr_t bda);
    static void get_last_connection();
    static esp_err_t set_last_connection(esp_bd_addr_t bda);
    static bool has_last_connection();
    static esp_err_t reconnect(esp_bd_addr_t peer);
    static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len);
    static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param);

    /*********************************
     * APP CORE METHODS
     ********************************/

    /* application task handler */
    static void bt_app_task_handler(void *arg);
    /* message sender for Work queue */
    static bool bt_app_send_msg(bt_app_msg_t *msg);

    /**
     * @brief    work dispatcher for the application task
     *
     * @param [in] p_cback       handler for the dispatched work (event handler)
     * @param [in] event         message event id
     * @param [in] p_params      pointer to the parameter
     * @param [in] param_len     length of the parameter
     *
     * @return  true if work dispatch successfully, false otherwise
     */
    static bool bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len);

    /**
     * @brief    start up the application task
     */
    static void bt_app_task_start_up(void);

    /**
     * @brief    shut down the application task
     */
    static void bt_app_task_shut_down(void);
    static esp_err_t media_ctrl(esp_a2d_media_ctrl_t ctrl);
};

#endif //A2DP_SOURCE_H
