#include "advertiser.h"
#include <furi_hal_random.h>
#include <string.h>

#define TAG "BleAdvTest"

bool advertiser_payload_valid(const uint8_t* data, size_t size) {
    if(!data || !size || size > EXTRA_BEACON_MAX_DATA_SIZE) return false;
    // Each AD structure contains its length byte, type byte and value.
    for(size_t offset = 0; offset < size;) {
        size_t length = data[offset];
        if(!length || length > size - offset - 1) return false;
        if(data[offset + 1] == 0xFF && length < 3) return false;
        offset += length + 1;
    }
    return true;
}

static bool advertiser_failure(Advertiser* advertiser, const char* operation) {
    // OFW 1.4.3 extra_beacon.c leaves its mutex locked on HCI failures.
    // Never retry or attempt restoration through that mutex after a failure.
    advertiser->faulted = true;
    FURI_LOG_E(TAG, "%s failed; HAL operations disabled, reboot required", operation);
    return false;
}

bool advertiser_initialize(Advertiser* advertiser) {
    if(!advertiser || advertiser->acquired || advertiser->faulted) return false;
    if(!furi_hal_bt_is_alive() || !furi_hal_bt_is_gatt_gap_supported()) {
        FURI_LOG_E(TAG, "BLE radio unavailable");
        return false;
    }
    if(furi_hal_bt_extra_beacon_is_active()) {
        FURI_LOG_E(TAG, "Extra beacon busy; leaving it untouched");
        return false;
    }
    const GapExtraBeaconConfig* previous = furi_hal_bt_extra_beacon_get_config();
    advertiser->previous_config_valid = previous != NULL;
    if(previous) advertiser->previous_config = *previous;
    advertiser->previous_size = furi_hal_bt_extra_beacon_get_data(advertiser->previous_data);
    advertiser->config = (GapExtraBeaconConfig){
        .min_adv_interval_ms = 200,
        .max_adv_interval_ms = 200,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_Neg20_85dBm,
        .address_type = GapAddressTypeRandom,
    };
    furi_hal_random_fill_buf(advertiser->config.address, sizeof(advertiser->config.address));
    // BLE static random address: high two bits 11, remaining bits neither all 0 nor all 1.
    advertiser->config.address[5] |= 0xC0;
    advertiser->config.address[0] = (advertiser->config.address[0] & 0xFE) | 0x02;
    advertiser->acquired = true;
    // The firmware owns this singleton; this is a logical claim, not an API handle/lock.
    FURI_LOG_I(TAG, "BLE resource acquired (logical singleton claim)");
    return true;
}

bool advertiser_configure(Advertiser* advertiser, const uint8_t* data, size_t size) {
    if(!advertiser || !advertiser->acquired || advertiser->running || advertiser->faulted)
        return false;
    if(!advertiser_payload_valid(data, size)) {
        FURI_LOG_E(TAG, "Rejected invalid advertisement length or AD structure");
        return false;
    }
    // Public OFW API replaces the legacy flash scan and raw HCI function pointer.
    if(!furi_hal_bt_extra_beacon_set_config(&advertiser->config))
        return advertiser_failure(advertiser, "configure");
    if(!furi_hal_bt_extra_beacon_set_data(data, (uint8_t)size))
        return advertiser_failure(advertiser, "set data");
    advertiser->configured = true;
    FURI_LOG_I(TAG, "Advertisement configured: %u bytes, 200 ms", (unsigned)size);
    return true;
}

bool advertiser_start(Advertiser* advertiser) {
    if(!advertiser || !advertiser->acquired || !advertiser->configured || advertiser->faulted)
        return false;
    if(advertiser->running) return true;
    if(!furi_hal_bt_extra_beacon_start()) return advertiser_failure(advertiser, "start");
    advertiser->running = true;
    FURI_LOG_I(TAG, "Advertising started");
    return true;
}

bool advertiser_stop(Advertiser* advertiser) {
    if(!advertiser || advertiser->faulted) return false;
    if(!advertiser->running) return true;
    if(!furi_hal_bt_extra_beacon_stop()) return advertiser_failure(advertiser, "stop");
    advertiser->running = false;
    FURI_LOG_I(TAG, "Advertising stopped");
    return true;
}

bool advertiser_deinitialize(Advertiser* advertiser) {
    if(!advertiser || advertiser->faulted) return false;
    if(!advertiser->acquired) return true;
    if(!advertiser_stop(advertiser)) return false;
    if(advertiser->previous_config_valid &&
       !furi_hal_bt_extra_beacon_set_config(&advertiser->previous_config))
        return advertiser_failure(advertiser, "restore config");
    if(!furi_hal_bt_extra_beacon_set_data(advertiser->previous_data, advertiser->previous_size))
        return advertiser_failure(advertiser, "restore data");
    advertiser->acquired = false;
    advertiser->configured = false;
    return true;
}
