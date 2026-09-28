#pragma once

#include <furi_hal_bt.h>
#include <stddef.h>

typedef struct {
    bool acquired;
    bool configured;
    bool running;
    bool faulted;
    bool previous_config_valid;
    GapExtraBeaconConfig previous_config;
    uint8_t previous_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t previous_size;
    GapExtraBeaconConfig config;
} Advertiser;

bool advertiser_payload_valid(const uint8_t* data, size_t size);
bool advertiser_initialize(Advertiser* advertiser);
bool advertiser_configure(Advertiser* advertiser, const uint8_t* data, size_t size);
bool advertiser_start(Advertiser* advertiser);
bool advertiser_stop(Advertiser* advertiser);
bool advertiser_deinitialize(Advertiser* advertiser);
