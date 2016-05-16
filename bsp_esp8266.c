#include "bsp_esp8266.h"

ESP8266_Type esp8266;

/*
需要移植的代码：
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//ESP8266调试输出串口函数,输出接收到的ESP8266数据到另外一个串口(只在开启调试模式下有效)
#if ESP8266_DEBUG_Enable == 1
	#define DEBUG_RECV_PRINTF(data)		HAL_USART2_SendOneByte(data)
#endif

//ESP8266电源IO控制
#define ESP8266_P_DN		GPIO_SetBits(GPIOA, GPIO_Pin_12)		//使能ESP8266电源
#define ESP8266_P_EN		GPIO_ResetBits(GPIOA, GPIO_Pin_12)	//关闭ESP8266电源


//接口函数-10毫秒级延时函数
#define BSP_ESP8266_Delay10ms_Port(ms)		CoTickDelay(ms*10)

//接口函数-发送命令函数(串口发送一串字符串)
#define BSP_ESP8266_SendCmd_Port(p_str)		HAL_USART1_SendStr(p_str)

//接口函数-发送数据函数(串口发送指定长度的数据)
#define BSP_ESP8266_SendData_Port(p_data,len)		HAL_USART1_SendData(p_data,len)

//接口函数-串口超时计数器开启/关闭函数
#define BSP_ESP8266_TimeOutStart_Port()			HAL_TIM2_TimeOut_Start()		//开启计数器并归零计数器	
#define BSP_ESP8266_TimeOutStop_Port()			HAL_TIM2_TimeOut_Stop()			//关闭计数器


/*
 ************************************************************
 *  名称：	BSP_ESP8266_IO_Init_Port()
 *  功能：	接口函数-ESP8266连接引脚初始化(如果开启调试模式,建议波特率小于调试输出串口波特率)
 *	输入：  rate-串口波特率Hz  timeout-串口一帧数据超时时间ms
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_IO_Init_Port(u32 rate,u16 timeout)
{
	//-----------------ESP8266电源IO初始化-----------------
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA ,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	//推挽输出
	GPIO_Init(GPIOA, &GPIO_InitStructure);	//PA12
	
	ESP8266_P_EN;	//ESP8266电源打开
	
	BSP_ESP8266_Delay10ms_Port(200);	//等待2S
	
	//-----------------串口接收超时定时器初始化-----------------
	HAL_TIM2_TimeOut_Init(timeout);
	BSP_ESP8266_TimeOutStop_Port();
	
	//-----------------USART IO初始化-----------------
	HAL_USART1_Init(rate);
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_IO_Default_Port()
 *  功能：	接口函数-ESP8266连接引脚失能（除电源控制引脚外）
 *	输入：  无
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_IO_Default_Port(void)
{
	//-----------------串口接收超时定时器失能-----------------
	HAL_TIM2_TimeOut_Default();
	
	//-----------------USART IO失能-----------------
	HAL_USART1_Default();
}


/*
 ************************************************************
 *  名称：	BSP_ESP8266_RecvFinished_Hook()
 *  功能：	ESP8266接收数据包完成后调用的钩子函数，可在此函数内对数据进行处理，由于是在中断中进行处理，所以函数内不可调用延时函数！
 			(接收到的数据ESP8266.recv_data[x]  接收到的字节数ESP8266.recv_data_len)
 *	输入：  无
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_RecvDataFinished_Hook(void)
{
	__NOP();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*
 ************************************************************
 *  名称：	BSP_ESP8266_StartingUp()
 *  功能：	ESP8266开机
 *	输入：  rate-串口波特率Hz  timeout-串口一帧数据超时时间ms
 *	输出：  true-开机成功  false-开机失败
 ************************************************************
*/
bool BSP_ESP8266_StartingUp(u32 rate,u16 timeout)
{
	esp8266.flag_timer_sta = 0;
  
	//ESP8266 IO初始化
	BSP_ESP8266_IO_Init_Port(rate,timeout);		
	
	esp8266.recv_point = 0;
	esp8266.flag_handle_cmd = 0;
	esp8266.recv_data_len = 0;

	return TRUE;
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_ShutDown()
 *  功能：	ESP8266关机
 *	输入：  无
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_ShutDown(void)
{
	//ESP8266电源关闭
	ESP8266_P_DN;
	
	//ESP8266 IO失能
	BSP_ESP8266_IO_Default_Port();

	//等待ESP8266关闭
	BSP_ESP8266_Delay10ms_Port(100);
	
	esp8266.flag_timer_sta = 0;
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_SendRecCmd()
 *  功能：	ESP8266 发送接收命令
 *	输入：  *cmd-发送的命令  *ack-要接收的命令  times-等待接收命令时间,单位s
 *	输出：  false-失败  true-成功
 ************************************************************
*/
bool BSP_ESP8266_SendRecCmd(char *cmd, char *ack, u8 times)
{
	if(times == 0) return FALSE;
	
	while(esp8266.flag_timer_sta == 1)	//等待接收完成一帧	
	{
		BSP_ESP8266_Delay10ms_Port(2);	//20ms
	}
	
	esp8266.recv_point = 0;
	esp8266.flag_handle_cmd = 0;
	esp8266.recv_buf[0] = 0;
	
	//发送命令
	BSP_ESP8266_SendCmd_Port(cmd);
	
	//等待检测回应命令
	while(1)
	{
		for(u8 i=0;i<10;i++)
		{
			BSP_ESP8266_Delay10ms_Port(10);	//100ms
		
			if(esp8266.flag_handle_cmd == 1)	//判断是否接收完成一帧
			{
				if(strstr((char*)esp8266.recv_buf,ack))	return TRUE;
				else return FALSE;
			}
		}
		
		times--;
		if(times == 0) return FALSE;
	}
}

/////////////////////////////////////////////////////////////////AT指令部分/////////////////////////////////////////////////////////////////

/*
 ************************************************************
 *  名称：	BSP_ESP8266_InitCheck()
 *  功能：	ESP8266 初始化检测
 *	输入：  无
 *	输出：  TRUE-初始化完成    FALSE-发送"AT"命令不回应
 ************************************************************
*/
bool BSP_ESP8266_InitCheck(void)
{
	u8 i;

	i=6;
	while(1)
	{
		if(TRUE == BSP_ESP8266_SendRecCmd("AT\r\n","OK",1)) break;
		
		BSP_ESP8266_Delay10ms_Port(200);
		i--;
		if(i == 0) return FALSE;
	}
	
	return TRUE;
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_VersionsGet()
 *  功能：	ESP8266 获取版本信息(方便调试的时候查看版本信息)
 *	输入：  无
 *	输出：  TRUE-成功   FALSE-失败
 ************************************************************
*/
bool BSP_ESP8266_VersionsGet(void)
{
	return BSP_ESP8266_SendRecCmd("AT+GMR\r\n","OK",2);
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_EchoSet()
 *  功能：	ESP8266 回显设定
 *	输入：  0-关闭回显  1-开启回显
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_EchoSet(u8 sta)
{
	if(sta == 0)
	{
		return BSP_ESP8266_SendRecCmd("ATE0\r\n","OK",2);
	}
	else if(sta == 1)
	{
		return BSP_ESP8266_SendRecCmd("ATE1\r\n","OK",2);
	}
	else
	{
		return FALSE;
	}
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_WifiModeSet()
 *  功能：	ESP8266 WIFI模式选择
 *	输入：  mode-1,Station模式  2,AP模式  3,AP兼Station模式
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_WifiModeSet(u8 mode)
{
	if(mode == 1)
	{
		return BSP_ESP8266_SendRecCmd("AT+CWMODE=1\r\n","OK",2);
	}
	else if(mode == 2)
	{
		return BSP_ESP8266_SendRecCmd("AT+CWMODE=2\r\n","OK",2);
	}
	else if(mode == 3)
	{
		return BSP_ESP8266_SendRecCmd("AT+CWMODE=3\r\n","OK",2);
	}
	else
	{
		return FALSE;
	}
}

//bool BSP_ESP8266_WifiGetList()

/*
 ************************************************************
 *  名称：	BSP_ESP8266_WifiJoinIn()
 *  功能：	ESP8266 连接一个WIFI
 *	输入：  ssid-名字(字符串)
						pwd-密码(字符串最长64字节)
						timeout-超时时间s
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_WifiJoinIn(char* ssid,char* pwd)
{
	char buf[256];
	
	sprintf(buf,"AT+CWJAP=\"%s\",\"%s\"\r\n",ssid,pwd);
	
	return BSP_ESP8266_SendRecCmd(buf,"OK",30);
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_WifiExit()
 *  功能：	ESP8266 退出WIFI连接
 *	输入：  无
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_WifiExit(void)
{
	return BSP_ESP8266_SendRecCmd("AT+CWQAP\r\n","OK",6);
}


/*
 ************************************************************
 *  名称：	BSP_ESP8266_TransmissionModeSet()
 *  功能：	ESP8266 传输模式选择
 *	输入：  mode-0,非透传模式  1,透传模式
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_TransmissionModeSet(u8 mode)
{
	if(mode == 0)
	{
		return BSP_ESP8266_SendRecCmd("AT+CIPMODE=0\r\n","OK",2);
	}
	else if(mode == 1)
	{
		return BSP_ESP8266_SendRecCmd("AT+CIPMODE=1\r\n","OK",2);
	}
	else
	{
		return FALSE;
	}
}
/*
 ************************************************************
 *  名称：	BSP_ESP8266_TcpGetStatus()
 *  功能：	ESP8266 获取TCP/UDP连接状态
 *	输入：  *status-返回连接状态 2获得IP  3建立连接  4失去连接
						timeout-超时时间s
 *	输出：  FALSE-设定失败  TRUE-设定成功
 ************************************************************
*/
bool BSP_ESP8266_TcpGetStatus(u8* status)
{
	char *p;
	
	if(TRUE == BSP_ESP8266_SendRecCmd("AT+CIPSTATUS\r\n","OK",4))
	{
		p = strstr((char*)esp8266.recv_buf, "STATUS:");
		*status = atoi(p+7);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_TcpConnect()
 *  功能：	ESP8266 建立TCP连接
 *	输入：  ipx-IP地址  port-端口号
 *	输出：  false-失败  true-成功
 ************************************************************
*/
bool BSP_ESP8266_TcpConnect(u8 ip1,u8 ip2,u8 ip3,u8 ip4,u16 port)
{
	char buf[60];
	
	sprintf(buf,"AT+CIPSTART=\"TCP\",\"%1d.%1d.%1d.%1d\",%1d\r\n",
          ip1,ip2,ip3,ip4,port);

	return BSP_ESP8266_SendRecCmd(buf,"CONNECT",30);
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_TcpSend()
 *  功能：	ESP8266 通过TCP连接发送数据
 *	输入：  data-数据  len-数据长度  timeout-发送超时时间
 *	输出：  false-失败  true-成功
 ************************************************************
*/
bool BSP_ESP8266_TcpSend(u8* data,u16 len,u16 timeout)
{
	char buf[32];
	
	sprintf(buf,"AT+CIPSEND=%1d\r\n",len);
	if(TRUE == BSP_ESP8266_SendRecCmd(buf,">",2))
	{
		BSP_ESP8266_SendData_Port(data,len);
		return BSP_ESP8266_SendRecCmd("","SEND OK",timeout);
	}
	else
	{
		return FALSE;
	}
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_TcpClose()
 *  功能：	ESP8266 关闭TCP连接
 *	输入：  无
 *	输出：  false-失败  true-成功
 ************************************************************
*/
bool BSP_ESP8266_TcpClose(void)
{
	u8 i=3;
	while(1)
	{
		if(TRUE == BSP_ESP8266_SendRecCmd("AT+CIPCLOSE\r\n","CLOSED",20))
		{
			return TRUE;
		}
		else
		{
			i--;
			if(i == 0)
			{
				return FALSE;
			}
		}
	}
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 ************************************************************
 *  名称：	BSP_ESP8266_RecvIRQ()
 *  功能：  ESP8266串口接收函数（需要挂在到串口接收中断函数里）
 *	输入：  data-串口接收到的数据
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_RecvIRQ(u8 data)
{
	BSP_ESP8266_TimeOutStart_Port();	//定时器清零
	esp8266.flag_timer_sta = 1;	
	
	if(esp8266.recv_point < (ESP8266_RECVBUF_NUM-1))
	{
		esp8266.recv_buf[esp8266.recv_point++] = data;	//接收数据到缓冲区
		esp8266.recv_buf[esp8266.recv_point] = 0;		//最后一个字符已0结尾 方便strstr查找
	}
	else
	{
		esp8266.recv_point = 0;
	}
#if ESP8266_DEBUG_Enable == 1	
	DEBUG_RECV_PRINTF(data);
#endif
}

/*
 ************************************************************
 *  名称：	BSP_ESP8266_RecvTimeOutIRQ()
 *  功能：  ESP8266串口接收超时函数（需要挂在到定时器溢出中断函数里）
 *	输入：  无
 *	输出：  无
 ************************************************************
*/
void BSP_ESP8266_RecvTimeOutIRQ(void)
{
	BSP_ESP8266_TimeOutStop_Port();	//定时器关闭
	esp8266.flag_timer_sta = 0;
	
	esp8266.flag_handle_cmd = 1;
	
	//----------------判断并接收TCP数据包(最多处理2连包数据)----------------
	u16 temp,i;
	
	//第一包数据
	char *p1 = strstr((char*)esp8266.recv_buf,"+IPD,");
	if(p1)
	{
		esp8266.recv_data_len = atoi(p1+5);		//获取接收字节数

		temp = esp8266.recv_data_len;		//计算ESP8266.recv_data_len的位数	
		i=0;
		while(temp)
		{
			temp = temp/10;
			i++;
		}
        
		p1 = p1+i+6;		//p1指向数据1的首地址

		for(i=0;i<esp8266.recv_data_len;i++)		//取出接收到的数据1
		{
			esp8266.recv_data[i] = p1[i];
		}
		
//		//第二包数据
//		char *p2 = strstr((char*)&p1[i+1],"+IPD,");	
//		if(p2)
//		{
//			u16 len1 = esp8266.recv_data_len;
//			u16 len2 = atoi(p2+5);	//获取接收字节数
//			esp8266.recv_data_len = len1+len2;	//一二包总接收字节数
//			
//			temp = len2;		
//			i=0;
//			while(temp)
//			{
//				temp = temp/10;
//				i++;
//			}
//			
//			p2 = p2+i+6;		//p2指向数据2的首地址
//			for(i=0;i<len2;i++)
//			{
//				esp8266.recv_data[len1+i] = p2[i];
//			}
//		}

		esp8266.recv_point = 0;
		esp8266.flag_handle_cmd = 0;
		esp8266.recv_buf[0] = 0;
		
		BSP_ESP8266_RecvDataFinished_Hook();		//接收完成钩子函数
		
		esp8266.recv_data_len = 0;
	}
}




