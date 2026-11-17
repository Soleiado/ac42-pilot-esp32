/*
 * Waveshare ESP32-S3-RS485-CAN pinout (SKU 32154 / 34781).
 *
 * CAN is the interface used by this project.  RS485/RTC pins are listed for
 * completeness but are not initialised.
 */
#pragma once

#define BOARD_CAN_TX_PIN   15
#define BOARD_CAN_RX_PIN   16
#define BOARD_RS485_TX_PIN 17
#define BOARD_RS485_RX_PIN 18
#define BOARD_RS485_DIR_PIN 21

#define BOARD_RTC_SDA_PIN  39
#define BOARD_RTC_SCL_PIN  38
#define BOARD_RTC_INT_PIN  40

/* NMEA 2000 is always 250 kbit/s. */
#define BOARD_CAN_BITRATE  250000
