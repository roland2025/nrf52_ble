/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <stdio.h>
#include <string.h>
#include <device.h>
#include <drivers/uart.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>

#include <usb/usb_device.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(cdc_acm_echo, LOG_LEVEL_INF);

#define DEVICE_NAME "GPS beacon"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RING_BUF_SIZE 1024
uint8_t ring_buffer[RING_BUF_SIZE];

struct ring_buf ringbuf;

// -21.25465,-159.72921 - 22 bytes max
#define GPS_COORD_SIZE 24
uint8_t gps_coords[GPS_COORD_SIZE]={0xff,0xff};
int gps_bytes=2;



/* Set Scan Response data */
static struct bt_data sd[]= {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

const struct device *cdc_acm;

bool isInt(uint8_t i){
    return (('0'==i)||('1'==i)||('2'==i)||('3'==i)||('4'==i)||('5'==i)||('6'==i)||('7'==i)||('8'==i)||('9'==i));
}
bool isAcceptable(uint8_t i){
    return (isInt(i)||(','==i)||('.'==i)||('-'==i));
}

bool addData(uint8_t d){

    bool ret=false;

    bool is_endl=('\n'==d || '\r'==d);
    bool is_too_many=false;

    // check coordinates format == //-21.25465,-159.72921
    if( is_endl && 8<gps_bytes )
    {
        bool is_acceptable_characters=true;

        is_too_many=(22<gps_bytes);

        int check=1;


        for(int i =2; i < gps_bytes;i++){
            int t=gps_coords[i];
            if(!isAcceptable(t))
              break;

            if('-'==t)
            {
              if(i==2)
              {
                // ok
              }else if(','==gps_coords[i-1]){
                //ok
              }
              else
                break;
            }
            if('.'==t&&!isInt(gps_coords[i-1]))
              break;

            if((3==check)&&('.'==t)) check=4;
            if((2==check)&&(','==t)) check=3;
            if((1==check)&&('.'==t)) check=2;
        }
        
        bool is_checks_ok=(4==check);


        printk("is_checks_ok: %s is_chars: %s\n", is_checks_ok?"yes":"no",is_acceptable_characters?"yes":"no");

        ret=(is_checks_ok && is_acceptable_characters&&!is_too_many);

        if(ret){

            printk("New GPS coordinates accepted: ");
            for(int i =2; i < gps_bytes;i++){
                printk("%c", gps_coords[i]);
            }
            printk("\n");

            gps_bytes=2;
        }

    }

    if(!is_endl){
        if(gps_bytes==GPS_COORD_SIZE)
        {
            // shift left
            for(int i =2; i < GPS_COORD_SIZE;i++){
                gps_coords[i]=gps_coords[i+1];
            }

            gps_bytes--;
        }

        gps_coords[gps_bytes]=d;

        gps_bytes++;

       //printk("GPS data: ");
       //   for(int i =2; i < gps_bytes;i++){
       //       printk("%c", gps_coords[i]);
       //   }
       // printk("\n");
    }else if(!ret){
        if(is_too_many){
            uint8_t msg[] ="Too many characters!\r\n";
            ring_buf_put(&ringbuf, msg, sizeof(msg));
        }else{
            uint8_t msg[] ="Invalid coordinate format!\r\n";
            ring_buf_put(&ringbuf, msg, sizeof(msg));
        }
        uart_irq_tx_enable(cdc_acm);
        gps_bytes=2;
    }
    return ret;
}


static void interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			int recv_len, rb_len;
			uint8_t buffer[64];
			size_t len = MIN(ring_buf_space_get(&ringbuf),
					 sizeof(buffer));

			recv_len = uart_fifo_read(dev, buffer, len);

			rb_len = ring_buf_put(&ringbuf, buffer, recv_len);
			if (rb_len < recv_len) {
				LOG_ERR("Drop %u bytes", recv_len - rb_len);
			}

			LOG_DBG("tty fifo -> ringbuf %d bytes", rb_len);

                        for(int i=0; i<recv_len;i++){
                            if(addData(buffer[i])){
                                // new coordinates accepted

                                uint8_t msg[] ="New GPS coordinates accepted!\r\n";
                                ring_buf_put(&ringbuf, msg, sizeof(msg));
            
                                uart_irq_tx_enable(cdc_acm);


                                // update coordinates data
                                struct bt_data upd_ad[] = {                      
                                        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
                                        BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xff, 0xff),
                                        BT_DATA(BT_DATA_MANUFACTURER_DATA,gps_coords, gps_bytes)
                                };

                                gps_bytes=2;

                                // update ble advertisement data
                                bt_le_adv_update_data(upd_ad, ARRAY_SIZE(upd_ad), sd, ARRAY_SIZE(sd));
                            }
                        }


			uart_irq_tx_enable(dev);
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

			LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
		}
	}
}


static const struct bt_data ad[] = {                      
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xff, 0xff),

        
        //59.493803;24.834213
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
		      0xff, 0xff, /*  UUID */
		      '5', '9', '.', '0', ',',
		      '2', '4', '.', '0' ) 
};


static void bt_ready(int err)
{

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

        sd[0].data= DEVICE_NAME;
        sd[0].data_len= DEVICE_NAME_LEN;


	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}



	printk("Beacon started, advertising as %s\n", DEVICE_NAME);
}

void main(void)
{
	int err;

        
	uint32_t baudrate, dtr = 0U;
	int ret;

	cdc_acm = device_get_binding("CDC_ACM_0");
	if (!cdc_acm) {
		LOG_ERR("CDC ACM device not found");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	ring_buf_init(&ringbuf, sizeof(ring_buffer), ring_buffer);

	printk("Starting Beacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
        
	LOG_INF("Wait for DTR");

	while (true) {
		uart_line_ctrl_get(cdc_acm, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		} else {
			/* Give CPU resources to low priority threads. */
			k_sleep(K_MSEC(100));
		}
	}

	LOG_INF("DTR set");

	/* They are optional, we use them to test the interrupt endpoint */
	ret = uart_line_ctrl_set(cdc_acm, UART_LINE_CTRL_DCD, 1);
	if (ret) {
		LOG_WRN("Failed to set DCD, ret code %d", ret);
	}

	ret = uart_line_ctrl_set(cdc_acm, UART_LINE_CTRL_DSR, 1);
	if (ret) {
		LOG_WRN("Failed to set DSR, ret code %d", ret);
	}

	/* Wait 1 sec for the host to do all settings */
	k_busy_wait(1000000);

	ret = uart_line_ctrl_get(cdc_acm, UART_LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(cdc_acm, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(cdc_acm);

        uint8_t msg[] ="Waiting for GPS coordinates (format: '-21.25465,-159.72921')!\r\n";
        ring_buf_put(&ringbuf, msg, sizeof(msg));
        uart_irq_tx_enable(cdc_acm);


}
