#include "stm32f1xx_hal.h"
#include "dataEx.h"
#include <string.h>
#include <stddef.h>

unsigned char esp32_calculate_crc(unsigned char *data, unsigned int length);
void dataEx_Error_Handler(void);
void buf2store();

UART_HandleTypeDef *p_huart = NULL;
uint8_t transmitter_State = TRANSMITTER_STATE_READY;
uint8_t Rx_State = RX_STATE_DENY;

transmitt_Buf_t transmitt_Buf;
receive_Buf_t   receive_Buf;
receive_Buf_t   store;
uint8_t resAccum;
uint8_t *receive_seek;
uint8_t *expect_end_seek;
uint8_t receive_permit = 0;
uint8_t Is_storeFull = 0;
uint16_t size_store = 0;
uint16_t lengthValue, last_lengthValue;

/* ****************************************************** */
int8_t uart_handle_rigistr(UART_HandleTypeDef *_huart)
{
 int8_t retval = STATUS_OK;
 if (_huart)
 {
   p_huart=_huart;
   transmitt_Buf.sof = SOF_SIGN;
   //prepare to first receive 
   receive_seek = (uint8_t *)&receive_Buf;
   expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
   Rx_State = RX_STATE_BEGIN;
   __HAL_UART_ENABLE_IT(p_huart, UART_IT_IDLE);
   if(HAL_UART_Receive_IT(p_huart, &resAccum, 1) != HAL_OK)
    dataEx_Error_Handler();
 }
 else
  retval = STATUS_ERROR;
 return (retval);
}

/* ***************************************************

*************************************************** */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  //permit next byte receive in Accumulator
  if(HAL_UART_Receive_IT(p_huart, &resAccum, 1) != HAL_OK)
    dataEx_Error_Handler();
  
  switch (Rx_State) //state machine 
  {
  case  RX_STATE_DENY:
    break;
  case  RX_STATE_BEGIN:
    *receive_seek = resAccum;
    if (receive_seek == &receive_Buf.sof)
      if (resAccum != SOF_SIGN)
      {
        Rx_State = RX_STATE_DENY;
        break;
      }
     receive_seek++;
     if (receive_seek == &receive_Buf.cmd)
     {
       lengthValue = receive_Buf.lsb + (receive_Buf.msb << 8);
       if ((lengthValue < FIELD_LEN_MIN) || (lengthValue > FIELD_LEN_MAX))
         Rx_State = RX_STATE_DENY;
       else
       {
        expect_end_seek = receive_seek + lengthValue;
        Rx_State = RX_STATE_HAVE_LEN;
       }
     }
    break;
    
  case  RX_STATE_HAVE_LEN:
    *receive_seek = resAccum;
     receive_seek++;
    if (receive_seek >= expect_end_seek)
      Rx_State = RX_STATE_HAVE_PACKET; //fall-through
    else
      break;    
    
  case  RX_STATE_HAVE_PACKET:
    if (!Is_storeFull)
    {
      buf2store();
      Is_storeFull = 1;
      Rx_State = RX_STATE_BEGIN;
    }
    break;
    
  default:
    Rx_State = RX_STATE_BEGIN;
  }
}
 /* if (receive_permit)
  {
    //check owerflow receive buffer
    if (receive_seek < (uint8_t *)&receive_Buf + sizeof(receive_Buf_t))
    {
      *receive_seek = resAccum;
      receive_seek++;

      if (receive_seek == &receive_Buf.cmd)
      {
        //definite the end of transaction knowing .lsb and .msb
        lengthValue = receive_Buf.lsb + (receive_Buf.msb << 8);
        if ((lengthValue < FIELD_LEN_MIN) || (lengthValue > FIELD_LEN_MAX))
        {
          receive_permit = 0; //packet can't be correct with such lengthValue !!
          return;
        }
        expect_end_seek = receive_seek + lengthValue;
      }
       
      if (receive_seek > expect_end_seek) //packet with length = lengthValue is received. 
      {
        if (!Is_storeFull)
          buf2store();
        else
         receive_permit = 0; 
      }
    }
     else 
       receive_permit = 0;  //denay overflow allocated structure
  }
} */

void buf2store()
{
  last_lengthValue = lengthValue;
  size_store = receive_seek - (uint8_t *)&receive_Buf;
  memcpy(&store, &receive_Buf, size_store);
   
  //prepare to next receive 
  receive_seek = (uint8_t *)&receive_Buf;
  expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
}

/* ***************************************************
 Answer validation and put data to pointer buf
*************************************************** */
int8_t process_answer (unsigned char *buf)
{
  uint8_t retval = -1;
  if (Is_storeFull)
  {
    if ((uint8_t) *((uint8_t *)&store + size_store) == esp32_calculate_crc(&store.cmd, last_lengthValue-1))
    {
     memcpy(buf, &store, size_store);
     retval = 0;
    }

    if (Rx_State == RX_STATE_HAVE_PACKET)
      buf2store();
    Rx_State = RX_STATE_DENY;
    Is_storeFull = 0;
    Rx_State = RX_STATE_BEGIN;
  }
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
      retval = STATUS_ERROR;  // very big lengthValue;
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

void Idle_detect_callback()
{
 if (Rx_State != RX_STATE_HAVE_PACKET)
 {
    receive_seek = (uint8_t *)&receive_Buf;
    expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
    Rx_State = RX_STATE_BEGIN;
 } 
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
