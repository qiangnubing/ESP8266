#ifndef __BSP_ESP8266_H__
#define __BSP_ESP8266_H__

/*----添加需要包含的头文件----*/
//////////////////////////////////////////////////////////////////
#include "hal_usart1.h"
#include "hal_tim2.h"
#include <CoOS.h>

#include "hal_usart2.h"
//////////////////////////////////////////////////////////////////

/*----需要挂载的函数（非常重要）----*/
////////////////////////////////////////////////////////////////////////////////////////////////
void BSP_ESP8266_RecvIRQ(u8 data);		//需要挂载到串口接收中断函数里 data-接收到的数据
void BSP_ESP8266_RecvTimeOutIRQ(void);		//需要挂载到定时器溢出中断函数里
////////////////////////////////////////////////////////////////////////////////////////////////

/*----参数配置----*/
////////////////////////////////////////////////////////////////////////
/*!<
	是否使用ESP8266调试模式
	0-不使用
	1-使用
*/
#define ESP8266_DEBUG_Enable 1

/*!<
	SIM900串口接收缓冲区大小
*/
#define ESP8266_RECVBUF_NUM 2048

/*!<
	ESP8266接收数据包缓冲区大小
*/
#define ESP8266_RECVDATA_NUM 2000
////////////////////////////////////////////////////////////////////////

typedef struct
{
  volatile u8 recv_buf[ESP8266_RECVBUF_NUM];
  volatile u8 recv_data[ESP8266_RECVDATA_NUM];
  volatile u16 recv_point;
  volatile u16 recv_data_len;
	
	volatile u8 flag_timer_sta;	//溢出中断定时器状态标志位 0-关闭 1-计时中
	volatile u8 flag_handle_cmd;	//接收完成标志位 0-未完成  1-接收完成需要处理
}ESP8266_Type;
extern ESP8266_Type esp8266;


bool BSP_ESP8266_SendRecCmd(char *cmd, char *ack, u8 times);

bool BSP_ESP8266_StartingUp(u32 rate,u16 timeout);
void BSP_ESP8266_ShutDown(void);

bool BSP_ESP8266_InitCheck(void);
bool BSP_ESP8266_VersionsGet(void);
bool BSP_ESP8266_EchoSet(u8 sta);

bool BSP_ESP8266_WifiModeSet(u8 mode);
bool BSP_ESP8266_WifiJoinIn(char* ssid,char* pwd);
bool BSP_ESP8266_WifiExit(void);

bool BSP_ESP8266_TransmissionModeSet(u8 mode);
bool BSP_ESP8266_TcpGetStatus(u8* status);
bool BSP_ESP8266_TcpConnect(u8 ip1,u8 ip2,u8 ip3,u8 ip4,u16 port);
bool BSP_ESP8266_TcpSend(u8* data,u16 len,u16 timeout);
bool BSP_ESP8266_TcpClose(void);

#endif
