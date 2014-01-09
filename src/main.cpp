/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Satish Nair, Zachary Crockett, Zach Supalla and Mohit Bhoite
 * @version V1.0.0
 * @date    13-March-2013
 * @brief   Main program body.
 ******************************************************************************
  Copyright (c) 2013 Spark Labs, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
  ******************************************************************************
 */
  
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spark_utilities.h"
extern "C" {
#include "usb_conf.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_prop.h"
#include "sst25vf_spi.h"
}

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
__IO uint8_t MemProcStack0[PROCESS_STACK0_SIZE];
__IO uint8_t MemProcStack1[PROCESS_STACK1_SIZE];
__IO uint32_t *topOfProcStack0;
__IO uint32_t *topOfProcStack1;
__IO int32_t activeProcStack = PROCESS_STACK_NOT_ACTIVE;	//-1, 0 or 1

volatile uint32_t TimingMillis;

uint8_t  USART_Rx_Buffer[USART_RX_DATA_SIZE];
uint32_t USART_Rx_ptr_in = 0;
uint32_t USART_Rx_ptr_out = 0;
uint32_t USART_Rx_length  = 0;

uint8_t USB_Rx_Buffer[VIRTUAL_COM_PORT_DATA_SIZE];
uint16_t USB_Rx_length = 0;
uint16_t USB_Rx_ptr = 0;

uint8_t  USB_Tx_State = 0;
uint8_t  USB_Rx_State = 0;

uint32_t USB_USART_BaudRate = 9600;

/* Extern variables ----------------------------------------------------------*/
extern LINE_CODING linecoding;

/* Private function prototypes -----------------------------------------------*/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);
static __IO uint32_t *Process_Stack_Init(void (*processTask)(void), __IO uint8_t *procStackBuffer, uint32_t procStackSize);
static void Spark_Process_Task(void);
static void Wiring_Process_Task(void);
//static void DIO_Toggle(DIO_TypeDef Dx);

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 * Function Name  : main.
 * Description    : main routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
int main(void)
{
#ifdef DFU_BUILD_ENABLE
	/* Set the Vector Table(VT) base location at 0x5000 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x5000);

	USE_SYSTEM_FLAGS = 1;
#endif

#ifdef SWD_JTAG_DISABLE
	/* Disable the Serial Wire JTAG Debug Port SWJ-DP */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
#endif

	Set_System();

	topOfProcStack0 = Process_Stack_Init(Spark_Process_Task, MemProcStack0, PROCESS_STACK0_SIZE);
	topOfProcStack1 = Process_Stack_Init(Wiring_Process_Task, MemProcStack1, PROCESS_STACK1_SIZE);

	NVIC_SetPriority(PendSV_IRQn, SYSTICK_IRQ_PRIORITY);
	NVIC_SetPriority(SVCall_IRQn, SYSTICK_IRQ_PRIORITY);

	SysTick_Configuration();

	/* Enable CRC clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);

#if defined (USE_SPARK_CORE_V02)
	LED_SetRGBColor(RGB_COLOR_WHITE);
	LED_On(LED_RGB);
	SPARK_LED_FADE = 1;

#if defined (SPARK_RTC_ENABLE)
	RTC_Configuration();
#endif
#endif

#ifdef IWDG_RESET_ENABLE
	/* Check if the system has resumed from IWDG reset */
	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{
		/* IWDGRST flag set */
		IWDG_SYSTEM_RESET = 1;

		/* Clear reset flags */
		RCC_ClearFlag();
	}

	/* Set IWDG Timeout to 3 secs */
	IWDG_Reset_Enable(3 * TIMING_IWDG_RELOAD);
#endif

#ifdef DFU_BUILD_ENABLE
	Load_SystemFlags();
#endif

	__SVC();

	/* Execution should not reach here */
	while(1)
	{
	}
}

/*******************************************************************************
 * Function Name  : Timing_Decrement
 * Description    : Decrements the various Timing variables related to SysTick.
 * Input          : None
 * Output         : Timing
 * Return         : None
 *******************************************************************************/
void Timing_Decrement(void)
{
	TimingMillis++;

	if (TimingDelay != 0x00)
	{
		TimingDelay--;
	}

	if (LED_RGB_OVERRIDE != 0)
	{
		if (NULL != LED_Signaling_Override)
		{
			LED_Signaling_Override();
		}
	}
	else if (TimingLED != 0x00)
	{
		TimingLED--;
	}
	else if(WLAN_SMART_CONFIG_START || SPARK_FLASH_UPDATE || Spark_Error_Count)
	{
		//Do nothing
	}
	else if(SPARK_LED_FADE)
	{
#if defined (USE_SPARK_CORE_V02)
		LED_Fade(LED_RGB);
		if(SPARK_HANDSHAKE_COMPLETED)
			TimingLED = 20;
		else
			TimingLED = 1;
#endif
	}
	else if(SPARK_HANDSHAKE_COMPLETED)
	{
#if defined (USE_SPARK_CORE_V01)
		LED_On(LED1);
#elif defined (USE_SPARK_CORE_V02)
		LED_SetRGBColor(RGB_COLOR_CYAN);
		LED_On(LED_RGB);
#endif
		SPARK_LED_FADE = 1;
	}
	else
	{
#if defined (USE_SPARK_CORE_V01)
		LED_Toggle(LED1);
#elif defined (USE_SPARK_CORE_V02)
		LED_Toggle(LED_RGB);
#endif
		if(SPARK_SOCKET_CONNECTED)
			TimingLED = 50;		//50ms
		else
			TimingLED = 100;	//100ms
	}

#ifdef SPARK_WLAN_ENABLE
	if(!WLAN_SMART_CONFIG_START && BUTTON_GetDebouncedTime(BUTTON1) >= 3000)
	{
		BUTTON_ResetDebouncedState(BUTTON1);

		if(!SPARK_WLAN_SLEEP)
		{
			if(WLAN_DHCP && !(SPARK_SOCKET_CONNECTED & SPARK_HANDSHAKE_COMPLETED))
			{
				//Work around to exit the blocking nature of socket calls
				Spark_ConnectAbort_WLANReset();
			}

			WLAN_SMART_CONFIG_START = 1;
		}
	}
	else if(BUTTON_GetDebouncedTime(BUTTON1) >= 7000)
	{
		BUTTON_ResetDebouncedState(BUTTON1);

		WLAN_DELETE_PROFILES = 1;
	}

	if(!SPARK_WLAN_SLEEP && SPARK_HANDSHAKE_COMPLETED)
	{
		if (TimingSparkCommTimeout >= TIMING_SPARK_COMM_TIMEOUT)
		{
			TimingSparkCommTimeout = 0;

			//Work around to reset WLAN in special cases such as
			//when the server goes down during OTA upgrade
			Spark_ConnectAbort_WLANReset();
		}
		else
		{
			TimingSparkCommTimeout++;
		}
	}
#endif

#ifdef IWDG_RESET_ENABLE
	if (TimingIWDGReload >= TIMING_IWDG_RELOAD)
	{
		TimingIWDGReload = 0;

		/* Reload IWDG counter */
		IWDG_ReloadCounter();
	}
	else
	{
		TimingIWDGReload++;
	}
#endif

	//Wait for SVC_Handler to run first
	if(activeProcStack != PROCESS_STACK_NOT_ACTIVE)
	{
		NVIC_INT_CTRL = NVIC_PENDSVSET;	//Trigger PendSV
	}
}

static __IO uint32_t *Process_Stack_Init(void (*processTask)(void), __IO uint8_t *procStackBuffer, uint32_t procStackSize)
{
	/* Initialize memory reserved for Process Stack */
	for(uint32_t index = 0; index < procStackSize; index++)
	{
		procStackBuffer[index] = 0x00;
	}

	/* Set Process stack value */
	uint32_t *topOfProcStack = (uint32_t *)((uint32_t)procStackBuffer + procStackSize);

	topOfProcStack--;
	*topOfProcStack = 0x01000000UL;				/* xPSR */
	topOfProcStack--;
	*topOfProcStack = (uint32_t)processTask;	/* PC */
	topOfProcStack--;
	*topOfProcStack = 0;						/* LR */
	topOfProcStack -= 5;						/* R12, R3, R2 and R1. */
	*topOfProcStack = NULL;						/* R0 */
	topOfProcStack -= 8;						/* R11, R10, R9, R8, R7, R6, R5 and R4. */

	return topOfProcStack;
}

static void Spark_Process_Task(void)
{
#ifdef SPARK_SFLASH_ENABLE
	sFLASH_Init();
#endif

#ifdef SPARK_WLAN_ENABLE
	SPARK_WLAN_Setup(Multicast_Presence_Announcement);
#endif

	/* Main loop */
	while (1)
	{
		//DIO_Toggle(D0);//For Context-Switch Testing only

#ifdef SPARK_WLAN_ENABLE

		SPARK_WLAN_Loop();

		if (SPARK_SOCKET_CONNECTED)
		{
			if (!SPARK_HANDSHAKE_COMPLETED)
			{
				int err = Spark_Handshake();
				if (err)
				{
					if (0 > err)
					{
						// Wrong key error, red
						LED_SetRGBColor(0xff0000);
					}
					else if (1 == err)
					{
						// RSA decryption error, orange
						LED_SetRGBColor(0xff6000);
					}
					else if (2 == err)
					{
						// RSA signature verification error, magenta
						LED_SetRGBColor(0xff00ff);
					}
					LED_On(LED_RGB);
				}
				else
				{
					SPARK_HANDSHAKE_COMPLETED = 1;
				}
			}

			if (!Spark_Communication_Loop())
			{
				if (LED_RGB_OVERRIDE)
				{
					LED_Signaling_Stop();
				}
				SPARK_FLASH_UPDATE = 0;
				SPARK_LED_FADE = 0;
				SPARK_HANDSHAKE_COMPLETED = 0;
				SPARK_SOCKET_CONNECTED = 0;
			}
		}
#endif
	}
}

static void Wiring_Process_Task(void)
{
#ifdef SPARK_WIRING_ENABLE
	if(NULL != setup)
	{
		setup();
	}
#endif

	while(1)
	{
		//DIO_Toggle(D1);//For Context-Switch Testing only

#ifdef SPARK_WIRING_ENABLE
		if(!SPARK_FLASH_UPDATE && !IWDG_SYSTEM_RESET)
		{
			if(NULL != loop)
			{
				//Execute user application loop
				loop();
			}

			userEventSend();
		}
#endif
	}
}

/* For Context-Switch Testing only */
//void DIO_Toggle(DIO_TypeDef Dx){
//	DIO_SetState(Dx, HIGH);
//	delay(50);
//	DIO_SetState(Dx, LOW);
//	delay(50);
//}

/*******************************************************************************
 * Function Name  : USB_USART_Init
 * Description    : Start USB-USART protocol.
 * Input          : baudRate.
 * Return         : None.
 *******************************************************************************/
void USB_USART_Init(uint32_t baudRate)
{
	linecoding.bitrate = baudRate;
	USB_Disconnect_Config();
	Set_USBClock();
	USB_Interrupts_Config();
	USB_Init();
}

/*******************************************************************************
 * Function Name  : USB_USART_Available_Data.
 * Description    : Return the length of available data received from USB.
 * Input          : None.
 * Return         : Length.
 *******************************************************************************/
uint8_t USB_USART_Available_Data(void)
{
	if(bDeviceState == CONFIGURED)
	{
		if(USB_Rx_State == 1)
		{
			return (USB_Rx_length - USB_Rx_ptr);
		}
	}

	return 0;
}

/*******************************************************************************
 * Function Name  : USB_USART_Receive_Data.
 * Description    : Return data sent by USB Host.
 * Input          : None
 * Return         : Data.
 *******************************************************************************/
int32_t USB_USART_Receive_Data(void)
{
	if(bDeviceState == CONFIGURED)
	{
		if(USB_Rx_State == 1)
		{
			if((USB_Rx_length - USB_Rx_ptr) == 1)
			{
				USB_Rx_State = 0;

				/* Enable the receive of data on EP3 */
				SetEPRxValid(ENDP3);
			}

			return USB_Rx_Buffer[USB_Rx_ptr++];
		}
	}

	return -1;
}

/*******************************************************************************
 * Function Name  : USB_USART_Send_Data.
 * Description    : Send Data from USB_USART to USB Host.
 * Input          : Data.
 * Return         : None.
 *******************************************************************************/
void USB_USART_Send_Data(uint8_t Data)
{
	if(bDeviceState == CONFIGURED)
	{
		USART_Rx_Buffer[USART_Rx_ptr_in] = Data;

		USART_Rx_ptr_in++;

		/* To avoid buffer overflow */
		if(USART_Rx_ptr_in == USART_RX_DATA_SIZE)
		{
			USART_Rx_ptr_in = 0;
		}
	}
}

/*******************************************************************************
 * Function Name  : Handle_USBAsynchXfer.
 * Description    : send data to USB.
 * Input          : None.
 * Return         : None.
 *******************************************************************************/
void Handle_USBAsynchXfer (void)
{

	uint16_t USB_Tx_ptr;
	uint16_t USB_Tx_length;

	if(USB_Tx_State != 1)
	{
		if (USART_Rx_ptr_out == USART_RX_DATA_SIZE)
		{
			USART_Rx_ptr_out = 0;
		}

		if(USART_Rx_ptr_out == USART_Rx_ptr_in)
		{
			USB_Tx_State = 0;
			return;
		}

		if(USART_Rx_ptr_out > USART_Rx_ptr_in) /* rollback */
				{
			USART_Rx_length = USART_RX_DATA_SIZE - USART_Rx_ptr_out;
				}
		else
		{
			USART_Rx_length = USART_Rx_ptr_in - USART_Rx_ptr_out;
		}

		if (USART_Rx_length > VIRTUAL_COM_PORT_DATA_SIZE)
		{
			USB_Tx_ptr = USART_Rx_ptr_out;
			USB_Tx_length = VIRTUAL_COM_PORT_DATA_SIZE;

			USART_Rx_ptr_out += VIRTUAL_COM_PORT_DATA_SIZE;
			USART_Rx_length -= VIRTUAL_COM_PORT_DATA_SIZE;
		}
		else
		{
			USB_Tx_ptr = USART_Rx_ptr_out;
			USB_Tx_length = USART_Rx_length;

			USART_Rx_ptr_out += USART_Rx_length;
			USART_Rx_length = 0;
		}
		USB_Tx_State = 1;
		UserToPMABufferCopy(&USART_Rx_Buffer[USB_Tx_ptr], ENDP1_TXADDR, USB_Tx_length);
		SetEPTxCount(ENDP1, USB_Tx_length);
		SetEPTxValid(ENDP1);
	}

}

/*******************************************************************************
 * Function Name  : Get_SerialNum.
 * Description    : Create the serial number string descriptor.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void Get_SerialNum(void)
{
	uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

	Device_Serial0 = *(uint32_t*)ID1;
	Device_Serial1 = *(uint32_t*)ID2;
	Device_Serial2 = *(uint32_t*)ID3;

	Device_Serial0 += Device_Serial2;

	if (Device_Serial0 != 0)
	{
		IntToUnicode (Device_Serial0, &Virtual_Com_Port_StringSerial[2] , 8);
		IntToUnicode (Device_Serial1, &Virtual_Com_Port_StringSerial[18], 4);
	}
}

/*******************************************************************************
 * Function Name  : HexToChar.
 * Description    : Convert Hex 32Bits value into char.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
	uint8_t idx = 0;

	for( idx = 0 ; idx < len ; idx ++)
	{
		if( ((value >> 28)) < 0xA )
		{
			pbuf[ 2* idx] = (value >> 28) + '0';
		}
		else
		{
			pbuf[2* idx] = (value >> 28) + 'A' - 10;
		}

		value = value << 4;

		pbuf[ 2* idx + 1] = 0;
	}
}

#ifdef USE_FULL_ASSERT
/*******************************************************************************
 * Function Name  : assert_failed
 * Description    : Reports the name of the source file and the source line number
 *                  where the assert_param error has occurred.
 * Input          : - file: pointer to the source file name
 *                  - line: assert_param error line source number
 * Output         : None
 * Return         : None
 *******************************************************************************/
void assert_failed(uint8_t* file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
	{
	}
}
#endif
