#define DT_DRV_COMPAT zmk_input_processor_profile_scaler

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/endpoints_types.h>

LOG_MODULE_REGISTER(profile_scaler, CONFIG_ZMK_LOG_LEVEL);

struct profile_scaler_config {
    const uint32_t *profiles; /* flat array: profile0 num0 den0 profile1 num1 den1 ... */
    uint8_t profile_count;    /* number of (profile, num, den) triplets */
    uint32_t fallback_num;
    uint32_t fallback_den;
};

struct profile_scaler_data {
    int32_t remainder_x;
    int32_t remainder_y;
};

/* Endpoint is system-wide state — track globally */
static bool is_usb = false;
static uint8_t active_profile_idx = 0;

static void get_ratio(const struct device *dev, uint32_t *num, uint32_t *den) {
    const struct profile_scaler_config *cfg = dev->config;

    if (!is_usb) {
        for (int i = 0; i < cfg->profile_count * 3; i += 3) {
            if (cfg->profiles[i] == active_profile_idx) {
                *num = cfg->profiles[i + 1];
                *den = cfg->profiles[i + 2];
                return;
            }
        }
    }

    *num = cfg->fallback_num;
    *den = cfg->fallback_den;
}

static int profile_scaler_handle_event(const struct device *dev,
                                        struct input_event *event,
                                        uint32_t param1,
                                        uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    struct profile_scaler_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint32_t num, den;
    get_ratio(dev, &num, &den);

    if (den == 0 || num == den) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t *remainder = (event->code == INPUT_REL_X) ? &data->remainder_x
                                                       : &data->remainder_y;

    /* Accumulate fractional counts across events to avoid precision loss */
    int32_t total = event->value * (int32_t)num + *remainder;
    int32_t scaled = total / (int32_t)den;
    *remainder = total % (int32_t)den;

    event->value = scaled;
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api profile_scaler_api = {
    .handle_event = profile_scaler_handle_event,
};

static int endpoint_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *ev = as_zmk_endpoint_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->endpoint.transport == ZMK_TRANSPORT_USB) {
        is_usb = true;
        LOG_DBG("Endpoint: USB - using fallback ratio");
    } else {
        is_usb = false;
        active_profile_idx = ev->endpoint.ble.profile_index;
        LOG_DBG("Endpoint: BLE profile %d", active_profile_idx);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(profile_scaler, endpoint_listener);
ZMK_SUBSCRIPTION(profile_scaler, zmk_endpoint_changed);

#define PROFILE_SCALER_INST(n)                                                        \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, profiles),                                   \
        (static const uint32_t profiles_##n[] = DT_INST_PROP(n, profiles);),          \
        (static const uint32_t profiles_##n[] = {};))                                 \
    static const struct profile_scaler_config config_##n = {                          \
        .profiles = profiles_##n,                                                     \
        .profile_count = COND_CODE_1(DT_INST_NODE_HAS_PROP(n, profiles),             \
            (DT_INST_PROP_LEN(n, profiles) / 3), (0)),                               \
        .fallback_num = DT_INST_PROP(n, fallback_numerator),                          \
        .fallback_den = DT_INST_PROP(n, fallback_denominator),                        \
    };                                                                                \
    static struct profile_scaler_data data_##n = {};                                  \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n,                      \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &profile_scaler_api);

DT_INST_FOREACH_STATUS_OKAY(PROFILE_SCALER_INST)
