/**
 *  @filename   :   epdif.cpp
 *  @brief      :   Implements EPD interface functions
 *                  Users have to implement all the functions in epdif.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "epdif.h"

static const char *TAG = "edpif";

#ifdef CONFIG_IDF_TARGET_ESP32
#define LCD_HOST    VSPI_HOST
#define DMA_CHAN    2
#elif defined CONFIG_IDF_TARGET_ESP32S2
#define LCD_HOST    SPI2_HOST
#define DMA_CHAN    LCD_HOST
#endif

static spi_device_handle_t g_spi;


// /* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
//  * until the transfer is complete.
//  *
//  * Since command transactions are usually small, they are handled in polling
//  * mode for higher speed. The overhead of interrupt transactions is more than
//  * just waiting for the transaction to complete.
//  */
// void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
// {
//     esp_err_t ret;
//     spi_transaction_t t;
//     memset(&t, 0, sizeof(t));       //Zero out the transaction
//     t.length=8;                     //Command is 8 bits
//     t.tx_buffer=&cmd;               //The data is the cmd itself
//     t.user=(void*)0;                //D/C needs to be set to 0
//     ret=spi_device_polling_transmit(spi, &t);  //Transmit!
//     assert(ret==ESP_OK);            //Should have had no issues.
// }

// /* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
//  * transfer is complete.
//  *
//  * Since data transactions are usually small, they are handled in polling
//  * mode for higher speed. The overhead of interrupt transactions is more than
//  * just waiting for the transaction to complete.
//  */
// void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
// {
//     esp_err_t ret;
//     spi_transaction_t t;
//     if (len==0) return;             //no need to send anything
//     memset(&t, 0, sizeof(t));       //Zero out the transaction
//     t.length=len*8;                 //Len is in bytes, transaction length is in bits.
//     t.tx_buffer=data;               //Data
//     t.user=(void*)1;                //D/C needs to be set to 1
//     ret=spi_device_polling_transmit(spi, &t);  //Transmit!
//     assert(ret==ESP_OK);            //Should have had no issues.
// }

// //This function is called (in irq context!) just before a transmission starts. It will
// //set the D/C line to the value indicated in the user field.
// void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
// {
//     int dc=(int)t->user;
//     gpio_set_level(PIN_NUM_DC, dc);
// }



// /* To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
//  * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
//  * because the D/C line needs to be toggled in the middle.)
//  * This routine queues these commands up as interrupt transactions so they get
//  * sent faster (compared to calling spi_device_transmit several times), and at
//  * the mean while the lines for next transactions can get calculated.
//  */
// static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata)
// {
//     esp_err_t ret;
//     int x;
//     //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
//     //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
//     static spi_transaction_t trans[6];

//     //In theory, it's better to initialize trans and data only once and hang on to the initialized
//     //variables. We allocate them on the stack, so we need to re-init them each call.
//     for (x=0; x<6; x++) {
//         memset(&trans[x], 0, sizeof(spi_transaction_t));
//         if ((x&1)==0) {
//             //Even transfers are commands
//             trans[x].length=8;
//             trans[x].user=(void*)0;
//         } else {
//             //Odd transfers are data
//             trans[x].length=8*4;
//             trans[x].user=(void*)1;
//         }
//         trans[x].flags=SPI_TRANS_USE_TXDATA;
//     }
//     trans[0].tx_data[0]=0x2A;           //Column Address Set
//     trans[1].tx_data[0]=0;              //Start Col High
//     trans[1].tx_data[1]=0;              //Start Col Low
//     trans[1].tx_data[2]=(320)>>8;       //End Col High
//     trans[1].tx_data[3]=(320)&0xff;     //End Col Low
//     trans[2].tx_data[0]=0x2B;           //Page address set
//     trans[3].tx_data[0]=ypos>>8;        //Start page high
//     trans[3].tx_data[1]=ypos&0xff;      //start page low
//     trans[3].tx_data[2]=(ypos+PARALLEL_LINES)>>8;    //end page high
//     trans[3].tx_data[3]=(ypos+PARALLEL_LINES)&0xff;  //end page low
//     trans[4].tx_data[0]=0x2C;           //memory write
//     trans[5].tx_buffer=linedata;        //finally send the line data
//     trans[5].length=320*2*8*PARALLEL_LINES;          //Data length, in bits
//     trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

//     //Queue all transactions.
//     for (x=0; x<6; x++) {
//         ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
//         assert(ret==ESP_OK);
//     }

//     //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
//     //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
//     //finish because we may as well spend the time calculating the next line. When that is done, we can call
//     //send_line_finish, which will wait for the transfers to be done and check their status.
// }




void DigitalWrite(int pin, int value)
{
    gpio_set_level((gpio_num_t)pin, value);
}

int DigitalRead(int pin)
{
    return gpio_get_level((gpio_num_t)pin);
}

void DelayMs(unsigned int delaytime)
{
    vTaskDelay(pdMS_TO_TICKS(delaytime));
}

void SpiTransfer(unsigned char data)
{
    esp_err_t ret;
    spi_transaction_t t;
    // if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 8;               //Len is in bytes, transaction length is in bits.
    t.tx_buffer = &data;             //Data
    // t.user=(void*)1;                //D/C needs to be set to 1
    ret = spi_device_polling_transmit(g_spi, &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
}

int IfInit(void)
{
    esp_err_t ret;
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set.
    io_conf.pin_bit_mask = ((1ULL << RST_PIN) | (1ULL << DC_PIN));
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUSY_PIN);
    gpio_config(&io_conf);

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = MOSI_PIN,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 128,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000,      //Clock out at 10 MHz
        .mode = 0,                              //SPI mode 0
        .spics_io_num = CS_PIN,                 //CS pin
        .queue_size = 1,                        //We want to be able to queue 7 transactions at a time
        // .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

    //Initialize the SPI bus
    ret = spi_bus_initialize(LCD_HOST, &buscfg, DMA_CHAN);
    // ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &g_spi);
    // ESP_ERROR_CHECK(ret);
    return 0;
}

int IfDeinit(void)
{
    esp_err_t ret;
    gpio_config_t io_conf={0};
    io_conf.pin_bit_mask = ((1ULL << RST_PIN) | (1ULL << DC_PIN) | (1ULL << BUSY_PIN));
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //Attach the LCD to the SPI bus
    ret = spi_bus_remove_device(g_spi);
    // ESP_ERROR_CHECK(ret);
    ret = spi_bus_free(LCD_HOST);
    // ESP_ERROR_CHECK(ret);
    return 0;
}