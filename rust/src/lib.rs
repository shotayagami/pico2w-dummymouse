#![no_std]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }
}

use core::ffi::{c_char, c_void};

const INVALID_HANDLE: u16 = 0xffff;
const SUCCESS: u8 = 0;
const HCI_EVENT_PACKET: u8 = 4;
const HCI_STATE_WORKING: u8 = 2;
const HCI_EVENT_DISCONNECTION_COMPLETE: u8 = 0x05;
const HCI_EVENT_LE_META: u8 = 0x3e;
const HCI_EVENT_HIDS_META: u8 = 0xf1;
const HCI_SUBEVENT_LE_CONNECTION_COMPLETE: u8 = 0x01;
const HIDS_SUBEVENT_INPUT_REPORT_ENABLE: u8 = 0x05;
const HIDS_SUBEVENT_CAN_SEND_NOW: u8 = 0x01;
const SM_EVENT_JUST_WORKS_REQUEST: u8 = 0xc8;
const SM_EVENT_PAIRING_COMPLETE: u8 = 0xd5;
include!(concat!(env!("OUT_DIR"), "/interval.rs"));
const BUTTON_DEBOUNCE_MS: u32 = 40;
const STATUS_LED_PIN: u32 = 15;
const CONTROL_BUTTON_PIN: u32 = 14;
const PILOT_LED_PIN: u32 = 16;

#[repr(C)]
struct TimerSource {
    item: *mut TimerSource,
    timeout: u32,
    process: Option<unsafe extern "C" fn(*mut TimerSource)>,
    context: *mut c_void,
}

extern "C" {
    fn dm_stdio_init();
    fn dm_init_gpio(pin: u32, output: bool, high: bool, pull_up: bool);
    fn dm_gpio_get(pin: u32) -> bool;
    fn dm_gpio_put(pin: u32, high: bool);
    fn dm_cyw43_init() -> i32;
    fn dm_poll();
    fn dm_now_ms() -> u32;
    fn dm_log(message: *const c_char, a: i32, b: i32, c: i32);
    fn dm_btstack_init();
    fn dm_advertise();
    fn dm_stop_advertising();
    fn dm_request_can_send(handle: u16) -> u8;
    fn dm_send_report(handle: u16, report: *const u8, len: u16) -> u8;
    fn dm_just_works_confirm(handle: u16);
    fn dm_event_type(packet: *const u8) -> u8;
    fn dm_event_state(packet: *const u8) -> u8;
    fn dm_event_subevent(packet: *const u8) -> u8;
    fn dm_event_status(packet: *const u8) -> u8;
    fn dm_event_handle(packet: *const u8) -> u16;
    fn dm_event_hids_enabled(packet: *const u8) -> bool;
    fn dm_register_handlers(
        callback: unsafe extern "C" fn(u8, u16, *mut u8, u16),
        timer: *mut TimerSource,
        timer_callback: unsafe extern "C" fn(*mut TimerSource),
    );
}

#[derive(Clone, Copy)]
struct Direction {
    dx: i8,
    dy: i8,
}
const DIRECTIONS: [Direction; 4] = [
    Direction { dx: 0, dy: -1 },
    Direction { dx: 0, dy: 1 },
    Direction { dx: -1, dy: 0 },
    Direction { dx: 1, dy: 0 },
];

static mut TIMER: TimerSource = TimerSource {
    item: core::ptr::null_mut(),
    timeout: 0,
    process: None,
    context: core::ptr::null_mut(),
};
static mut CONNECTION: u16 = INVALID_HANDLE;
static mut NEXT_DIRECTION: usize = 0;
static mut READY: bool = false;
static mut NOTIFICATIONS: bool = false;
static mut PAUSED: bool = false;
static mut CAN_SEND: bool = false;
static mut REPORT_PENDING: bool = false;
static mut BUTTON_LAST: bool = false;
static mut BUTTON_STABLE: bool = false;
static mut BUTTON_CHANGED_AT: u32 = 0;

unsafe fn log(message: &'static [u8], a: u32, b: u32) {
    dm_log(message.as_ptr().cast(), a as i32, b as i32, 0);
}

unsafe fn schedule_report() {
    if !PAUSED && NOTIFICATIONS && CONNECTION != INVALID_HANDLE {
        dm_timer_schedule(&raw mut TIMER, REPORT_INTERVAL_MS);
    }
}

extern "C" {
    fn dm_timer_schedule(timer: *mut TimerSource, delay_ms: u32);
    fn dm_timer_remove(timer: *mut TimerSource);
}

unsafe fn set_paused(paused: bool) {
    if PAUSED == paused {
        return;
    }
    PAUSED = paused;
    REPORT_PENDING = false;
    CAN_SEND = false;
    dm_timer_remove(&raw mut TIMER);
    if paused {
        log(b"Mouse movement paused.\0", 0, 0);
    } else {
        log(b"Mouse movement resumed.\0", 0, 0);
        schedule_report();
    }
}

unsafe fn send_if_ready() {
    if PAUSED || !CAN_SEND || !REPORT_PENDING || CONNECTION == INVALID_HANDLE {
        return;
    }
    let d = DIRECTIONS[NEXT_DIRECTION];
    let report = [0u8, d.dx as u8, d.dy as u8];
    CAN_SEND = false;
    REPORT_PENDING = false;
    let status = dm_send_report(CONNECTION, report.as_ptr(), report.len() as u16);
    dm_log(
        b"Mouse input report sent; status=%d, dx=%d, dy=%d.\n\0"
            .as_ptr()
            .cast(),
        status as i32,
        d.dx as i32,
        d.dy as i32,
    );
    if status == SUCCESS {
        NEXT_DIRECTION = (NEXT_DIRECTION + 1) % DIRECTIONS.len();
    }
    schedule_report();
}

unsafe extern "C" fn timer_fired(_: *mut TimerSource) {
    if PAUSED || !NOTIFICATIONS {
        return;
    }
    if CONNECTION == INVALID_HANDLE {
        log(b"Mouse timer fired without an active connection.\0", 0, 0);
        return;
    }
    REPORT_PENDING = true;
    let status = dm_request_can_send(CONNECTION);
    log(
        b"Mouse timer fired; request-can-send status=%d.\0",
        status as u32,
        0,
    );
}

unsafe extern "C" fn packet_handler(packet_type: u8, _: u16, packet: *mut u8, _: u16) {
    if packet_type != HCI_EVENT_PACKET || packet.is_null() {
        return;
    }
    match dm_event_type(packet) {
        0x60 => {
            // BTSTACK_EVENT_STATE
            if dm_event_state(packet) == HCI_STATE_WORKING {
                READY = true;
                dm_advertise();
            }
        }
        HCI_EVENT_LE_META
            if dm_event_subevent(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE
                && dm_event_status(packet) == SUCCESS =>
        {
            CONNECTION = dm_event_handle(packet);
            dm_stop_advertising();
            log(
                b"Bluetooth connected; waiting for HID input subscription.\0",
                0,
                0,
            );
        }
        HCI_EVENT_DISCONNECTION_COMPLETE => {
            dm_timer_remove(&raw mut TIMER);
            CONNECTION = INVALID_HANDLE;
            NOTIFICATIONS = false;
            CAN_SEND = false;
            REPORT_PENDING = false;
            if READY {
                dm_advertise();
            }
            log(b"Bluetooth disconnected; advertising again.\0", 0, 0);
        }
        HCI_EVENT_HIDS_META => match dm_event_subevent(packet) {
            HIDS_SUBEVENT_INPUT_REPORT_ENABLE => {
                CONNECTION = dm_event_handle(packet);
                NOTIFICATIONS = dm_event_hids_enabled(packet);
                if NOTIFICATIONS {
                    log(b"HID input notifications enabled.\0", 0, 0);
                    schedule_report();
                } else {
                    log(b"HID input notifications disabled.\0", 0, 0);
                    dm_timer_remove(&raw mut TIMER);
                    REPORT_PENDING = false;
                    CAN_SEND = false;
                }
            }
            HIDS_SUBEVENT_CAN_SEND_NOW => {
                CAN_SEND = true;
                log(b"BTstack reports CAN_SEND_NOW.\0", 0, 0);
                send_if_ready();
            }
            _ => (),
        },
        SM_EVENT_JUST_WORKS_REQUEST => {
            dm_just_works_confirm(dm_event_handle(packet));
            log(b"Confirming BLE Just Works pairing.\0", 0, 0);
        }
        SM_EVENT_PAIRING_COMPLETE => {
            if dm_event_status(packet) == SUCCESS {
                log(b"BLE pairing complete.\0", 0, 0);
            } else {
                log(
                    b"BLE pairing failed; status=%d.\0",
                    dm_event_status(packet) as u32,
                    0,
                );
            }
        }
        _ => (),
    }
}

unsafe fn poll_button() {
    let now = dm_now_ms();
    let reading = !dm_gpio_get(CONTROL_BUTTON_PIN);
    if reading != BUTTON_LAST {
        BUTTON_LAST = reading;
        BUTTON_CHANGED_AT = now;
    }
    if now.wrapping_sub(BUTTON_CHANGED_AT) >= BUTTON_DEBOUNCE_MS && BUTTON_STABLE != BUTTON_LAST {
        BUTTON_STABLE = BUTTON_LAST;
        if BUTTON_STABLE {
            set_paused(!PAUSED);
        }
    }
}

unsafe fn update_led() {
    let now = dm_now_ms();
    let on = !PAUSED
        && CONNECTION != INVALID_HANDLE
        && if NOTIFICATIONS {
            now % 2000 < 100
        } else {
            now % 250 < 125
        };
    dm_gpio_put(STATUS_LED_PIN, on);
}

#[no_mangle]
pub unsafe extern "C" fn rust_app_init() -> i32 {
    dm_stdio_init();
    dm_init_gpio(PILOT_LED_PIN, true, true, false);
    dm_init_gpio(STATUS_LED_PIN, true, false, false);
    dm_init_gpio(CONTROL_BUTTON_PIN, false, false, true);
    BUTTON_LAST = !dm_gpio_get(CONTROL_BUTTON_PIN);
    BUTTON_STABLE = BUTTON_LAST;
    BUTTON_CHANGED_AT = dm_now_ms();
    if dm_cyw43_init() != 0 {
        log(b"CYW43 initialization failed.\0", 0, 0);
        return 1;
    }
    dm_btstack_init();
    dm_register_handlers(packet_handler, &raw mut TIMER, timer_fired);
    0
}

#[no_mangle]
pub unsafe extern "C" fn rust_app_poll() {
    dm_poll();
    poll_button();
    update_led();
}
