# STM32F103_UART_DataExchange
***
This example implements data exchange between two devices via the UART interface. 
The communication protocol is specified: see structures: transmitt_Buf_t and receive_Buf_t
***
## Tools:
Hardware: STM32F103C8 board,    
Debugger: ST-Link with SWO connect or J-Link,  
IDE: IAR EWARM 8.50,  
UART terminal: https://www.eltima.com/products/serial-port-terminal/
## Transmitting:
The transfer is initiated and performed with DMA.
## Receiving:
Getting of characters is performed byte-by-byte and all checks are performed as available:
1. SOF signature
2. Correct length
3. IDLE state
The checksum is not parsed at this stage.
A two-stage queue is implemented, like the hardware queue of the controller itself.
The first packet with correct checks (except for CRC) is sent from the working buffer to the internal buffer. 
The second packet can be receiving. At the end of reception the second packet, it trying to be writen into the internal buffer. 
If the first packet has not yet been read out, state machine is suspend and new data is no longer received.
As soon as the user reads the first packet from the internal buffer, the second packet going from the working buffer to the internal buffer. And new bytes receiving will be permit again.
CRC control performs at the stage of dropping from the internal buffer to the user one (see process_answer)
***
## Exception Handling:
A sudden stop of the IDLE while the packet is not yet collected always means ignoring the uncollected and starting to listen to a new packet.
Hardware errors are ignored. If there was a failure, it will be eliminated during the CRC control stage.
## Logging:
Logging is implemented through SWO. You do not need to add code if use IAR EWARM.
If logging is not required remove SWO_PRINTF in preprocessor macros.
A $ tags $ mean information messages from interrupts. It print from interrupt, because you have a little mix. But it's not important

PIN 13 LED displays wrong received packet (CRC not OK)
