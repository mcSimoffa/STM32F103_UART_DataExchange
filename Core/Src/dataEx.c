#include "stm32f1xx_hal.h"
#include "dataEx.h"
#include <string.h>
#include <stddef.h>

unsigned char esp32_calculate_crc(unsigned char *data, unsigned int length);
void dataEx_Error_Handler(void);

UART_HandleTypeDef *p_huart = NULL;
uint8_t transmitter_State = TRANSMITTER_STATE_READY;


transmitt_Buf_t transmitt_Buf;
receive_Buf_t   receive_Buf;
receive_Buf_t   last_receive;
uint8_t *receive_seek;
uint8_t *expect_end_seek;


uint8_t size_last_receive = -1;


/* ****************************************************** */
int8_t uart_handle_rigistr(UART_HandleTypeDef *_huart)
{
 int8_t retval = STATUS_OK;
 if (_huart)
 {
   p_huart=_huart;
   transmitt_Buf.sof = 0;
   //prepare to first receive 
   receive_seek = (uint8_t *)&receive_Buf;
   expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
   __HAL_UART_ENABLE_IT(p_huart, UART_IT_IDLE);
   
   //permitt receiving
   if(HAL_UART_Receive_IT(p_huart, receive_seek, 1) != HAL_OK)
    dataEx_Error_Handler();
 }
 else
  retval = STATUS_ERROR;
 
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

 /**
  * @brief  Tx Transfer completed callback
  * @param  UartHandle: UART handle. 
  * @note   this routine instead weak stm32f1xx_hal_uart
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  transmitter_State = TRANSMITTER_STATE_READY;
}
/* ***************************************************

*************************************************** */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t accum;
  uint16_t payload;
  if(HAL_UART_Receive_IT(p_huart, &accum, 1) != HAL_OK)
    dataEx_Error_Handler();

  //check owerflow receive buffer
  if (receive_seek < (uint8_t *)&receive_Buf + sizeof(receive_Buf_t))
  {
    *receive_seek = accum;
    receive_seek++;

    if (receive_seek == &receive_Buf.cmd)
    {
      //definite the end of transaction knowing lsb and msb
      payload = receive_Buf.lsb + (receive_Buf.msb << 8);
      expect_end_seek = receive_seek + payload;
    }
     
    if (receive_seek > expect_end_seek) //maybe packet received. 
    {
      size_last_receive = receive_seek - (uint8_t *)&receive_Buf;
      memcpy(&last_receive, &receive_Buf, size_last_receive);
      
      //prepare to next receive 
     receive_seek = (uint8_t *)&receive_Buf;
     expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
    }
  }
} 


 // if (accum == esp32_calculate_crc(&receive_Buf.cmd, payload-1))
void Idle_detect_callback()
{
  
  asm("nop");
}


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
