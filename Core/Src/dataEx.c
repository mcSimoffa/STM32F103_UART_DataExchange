#include "stm32f1xx_hal.h"
#include "dataEx.h"
#include <string.h>
#include <stddef.h>

unsigned char esp32_calculate_crc(unsigned char *data, unsigned int length);
void dataEx_Error_Handler(void);

UART_HandleTypeDef *p_huart = NULL;
uint8_t transmitter_State = TRANSMITTER_STATE_READY;
typedef struct
{
  uint8_t sof;
  uint8_t msb;
  uint8_t lsb;
  uint8_t cmd;
  uint8_t data[PAYLOAD_LEN+CRC_SIZE];  //data and crc
} transmitt_Buf_t;

transmitt_Buf_t transmitt_Buf;
/* ****************************************************** */
int8_t uart_handle_rigistr(UART_HandleTypeDef *_huart)
{
  int8_t retval = STATUS_OK;
 if (_huart)
   p_huart=_huart;
 else
  retval = STATUS_ERROR;
 transmitt_Buf.sof = 0;
 return (retval);
}


/* ****************************************************** */
int8_t send_request(uint8_t cmd, uint8_t *data, uint16_t data_len)
{
  int8_t retval = STATUS_OK;
  if (transmitter_State == TRANSMITTER_STATE_READY)
  {
    if (data_len <= PAYLOAD_LEN)
    {
      uint16_t defined_len = data_len + sizeof(transmitt_Buf.cmd) + CRC_SIZE;
      transmitt_Buf.msb = (uint8_t)(defined_len >> 8);
      transmitt_Buf.lsb = (uint8_t)(defined_len & 0x00FF);
      transmitt_Buf.cmd = cmd;
      uint8_t *p_crc = memcpy(&transmitt_Buf.data, data, data_len);
      p_crc += data_len;
      *p_crc = esp32_calculate_crc(&transmitt_Buf.cmd, data_len + sizeof(transmitt_Buf.cmd));
      //initiate transmitt with DMA
      if(HAL_UART_Transmit_DMA(p_huart, (uint8_t *)&transmitt_Buf, offsetof(transmitt_Buf_t,cmd)+ defined_len) != HAL_OK)
        dataEx_Error_Handler();
      
      transmitter_State = TRANSMITTER_STATE_BUSY;
    }
    else
      retval = STATUS_ERROR;  // very big payload;
  }
  else
      retval = STATUS_ERROR;  // transmitter busy;
  
  return (retval);
}

/* ****************************************************** 
if(HAL_UART_Receive_IT(p_huart, inBox, 1) != HAL_OK)
  {
    Error_Handler();
  }

  transmitterReady = 1; */

 
 
 
 /**
  * @brief  Tx Transfer completed callback
  * @param  UartHandle: UART handle. 
  * @note   This example shows a simple way to report end of DMA Tx transfer, and 
  *         you can add your own implementation. 
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  transmitter_State = TRANSMITTER_STATE_READY;
}

/*void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  asm("nop");
  uint8_t a=huart->RxState;
  a++;
  if(HAL_UART_Receive_IT(huart, inBox, 1) != HAL_OK)
  {
    Error_Handler();
  }
  
} */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  uint8_t a= huart->ErrorCode;
  
}


unsigned char esp32_calculate_crc(unsigned char *data, unsigned int length)
{
	unsigned char fb, cnt, byte, crc = 0;
	unsigned int i;

	for(i = 0; i < length; i++) 
  {
		byte = data[i];
		cnt = 8;

		while (cnt--)
    {
			fb = (crc ^ byte) & 0x01;
			if (fb == 0x01) 
      {
				crc ^= 0x18;
			}
			crc = (crc >> 1) & 0x7F;
			if (fb == 0x01) 
      {
				crc |= 0x80;
			}
			byte >>= 1;
		}
	}
	return crc;
}

void dataEx_Error_Handler(void)
{
  while (1)
    asm("nop");
}
