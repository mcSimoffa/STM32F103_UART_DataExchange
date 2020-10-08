# STM32F103_UART_DataExchange
***
This example implements data exchange between two devices via the UART interface. 
The communication protocol is specified: see structures: transmitt_Buf_t and receive_Buf_t
***
## Transmitting:
The transfer is initiated and performed with DMA.
Reception of characters is performed byte-by-byte in the preview and all checks are performed as available:
1. SOF signature
2. Correct length
3. IDLE state
The checksum is not parsed at this stage.
***
## Receiving Queue:
A two-stage queue is implemented, following the example of the hardware queue of the controller itself.
The first packet with correct checks (except for CRC) is sent from the working buffer to the internal buffer. 
The second packet can be receiving. At the end of reception, an attempt is made to write the second packet into the internal buffer. 
If the first packet has not yet been read out, then the second packet is not written and new data is no longer received.
As soon as the user reads the first packet from the internal buffer, the second packet from the working buffer is dropped into it. 
And receiving will be permit again.
CRC control is carried out at the stage of dropping from the internal buffer to the user one (see process_answer)
***
## Except Handling:
A sudden stop of the IDLE while the packet is not yet collected always means ignoring the uncollected and starting to listen to a new packet.
Hardware errors are ignored. If there was a failure, it will be eliminated during the CRC control stage.
## Logging:
Logging is implemented through SWO. You do not need to add code if use IAR EWARM.
If logging is not required remove SWO_PRINTF in preprocessor macros.
A $ tags $ mean information messages from interrupts.

PIN 13 LED displays wrong received packet (CRC not OK)
