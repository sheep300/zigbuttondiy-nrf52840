/*
 * ZigButtonDIY
 *
 * VERSION TEST SCHEDULER ZBOSS + BATTERIE CR2032
 *
 * nRF52840 + Zigbee
 *
 * P0.17 :
 * contact sec bistable vers GND
 *
 * Batterie :
 *
 * CR2032 sur RAW / GND
 *
 * mesure interne :
 *
 *     VDDH -> SAADC VDDHDIV5
 *
 * donc :
 *
 *     tension batterie =
 *     tension ADC x 5
 *
 * La conversion tension -> %
 * est faite ICI dans le firmware.
 *
 * Zigbee2MQTT reçoit directement :
 *
 * BatteryPercentageRemaining
 *
 * Aucun heartbeat.
 * Aucun watchdog.
 *
 * Les transmissions déclenchées par le bouton
 * passent par le scheduler ZBOSS.
 */

#define ZB_HA_DEFINE_DEVICE_DIMMER_SWITCH

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


/*
 * ---------------------------------------------------------
 * CONFIGURATION
 * ---------------------------------------------------------
 */

#define BUTTON_PIN       17
#define SWITCH_ENDPOINT  1
#define DEBOUNCE_TIME_MS 35


/*
 * VDDHDIV5 :
 *
 * le SAADC reçoit VDDH / 5.
 *
 * Il faut donc multiplier la tension
 * convertie par 5 pour retrouver VBAT.
 */

#define BATTERY_ADC_MULTIPLIER 5


/*
 * ---------------------------------------------------------
 * GPIO
 * ---------------------------------------------------------
 */

static const struct device *gpio0 =
    DEVICE_DT_GET(DT_NODELABEL(gpio0));

static struct gpio_callback button_gpio_cb;
static struct k_work_delayable debounce_work;

static int last_button_state;


/*
 * ---------------------------------------------------------
 * ADC
 * ---------------------------------------------------------
 */

static const struct adc_dt_spec battery_adc =
    ADC_DT_SPEC_GET_BY_IDX(
        DT_PATH(zephyr_user),
        0
    );


/*
 * ---------------------------------------------------------
 * CONTEXTE ZIGBEE
 * ---------------------------------------------------------
 */

struct zigbutton_device_ctx
{
    zb_zcl_basic_attrs_t basic_attr;
    zb_zcl_identify_attrs_t identify_attr;

    /*
     * 0x0020
     *
     * unité = 100 mV
     */
    zb_uint8_t battery_voltage;

    /*
     * 0x0021
     *
     * unité = 0,5 %
     *
     * 200 = 100 %
     */
    zb_uint8_t battery_percentage_remaining;
};

static struct zigbutton_device_ctx dev_ctx;


/*
 * ---------------------------------------------------------
 * BASIC
 * ---------------------------------------------------------
 */

ZB_ZCL_DECLARE_BASIC_SERVER_ATTRIB_LIST(
    basic_attr_list,
    &dev_ctx.basic_attr.zcl_version,
    &dev_ctx.basic_attr.power_source
);


/*
 * ---------------------------------------------------------
 * IDENTIFY
 * ---------------------------------------------------------
 */

ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(
    identify_attr_list,
    &dev_ctx.identify_attr.identify_time
);


/*
 * ---------------------------------------------------------
 * POWER CONFIGURATION
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * CLUSTERS
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * SIMPLE DESCRIPTOR
 * ---------------------------------------------------------
 */

ZB_DECLARE_SIMPLE_DESC(3, 5);

ZB_AF_SIMPLE_DESC_TYPE(3, 5)
simple_desc_zigbutton_ep =
{
    SWITCH_ENDPOINT,

    ZB_AF_HA_PROFILE_ID,

    ZB_HA_DIMMER_SWITCH_DEVICE_ID,

    ZB_HA_DEVICE_VER_DIMMER_SWITCH,

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


/*
 * ---------------------------------------------------------
 * ENDPOINT
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * INITIALISATION ADC
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * MESURE VDDH / BATTERIE
 * ---------------------------------------------------------
 */

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


    /*
     * Première conversion.
     */

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


    /*
     * Conversion valeur brute -> mV
     * à l'entrée du SAADC.
     */

    err =
        adc_raw_to_millivolts_dt(
            &battery_adc,
            &mv
        );


    if (err != 0)
    {
        return err;
    }


    /*
     * L'entrée interne est VDDH / 5.
     *
     * Exemple :
     *
     * VDDH = 3,00 V
     *
     * SAADC voit environ :
     *
     * 600 mV
     *
     * donc :
     *
     * 600 x 5 = 3000 mV
     */

    mv *= BATTERY_ADC_MULTIPLIER;


    return (int)mv;
}


/*
 * ---------------------------------------------------------
 * CR2032 : TENSION -> POURCENTAGE
 * ---------------------------------------------------------
 *
 * Il ne s'agit PAS d'une simple règle de trois.
 *
 * Une pile bouton CR2032 reste longtemps
 * autour de 2,8 à 3,0 V puis chute davantage
 * en fin de vie.
 *
 * Cette table est volontairement raisonnable
 * pour notre usage d'interrupteur Zigbee.
 */

static uint8_t battery_percent_from_mv(
    int mv)
{
    /*
     * 3,00 V et plus :
     * pleine.
     */

    if (mv >= 3000)
    {
        return 100;
    }


    /*
     * 2,90 -> 3,00 V
     *
     * 90 -> 100 %
     */

    if (mv >= 2900)
    {
        return
            90 +
            ((mv - 2900) * 10) / 100;
    }


    /*
     * 2,80 -> 2,90 V
     *
     * 75 -> 90 %
     */

    if (mv >= 2800)
    {
        return
            75 +
            ((mv - 2800) * 15) / 100;
    }


    /*
     * 2,70 -> 2,80 V
     *
     * 55 -> 75 %
     */

    if (mv >= 2700)
    {
        return
            55 +
            ((mv - 2700) * 20) / 100;
    }


    /*
     * 2,60 -> 2,70 V
     *
     * 35 -> 55 %
     */

    if (mv >= 2600)
    {
        return
            35 +
            ((mv - 2600) * 20) / 100;
    }


    /*
     * 2,50 -> 2,60 V
     *
     * 20 -> 35 %
     */

    if (mv >= 2500)
    {
        return
            20 +
            ((mv - 2500) * 15) / 100;
    }


    /*
     * 2,40 -> 2,50 V
     *
     * 10 -> 20 %
     */

    if (mv >= 2400)
    {
        return
            10 +
            ((mv - 2400) * 10) / 100;
    }


    /*
     * 2,20 -> 2,40 V
     *
     * 0 -> 10 %
     */

    if (mv >= 2200)
    {
        return
            ((mv - 2200) * 10) / 200;
    }


    return 0;
}


/*
 * ---------------------------------------------------------
 * PREPARATION DES VALEURS BATTERIE ZIGBEE
 * ---------------------------------------------------------
 *
 * Cette fonction fait uniquement la mesure ADC et le calcul.
 * Elle ne lance aucune opération ZBOSS.
 */

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


    /*
     * -----------------------------------------------------
     * BatteryVoltage
     * -----------------------------------------------------
     *
     * Zigbee :
     *
     * 1 unité = 100 mV
     *
     * 2,99 V -> 30
     */

    int voltage_zigbee =
        (mv + 50) / 100;


    if (voltage_zigbee > 254)
    {
        voltage_zigbee = 254;
    }


    *battery_voltage =
        (zb_uint8_t)voltage_zigbee;


    /*
     * -----------------------------------------------------
     * BatteryPercentageRemaining
     * -----------------------------------------------------
     *
     * Notre firmware connaît le type de pile :
     *
     * CR2032.
     *
     * C'est donc ICI que le %
     * est calculé.
     */

    uint8_t percent =
        battery_percent_from_mv(
            mv
        );


    /*
     * Format Zigbee :
     *
     * 1 unité = 0,5 %
     *
     * donc :
     *
     * 100 % = 200
     */

    *battery_percentage_remaining =
        (zb_uint8_t)(
            percent * 2
        );


    return true;
}


/*
 * ---------------------------------------------------------
 * REPORT BATTERIE
 * ---------------------------------------------------------
 */

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


    /*
     * BatteryVoltage 0x0020
     */

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


    /*
     * BatteryPercentageRemaining 0x0021
     */

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


/*
 * ---------------------------------------------------------
 * TOGGLE
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * ACTION BOUTON DANS LE SCHEDULER ZBOSS
 * ---------------------------------------------------------
 *
 * param :
 *   0 = mesure batterie invalide
 *   1 = mesure batterie valide
 *
 * battery_data :
 *   octet haut = BatteryVoltage
 *   octet bas  = BatteryPercentageRemaining
 *
 * Le passage des deux valeurs dans le paramètre 16 bits
 * évite de partager un buffer mutable entre la workqueue
 * Zephyr et le scheduler ZBOSS.
 */

static void zigbee_button_action(
    zb_uint8_t param,
    zb_uint16_t battery_data)
{
    bool battery_valid =
        (param != 0);


    /*
     * Nous sommes maintenant dans le scheduler ZBOSS.
     */

    user_input_indicate();


    if (battery_valid)
    {
        dev_ctx.battery_voltage =
            (zb_uint8_t)(
                (battery_data >> 8) & 0xFF
            );


        dev_ctx.battery_percentage_remaining =
            (zb_uint8_t)(
                battery_data & 0xFF
            );
    }


    /*
     * Priorité au Toggle :
     * c'est l'action utilisateur importante.
     */

    zb_buf_get_out_delayed(
        send_toggle
    );


    /*
     * Puis le report batterie, uniquement si la mesure ADC
     * était valide.
     */

    if (battery_valid)
    {
        zb_buf_get_out_delayed(
            send_battery_report
        );
    }
}


/*
 * ---------------------------------------------------------
 * DEBOUNCE
 * ---------------------------------------------------------
 */

static void debounce_handler(
    struct k_work *work)
{
    ARG_UNUSED(work);


    int state =
        gpio_pin_get(
            gpio0,
            BUTTON_PIN
        );


    if (state == last_button_state)
    {
        return;
    }


    last_button_state =
        state;


    /*
     * Côté Zephyr uniquement :
     * mesure ADC et calcul des deux attributs batterie.
     */

    zb_uint8_t battery_voltage;
    zb_uint8_t battery_percentage_remaining;


    bool battery_valid =
        battery_measure_zigbee_values(
            &battery_voltage,
            &battery_percentage_remaining
        );


    /*
     * On transporte les deux octets directement dans le
     * paramètre utilisateur du scheduler ZBOSS.
     */

    zb_uint16_t battery_data =
        ((zb_uint16_t)battery_voltage << 8) |
        (zb_uint16_t)battery_percentage_remaining;


    /*
     * C'est volontairement la seule entrée vers ZBOSS
     * depuis cette workqueue Zephyr : l'API du scheduler.
     */

    (void)ZB_SCHEDULE_APP_CALLBACK2(
        zigbee_button_action,
        battery_valid ? 1 : 0,
        battery_data
    );
}


/*
 * ---------------------------------------------------------
 * INTERRUPTION GPIO
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * INITIALISATION GPIO
 * ---------------------------------------------------------
 */

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


/*
 * ---------------------------------------------------------
 * ATTRIBUTS ZIGBEE
 * ---------------------------------------------------------
 */

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


    /*
     * Première lecture locale.
     *
     * Aucun report Zigbee ici.
     * Le stack n'est pas encore lancé à ce moment-là.
     */

    (void)battery_measure_zigbee_values(
        &dev_ctx.battery_voltage,
        &dev_ctx.battery_percentage_remaining
    );
}


/*
 * ---------------------------------------------------------
 * ZBOSS SIGNAL HANDLER
 * ---------------------------------------------------------
 */

void zboss_signal_handler(
    zb_bufid_t bufid)
{
    (void)zigbee_default_signal_handler(
        bufid
    );


    if (bufid)
    {
        zb_buf_free(
            bufid
        );
    }
}


/*
 * ---------------------------------------------------------
 * MAIN
 * ---------------------------------------------------------
 */

int main(void)
{
    int err;


    /*
     * GPIO
     */

    err =
        button_init();


    if (err != 0)
    {
        return 0;
    }


    /*
     * ADC
     */

    err =
        battery_adc_init();


    if (err != 0)
    {
        return 0;
    }


    /*
     * Attributs Zigbee
     */

    zigbee_attributes_init();


    /*
     * Device Zigbee
     */

    ZB_AF_REGISTER_DEVICE_CTX(
        &zigbutton_ctx
    );


    /*
     * Sleepy End Device.
     */

    zigbee_configure_sleepy_behavior(
        true
    );


    /*
     * Coupure RAM inutilisée.
     */

    if (
        IS_ENABLED(
            CONFIG_RAM_POWER_DOWN_LIBRARY
        )
    )
    {
        power_down_unused_ram();
    }


    /*
     * Aucun heartbeat.
     * Aucun watchdog.
     */

    zigbee_enable();


    return 0;
}