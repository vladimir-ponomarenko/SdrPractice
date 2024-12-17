#include <iio/iio.h>
#include <iio/iio-debug.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include "./myfile.h"

#define MHZ(x) ((long long)(x * 1000000.0 + .5))
#define GHZ(x) ((long long)(x * 1000000000.0 + .5))

static struct iio_context *ctx = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *rx0_q = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_channel *tx0_q = NULL;
static struct iio_buffer *rxbuf = NULL;
static struct iio_buffer *txbuf = NULL;
static struct iio_stream *rxstream = NULL;
static struct iio_stream *txstream = NULL;
static struct iio_channels_mask *rxmask = NULL;
static struct iio_channels_mask *txmask = NULL;

static int16_t buffer[15000000];
static int buffer_index = 0;

static void shutdown(void) {
    printf("* Destroying streams\n");
    if (rxstream) { iio_stream_destroy(rxstream); }
    if (txstream) { iio_stream_destroy(txstream); }

    printf("* Destroying buffers\n");
    if (rxbuf) { iio_buffer_destroy(rxbuf); }
    if (txbuf) { iio_buffer_destroy(txbuf); }

    printf("* Destroying channel masks\n");
    if (rxmask) { iio_channels_mask_destroy(rxmask); }
    if (txmask) { iio_channels_mask_destroy(txmask); }

    printf("* Destroying context\n");
    if (ctx) { iio_context_destroy(ctx); }
}

void sigint_handler(int sig_no) {
    printf("CTRL-C pressed\n");
    shutdown();
    
    std::ofstream rx_file("rx_data.txt");
    if (rx_file.is_open()) {
        for (int i = 0; i < buffer_index; i += 2) {
            rx_file << buffer[i] << "," << buffer[i + 1] << std::endl;
        }
        rx_file.close();
        printf("Data written to rx_data.txt\n");
    } else {
        std::cerr << "Error opening file for writing: rx_data.txt" << std::endl;
    }

    exit(0);
}

struct stream_cfg {
    long long bw_hz;                // Полоса пропускания в Гц
    long long fs_hz;                // Частота дискретизации в Гц
    long long lo_hz;                // Частота локального генератора в Гц
    const char* rfport;             // Порт RF
    const char* rx_gain_mode;       // Режим усиления для RX
    long long tx_gain;              // Уровень усиления для TX
    long long noise_suppression;    // Подавление шумов
    const char* iq_format;           // Формат IQ
    long long sample_size;           // Размер выборки
};

int main() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, NULL);

    struct stream_cfg rxcfg;
    struct stream_cfg txcfg;

    // RX stream config
    rxcfg.bw_hz = MHZ(20);               // Полоса 20 MHz
    rxcfg.fs_hz = MHZ(10);               // Частота дискретизации 10 MS/s
    rxcfg.lo_hz = MHZ(1000);             // 1 GHz
    rxcfg.rfport = "A_BALANCED";         // порт A_BALANCED
    rxcfg.rx_gain_mode = "fast_attack";  // Быстрая атака, чтобы лучше реагировать на изменения сигнала
    rxcfg.noise_suppression = 1;         // 1 = включить подавление шумов
    rxcfg.iq_format = "complex";          // Формат IQ
    rxcfg.sample_size = 2;                // 2 байта на выборку

    // TX stream config
    txcfg.bw_hz = MHZ(20);               // Полоса 20 MHz
    txcfg.fs_hz = MHZ(10);               // Частота дискретизации 10 MS/s
    txcfg.lo_hz = MHZ(1000);             // 1 GHz
    txcfg.rfport = "A";                  // порт A
    txcfg.tx_gain = 70;                  // Уровень усиления TX
    txcfg.noise_suppression = 1;         // 1 = включить подавление шумов
    txcfg.iq_format = "complex";          // Формат IQ
    txcfg.sample_size = 2;                // 2 байта на выборку

    ctx = iio_create_context(NULL, "ip:192.168.3.1");
    if (!ctx) {
        std::cerr << "Unable to create IIO context addr: " << "ip:192.168.3.1" << std::endl;
        return 1;
    } else {
        std::cout << "IIO context created successfully, addr: " << "ip:192.168.3.1" << std::endl;
    }

    printf("* Инициализация AD9361 устройств\n");
    struct iio_device *phy_dev;
    struct iio_device *tx_dev;
    struct iio_device *rx_dev;

    tx_dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    if (!tx_dev) {
        std::cerr << "Unable to find TX device" << std::endl;
        return 1;
    }

    rx_dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    if (!rx_dev) {
        std::cerr << "Unable to find RX device" << std::endl;
        return 1;
    }

    phy_dev = iio_context_find_device(ctx, "ad9361-phy");
    if (!phy_dev) {
        std::cerr << "Unable to find PHY device" << std::endl;
        return 1;
    }

    printf("* Настройка параметров %s канала AD9361 \n", "TX");
    struct iio_channel *tx_chn = iio_device_find_channel(phy_dev, "voltage0", true);
    const struct iio_attr *tx_rf_port_attr = iio_channel_find_attr(tx_chn, "rf_port_select");
    iio_attr_write_string(tx_rf_port_attr, txcfg.rfport);
    
    const struct iio_attr *tx_bw_attr = iio_channel_find_attr(tx_chn, "rf_bandwidth");
    if (tx_bw_attr) {
        iio_attr_write_longlong(tx_bw_attr, txcfg.bw_hz);
    }

    const struct iio_attr *tx_fs_attr = iio_channel_find_attr(tx_chn, "sampling_frequency");
    if (tx_fs_attr) {
        iio_attr_write_longlong(tx_fs_attr, txcfg.fs_hz);
    }

    struct iio_channel *tx_lo_chn = iio_device_find_channel(phy_dev, "altvoltage1", true);
    const struct iio_attr *tx_lo_attr = iio_channel_find_attr(tx_lo_chn, "frequency");
    iio_attr_write_longlong(tx_lo_attr, txcfg.lo_hz);

    const struct iio_attr *tx_gain_attr = iio_channel_find_attr(tx_chn, "hardwaregain");
    if (tx_gain_attr) {
        iio_attr_write_longlong(tx_gain_attr, txcfg.tx_gain);
    }

    const struct iio_attr *tx_noise_attr = iio_channel_find_attr(tx_chn, "noise_suppression");
    if (tx_noise_attr) {
        iio_attr_write_longlong(tx_noise_attr, txcfg.noise_suppression);
    }

    printf("* Настройка параметров %s канала AD9361 \n", "RX");
    struct iio_channel *rx_chn = iio_device_find_channel(phy_dev, "voltage0", false);
    const struct iio_attr *rx_rf_port_attr = iio_channel_find_attr(rx_chn, "rf_port_select");
    iio_attr_write_string(rx_rf_port_attr, rxcfg.rfport);

    const struct iio_attr *rx_bw_attr = iio_channel_find_attr(rx_chn, "rf_bandwidth");
    if (rx_bw_attr) {
        iio_attr_write_longlong(rx_bw_attr, rxcfg.bw_hz);
    }

    const struct iio_attr *rx_fs_attr = iio_channel_find_attr(rx_chn, "sampling_frequency");
    if (rx_fs_attr) {
        iio_attr_write_longlong(rx_fs_attr, rxcfg.fs_hz);
    }

    struct iio_channel *rx_lo_chn = iio_device_find_channel(phy_dev, "altvoltage0", true);
    const struct iio_attr *rx_lo_attr = iio_channel_find_attr(rx_lo_chn, "frequency");
    iio_attr_write_longlong(rx_lo_attr, rxcfg.lo_hz);

    const struct iio_attr *rx_gain_attr = iio_channel_find_attr(rx_chn, "gain_control_mode");
    if (rx_gain_attr) {
        iio_attr_write_string(rx_gain_attr, rxcfg.rx_gain_mode);
    }

    const struct iio_attr *rx_noise_attr = iio_channel_find_attr(rx_chn, "noise_suppression");
    if (rx_noise_attr) {
        iio_attr_write_longlong(rx_noise_attr, rxcfg.noise_suppression);
    }

    printf("* Инициализация потоков I/Q %s канала AD9361 \n", "TX");
    tx0_i = iio_device_find_channel(tx_dev, "voltage0", true);
    tx0_q = iio_device_find_channel(tx_dev, "voltage1", true);
    rx0_i = iio_device_find_channel(rx_dev, "voltage0", false);
    rx0_q = iio_device_find_channel(rx_dev, "voltage1", false);

    rxmask = iio_create_channels_mask(iio_device_get_channels_count(rx_dev));
    if (!rxmask) {
        fprintf(stderr, "Unable to alloc RX channels mask\n");
    }

    txmask = iio_create_channels_mask(iio_device_get_channels_count(tx_dev));
    if (!txmask) {
        fprintf(stderr, "Unable to alloc TX channels mask\n");
    }

    printf("* Enabling IIO streaming channels\n");
    iio_channel_enable(rx0_i, rxmask);
    iio_channel_enable(rx0_q, rxmask);
    iio_channel_enable(tx0_i, txmask);
    iio_channel_enable(tx0_q, txmask);

    printf("* Creating IIO buffers\n");
    rxbuf = iio_device_create_buffer(rx_dev, 0, rxmask);
    if (!rxbuf) {
        std::cerr << "Unable to create RX buffer" << std::endl;
        return 1;
    }
    txbuf = iio_device_create_buffer(tx_dev, 0, txmask);
    if (!txbuf) {
        std::cerr << "Unable to create TX buffer" << std::endl;
        return 1;
    }

    int buffer_size = pow(2, 13);
    rxstream = iio_buffer_create_stream(rxbuf, 4, buffer_size);
    if (!rxstream) {
        std::cerr << "Unable to create RX stream" << std::endl;
        return 1;
    }
    txstream = iio_buffer_create_stream(txbuf, 4, buffer_size);
    if (!txstream) {
        std::cerr << "Unable to create TX stream" << std::endl;
        return 1;
    }

    size_t rx_sample_sz, tx_sample_sz;
    rx_sample_sz = iio_device_get_sample_size(rx_dev, rxmask);
    tx_sample_sz = iio_device_get_sample_size(tx_dev, txmask);
    printf("* rx_sample_sz = %zu\n", rx_sample_sz);
    printf("* tx_sample_sz = %zu\n", tx_sample_sz);

    while (buffer_index < sizeof(buffer) / sizeof(buffer[0])) {
        int16_t *p_dat, *p_end;
        ptrdiff_t p_inc;
        uint32_t samples_cnt = 0;

        const struct iio_block *rxblock = iio_stream_get_next_block(rxstream);
        const struct iio_block *txblock = iio_stream_get_next_block(txstream);

        // Запись данных в TX канал
        p_inc = tx_sample_sz;
        p_end = static_cast<int16_t *>(iio_block_end(txblock));
        int index = 0;
        for (p_dat = static_cast<int16_t *>(iio_block_first(txblock, tx0_i)); p_dat < p_end; p_dat += p_inc / sizeof(*p_dat)) {
            // Генерировать тестовый сигнал
            p_dat[0] = (int16_t)(32767 * sin(2 * M_PI * 1000 * (index / (double)txcfg.fs_hz))); // реальная часть
            p_dat[1] = (int16_t)(32767 * sin(2 * M_PI * 1000 * (index / (double)txcfg.fs_hz) + M_PI / 2)); // мнимая часть
            index++;
            if (index == 128) {
                index = 0;
            }
        }

        // Сбор данных из RX канала
        p_inc = rx_sample_sz;
        p_end = static_cast<int16_t *>(iio_block_end(rxblock));
        for (p_dat = static_cast<int16_t *>(iio_block_first(rxblock, rx0_i)); p_dat < p_end; p_dat += p_inc / sizeof(*p_dat)) {
            if (buffer_index < sizeof(buffer) / sizeof(buffer[0]) - 2) {
                buffer[buffer_index++] = p_dat[0];
                buffer[buffer_index++] = p_dat[1];
                samples_cnt++;
            }
        }

        printf("RX samples count = %d\n", samples_cnt);
    }

    shutdown();
    return 0;
}