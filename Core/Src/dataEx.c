#include "stm32f1xx_hal.h"
#include "dataEx.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

unsigned char esp32_calculate_crc(unsigned char *data, unsigned int length);
void dataEx_Error_Handler(void);
void buf2store();

UART_HandleTypeDef *p_huart = NULL;
uint8_t transmitter_State = TRANSMITTER_STATE_READY;
volatile uint8_t Rx_State = RX_STATE_DENY;

transmitt_Buf_t transmitt_Buf;
receive_Buf_t   receive_Buf;
receive_Buf_t   store;
uint8_t resAccum;
volatile uint8_t *receive_seek;
volatile uint8_t *expect_end_seek;
volatile uint8_t storeIsFull = 0;
uint16_t size_store = 0;
volatile uint16_t lengthValue; // CMD + STATUS + DATA[] + CRC. incoming length
uint16_t last_lengthValue;

/* ***************************************************
This function pass UART handle to dataEx module,
initialize variable and permit receiving UART packets
Tt should be exected before first send packet
parametr:   _huart - pointer to UART handle structure
return:     TATUS_OK if correct or STATUS_ERROR if wrong _huart
*************************************************** */
int8_t uart_handle_registr(UART_HandleTypeDef *_huart)
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
Receive state machine implementation
parametr: _huart - pointer to UART handle structure
A byte receiving is permit always from physical UART device, to prevent owerflow USART_DR registr
A byte handling depend of state:
RX_STATE_DENY - isn't action. Byte from UART will be lost
RX_STATE_BEGIN - there is doing accumulate fields SOF LSB MSB,
  next step - definition 'lengthValue' - remaining lenth (CMD STATUS DATA [] CRC)
RX_STATE_HAVE_LEN - there is doing accumulate data and checking buffer owerflow
RX_STATE_HAVE_PACKET - there is trying to copy accumulated data to internal store.
If internal buffer is full, than copy impossible, consequently accumulate from UART stopped. 
The new byte from physical UART  will be lost 
*************************************************** */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  //permit next byte receive in Accumulator
  if(HAL_UART_Receive_IT(p_huart, &resAccum, 1) != HAL_OK)
    dataEx_Error_Handler();
  
  switch (Rx_State) //state machine 
  {
  case  RX_STATE_DENY:
    printf("\r\n$Deny$");
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
       {
         Rx_State = RX_STATE_DENY;
         printf("\r\n$Wrong Len$ ");
       } 
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
    if (!storeIsFull)
    {
      buf2store();
      storeIsFull = 1;
      Rx_State = RX_STATE_BEGIN;
    }
    else
    {
     printf("\r\n$Storefull$"); 
    }
    break;
    
  default:
    Rx_State = RX_STATE_BEGIN;
  }
}

/* ***************************************************
copy from buffer to internal store
it can be executed only if Rx_State == RX_STATE_HAVE_PACKET
*************************************************** */
void buf2store()
{
  last_lengthValue = lengthValue;
  size_store = receive_seek - (uint8_t *)&receive_Buf;
  memcpy(&store, &receive_Buf, size_store);
  printf(" $Stored %d$ ",size_store);
    
  //prepare to next receive 
  receive_seek = (uint8_t *)&receive_Buf;
  expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
}

/* ***************************************************
Check Packet in internal storage
if internal storage don't contain complete packet - Function return "-1"
if it have a complete packet (n bytes) - Function return "n"
NOTE: this packet was not verificated! crc maybe not OK
just it is some packet.
*************************************************** */
 int16_t polling(void)
 {
   return((storeIsFull) ? size_store : -1);
 }

/* ***************************************************
Answer validation and put data to pointer buf
parametr: buf - pointer to user InBox buffer
return  STATUS_OK if CRC correct
        STATUS_ERROR if wrong CRC
*************************************************** */
int8_t process_answer (unsigned char *buf)
{
  uint8_t retval = STATUS_ERROR;
  if (storeIsFull)
  {
    if ((uint8_t) *((uint8_t *)&store + size_store-1) == esp32_calculate_crc(&store.cmd, last_lengthValue-1))
    {
     memcpy(buf, &store, size_store);
     size_store = 0;
     retval = STATUS_OK;
    }

    if (Rx_State == RX_STATE_HAVE_PACKET)
    {
      buf2store();
      Rx_State = RX_STATE_BEGIN;
    }
    else
     storeIsFull = 0; 
  }
  return (retval);
}

/* ******************************************************
Pushing packet to queue for transmitt
cmd – Command code
data - Pointer to the optional command data (can be equal to NULL)
data_len - Length of the provided command data (can be equal to 0)
return: 
  STATUS_OK (0) - if packet successfully created and pushed to the interrupt TX queue, 
  STATUS_ERROR (-1) otherwise
********************************************************* */
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
    {
      retval = STATUS_ERROR;
      printf(" $Very big Len$ ");
    }
  }
  else
  {
    retval = STATUS_ERROR;
    printf(" $Transmitter busy. Try again$ ");
  } 
  return (retval);
}

 /**
  * @brief  Tx Transfer completed callback
  It give allow to next transmit after the end of current transmission
  * @param  UartHandle: UART handle. 
  * @note   this routine instead weak stm32f1xx_hal_uart
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  transmitter_State = TRANSMITTER_STATE_READY;
}

/* ******************************************************
Reinitializing state mashine if suddenly happen IDLE state
********************************************************* */
void Idle_detect_callback()
{
  printf(" $IDLE$ ");
 if (Rx_State != RX_STATE_HAVE_PACKET)
 {
    receive_seek = (uint8_t *)&receive_Buf;
    expect_end_seek = (uint8_t *)&receive_Buf + sizeof(receive_Buf_t);
    Rx_State = RX_STATE_BEGIN;
 }
  __HAL_UART_CLEAR_PEFLAG(p_huart); //clear IDLE flag
}

/* ******************************************************
Clear ERROR FLAGs after apparate UART errors
********************************************************* */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  printf(" $UART error %X$ ",huart->ErrorCode);
  __HAL_UART_CLEAR_PEFLAG(p_huart); //clear Errors flag
  if(HAL_UART_Receive_IT(p_huart, &resAccum, 1) != HAL_OK)
    dataEx_Error_Handler();
}

/* ******************************************************
CRC calculation
imported routine
parametr: data - pointer to memory data for CRC calculate
          length - size of data from CRC calculation
return: CRC
********************************************************* */
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

/* ******************************************************
dataEx Error handler
********************************************************* */
void dataEx_Error_Handler(void)
{
  printf(" $dataEx_Error_Handler$ ");
}
