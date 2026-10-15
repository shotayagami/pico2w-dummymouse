#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "btstack.h"
#include "dummymouse.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#ifndef REPORT_INTERVAL_MS
#define REPORT_INTERVAL_MS (60u * 1000u)
#endif

#define STATUS_LED_PIN 15
#define CONTROL_BUTTON_PIN 14
#define PILOT_LED_PIN 16
#define BUTTON_DEBOUNCE_MS 40u
#define STATUS_FAST_BLINK_INTERVAL_MS 250u
#define STATUS_FAST_BLINK_DURATION_MS 125u
#define STATUS_SLOW_BLINK_INTERVAL_MS 2000u
#define STATUS_SLOW_BLINK_DURATION_MS 100u

typedef struct {
    int8_t dx;
    int8_t dy;
} mouse_direction_t;

static const mouse_direction_t mouse_directions[] = {
    {0, -1},  // up
    {0, 1},   // down
    {-1, 0},  // left
    {1, 0},   // right
};
static const char *const mouse_direction_names[] = {"up", "down", "left", "right"};
static uint8_t next_direction;

static hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
static btstack_timer_source_t mouse_timer;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static bool btstack_ready;
static bool can_send_now;
static bool mouse_report_pending;
static bool notifications_enabled;
static bool mouse_paused;
static bool button_last_reading;
static bool button_stable_reading;
static uint32_t button_last_change_ms;

static const uint8_t advertising_data[] = {
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xc2, 0x03,
    0x0b, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'D', 'u', 'm', 'm', 'y', 'M', 'o', 'u', 's', 'e',
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
    0x12, 0x18,
};

// Report ID 1 matches the first input report in BTstack's HIDS GATT template.
static const uint8_t mouse_report_descriptor[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x09, 0x01,       //   Usage (Pointer)
    0xa1, 0x00,       //   Collection (Physical)
    0x05, 0x09,       //     Usage Page (Button)
    0x19, 0x01,       //     Usage Minimum (Button 1)
    0x29, 0x03,       //     Usage Maximum (Button 3)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x03,       //     Report Count (3)
    0x75, 0x01,       //     Report Size (1)
    0x81, 0x02,       //     Input (Data, Variable, Absolute)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x05,       //     Report Size (5)
    0x81, 0x03,       //     Input (Constant, Variable, Absolute)
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7f,       //     Logical Maximum (127)
    0x75, 0x08,       //     Report Size (8)
    0x95, 0x02,       //     Report Count (2)
    0x81, 0x06,       //     Input (Data, Variable, Relative)
    0xc0,
    0xc0,
};

static void start_advertising(void) {
    if (!btstack_ready) {
        return;
    }
    gap_advertisements_set_data(sizeof(advertising_data), (uint8_t *)advertising_data);
    gap_advertisements_enable(1);
    printf("Advertising as DummyMouse; pair from the host Bluetooth settings.\n");
}

static void schedule_mouse_report(void) {
    if (mouse_paused || !notifications_enabled || connection_handle == HCI_CON_HANDLE_INVALID) {
        return;
    }
    btstack_run_loop_set_timer(&mouse_timer, REPORT_INTERVAL_MS);
    btstack_run_loop_add_timer(&mouse_timer);
}

static void set_mouse_paused(bool paused) {
    if (mouse_paused == paused) {
        return;
    }

    mouse_paused = paused;
    mouse_report_pending = false;
    can_send_now = false;
    btstack_run_loop_remove_timer(&mouse_timer);

    if (mouse_paused) {
        printf("Mouse movement paused.\n");
    } else {
        printf("Mouse movement resumed.\n");
        schedule_mouse_report();
    }
}

static void poll_control_button(void) {
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    const bool reading = gpio_get(CONTROL_BUTTON_PIN) == 0;

    if (reading != button_last_reading) {
        button_last_reading = reading;
        button_last_change_ms = now;
    }

    if ((uint32_t)(now - button_last_change_ms) >= BUTTON_DEBOUNCE_MS &&
        button_stable_reading != button_last_reading) {
        button_stable_reading = button_last_reading;
        if (button_stable_reading) {
            set_mouse_paused(!mouse_paused);
        }
    }
}

static void update_status_led(void) {
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    bool led_on = false;

    if (!mouse_paused && connection_handle != HCI_CON_HANDLE_INVALID) {
        if (notifications_enabled) {
            led_on = (now % STATUS_SLOW_BLINK_INTERVAL_MS) < STATUS_SLOW_BLINK_DURATION_MS;
        } else {
            led_on = (now % STATUS_FAST_BLINK_INTERVAL_MS) < STATUS_FAST_BLINK_DURATION_MS;
        }
    }
    gpio_put(STATUS_LED_PIN, led_on);
}

static void mouse_timer_handler(btstack_timer_source_t *timer) {
    (void)timer;
    if (mouse_paused || !notifications_enabled) {
        return;
    }
    if (connection_handle == HCI_CON_HANDLE_INVALID) {
        printf("Mouse timer fired without an active connection.\n");
        return;
    }
    mouse_report_pending = true;
    const uint8_t status = hids_device_request_can_send_now_event(connection_handle);
    printf("Mouse timer fired; request-can-send status=0x%02x.\n", status);
}

static void send_mouse_report_if_ready(void) {
    if (mouse_paused || !can_send_now || !mouse_report_pending ||
        connection_handle == HCI_CON_HANDLE_INVALID) {
        return;
    }

    const mouse_direction_t direction = mouse_directions[next_direction];
    const uint8_t report[] = {0, (uint8_t)direction.dx, (uint8_t)direction.dy};
    const char *name = mouse_direction_names[next_direction];
    can_send_now = false;
    mouse_report_pending = false;
    const uint8_t status = hids_device_send_input_report(connection_handle, report, sizeof(report));
    printf("Mouse input report sent; status=0x%02x, direction=%s, dx=%d, dy=%d.\n",
           status, name, (int8_t)report[1], (int8_t)report[2]);
    if (status == ERROR_CODE_SUCCESS) {
        next_direction = (uint8_t)((next_direction + 1) % (sizeof(mouse_directions) / sizeof(mouse_directions[0])));
    }
    schedule_mouse_report();
}

static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                btstack_ready = true;
                start_advertising();
            }
            break;

        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE &&
                hci_subevent_le_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
                connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                gap_advertisements_enable(0);
                printf("Bluetooth connected; handle=0x%04x, waiting for HID input subscription.\n",
                       connection_handle);
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            btstack_run_loop_remove_timer(&mouse_timer);
            connection_handle = HCI_CON_HANDLE_INVALID;
            notifications_enabled = false;
            can_send_now = false;
            mouse_report_pending = false;
            start_advertising();
            printf("Bluetooth disconnected; advertising again.\n");
            break;

        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)) {
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    connection_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                    if (hids_subevent_input_report_enable_get_enable(packet)) {
                        printf("HID input notifications enabled.\n");
                        notifications_enabled = true;
                        schedule_mouse_report();
                    } else {
                        printf("HID input notifications disabled.\n");
                        notifications_enabled = false;
                        btstack_run_loop_remove_timer(&mouse_timer);
                        mouse_report_pending = false;
                        can_send_now = false;
                    }
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    can_send_now = true;
                    printf("BTstack reports CAN_SEND_NOW.\n");
                    send_mouse_report_if_ready();
                    break;
                default:
                    break;
            }
            break;

        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            printf("Confirming BLE Just Works pairing.\n");
            break;

        case SM_EVENT_PAIRING_COMPLETE:
            printf("BLE pairing %s.\n",
                   sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS
                       ? "complete" : "failed");
            break;

        default:
            break;
    }
}

int main(void) {
    stdio_init_all();
    gpio_init(PILOT_LED_PIN);
    gpio_set_dir(PILOT_LED_PIN, GPIO_OUT);
    gpio_put(PILOT_LED_PIN, true);
    sleep_ms(1500);
    printf("Starting DummyMouse on Pico 2 W.\n");

    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, false);
    gpio_init(CONTROL_BUTTON_PIN);
    gpio_set_dir(CONTROL_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(CONTROL_BUTTON_PIN);
    button_last_reading = gpio_get(CONTROL_BUTTON_PIN) == 0;
    button_stable_reading = button_last_reading;
    button_last_change_ms = to_ms_since_boot(get_absolute_time());

    if (cyw43_arch_init()) {
        printf("CYW43 initialization failed.\n");
        return 1;
    }

    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    att_server_init(profile_data, NULL, NULL);
    device_information_service_server_init();
    device_information_service_server_set_pnp_id(2, 0x2e8a, 0x0001, 0x0100);
    hids_device_init(0, mouse_report_descriptor, sizeof(mouse_report_descriptor));

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    hids_device_register_packet_handler(packet_handler);

    btstack_run_loop_set_timer_handler(&mouse_timer, &mouse_timer_handler);
    hci_power_control(HCI_POWER_ON);

    while (true) {
        cyw43_arch_poll();
        poll_control_button();
        update_status_led();
        tight_loop_contents();
    }
}
