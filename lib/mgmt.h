#ifndef __MGMT_H__
#define __MGMT_H__

#include <stdint.h>
#include "sunxi_usart.h"

#define CRC_32_POLY (uint32_t)0x04C11DB7
#define MAGIC_REQ	(uint16_t)0xDEAD
#define MAGIC_RES	(uint16_t)0xBEEF

#define CMD_NOP		  0x00
#define CMD_INFO	  0x01
#define CMD_WDG		  0x02
#define CMD_OFF		  0x03
#define CMD_RST		  0x04
#define CMD_VOLT	  0x05
#define CMD_FEL		  0x06
#define CMD_GET_DATE  0x07
#define CMD_SET_DATE  0x08
#define CMD_GET_SLOTS 0x09
#define CMD_SET_SLOT  0x0A
#define CMD_GET_KEY	  0x0B
#define CMD_SET_KEY	  0x0C
#define CMD_BOOT_STRT 0x0D
#define CMD_BOOT_OK	  0x0E

#define WDG_MODE_ACTIVE 1 // Reset when timeout
#define WDG_MODE_STANBY 2 // Standby when timeout

typedef struct {
	uint32_t year : 8; /**< @brief Years since 1980.           */
	uint32_t month : 4; /**< @brief Months 1..12.               */
	uint32_t dstflag : 1; /**< @brief DST correction flag.        */
	uint32_t dayofweek : 3; /**< @brief Day of week 1..7.           */
	uint32_t day : 5; /**< @brief Day of the month 1..31.     */
	uint32_t millisecond : 27; /**< @brief Milliseconds since midnight.*/
} RTCDateTime;

typedef struct __attribute__((__packed__)) {
	uint16_t magic;
	uint8_t	 padding;
	uint8_t	 cmd;
	uint8_t	 data[32];
	uint32_t crc;
} mgmt_packet_t;

typedef struct __attribute__((__packed__)) {
	uint8_t active;
	uint8_t number;
	uint8_t retries;
} slot_msg_t;

typedef struct __attribute__((__packed__)) {
	uint16_t	revision;
	uint8_t		fw_version[3];
	uint8_t		hw_version;
	RTCDateTime last_updated;
} info_msg_t;

typedef struct __attribute__((__packed__)) {
	uint8_t timeout;
	uint8_t mode;
} wdg_msg_t;

typedef struct __attribute__((__packed__)) {
	uint8_t type;
	uint8_t key;
} key_msg_t;

uint8_t mgmt_get_time(sunxi_usart_t *usart, uint8_t *buf);
uint8_t mgmt_get_slot(sunxi_usart_t *usart, uint8_t *buf);
uint8_t mgmt_boot(sunxi_usart_t *usart, uint8_t *buf);

#endif
