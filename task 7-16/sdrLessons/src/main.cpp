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

/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))


/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *rx0_q = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_channel *tx0_q = NULL;
static struct iio_buffer  *rxbuf = NULL;
static struct iio_buffer  *txbuf = NULL;
static struct iio_stream  *rxstream = NULL;
static struct iio_stream  *txstream = NULL;
static struct iio_channels_mask *rxmask = NULL;
static struct iio_channels_mask *txmask = NULL;



/* cleanup and exit */
static void shutdown(void)
{
	printf("* Destroying streams\n");
	if (rxstream) {iio_stream_destroy(rxstream); }
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

struct sigaction old_action;

void sigint_handler(int sig_no)
{
    printf("CTRL-C pressed\n");
    sigaction(SIGINT, &old_action, NULL);
    shutdown();
    exit(0);
}

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
};

int main(){
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    // Конфиг. параметры "потоков"
	struct stream_cfg rxcfg;
	struct stream_cfg txcfg;

    // RX stream config
	rxcfg.bw_hz = MHZ(10);   // 2 MHz rf bandwidth
	rxcfg.fs_hz = MHZ(10);   // 2.5 MS/s rx sample rate
	rxcfg.lo_hz = MHZ(1000); // 2.5 GHz rf frequency
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

	// TX stream config
	txcfg.bw_hz = MHZ(10); // 1 MHz rf bandwidth
	txcfg.fs_hz = MHZ(10);   // 2.5 MS/s tx sample rate
	txcfg.lo_hz = MHZ(1000); // 2.5 GHz rf frequency
	txcfg.rfport = "A"; // port A (select for rf freq.)


    // Initialize IIO context
    ctx = iio_create_context(NULL, "ip:192.168.2.1");
    if(!ctx){
        std::cerr << "Unable to create IIO context addr: " << "ip:192.168.2.1" << std::endl;
        return 1;
    } else {
        std::cout << "IIO context created successfully, addr: " << "ip:192.168.2.1" << std::endl;
    }

    // The ad9361-phy driver entirely Controls the AD9361.
    // The cf-ad9361-lpc is the ADC/RX capture driver that controls the RX DMA and the RX HDL core.
    // The cf-ad9361-dds-core-lpc is the DAC/TX output driver that controls the TX DMA and the TX HDL core, including the DDS.

    printf("* Инициализация AD9361 устройств\n");
    struct iio_device *phy_dev;
    struct iio_device *tx_dev;
    struct iio_device *rx_dev;
    
    tx_dev =    iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    rx_dev =    iio_context_find_device(ctx, "cf-ad9361-lpc");
    phy_dev  =  iio_context_find_device(ctx, "ad9361-phy");



    printf("* Настройка параметров %s канала AD9361 \n", "TX");
	struct iio_channel *tx_chn = NULL;
    tx_chn = iio_device_find_channel(phy_dev, "voltage0", true);
    const struct iio_attr *tx_rf_port_attr = iio_channel_find_attr(tx_chn, "rf_port_select");
    iio_attr_write_string(tx_rf_port_attr, txcfg.rfport);
    const struct iio_attr *tx_bw_attr = iio_channel_find_attr(tx_chn, "rf_bandwidth");
    iio_attr_write_longlong(tx_bw_attr, txcfg.bw_hz);
    const struct iio_attr *tx_fs_attr = iio_channel_find_attr(tx_chn, "sampling_frequency");
    iio_attr_write_longlong(tx_fs_attr, txcfg.fs_hz);
    printf("* Настройка частоты опорного генератора (lo, local oscilator)  %s \n", "TX");
    struct iio_channel *tx_lo_chn = NULL;
    tx_lo_chn = iio_device_find_channel(phy_dev, "altvoltage1", true);
    const struct iio_attr *tx_lo_attr = iio_channel_find_attr(tx_lo_chn, "frequency");
    iio_attr_write_longlong(tx_lo_attr, txcfg.lo_hz);
    const struct iio_attr *tx_gain_attr = iio_channel_find_attr(tx_chn, "hardwaregain");
    iio_attr_write_longlong(tx_gain_attr, 70);

    printf("* Настройка параметров %s канала AD9361 \n", "RX");
    struct iio_channel *rx_chn = NULL;
    rx_chn = iio_device_find_channel(phy_dev, "voltage0", false);
    const struct iio_attr *rx_rf_port_attr = iio_channel_find_attr(rx_chn, "rf_port_select");
    iio_attr_write_string(rx_rf_port_attr, rxcfg.rfport);
    const struct iio_attr *rx_bw_attr = iio_channel_find_attr(rx_chn, "rf_bandwidth");
    iio_attr_write_longlong(rx_bw_attr, rxcfg.bw_hz);
    const struct iio_attr *rx_fs_attr = iio_channel_find_attr(rx_chn, "sampling_frequency");
    iio_attr_write_longlong(rx_fs_attr, rxcfg.fs_hz);
    printf("* Настройка частоты опорного генератора (lo, local oscilator)  %s \n", "RX");
    struct iio_channel *rx_lo_chn = NULL;
    rx_lo_chn = iio_device_find_channel(phy_dev, "altvoltage0", true);
    const struct iio_attr *rx_lo_attr = iio_channel_find_attr(rx_lo_chn, "frequency");
    iio_attr_write_longlong(rx_lo_attr, rxcfg.lo_hz);
    const struct iio_attr *rx_gain_attr = iio_channel_find_attr(rx_chn, "gain_control_mode");
    iio_attr_write_string(rx_gain_attr, "slow_attack");

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

    printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
	rxbuf = iio_device_create_buffer(rx_dev, 0, rxmask);
    txbuf = iio_device_create_buffer(tx_dev, 0, txmask);

    int buffer_size = pow(2, 13);
    rxstream = iio_buffer_create_stream(rxbuf, 4, buffer_size);
    txstream = iio_buffer_create_stream(txbuf, 4, buffer_size);

    // RX and TX sample size
	size_t rx_sample_sz, tx_sample_sz;
    rx_sample_sz = iio_device_get_sample_size(rx_dev, rxmask);
	tx_sample_sz = iio_device_get_sample_size(tx_dev, txmask);
    printf("* rx_sample_sz = %l\n",rx_sample_sz);
    printf("* tx_sample_sz = %l\n",tx_sample_sz);

    const struct iio_block *txblock, *rxblock;
    // Открываем файл для записи RX данных
    std::ofstream rx_file("rx_data.txt");
    if (!rx_file.is_open()) {
        std::cerr << "Ошибка при открытии файла для записи: rx_data.txt" << std::endl;
        shutdown();
        return 1;
    }

    int32_t i = 0;
    int index = 0;
    while (1)
    {
        int16_t *p_dat, *p_end;
		ptrdiff_t p_inc;
        uint32_t samples_cnt = 0;

        /*  Каждый вызов "iio_stream_get_next_block": 
                - отправляет данные с буфера на передачу\прием, 
                - выдает указатели на новый участок буфера.
            Есть вариант low-level вызовов:
                - enqueue()
                - dequeue()
        */
        rxblock = iio_stream_get_next_block(rxstream);
        txblock = iio_stream_get_next_block(txstream);

        /* WRITE: Get pointers to TX buf and write IQ to TX buf port 0 */
        p_inc = tx_sample_sz;
        p_end = static_cast<int16_t *>(iio_block_end(txblock));
        int counter_i = 0;
        for (p_dat = static_cast<int16_t *>(iio_block_first(txblock, tx0_i)); p_dat < p_end;
            p_dat += p_inc / sizeof(*p_dat))
        {
            p_dat[0] = x_bb_real[i];
            p_dat[1] = x_bb_imag[i]; 
        index++;
        if(index == 128)
        index = 0;
        }
        
        /* READ: Get pointers to RX buf and read IQ from RX buf port 0 */
        p_inc = rx_sample_sz;
        p_end = static_cast<int16_t *>(iio_block_end(rxblock));
        for (p_dat = static_cast<int16_t *> (iio_block_first(rxblock, rx0_i)); p_dat < p_end;
             p_dat += p_inc / sizeof(*p_dat)) {
            /* Запись данных RX в файл */
            int16_t i = p_dat[0];
            int16_t q = p_dat[1];

            // Запись в файл
            rx_file << i << "," << q << std::endl; // Здесь вы можете менять формат записи 
            samples_cnt++;
        }
        printf("RX samples_cnt = %d\n", samples_cnt);
        printf("i = %d\n", i);
    }
    
    // Закрываем файл после завершения
    rx_file.close();
    shutdown();

    return 0;
}