/* LiPo 1S candidate: boot sample/report after join, then every 4 hours.
 * No ADC/report in the button path. VDDH input and x5 preserved.
 * This is NOT a single-factor C/D diagnostic and is not hardware calibrated.
 * Confirm 3.7 V nominal / 4.2 V full and VDDH-to-battery tracking before relying
 * on percentage. Never use the reported percentage as battery protection.
 */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/util.h>

#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zboss_api_zcl.h>

#include <zcl/zb_zcl_power_config.h>
#include <ha/zb_ha_dimmer_switch.h>

#include <zigbee/zigbee_app_utils.h>
#include <zb_nrf_platform.h>

#define BUTTON_PIN       17
#define SWITCH_ENDPOINT  1
#define DEBOUNCE_TIME_MS 35

#include "battery_config.h"
#define BATTERY_ADC_MULTIPLIER 5
#define BATTERY_REPORT_INTERVAL_HOURS 4

static const struct device *gpio0 =
    DEVICE_DT_GET(DT_NODELABEL(gpio0));

static struct gpio_callback button_gpio_cb;
static struct k_work_delayable debounce_work;

static int last_button_state;
static struct k_work_delayable battery_work;
static bool boot_report_sent;

volatile uint32_t battery_adc_errors, battery_schedule_errors;
volatile uint32_t toggle_schedule_errors, zigbee_buffer_errors;
volatile int battery_last_mv;

static const struct adc_dt_spec battery_adc =
    ADC_DT_SPEC_GET_BY_IDX(
        DT_PATH(zephyr_user),
        0
    );

struct zigbutton_device_ctx
{
    zb_zcl_basic_attrs_t basic_attr;
    zb_zcl_identify_attrs_t identify_attr;

    zb_uint8_t battery_voltage;

    zb_uint8_t battery_percentage_remaining;
};

static struct zigbutton_device_ctx dev_ctx;

ZB_ZCL_DECLARE_BASIC_SERVER_ATTRIB_LIST(
    basic_attr_list,
    &dev_ctx.basic_attr.zcl_version,
    &dev_ctx.basic_attr.power_source
);

ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(
    identify_attr_list,
    &dev_ctx.identify_attr.identify_time
);

ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(
    power_config_attr_list,
    ZB_ZCL_POWER_CONFIG
)

    ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID(
        &dev_ctx.battery_voltage,
    ),

    ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID(
        &dev_ctx.battery_percentage_remaining,
    ),

ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST;

static zb_zcl_cluster_desc_t zigbutton_clusters[] =
{
    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_BASIC,
        ZB_ZCL_ARRAY_SIZE(
            basic_attr_list,
            zb_zcl_attr_t
        ),
        basic_attr_list,
        ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_IDENTIFY,
        ZB_ZCL_ARRAY_SIZE(
            identify_attr_list,
            zb_zcl_attr_t
        ),
        identify_attr_list,
        ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ZB_ZCL_ARRAY_SIZE(
            power_config_attr_list,
            zb_zcl_attr_t
        ),
        power_config_attr_list,
        ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_IDENTIFY,
        0,
        NULL,
        ZB_ZCL_CLUSTER_CLIENT_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_SCENES,
        0,
        NULL,
        ZB_ZCL_CLUSTER_CLIENT_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_GROUPS,
        0,
        NULL,
        ZB_ZCL_CLUSTER_CLIENT_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_ON_OFF,
        0,
        NULL,
        ZB_ZCL_CLUSTER_CLIENT_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    ),

    ZB_ZCL_CLUSTER_DESC(
        ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        0,
        NULL,
        ZB_ZCL_CLUSTER_CLIENT_ROLE,
        ZB_ZCL_MANUF_CODE_INVALID
    )
};

ZB_DECLARE_SIMPLE_DESC(3, 5);

ZB_AF_SIMPLE_DESC_TYPE(3, 5)
simple_desc_zigbutton_ep =
{
    SWITCH_ENDPOINT,

    ZB_AF_HA_PROFILE_ID,

    ZB_HA_DIMMER_SWITCH_DEVICE_ID,

    0 ,

    0,

    3,
    5,

    {
        ZB_ZCL_CLUSTER_ID_BASIC,
        ZB_ZCL_CLUSTER_ID_IDENTIFY,
        ZB_ZCL_CLUSTER_ID_POWER_CONFIG,

        ZB_ZCL_CLUSTER_ID_ON_OFF,
        ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        ZB_ZCL_CLUSTER_ID_SCENES,
        ZB_ZCL_CLUSTER_ID_GROUPS,
        ZB_ZCL_CLUSTER_ID_IDENTIFY,
    }
};

ZB_AF_DECLARE_ENDPOINT_DESC(
    zigbutton_ep,

    SWITCH_ENDPOINT,

    ZB_AF_HA_PROFILE_ID,

    0,
    NULL,

    ZB_ZCL_ARRAY_SIZE(
        zigbutton_clusters,
        zb_zcl_cluster_desc_t
    ),

    zigbutton_clusters,

    (zb_af_simple_desc_1_1_t *)
        &simple_desc_zigbutton_ep,

    0,
    NULL,

    0,
    NULL
);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
    zigbutton_ctx,
    zigbutton_ep
);

static int battery_adc_init(void)
{
    if (!adc_is_ready_dt(&battery_adc))
    {
        return -ENODEV;
    }

    return adc_channel_setup_dt(
        &battery_adc
    );
}

static int battery_measure_mv(void)
{
    int err;

    int16_t raw;
    int32_t mv;

    struct adc_sequence sequence =
    {
        .buffer = &raw,
        .buffer_size = sizeof(raw),
    };

    err =
        adc_sequence_init_dt(
            &battery_adc,
            &sequence
        );

    if (err != 0)
    {
        return err;
    }

    err =
        adc_read(
            battery_adc.dev,
            &sequence
        );

    if (err != 0)
    {
        return err;
    }

    mv = raw;

    err =
        adc_raw_to_millivolts_dt(
            &battery_adc,
            &mv
        );

    if (err != 0)
    {
        return err;
    }

    mv *= BATTERY_ADC_MULTIPLIER;
    
    mv = (int32_t)(((int64_t)mv * BATTERY_CAL_GAIN_NUM +
                   BATTERY_CAL_GAIN_DEN / 2) / BATTERY_CAL_GAIN_DEN)
         + BATTERY_CAL_OFFSET_MV;
    battery_last_mv = mv;

    return (int)mv;
}

static uint8_t battery_percent_from_mv(int mv)
{
    static const struct { int mv; uint8_t percent; } curve[] = {
        {3000, 0}, {3300, 1}, {3500, 5}, {3600, 10}, {3700, 25},
        {3800, 45}, {3900, 65}, {4000, 80}, {4100, 90}, {4200, 100}
    };
    if (mv <= curve[0].mv) { return 0; }
    for (size_t i = 1; i < ARRAY_SIZE(curve); i++) {
        if (mv < curve[i].mv) {
            return curve[i - 1].percent +
                (mv - curve[i - 1].mv) *
                (curve[i].percent - curve[i - 1].percent) /
                (curve[i].mv - curve[i - 1].mv);
        }
    }
    return 100;
}

static bool battery_measure_zigbee_values(
    zb_uint8_t *battery_voltage,
    zb_uint8_t *battery_percentage_remaining)
{
    int mv =
        battery_measure_mv();

    if (mv <= 0)
    {
        *battery_voltage =
            ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;

        *battery_percentage_remaining =
            ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;

        return false;
    }

    int voltage_zigbee =
        (mv + 50) / 100;

    if (voltage_zigbee > 254)
    {
        voltage_zigbee = 254;
    }

    *battery_voltage =
        (zb_uint8_t)voltage_zigbee;

    uint8_t percent =
        battery_percent_from_mv(
            mv
        );

    *battery_percentage_remaining =
        (zb_uint8_t)(
            percent * 2
        );

    return true;
}

static void send_battery_report(
    zb_bufid_t bufid)
{
    zb_uint16_t destination =
        0x0000;

    zb_uint8_t *ptr;

    if (!ZB_JOINED())
    {
        zb_buf_free(
            bufid
        );

        return;
    }

    ptr =
        ZB_ZCL_START_PACKET(
            bufid
        );

    ZB_ZCL_CONSTRUCT_GENERAL_COMMAND_REQ_FRAME_CONTROL_EXT(
        ptr,

        ZB_ZCL_NOT_MANUFACTURER_SPECIFIC,

        ZB_ZCL_FRAME_DIRECTION_TO_CLI,

        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );

    ZB_ZCL_CONSTRUCT_COMMAND_HEADER(
        ptr,

        ZB_ZCL_GET_SEQ_NUM(),

        ZB_ZCL_CMD_REPORT_ATTRIB
    );

    ZB_ZCL_PACKET_PUT_DATA16_VAL(
        ptr,

        ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID
    );

    ZB_ZCL_PACKET_PUT_DATA8(
        ptr,

        ZB_ZCL_ATTR_TYPE_U8
    );

    ZB_ZCL_PACKET_PUT_DATA8(
        ptr,

        dev_ctx.battery_voltage
    );

    ZB_ZCL_PACKET_PUT_DATA16_VAL(
        ptr,

        ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID
    );

    ZB_ZCL_PACKET_PUT_DATA8(
        ptr,

        ZB_ZCL_ATTR_TYPE_U8
    );

    ZB_ZCL_PACKET_PUT_DATA8(
        ptr,

        dev_ctx.battery_percentage_remaining
    );

    ZB_ZCL_FINISH_PACKET(
        bufid,
        ptr
    )

    ZB_ZCL_SEND_COMMAND_SHORT(
        bufid,

        destination,

        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,

        1,

        SWITCH_ENDPOINT,

        ZB_AF_HA_PROFILE_ID,

        ZB_ZCL_CLUSTER_ID_POWER_CONFIG,

        NULL
    );
}

static void send_toggle(
    zb_bufid_t bufid)
{
    zb_uint16_t destination =
        0x0000;

    if (!ZB_JOINED())
    {
        zb_buf_free(
            bufid
        );

        return;
    }

    ZB_ZCL_ON_OFF_SEND_REQ(
        bufid,

        destination,

        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,

        1,

        SWITCH_ENDPOINT,

        ZB_AF_HA_PROFILE_ID,

        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,

        ZB_ZCL_CMD_ON_OFF_TOGGLE_ID,

        NULL
    );
}

static void zigbee_button_action(zb_uint8_t unused, zb_uint16_t unused2)
{
    ARG_UNUSED(unused);
    ARG_UNUSED(unused2);
    user_input_indicate();
    if (zb_buf_get_out_delayed(send_toggle) != RET_OK) {
        zigbee_buffer_errors++;
    }
}

static void battery_update_zigbee(zb_uint8_t valid, zb_uint16_t packed)
{
    ARG_UNUSED(valid);
    dev_ctx.battery_voltage = (zb_uint8_t)(packed >> 8);
    dev_ctx.battery_percentage_remaining = (zb_uint8_t)packed;
    if (ZB_JOINED() && zb_buf_get_out_delayed(send_battery_report) != RET_OK) {
        zigbee_buffer_errors++;
    }
}

static void battery_periodic_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    zb_uint8_t voltage, percentage;
    bool valid = battery_measure_zigbee_values(&voltage, &percentage);
    if (!valid) { battery_adc_errors++; }
    zb_uint16_t packed = ((zb_uint16_t)voltage << 8) | percentage;
    if (ZB_SCHEDULE_APP_CALLBACK2(battery_update_zigbee, valid, packed) != RET_OK) {
        battery_schedule_errors++;
    }
    k_work_reschedule(&battery_work, K_HOURS(BATTERY_REPORT_INTERVAL_HOURS));
}

static void debounce_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    int state = gpio_pin_get(gpio0, BUTTON_PIN);
    if (state < 0 || state == last_button_state) { return; }
    last_button_state = state;
    if (ZB_SCHEDULE_APP_CALLBACK2(zigbee_button_action, 0, 0) != RET_OK) {
        toggle_schedule_errors++;
    }
}

static void button_interrupt(
    const struct device *dev,

    struct gpio_callback *cb,

    uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(
        &debounce_work,

        K_MSEC(
            DEBOUNCE_TIME_MS
        )
    );
}

static int button_init(void)
{
    int err;

    if (!device_is_ready(gpio0))
    {
        return -ENODEV;
    }

    err =
        gpio_pin_configure(
            gpio0,

            BUTTON_PIN,

            GPIO_INPUT |
            GPIO_PULL_UP
        );

    if (err != 0)
    {
        return err;
    }

    last_button_state =
        gpio_pin_get(
            gpio0,
            BUTTON_PIN
        );

    k_work_init_delayable(
        &debounce_work,

        debounce_handler
    );

    gpio_init_callback(
        &button_gpio_cb,

        button_interrupt,

        BIT(BUTTON_PIN)
    );

    err =
        gpio_add_callback(
            gpio0,

            &button_gpio_cb
        );

    if (err != 0)
    {
        return err;
    }

    return gpio_pin_interrupt_configure(
        gpio0,

        BUTTON_PIN,

        GPIO_INT_EDGE_BOTH
    );
}

static void zigbee_attributes_init(void)
{
    dev_ctx.basic_attr.zcl_version =
        ZB_ZCL_VERSION;

    dev_ctx.basic_attr.power_source =
        ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;

    dev_ctx.identify_attr.identify_time =
        ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

    dev_ctx.battery_voltage =
        ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;

    dev_ctx.battery_percentage_remaining =
        ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;

    (void)battery_measure_zigbee_values(
        &dev_ctx.battery_voltage,
        &dev_ctx.battery_percentage_remaining
    );
}

void zboss_signal_handler(zb_bufid_t bufid)
{
    zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, NULL);
    (void)zigbee_default_signal_handler(bufid);
    
    if (!boot_report_sent &&
        (signal == ZB_BDB_SIGNAL_STEERING || signal == ZB_BDB_SIGNAL_DEVICE_REBOOT) &&
        ZB_GET_APP_SIGNAL_STATUS(bufid) == RET_OK && ZB_JOINED()) {
        if (zb_buf_get_out_delayed(send_battery_report) == RET_OK) {
            boot_report_sent = true;
        } else {
            zigbee_buffer_errors++;
        }
    }
    if (bufid) { zb_buf_free(bufid); }
}

int main(void)
{
    int err;

    err =
        button_init();

    if (err != 0)
    {
        return 0;
    }

    err =
        battery_adc_init();

    if (err != 0)
    {
        return 0;
    }

    zigbee_attributes_init();

    ZB_AF_REGISTER_DEVICE_CTX(
        &zigbutton_ctx
    );

    zigbee_configure_sleepy_behavior(
        true
    );

    if (
        IS_ENABLED(
            CONFIG_RAM_POWER_DOWN_LIBRARY
        )
    )
    {
        power_down_unused_ram();
    }

    k_work_init_delayable(&battery_work, battery_periodic_handler);
    k_work_schedule(&battery_work, K_HOURS(BATTERY_REPORT_INTERVAL_HOURS));
    zigbee_enable();

    return 0;
}