/*
 * SPDX-FileCopyrightText: 2022 Zauberzeug GmbH
 *
 * SPDX-License-Identifier: MIT
 */

#include "esp32-ble-command.h"

#include <cassert>

#include <esp_bt.h>
#include <esp_log.h>
#include <esp_nimble_hci.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <esp_zeug/ble/gatts.h>
#include <esp_zeug/ble/uuid.h>
#include <esp_zeug/frtos-util.h>
#include <esp_zeug/util.h>

#include "sdkconfig.h"

namespace ZZ::BleCommand {

/* This buffer will be used for advertising, so keep the device name
 * truncated to 29 bytes here */
static Util::TextBuffer<29 + 1> l_deviceName{};
static constexpr ble_uuid128_t serviceUuid{CONFIG_ZZ_BLE_COM_SVC_UUID ""_uuid128};
static constexpr ble_uuid128_t characteristicUuid{CONFIG_ZZ_BLE_COM_CHR_UUID ""_uuid128};
static constexpr ble_uuid128_t notifyCharaUuid{CONFIG_ZZ_BLE_COM_SEND_CHR_UUID ""_uuid128};
static constexpr esp_power_level_t defaultPowerLevel{ESP_PWR_LVL_P9};

/* Range: 0x001B-0x00FB */
static constexpr std::uint16_t txDataLength{0xFB};

/* Range: 0x0148-0x0848 (stated max 0x4290 leads to BLE_HS_ECONTROLLER) */
static constexpr std::uint16_t txDataTime{0x0848};

static const char TAG[]{"BleCom"};

using namespace FrtosUtil;

static uint8_t l_ownAddrType;
static CommandCallback l_clientCallback;
static bool l_running{false};

static std::uint16_t l_notifyCharaValueHandle;
static std::uint16_t l_currentCon{BLE_HS_CONN_HANDLE_NONE};

static auto advertise() -> void;

static auto onGapEvent(struct ble_gap_event *event, void *) -> int {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGV(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        if (event->connect.status == 0) {
            /* Max packet length, min transmission time */
            ESP_LOGI(TAG, "set_data_len(%X, %X)", txDataLength, txDataTime);
            int rc = ble_gap_set_data_len(event->connect.conn_handle,
                                          txDataLength, txDataTime);
            if (rc != 0) {
                ESP_LOGW(TAG, "set_data_len failed; rc=0x%X", rc);
            }

            l_currentCon = event->connect.conn_handle;
        }

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGV(TAG, "disconnect; reason=%d", event->disconnect.reason);

        if (event->disconnect.conn.conn_handle == l_currentCon) {
            l_currentCon = BLE_HS_CONN_HANDLE_NONE;
        }

        /* Connection terminated; resume advertising. */
        advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGV(TAG, "advertise complete; reason=%d",
                 event->adv_complete.reason);
        advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGV(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;
    }

    return 0;
}

static auto advertise() -> void {
    if (!l_running) {
        return;
    }

    ble_hs_adv_fields fields{};

    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = defaultPowerLevel;

    static constexpr ble_uuid16_t alertUuid{"1811"_uuid16};
    fields.uuids16 = &alertUuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc;
    rc = ble_gap_adv_set_fields(&fields);

    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    /* Use up the entire scan response payload for the device name */
    ble_hs_adv_fields scanFields{};
    scanFields.name = reinterpret_cast<const uint8_t *>(l_deviceName.data());
    scanFields.name_len = l_deviceName.length();
    scanFields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&scanFields);

    if (rc != 0) {
        ESP_LOGE(TAG, "error setting scan response data; rc=%d", rc);
        return;
    }

    /* Begin advertising. */
    ble_gap_adv_params advParams{};
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(l_ownAddrType, NULL, BLE_HS_FOREVER,
                           &advParams, onGapEvent, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

Task<NIMBLE_STACK_SIZE> hostTask{
    "ble_host",
    Core::PRO,
    []() {
        /* This function will return only when nimble_port_stop() is executed */
        nimble_port_run();

        /* Cleanup */
        nimble_port_deinit();
        Task<>::haltCurrent();
    },
};

static const Ble::Gatts::Service lizardComService{
    serviceUuid,
    {
        Ble::Gatts::Characteristic{
            characteristicUuid,
            BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            [](std::uint16_t, std::uint16_t, ble_gatt_access_ctxt *ctx) -> int {
                const std::string_view command{reinterpret_cast<char *>(ctx->om->om_data), ctx->om->om_len};
                l_clientCallback(command);

                return 0;
            },
        },
        Ble::Gatts::Characteristic{
            notifyCharaUuid,
            BLE_GATT_CHR_F_NOTIFY,
            &l_notifyCharaValueHandle,
            [](std::uint16_t, std::uint16_t, ble_gatt_access_ctxt *ctx) -> int {
                /* not to be read directly, only accessible via notifications */
                return 0;
            },
        },
    },
};

const std::array services{
    lizardComService.def(),
    ble_gatt_svc_def{},
};

auto init(const std::string_view &deviceName,
          CommandCallback onCommand) -> void {
    l_deviceName = decltype(l_deviceName)(deviceName);
    l_clientCallback = onCommand;
    l_running = true;

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    nimble_port_init();

    /* Initialize the NimBLE host configuration. */

    ble_hs_cfg.reset_cb = [](int reason) {
        ESP_LOGV(TAG, "Resetting state; reason=%d", reason);
    };

    ble_hs_cfg.sync_cb = []() {
        int rc;

        rc = ble_hs_util_ensure_addr(0);
        assert(rc == 0);

        /* Figure out address to use while advertising (no privacy for now) */
        rc = ble_hs_id_infer_auto(0, &l_ownAddrType);
        if (rc != 0) {
            ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
            return;
        }

        /* Begin advertising. */
        advertise();
    };

    ble_hs_cfg.gatts_register_cb = nullptr;
    ble_hs_cfg.store_status_cb = nullptr;
    ble_hs_cfg.sm_sc = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc;

    rc = ble_gatts_count_cfg(services.data());
    assert(rc == 0);

    rc = ble_gatts_add_svcs(services.data());
    assert(rc == 0);

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, defaultPowerLevel);

    /* This device name will be exposed as an attribute as part of GAP,
     * but not within advertisement packets */
    ble_svc_gap_device_name_set(deviceName.data());

    hostTask.run();
}

auto send(const std::string_view &data) -> int {
    if (l_currentCon == BLE_HS_CONN_HANDLE_NONE) {
        return BLE_HS_ENOTCONN;
    }

    os_mbuf *om{ble_hs_mbuf_from_flat(data.data(), data.length())};
    if (om == nullptr) {
        return BLE_HS_ENOMEM;
    }

    return ble_gattc_notify_custom(l_currentCon, l_notifyCharaValueHandle, om);
}

auto fini() -> void {
    l_running = false;

    nimble_port_stop();
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_deinit());
}

} // namespace ZZ::BleCommand
