// Define to prevent recursive inclusion
#ifndef __DATAEX_H
#define __DATAEX_H

//buffers configuration
#define PAYLOAD_LEN           1024
#define FIELD_LEN_MIN         3                   // cmd + status + crc
#define FIELD_LEN_MAX         (PAYLOAD_LEN + 3)   // cmd + status + payloadMax + crc
#define CRC_SIZE              1
#define SOF_SIGN              0x00

#define STATUS_OK             0
#define STATUS_ERROR          -1

//transmitter state mashine
#define TRANSMITTER_STATE_READY   0
#define TRANSMITTER_STATE_BUSY    1

//reciever state mashine
#define RX_STATE_DENY             0
#define RX_STATE_BEGIN            1
#define RX_STATE_HAVE_LEN         2
#define RX_STATE_HAVE_PACKET      3

typedef struct
{
  uint8_t sof;
  uint8_t msb;
  uint8_t lsb;
  uint8_t cmd;
  uint8_t data[PAYLOAD_LEN + CRC_SIZE];  //data and crc
} transmitt_Buf_t;

typedef struct
{
  uint8_t sof;
  uint8_t msb;
  uint8_t lsb;
  uint8_t cmd;
  uint8_t status;
  uint8_t data[PAYLOAD_LEN + CRC_SIZE];  //data and crc
} receive_Buf_t;

//API fuctions
int8_t uart_handle_registr(UART_HandleTypeDef *_huart);
int8_t send_request(uint8_t cmd, uint8_t *data, uint16_t data_len);
int16_t polling(void);
int8_t process_answer (unsigned char *buf);

#endif  //__DATAEX_H



