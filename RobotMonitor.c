/**
  ******************************************************************************
  * @file      RobotMonitor.c
  * @author    M3chD09
  * @brief     Robot monitor handler, communicating with NUC
  * @version   1.0
  * @date      8th Dec 2018
  ******************************************************************************

  ==============================================================================
                     ##### How to use this tool #####
  ==============================================================================
    [..]
    Change the value of RM_UART in file "RobotMonitor.c" to the UART used to communicate with NUC. The default value is huart1.
    [..]
    Add the following code in file *stm32f4xx_it.c* and *freertos.c*.
    ***CODE BEGIN***
    #include "RobotMonitor.h"
    ***CODE END***
    [..]
    Call "receiveRobotMonitorRxBuf()" in function "USARTx_IRQHandler".
    [..]
    Add the following code between "USER CODE BEGIN RTOS_THREADS" and "USER CODE END RTOS_THREADS" in "freertos.c".
    ***CODE BEGIN***
    osThreadDef(robotMonitorTask, tskRobotMonitor, osPriorityNormal, 0, 1024);
    robotMonitorHandle = osThreadCreate(osThread(robotMonitorTask), NULL);
    ***CODE END***


  ******************************************************************************
  */
#include "RobotMonitor.h"

/* UART define */
/* Change huart1 to the UART you use to communicate with the NUC */
#define RM_UART huart1

/** @defgroup RM_board_define board ID define
  * @brief borad ID      
  * @{
  */
#define RM_BOARD_1 0x01
#define RM_BOARD_2 0x02
#define RM_BOARD_3 0x03
/**
  * @}
  */

/** @defgroup RM_act_define action information define
  * @brief action information     
  * @{
  */
#define RM_ACT_READ 0x01
#define RM_ACT_READRETURN 0x02
#define RM_ACT_WRITE 0x03
#define RM_ACT_WRITERETURN 0x04
/**
  * @}
  */

/** @defgroup RM_error_define error information define
  * @brief  error information     
  * @{
  */
#define RM_ERROR_NOSUCHBOARD 0xfe
#define RM_ERROR_NOSUCHACT 0xfd
#define RM_ERROR_NOSUCHADDR 0xfc
/**
  * @}
  */

/** 
  * @brief robot monitor data structure definition  
  */
typedef struct
{
	uint8_t board:8;        /*!< The ID of the borad to be operated.
                               This parameter can be any value of @ref RM_board_define */
	
	uint8_t act:8;          /*!< The action to be performed or the error information to be sent.
                               This parameter can be any value of @ref RM_act_define or @ref RM_error_define */
	
	uint8_t dataNum:8;      /*!< Size amount of data to be operated.
                               This parameter must be a number between Min_Data = 0 and Max_Data = 8. */
	
	uint32_t addr:32;       /*!< The address of MCU to be operated.
                               This parameter must be a number between Min_Data = 0x20000000 and Max_Data = 0x80000000. */
	
	uint64_t dataBuf:64;    /*!< The data read or to be written.
                                This parameter must be a variable of type uint64_t */
	
}__attribute__((packed)) robotMonitorData_t;

/** 
  * @brief robot monitor data to receive union definition  
  */
typedef union
{
	robotMonitorData_t robotMonitorRxData;
	uint8_t robotMonitorRxBuf[15];
}robotMonitorRxUnion_t;
robotMonitorRxUnion_t robotMonitorRxUnion;

/** 
  * @brief robot monitor data to transmit structure definition  
  */
typedef union
{
	robotMonitorData_t robotMonitorTxData;
	uint8_t robotMonitorTxBuf[15];
}robotMonitorTxUnion_t;
robotMonitorTxUnion_t robotMonitorTxUnion;

/* definition of thread */
osThreadId robotMonitorHandle;
/* the flag of the message received form NUC */
uint8_t isGetRobotMonitor;

/**
  * @brief  Receives robot monitor data in idle interrupt mode.
  * @param  None
  * @retval None
  */
void receiveRobotMonitorRxBuf(void)
{
	uint32_t temp=temp;
	
	/* Check if it is an idle interrupt */
	if((__HAL_UART_GET_FLAG(&RM_UART,UART_FLAG_IDLE) != RESET))
	{
		isGetRobotMonitor=1;
		__HAL_UART_CLEAR_IDLEFLAG(&RM_UART);
		HAL_UART_DMAStop(&RM_UART);
		temp = RM_UART.hdmarx->Instance->NDTR;
		
		/* Receive robot monitor data */
		HAL_UART_Receive_DMA(&RM_UART,robotMonitorRxUnion.robotMonitorRxBuf,sizeof(robotMonitorRxUnion.robotMonitorRxBuf));
		
	}
}

/**
  * @brief  Reads the variable in flash memory of the given address.
  * @param  None
  * @retval None
  */
void readFlash()
{
	/* Clear the data buffer to be sent */
	memset((uint8_t *)&robotMonitorTxUnion.robotMonitorTxBuf,0,sizeof(robotMonitorTxUnion.robotMonitorTxBuf));
	
	/* Prepare the data to send */
	robotMonitorTxUnion.robotMonitorTxData.board=robotMonitorRxUnion.robotMonitorRxData.board;
	robotMonitorTxUnion.robotMonitorTxData.act=RM_ACT_READRETURN;
	robotMonitorTxUnion.robotMonitorTxData.addr=robotMonitorRxUnion.robotMonitorRxData.addr;
	
	/* Reads the variable in flash memory */
	uint16_t dataNum;
	for(dataNum=0;dataNum<robotMonitorRxUnion.robotMonitorRxData.dataNum;dataNum++)
	{
		*(robotMonitorTxUnion.robotMonitorTxBuf+7+dataNum)=*(__IO uint8_t*)robotMonitorRxUnion.robotMonitorRxData.addr++;
	}
	robotMonitorTxUnion.robotMonitorTxData.dataNum=dataNum;
	
	/* Send return data */
	HAL_UART_Transmit_DMA(&RM_UART,robotMonitorTxUnion.robotMonitorTxBuf,7+dataNum);	
}

/**
  * @brief  Writes the given data buffer to the flash of the given address.
  * @param  None
  * @retval None
  */
void writeFlash()
{
	/* Clear the data buffer to be sent */
	memset((uint8_t *)&robotMonitorTxUnion.robotMonitorTxBuf,0,sizeof(robotMonitorTxUnion.robotMonitorTxBuf));
	
	/* Prepare the data to send */
	robotMonitorTxUnion.robotMonitorTxData.board=robotMonitorRxUnion.robotMonitorRxData.board;
	robotMonitorTxUnion.robotMonitorTxData.addr=robotMonitorRxUnion.robotMonitorRxData.addr;
	
	/* Write data buffer */
	if(HAL_FLASH_Unlock()==HAL_OK)
    if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,robotMonitorRxUnion.robotMonitorRxData.addr,robotMonitorRxUnion.robotMonitorRxData.dataBuf)==HAL_OK)
			if(HAL_FLASH_Lock()==HAL_OK)
				robotMonitorTxUnion.robotMonitorTxData.act=RM_ACT_WRITERETURN;

	/* Send return data */
	HAL_UART_Transmit_DMA(&RM_UART,robotMonitorTxUnion.robotMonitorTxBuf,7);
}

/**
  * @brief  Send an error message to the NUC.
  * @param  err: the error information.
  * @retval None
  */
void returnError(uint8_t err)
{
	/* Clear the data buffer to be sent */
	memset((uint8_t *)&robotMonitorTxUnion.robotMonitorTxBuf,0,sizeof(robotMonitorTxUnion.robotMonitorTxBuf));
	
	/* Prepare the data to send */
	robotMonitorTxUnion.robotMonitorTxData.board=robotMonitorRxUnion.robotMonitorRxData.board;
	robotMonitorTxUnion.robotMonitorTxData.act=err;
	robotMonitorTxUnion.robotMonitorTxData.addr=robotMonitorRxUnion.robotMonitorRxData.addr;
	robotMonitorTxUnion.robotMonitorTxData.dataNum=robotMonitorRxUnion.robotMonitorRxData.dataNum;
	
	/* Send the error message */
	HAL_UART_Transmit_DMA(&RM_UART,robotMonitorTxUnion.robotMonitorTxBuf,8);
}

/**
  * @brief  Function implementing the robotMonitorTask thread
  * @param  argument: Not used
  * @retval None
  */
void tskRobotMonitor(void const * argument)
{
	HAL_UART_Receive_DMA(&RM_UART,robotMonitorRxUnion.robotMonitorRxBuf,sizeof(robotMonitorRxUnion.robotMonitorRxBuf));
	__HAL_UART_ENABLE_IT(&RM_UART, UART_IT_IDLE);
	/* Infinite loop */
	for(;;)
	{
		/* Check if data is received */
		if (isGetRobotMonitor)
		{
			
			/* Check if it is a valid board ID */
			if(robotMonitorRxUnion.robotMonitorRxData.board==RM_BOARD_1)
			{
				
				/* Check if it is a valid action information and execute */
				if(robotMonitorRxUnion.robotMonitorRxData.act==RM_ACT_READ)
				{
					readFlash();
				}
				else if(robotMonitorRxUnion.robotMonitorRxData.act==RM_ACT_WRITE)
				{
					writeFlash();
				}
				else
				{
					returnError(RM_ERROR_NOSUCHACT);
				}
				
			}
			else if(robotMonitorRxUnion.robotMonitorRxData.board==RM_BOARD_2||robotMonitorRxUnion.robotMonitorRxData.board==RM_BOARD_3)
			{
				//TODO
			}
			else
			{
				returnError(RM_ERROR_NOSUCHBOARD);
			}
			isGetRobotMonitor=0;
		}
		osDelay(1);
	}
}
