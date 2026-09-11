#include "app_button.h"
#include <wiced_bt_event.h>
#include <wiced_platform.h>
#include <wiced_timer.h>
#include <hal/wiced_hal_gpio.h>

#include "hci_control_hfp_ag.h"
#include "hci_control_hfp_hf.h"

#define APP_BUTTON_CLICK_WINDOW_MS 600

typedef enum
{
    APP_BUTTON_ACTION_CALL,
    APP_BUTTON_ACTION_CONNECT_INTERCOM,
    APP_BUTTON_ACTION_TOGGLE_INTERCOM_AUDIO
} app_button_action_t;

static wiced_timer_t app_button_timer;
static volatile wiced_bool_t app_button_down;
static volatile uint8_t app_button_click_count;

static const app_button_action_t call_action =
    APP_BUTTON_ACTION_CALL;
static const app_button_action_t connect_action =
    APP_BUTTON_ACTION_CONNECT_INTERCOM;
static const app_button_action_t audio_action =
    APP_BUTTON_ACTION_TOGGLE_INTERCOM_AUDIO;

static int app_button_execute(void *data)
{
    const app_button_action_t action =
        *(const app_button_action_t *)data;

    WICED_BT_TRACE("SW3 execute action=%u\n", action);

    switch (action)
    {
        case APP_BUTTON_ACTION_CALL:
            hci_control_hf_button_call_action();
            break;

        case APP_BUTTON_ACTION_CONNECT_INTERCOM:
            hci_control_ag_connect_corider();
            break;

        case APP_BUTTON_ACTION_TOGGLE_INTERCOM_AUDIO:
            hci_control_ag_toggle_corider_audio();
            break;

        default:
            break;
    }

    return 0;
}

static void app_button_timer_callback(TIMER_PARAM_TYPE parameter)
{
    const app_button_action_t *action;

    (void)parameter;

    if (app_button_click_count == 1)
    {
        action = &call_action;
    }
    else if (app_button_click_count == 2)
    {
        action = &connect_action;
    }
    else
    {
        app_button_click_count = 0;
        return;
    }

    WICED_BT_TRACE("SW3 click count=%u\n", app_button_click_count);
    app_button_click_count = 0;

    if (!wiced_app_event_serialize(app_button_execute, (void *)action))
    {
        WICED_BT_TRACE("Failed to queue SW3 action\n");
    }
}

static void app_button_gpio_callback(void *user_data, uint8_t pin)
{
    uint32_t pin_state;

    (void)user_data;
    (void)pin;

    pin_state =
        wiced_hal_gpio_get_pin_input_status(WICED_BUTTON3);

    WICED_BT_TRACE("SW3 edge pin=%u state=%u\n", pin, pin_state);

    if (pin_state == GPIO_PIN_OUTPUT_LOW)
    {
        /* Ignore switch bounce after the initial falling edge. */
        if (app_button_down)
        {
            return;
        }

        app_button_down = WICED_TRUE;
        return;
    }

    /* Ignore switch bounce after the initial rising edge. */
    if (!app_button_down)
    {
        return;
    }

    app_button_down = WICED_FALSE;
    if (app_button_click_count < 3)
    {
        app_button_click_count++;
    }

    wiced_stop_timer(&app_button_timer);

    if (app_button_click_count == 3)
    {
        WICED_BT_TRACE("SW3 click count=3\n");
        app_button_click_count = 0;

        if (!wiced_app_event_serialize(app_button_execute, (void *)&audio_action))
        {
            WICED_BT_TRACE("Failed to queue SW3 action\n");
        }
    }
    else
    {
        wiced_start_timer(&app_button_timer, APP_BUTTON_CLICK_WINDOW_MS);
    }
}

void app_button_init(void)
{
    app_button_down = WICED_FALSE;
    app_button_click_count = 0;

    wiced_init_timer(
        &app_button_timer,
        app_button_timer_callback,
        0,
        WICED_MILLI_SECONDS_TIMER);

    wiced_hal_gpio_register_pin_for_interrupt(
                WICED_BUTTON3,
        app_button_gpio_callback,
        NULL);

    wiced_hal_gpio_configure_pin(
                WICED_BUTTON3,
                WICED_GPIO_BUTTON_SETTINGS(GPIO_EN_INT_BOTH_EDGE),
        GPIO_PIN_OUTPUT_HIGH);

        WICED_BT_TRACE("SW3 initialized gpio=%u state=%u\n",
                                     WICED_BUTTON3,
                                     wiced_hal_gpio_get_pin_input_status(WICED_BUTTON3));
}
