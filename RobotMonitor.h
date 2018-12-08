/**
  ******************************************************************************
  * @file      RobotMonitor.h
  * @author    M3chD09
  * @brief     Header file of RobotMonitor.c
  * @version   1.0
  * @date      8th Dec 2018
  ******************************************************************************

  ******************************************************************************
  */
#ifndef _ROBOTMONITOR_H_
#define _ROBOTMONITOR_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "cmsis_os.h"

extern osThreadId robotMonitorHandle;

void receiveRobotMonitorRxBuf(void);
void tskRobotMonitor(void const * argument);

#ifdef __cplusplus
}
#endif

#endif
