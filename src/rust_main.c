#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "btstack.h"
#include "dummymouse.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

typedef btstack_timer_source_t rust_timer_source_t;

static btstack_packet_callback_registration_t hci_registration;
static btstack_packet_callback_registration_t sm_registration;
static void (*rust_packet_handler)(uint8_t, uint16_t, uint8_t *, uint16_t);

extern int rust_app_init(void);
extern void rust_app_poll(void);

void dm_stdio_init(void) {
    stdio_init_all();
    gpio_init(16);
    gpio_set_dir(16, GPIO_OUT);
    gpio_put(16, true);
    sleep_ms(1500);
    printf("Starting Rust DummyMouse on Pico 2 W.\n");
}
void dm_init_gpio(uint32_t pin, bool output, bool high, bool pull_up) {
    gpio_init(pin); gpio_set_dir(pin, output); if (pull_up) gpio_pull_up(pin); gpio_put(pin, high);
}
bool dm_gpio_get(uint32_t pin) { return gpio_get(pin); }
void dm_gpio_put(uint32_t pin, bool high) { gpio_put(pin, high); }
int dm_cyw43_init(void) { return cyw43_arch_init(); }
void dm_poll(void) { cyw43_arch_poll(); tight_loop_contents(); }
uint32_t dm_now_ms(void) { return to_ms_since_boot(get_absolute_time()); }
void dm_log(const char *format, int32_t a, int32_t b, int32_t c) {
    printf(format, a, b, c);
    size_t len = 0;
    while (format[len] != '\0') ++len;
    if (len == 0 || format[len - 1] != '\n') printf("\n");
}
void dm_timer_schedule(rust_timer_source_t *timer, uint32_t delay_ms) {
    btstack_run_loop_set_timer((btstack_timer_source_t *)timer, delay_ms);
    btstack_run_loop_add_timer((btstack_timer_source_t *)timer);
}
void dm_timer_remove(rust_timer_source_t *timer) { btstack_run_loop_remove_timer((btstack_timer_source_t *)timer); }
uint8_t dm_request_can_send(uint16_t handle) { return hids_device_request_can_send_now_event(handle); }
uint8_t dm_send_report(uint16_t handle, const uint8_t *report, uint16_t len) {
    return hids_device_send_input_report(handle, report, len);
}
void dm_just_works_confirm(uint16_t handle) { sm_just_works_confirm(handle); }
uint8_t dm_event_type(const uint8_t *p) { return hci_event_packet_get_type(p); }
uint8_t dm_event_state(const uint8_t *p) { return btstack_event_state_get_state(p); }
uint8_t dm_event_subevent(const uint8_t *p) {
    if (hci_event_packet_get_type(p) == HCI_EVENT_HIDS_META)
        return hci_event_hids_meta_get_subevent_code(p);
    return hci_event_le_meta_get_subevent_code(p);
}
uint8_t dm_event_status(const uint8_t *p) {
    switch (hci_event_packet_get_type(p)) {
        case HCI_EVENT_LE_META: return hci_subevent_le_connection_complete_get_status(p);
        case SM_EVENT_PAIRING_COMPLETE: return sm_event_pairing_complete_get_status(p);
        default: return 0;
    }
}
uint16_t dm_event_handle(const uint8_t *p) {
    switch (hci_event_packet_get_type(p)) {
        case HCI_EVENT_LE_META: return hci_subevent_le_connection_complete_get_connection_handle(p);
        case HCI_EVENT_HIDS_META: return hids_subevent_input_report_enable_get_con_handle(p);
        case SM_EVENT_JUST_WORKS_REQUEST: return sm_event_just_works_request_get_handle(p);
        default: return 0xffff;
    }
}
bool dm_event_hids_enabled(const uint8_t *p) { return hids_subevent_input_report_enable_get_enable(p); }

static void bridge_packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (rust_packet_handler) rust_packet_handler(type, channel, packet, size);
}
void dm_register_handlers(void (*callback)(uint8_t, uint16_t, uint8_t *, uint16_t),
                          rust_timer_source_t *timer,
                          void (*timer_callback)(rust_timer_source_t *)) {
    rust_packet_handler = callback;
    hci_registration.callback = bridge_packet_handler;
    sm_registration.callback = bridge_packet_handler;
    hci_add_event_handler(&hci_registration);
    sm_add_event_handler(&sm_registration);
    hids_device_register_packet_handler(bridge_packet_handler);
    btstack_run_loop_set_timer_handler((btstack_timer_source_t *)timer,
                                       (void (*)(btstack_timer_source_t *))timer_callback);
    hci_power_control(HCI_POWER_ON);
}
void dm_btstack_init(void) {
    l2cap_init(); sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    att_server_init(profile_data, NULL, NULL);
    device_information_service_server_init();
    device_information_service_server_set_pnp_id(2, 0x2e8a, 0x0001, 0x0100);
    static const uint8_t report_descriptor[] = {
        0x05,0x01,0x09,0x02,0xa1,0x01,0x85,0x01,0x09,0x01,0xa1,0x00,
        0x05,0x09,0x19,0x01,0x29,0x03,0x15,0x00,0x25,0x01,0x95,0x03,
        0x75,0x01,0x81,0x02,0x95,0x01,0x75,0x05,0x81,0x03,0x05,0x01,
        0x09,0x30,0x09,0x31,0x15,0x81,0x25,0x7f,0x75,0x08,0x95,0x02,
        0x81,0x06,0xc0,0xc0
    };
    hids_device_init(0, report_descriptor, sizeof(report_descriptor));
}
void dm_advertise(void) {
    static const uint8_t data[] = {
        0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
        0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xc2, 0x03,
        0x0b, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'D','u','m','m','y','M','o','u','s','e',
        0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x12, 0x18,
    };
    gap_advertisements_set_data(sizeof(data), (uint8_t *)data);
    gap_advertisements_enable(1);
    printf("Advertising as DummyMouse; pair from the host Bluetooth settings.\n");
}
void dm_stop_advertising(void) { gap_advertisements_enable(0); }
int main(void) {
    if (rust_app_init() != 0) return 1;
    while (true) rust_app_poll();
}
