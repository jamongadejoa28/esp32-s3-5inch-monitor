#include "services/service.h"
#include "board/board_config.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
static mex_snapshot_t state;
static SemaphoreHandle_t mutex;
static QueueHandle_t queue;
static TaskHandle_t worker_task;
static uint8_t unit = 1;
static command_state_t result;
static uint32_t serial;
static uint8_t queued_unit;
static int exchange(uint8_t target, uint8_t fc, uint16_t addr, uint16_t value, uint16_t *out, size_t cap) {
    uint8_t q[8], r[256];
    mex_request(target, fc, addr, value, q);
    uart_flush_input(BOARD_RS485_UART);
    vTaskDelay(pdMS_TO_TICKS(5));
    uart_write_bytes(BOARD_RS485_UART, q, 8);
    uart_wait_tx_done(BOARD_RS485_UART, pdMS_TO_TICKS(100));
    size_t n = 0;
    int64_t until = esp_timer_get_time() + 350000;
    while (n < sizeof(r) && esp_timer_get_time() < until) {
        int got = uart_read_bytes(BOARD_RS485_UART, r + n, sizeof(r) - n, pdMS_TO_TICKS(15));
        if (got > 0)
            n += got;
        if (n >= 3) {
            size_t expected = (r[1] & 128) ? 5 : ((fc == 5 || fc == 6) ? 8 : r[2] + 5);
            if (n >= expected)
                break;
        }
    }
    return mex_response(q, r, n, out, cap);
}
void service_snapshot(mex_snapshot_t *out) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    *out = state;
    xSemaphoreGive(mutex);
}
command_state_t service_result(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    command_state_t r = result;
    xSemaphoreGive(mutex);
    return r;
}
static void set_result(command_state_t r) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    result = r;
    xSemaphoreGive(mutex);
}
void service_set_unit(uint8_t u) {
    if (u < 1 || u > 99)
        return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    unit = u;
    state.online = false;
    xSemaphoreGive(mutex);
}
uint8_t service_unit(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    uint8_t u = unit;
    xSemaphoreGive(mutex);
    return u;
}
bool service_send(uint8_t fc, uint16_t address, int32_t raw, bool stopped) {
    mex_snapshot_t s;
    service_snapshot(&s);
    mex_command_t c = {fc, address, raw, stopped, ++serial};
    if (service_result() == CMD_PENDING)
        return false;
    if (!mex_validate(&c, s.online && esp_timer_get_time() / 1000 - s.received_ms < 3000, mex_running(&s))) {
        set_result(CMD_REJECTED);
        return false;
    }
    queued_unit = service_unit();
    set_result(CMD_PENDING);
    if (xQueueSend(queue, &c, 0) != pdTRUE) {
        set_result(CMD_REJECTED);
        return false;
    }
    return true;
}
static void worker(void *unused) {
    (void)unused;
    for (;;) {
        uint8_t target = service_unit();
        command_state_t completion = CMD_IDLE;
        mex_command_t c;
        if (xQueueReceive(queue, &c, 0) == pdTRUE) {
            mex_snapshot_t s;
            service_snapshot(&s);
            if (target != queued_unit ||
                !mex_validate(&c, s.online && esp_timer_get_time() / 1000 - s.received_ms < 3000,
                              mex_running(&s))) {
                completion = CMD_REJECTED;
            } else {
                uint16_t addr = c.fc == 5 ? c.address - 1 : c.address - 40001;
                uint16_t value = c.fc == 5 ? (c.value ? 0xff00 : 0) : (uint16_t)c.value;
                int ack = exchange(target, c.fc, addr, value, NULL, 0);
                uint16_t verify[8] = {0};
                int read = exchange(target, c.fc == 5 ? 1 : 3, c.fc == 5 ? (addr / 8) * 8 : addr,
                                    c.fc == 5 ? 8 : 1, verify, 8);
                uint16_t actual = c.fc == 5 ? verify[addr % 8] : verify[0];
                if (c.address == 40005)
                    actual &= 255;
                completion = ack > 0
                                 ? CMD_REJECTED
                                 : (read == 0 && actual == (uint16_t)c.value ? CMD_APPLIED : CMD_UNCERTAIN);
            }
        }
        mex_snapshot_t next = {0};
        bool ok = true;
        for (unsigned i = 0; i < 9; i++) {
            mex_block_t b = mex_blocks[i];
            uint16_t values[16] = {0};
            if (exchange(target, b.fc, b.start, b.count, values, 16)) {
                ok = false;
                break;
            }
            for (int j = 0; j < b.count; j++) {
                if (b.fc == 1)
                    next.coils |= values[j] << (b.start + j);
                else
                    next.regs[b.start + j + 1] = values[j];
            }
        }
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (unit == target) {
            if (ok) {
                next.received_ms = esp_timer_get_time() / 1000;
                next.sequence = state.sequence + 1;
                next.online = true;
                state = next;
            } else
                state.online = false;
        }
        xSemaphoreGive(mutex);
        // Keep CMD_PENDING until the full post-command snapshot has been published.
        // Consumers must never confirm a newer edit using a pre-command snapshot.
        if (completion != CMD_IDLE)
            set_result(completion);
        vTaskDelay(pdMS_TO_TICKS(ok ? 250 : 1000));
    }
}
void service_init(void) {
    mutex = xSemaphoreCreateMutex();
    queue = xQueueCreate(1, sizeof(mex_command_t));
    ESP_ERROR_CHECK(mutex && queue ? ESP_OK : ESP_ERR_NO_MEM);
    uart_config_t c = {.baud_rate = BOARD_RS485_BAUD,
                       .data_bits = UART_DATA_8_BITS,
                       .parity = UART_PARITY_DISABLE,
                       .stop_bits = UART_STOP_BITS_1,
                       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                       .source_clk = UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_driver_install(BOARD_RS485_UART, 512, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BOARD_RS485_UART, &c));
    ESP_ERROR_CHECK(uart_set_pin(BOARD_RS485_UART, BOARD_RS485_TX_GPIO, BOARD_RS485_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(xTaskCreate(worker, "modbus", 4096, NULL, 5, &worker_task) == pdPASS ? ESP_OK
                                                                                         : ESP_ERR_NO_MEM);
}
uint32_t service_stack_free_min(void) {
    return worker_task ? uxTaskGetStackHighWaterMark(worker_task) : 0;
}
