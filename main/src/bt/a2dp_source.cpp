//
// Created by Clas Zachrisson on 2025-04-18.
//
#include "a2dp_source.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_a2dp_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
//#include "esp_avrc_api.h"
#include <cstring>

enum {
    BT_APP_STACK_UP_EVT   = 0x0000,    /* event for stack up */
    BT_APP_HEART_BEAT_EVT = 0xff00,    /* event for heart beat */
};

/* A2DP global states */
enum {
    APP_AV_STATE_IDLE,
    APP_AV_STATE_DISCOVERING,
    APP_AV_STATE_DISCOVERED,
    APP_AV_STATE_UNCONNECTED,
    APP_AV_STATE_CONNECTING,
    APP_AV_STATE_CONNECTED,
    APP_AV_STATE_DISCONNECTING,
};

/* sub states of APP_AV_STATE_CONNECTED */
enum {
    APP_AV_MEDIA_STATE_IDLE,
    APP_AV_MEDIA_STATE_STARTING,
    APP_AV_MEDIA_STATE_STARTED,
    APP_AV_MEDIA_STATE_STOPPING,
};

static constexpr char BT_AV_TAG[] = "a2dp_source";
constexpr char LOCAL_DEVICE_NAME[] = "ESP_A2DP_SRC";

esp_a2d_source_data_cb_t A2DPSource::bt_app_a2d_data_cb = nullptr;      /* Bluetooth data callback function to be changed */
esp_bd_addr_t A2DPSource::s_peer_bda = {0};                             /* Bluetooth Device Address of peer device*/
uint8_t A2DPSource::s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];       /* Bluetooth Device Name of peer device*/
int A2DPSource::s_a2d_state = APP_AV_STATE_IDLE;                        /* A2DP global state */
TimerHandle_t A2DPSource::s_tmr;                                        /* handle of heart beat timer */
int A2DPSource::s_media_state = APP_AV_MEDIA_STATE_IDLE;                /* sub states of APP_AV_STATE_CONNECTED */
int A2DPSource::s_intv_cnt = 0;                                         /* count of heart beat intervals */
int A2DPSource::s_connecting_intv = 0;                                  /* count of heart beat intervals for connecting */
uint32_t A2DPSource::s_pkt_cnt = 0;                                     /* count of packets */
uint8_t A2DPSource::reconnects = 0;
esp_bd_addr_t A2DPSource::last_connection = {0,0,0,0,0,0};

static constexpr char remote_device_name[] = "Beoplay H8i";       /* Remote device name to connect to*/

char* A2DPSource::bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == nullptr || str == nullptr || size < 18) {
        return nullptr;
    }

    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

esp_err_t A2DPSource::write_bda_to_nvs(const char* name, esp_bd_addr_t bda) {
    nvs_handle my_handle;
    esp_err_t err;

    err = nvs_open("connected_bda", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "NVS OPEN ERROR");
        return false;
    }
    err = nvs_set_blob(my_handle, "last_bda", bda, ESP_BD_ADDR_LEN);
    if (err != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "NVS WRITE ERROR");
    }
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "NVS COMMIT ERROR");
    }
    nvs_close(my_handle);
    return err;
}

void A2DPSource::get_last_connection(){
    nvs_handle my_handle;
    esp_err_t err;

    err = nvs_open("connected_bda", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(BT_AV_TAG,"NVS OPEN ERROR");
    }

    esp_bd_addr_t bda;
    size_t size = sizeof(bda);
    err = nvs_get_blob(my_handle, "last_bda", bda, &size);
    if ( err != ESP_OK) {
        if ( err == ESP_ERR_NVS_NOT_FOUND ) {
            ESP_LOGE(BT_AV_TAG, "nvs_blob does not exist");
        } else {
            ESP_LOGE(BT_AV_TAG, "nvs_get_blob failed");
        }
    }
    nvs_close(my_handle);
    if (err == ESP_OK) {
        memcpy(last_connection,bda,size);
    }
}

esp_err_t A2DPSource::set_last_connection(esp_bd_addr_t bda){
    //ESP_LOGD(BT_AV_TAG, "set_last_connection: %s", bda2str(bda));

    // same value: nothing to store
    if (memcmp(bda, last_connection, ESP_BD_ADDR_LEN) == 0) {
        ESP_LOGD(BT_AV_TAG, "no change!");
        return ESP_OK;
    }
    esp_err_t err = write_bda_to_nvs("last_bda", bda);
    // update last_connection variable
    memcpy(last_connection, bda, ESP_BD_ADDR_LEN);
    return err;
}

bool A2DPSource::has_last_connection() {
    esp_bd_addr_t empty_connection = {0,0,0,0,0,0};
    int result = memcmp(last_connection, empty_connection, ESP_BD_ADDR_LEN);
    return result!=0;
}
esp_err_t A2DPSource::reconnect(esp_bd_addr_t peer){
    if(reconnects > 2)
        return ESP_BT_STATUS_HCI_REPEATED_ATTEMPTS;
    reconnects++;
    esp_err_t err = esp_a2d_source_connect(peer);
    if (err!=ESP_OK){
        ESP_LOGE(BT_AV_TAG, "esp_a2d_source_connect:%d", err);
        return err;
    }
    reconnects = 0;
    return err;
}


bool A2DPSource::get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = nullptr;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    /* get complete or short local name from eir data */
    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

void A2DPSource::filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    uint32_t cod = 0;     /* class of device */
    int32_t rssi = -129;  /* invalid value */
    uint8_t *eir = nullptr;
    esp_bt_gap_dev_prop_t *p;

    /* handle the discovery results */
    ESP_LOGI(BT_AV_TAG, "Scanned device: %s", bda2str(param->disc_res.bda, bda_str, 18));
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *static_cast<uint32_t*>(p->val);
            ESP_LOGI(BT_AV_TAG, "--Class of Device: 0x%lX", cod);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *static_cast<int8_t*>(p->val);
            ESP_LOGI(BT_AV_TAG, "--RSSI: %ld", rssi);
            break;
        case ESP_BT_GAP_DEV_PROP_EIR:
            eir = static_cast<uint8_t*>(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
        default:
            break;
        }
    }

    /* search for device with MAJOR service class as "rendering" in COD */
    if (!esp_bt_gap_is_valid_cod(cod) ||
            !(esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING)) {
        return;
            }

    /* search for target device in its Extended Inqury Response */
    if (eir) {
        get_name_from_eir(eir, s_peer_bdname, nullptr);
        int comp = strcmp(reinterpret_cast<char*>(s_peer_bdname), remote_device_name);
        //ESP_LOGI(BT_AV_TAG, "Potential target device, address %s, name %s, match: %d", bda_str, s_peer_bdname, comp);
        if (comp == 0) {
            ESP_LOGI(BT_AV_TAG, "Found a target device, address %s, name %s", bda_str, s_peer_bdname);
            s_a2d_state = APP_AV_STATE_DISCOVERED;
            memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
            ESP_LOGI(BT_AV_TAG, "Cancel device discovery ...");
            esp_bt_gap_cancel_discovery();
        }
    }
}

void A2DPSource::bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param)
{
    uint8_t *bda = nullptr;
    switch (event) {
    /* when device discovered a result, this event comes */
    case ESP_BT_GAP_DISC_RES_EVT: {
        if (s_a2d_state == APP_AV_STATE_DISCOVERING) {
            filter_inquiry_scan_result(param);
        }
        break;
    }
    /* when discovery state changed, this event comes */
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            if (s_a2d_state == APP_AV_STATE_DISCOVERED) {
                s_a2d_state = APP_AV_STATE_CONNECTING;
                ESP_LOGI(BT_AV_TAG, "Device discovery stopped.");
                ESP_LOGI(BT_AV_TAG, "a2dp connecting to peer: %s", s_peer_bdname);
                /* connect source to peer device specified by Bluetooth Device Address */
                esp_a2d_source_connect(s_peer_bda);
            } else {
                /* not discovered, continue to discover */
                ESP_LOGI(BT_AV_TAG, "Device discovery failed, continue to discover...");
                esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
            }
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            ESP_LOGI(BT_AV_TAG, "Discovery started.");
        }
        break;
    }
    /* when authentication completed, this event comes */
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(BT_AV_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;
    }
    /* when Security Simple Pairing user confirmation requested, this event comes */
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06lu", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    /* when Security Simple Pairing passkey notified, this event comes */
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06lu", param->key_notif.passkey);
        break;
    /* when Security Simple Pairing passkey requested, this event comes */
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
    /* when GAP mode changed, this event comes */
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d", param->mode_chg.mode);
        /*if (param->mode_chg.mode == 2)
        {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        }*/
        break;
    case ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT:
        if (param->get_dev_name_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT device name: %s", param->get_dev_name_cmpl.name);
        } else {
            ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT failed, state: %d", param->get_dev_name_cmpl.status);
        }
        break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        bda = static_cast<uint8_t*>(param->acl_conn_cmpl_stat.bda);
        s_a2d_state = APP_AV_STATE_CONNECTED;
        set_last_connection(bda);
        //esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        break;
    /* when ACL disconnection completed, this event comes */
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        bda = static_cast<uint8_t*>(param->acl_disconn_cmpl_stat.bda);
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
                bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);
    /* other */
    default: {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
}

void A2DPSource::bt_app_a2d_heart_beat(TimerHandle_t arg)
{
    bt_app_work_dispatch(bt_app_av_sm_hdlr, BT_APP_HEART_BEAT_EVT, nullptr, 0);
}

void A2DPSource::bt_app_av_sm_hdlr(uint16_t event, void *param)
{
    ESP_LOGI(BT_AV_TAG, "%s state: %d, event: 0x%x", __func__, s_a2d_state, event);

    if (event==ESP_A2D_AUDIO_CFG_EVT)
    {
        ESP_LOGI(BT_AV_TAG, "%s AUDIO_CFG_EVT", __func__);
    }
    /* select handler according to different states */
    switch (s_a2d_state) {
    case APP_AV_STATE_DISCOVERING:
    case APP_AV_STATE_DISCOVERED:
        break;
    case APP_AV_STATE_UNCONNECTED:
        bt_app_av_state_unconnected_hdlr(event, param);
        break;
    case APP_AV_STATE_CONNECTING:
        bt_app_av_state_connecting_hdlr(event, param);
        break;
    case APP_AV_STATE_CONNECTED:
        bt_app_av_state_connected_hdlr(event, param);
        break;
    case APP_AV_STATE_DISCONNECTING:
        bt_app_av_state_disconnecting_hdlr(event, param);
        break;
    default:
        ESP_LOGE(BT_AV_TAG, "%s invalid state: %d", __func__, s_a2d_state);
        break;
    }
}
void A2DPSource::bt_app_av_state_unconnected_hdlr(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = nullptr;
    /* handle the events of interest in unconnected state */
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(BT_AV_TAG, "bt_app_av_state_unconnected_hdlr AUDIO_CFG_EVT");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;
    case BT_APP_HEART_BEAT_EVT: {
        uint8_t *bda = s_peer_bda;
        ESP_LOGI(BT_AV_TAG, "a2dp connecting to peer: %02x:%02x:%02x:%02x:%02x:%02x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        esp_a2d_source_connect(s_peer_bda);
        s_a2d_state = APP_AV_STATE_CONNECTING;
        s_connecting_intv = 0;
        break;
    }
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__, a2d->a2d_report_delay_value_stat.delay_value);
        break;
    }
    default: {
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
    }
}

void A2DPSource::bt_app_av_state_connecting_hdlr(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = nullptr;

    /* handle the events of interest in connecting state */
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            ESP_LOGI(BT_AV_TAG, "a2dp connected");
            s_a2d_state =  APP_AV_STATE_CONNECTED;
            s_media_state = APP_AV_MEDIA_STATE_IDLE;
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_a2d_state =  APP_AV_STATE_UNCONNECTED;
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        ESP_LOGI(BT_AV_TAG, "Audio codec configured: sample rate %d, channels %d",
                 a2d->audio_cfg.mcc.type, a2d->audio_cfg.mcc.cie.sbc);
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;
    case BT_APP_HEART_BEAT_EVT:
        /**
         * Switch state to APP_AV_STATE_UNCONNECTED
         * when connecting lasts more than 2 heart beat intervals.
         */
        if (++s_connecting_intv >= 2) {
            s_a2d_state = APP_AV_STATE_UNCONNECTED;
            s_connecting_intv = 0;
        }
        break;
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__, a2d->a2d_report_delay_value_stat.delay_value);
        break;
    }
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

void A2DPSource::bt_app_av_media_proc(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = nullptr;

    switch (s_media_state) {
    case APP_AV_MEDIA_STATE_IDLE: {
        if (event == BT_APP_HEART_BEAT_EVT) {
            ESP_LOGI(BT_AV_TAG, "a2dp media ready checking ...");
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
            a2d = static_cast<esp_a2d_cb_param_t*>(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                ESP_LOGI(BT_AV_TAG, "a2dp media ready, starting ...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                s_media_state = APP_AV_MEDIA_STATE_STARTING;
            }
        }
        break;
    }
    case APP_AV_MEDIA_STATE_STARTING: {
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
            a2d = static_cast<esp_a2d_cb_param_t*>(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                ESP_LOGI(BT_AV_TAG, "a2dp media start successfully.");
                s_intv_cnt = 0;
                s_media_state = APP_AV_MEDIA_STATE_STARTED;
            } else {
                /* not started successfully, transfer to idle state */
                ESP_LOGI(BT_AV_TAG, "a2dp media start failed.");
                s_media_state = APP_AV_MEDIA_STATE_IDLE;
            }
        }
        break;
    }
    case APP_AV_MEDIA_STATE_STARTED: {
        // if (event == BT_APP_HEART_BEAT_EVT) {
        //     /* stop media after 10 heart beat intervals */
        //     /*if (++s_intv_cnt >= 10) {
        //         ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
        //         esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        //         s_media_state = APP_AV_MEDIA_STATE_STOPPING;
        //         s_intv_cnt = 0;
        //     }*/
        //     esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        // } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        // {
        //     if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
        //             a2d->media_ctrl_stat.status != ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
        //         ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
        //         esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        //         s_media_state = APP_AV_MEDIA_STATE_STOPPING;
        //         s_intv_cnt = 0;
        //     }
        // }
        break;
    }
    case APP_AV_MEDIA_STATE_STOPPING: {
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
            /*a2d = static_cast<esp_a2d_cb_param_t*>(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                ESP_LOGI(BT_AV_TAG, "a2dp media suspend successfully, disconnecting...");
                s_media_state = APP_AV_MEDIA_STATE_IDLE;
                esp_a2d_source_disconnect(s_peer_bda);
                s_a2d_state = APP_AV_STATE_DISCONNECTING;
            } else {
                ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
            }*/
            a2d = static_cast<esp_a2d_cb_param_t*>(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                s_media_state = APP_AV_MEDIA_STATE_IDLE;
            } else
            {
                ESP_LOGI(BT_AV_TAG, "a2dp media suspending...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
            }
        }
        break;
    }
    default: {
        break;
    }
    }
}

void A2DPSource::bt_app_av_state_connected_hdlr(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = nullptr;

    /* handle the events of interest in connected state */
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
            s_a2d_state = APP_AV_STATE_UNCONNECTED;
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
            s_pkt_cnt = 0;
        }
        break;
    }
    case ESP_A2D_AUDIO_CFG_EVT:
        // not supposed to occur for A2DP source
        ESP_LOGI(BT_AV_TAG, "bt_app_av_state_connected_hdlr AUDIO_CFG_EVT");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT: {
        bt_app_av_media_proc(event, param);
        break;
    }
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        ESP_LOGI(BT_AV_TAG, "%s, delay value: %u * 1/10 ms", __func__, a2d->a2d_report_delay_value_stat.delay_value);
        break;
    }
    default: {
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
    }
}

void A2DPSource::bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = nullptr;

    /* handle the events of interest in disconnecing state */
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(BT_AV_TAG, "a2dp disconnected");
            s_a2d_state =  APP_AV_STATE_UNCONNECTED;
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(BT_AV_TAG, "bt_app_av_state_disconnecting_hdlr AUDIO_CFG_EVT");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
        break;
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT: {
        a2d = static_cast<esp_a2d_cb_param_t*>(param);
        ESP_LOGI(BT_AV_TAG, "%s, delay value: 0x%u * 1/10 ms", __func__, a2d->a2d_report_delay_value_stat.delay_value);
        break;
    }
    default: {
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
    }
}

void A2DPSource::bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    bt_app_work_dispatch(bt_app_av_sm_hdlr, event, param, sizeof(esp_a2d_cb_param_t));
}

void A2DPSource::bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    switch (event) {
        /* when stack up worked, this event comes */
    case BT_APP_STACK_UP_EVT: {
            esp_bt_gap_set_device_name(LOCAL_DEVICE_NAME);
            esp_bt_gap_register_callback(bt_gap_cb);

            //esp_avrc_ct_init();
            //esp_avrc_ct_register_callback(bt_app_rc_ct_cb);

            //esp_avrc_rn_evt_cap_mask_t evt_set = {0};
            //esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
            //ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));

            esp_a2d_source_init();
            esp_a2d_register_callback(&bt_app_a2d_cb);
            esp_a2d_source_register_data_callback(bt_app_a2d_data_cb);
            //ESP_LOGI(BT_AV_TAG, "data cb: %p", bt_app_a2d_data_cb);

            /* Avoid the state error of s_a2d_state caused by the connection initiated by the peer device. */
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            esp_bt_gap_get_device_name();

            esp_err_t err = ESP_ERR_NOT_FOUND;
            if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED){
                get_last_connection();

                if (true){ //if(has_last_connection()){
                    err = reconnect(last_connection);
                    memcpy(s_peer_bda, last_connection, sizeof(esp_bd_addr_t));
                }
            }
            if (err != ESP_OK)
            {
                ESP_LOGI(BT_AV_TAG, "Starting device discovery...");
                s_a2d_state = APP_AV_STATE_DISCOVERING;
                esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
            }


            /* create and start heart beat timer */
            int tmr_id = 0;
            s_tmr = xTimerCreate("connTmr", 10000 / portTICK_PERIOD_MS,
                                 pdTRUE, &tmr_id, bt_app_a2d_heart_beat);
            xTimerStart(s_tmr, portMAX_DELAY);
            break;
    }
        /* other */
    default: {
            ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
            break;
    }
    }
}

void A2DPSource::register_a2d_source_data_cb(esp_a2d_source_data_cb_t a2d_source_data_cb)
{
    bt_app_a2d_data_cb = a2d_source_data_cb;
}

A2DPSource::A2DPSource(){}

void A2DPSource::init()
{
    char bda_str[18] = {};
    /* initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s initialize controller failed", __func__);
        return;
    }
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s enable controller failed", __func__);
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    //bluedroid_cfg.ssp_en = false;

    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed", __func__);
        return;
    }

    /* set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    ESP_LOGI(BT_AV_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
    bt_app_task_start_up();
    /* Bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_STACK_UP_EVT, nullptr, 0);
}

void A2DPSource::start()
{
    ESP_LOGI(BT_AV_TAG, "Starting A2DPSource");
    //bt_task_work_dispatch(media_ctrl)
    esp_err_t err = this->media_ctrl(ESP_A2D_MEDIA_CTRL_START);
    if (err != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "Media start failed");
    }
}

int A2DPSource::get_state()
{
    return s_a2d_state;
}

esp_err_t A2DPSource::media_ctrl(const esp_a2d_media_ctrl_t ctrl)
{
    return esp_a2d_media_ctrl(ctrl);
}
