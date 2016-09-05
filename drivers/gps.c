/**
  ******************************************************************************
  * @file    gps.c
  * @author  w
  * @version V1.0
  * @Library
  * @date    05/18/2015
  * @brief   drivers for gps
  *****************************************************************************
**/

/* Includes ------------------------------------------------------------------*/
#include "gps.h"

#include "stdio.h"	 
#include "stdarg.h"	 
#include "string.h"	 
#include "math.h"
#include "led.h"
#include "usart.h"

/* Global Variables ----------------------------------------------------------*/
GPSDATA_ST stgpsdata;             // ʵʱ��ȡ��gps����

u8 Choose_mode;
nmea_msg gpsx;        //GPS��Ϣ
extern u8 num;
u16 jing;

/* Private Macros ------------------------------------------------------------*/

/* Private Variables ---------------------------------------------------------*/

/* Private Functions ---------------------------------------------------------*/

/* Public Functions ----------------------------------------------------------*/

//��buf����õ���cx���������ڵ�λ��
//����ֵ:0~0XFE,����������λ�õ�ƫ��.
//       0XFF,�������ڵ�cx������		
u8 const ASCII[10]={48,49,50,51,52,53,54,55,56,57};
void change(u8 *buf,u16 m)
{
	u8 wei[5],i;
	wei[0]=m/10000;
	wei[1]=m%10000/1000;
	wei[2]=m%1000/100;
	wei[3]=m%100/10;
	wei[4]=m%10;
	
	for(i=0;i<=4;i++)
	{
		buf[i+1]=ASCII[wei[i]];
//		printf("%d\r\n",buf[i+1]);
	}
	buf[0]=5;
}

u8 NMEA_Comma_Pos(u8 *buf,u8 cx)
{	 		    
	u8 *p=buf;
	while(cx)
	{		 
		if(*buf=='*'||*buf<' '||*buf>'z')return 0XFF;//����'*'���߷Ƿ��ַ�,�򲻴��ڵ�cx������
		if(*buf==',')cx--;
		buf++;
	}
	return buf-p;	 
}

//m^n����
//����ֵ:m^n�η�.
u32 NMEA_Pow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}

//strת��Ϊ����,��','����'*'����
//buf:���ִ洢��
//dx:С����λ��,���ظ����ú���
//����ֵ:ת�������ֵ
int NMEA_Str2num(u8 *buf,u8*dx)
{
	u8 *p=buf;
	u32 ires=0,fres=0;
	u8 ilen=0,flen=0,i;
	u8 mask=0;
	int res;
 
	while(1) //�õ�������С���ĳ���
	{
		if(*p=='-'){mask|=0X02;p++;}//�Ǹ���
		if(*p==','||(*p=='*'))break;//����������
		if(*p=='.'){mask|=0X01;p++;}//����С������
		else if(*p>'9'||(*p<'0'))	//�зǷ��ַ�
		{	
			ilen=0;
			flen=0;
			break;
		}	
		if(mask&0X01)flen++;
		else ilen++;
		p++;
	}
	if(mask&0X02)buf++;	//ȥ������
	for(i=0;i<ilen;i++)	//�õ�������������
	{  
		ires+=NMEA_Pow(10,ilen-1-i)*(buf[i]-'0');
	}
    //if(flen>5)flen=5;	//���ȡ5λС��
	*dx=flen;	 		//С����λ��
	for(i=0;i<flen;i++)	//�õ�С����������
	{  
		fres+=NMEA_Pow(10,flen-1-i)*(buf[ilen+1+i]-'0');
	} 
	res=ires*NMEA_Pow(10,flen)+fres;
	if(mask&0X02)res=-res;		   
	return res;
}

//����GPGSV��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void NMEA_GPGSV_Analysis(nmea_msg *gpsx,u8 *buf)
{
	u8 *p,*p1,dx;
	u8 len,i,j,slx=0;
	u8 posx;   	 

	p=buf;
	p1=(u8*)strstr((const char *)p,"$GPGSV");
	len=p1[7]-'0';								//�õ�GPGSV������
	posx=NMEA_Comma_Pos(p1,3); 					//�õ��ɼ���������

	if(posx!=0XFF)
        gpsx->svnum=NMEA_Str2num(p1+posx,&dx);

	for(i=0;i<len;i++)
	{	 
		p1=(u8*)strstr((const char *)p,"$GPGSV");  
		for(j=0;j<4;j++)
		{	  
			posx=NMEA_Comma_Pos(p1,4+j*4);
            
			if(posx!=0XFF)
                gpsx->slmsg[slx].num=NMEA_Str2num(p1+posx,&dx);	//�õ����Ǳ��
			else
                break; 
			
            posx=NMEA_Comma_Pos(p1,5+j*4);
			
            if(posx!=0XFF)
                gpsx->slmsg[slx].eledeg=NMEA_Str2num(p1+posx,&dx);//�õ��������� 
			else
                break;

			posx=NMEA_Comma_Pos(p1,6+j*4);

			if(posx!=0XFF)
                gpsx->slmsg[slx].azideg=NMEA_Str2num(p1+posx,&dx);//�õ����Ƿ�λ��
			else
                break; 

			posx=NMEA_Comma_Pos(p1,7+j*4);

			if(posx!=0XFF)
                gpsx->slmsg[slx].sn=NMEA_Str2num(p1+posx,&dx);	//�õ����������
			else
                break;

			slx++;	   
		}   
 		p=p1+1;//�л�����һ��GPGSV��Ϣ
	}   
}

//����GPGGA��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void NMEA_GPGGA_Analysis(nmea_msg *gpsx,u8 *buf)
{
	u8 *p1,dx;			 
	u8 posx;

    if(Choose_mode==0|Choose_mode==1)
        p1=(u8*)strstr((const char *)buf,"$GPGGA");

	if(Choose_mode==2)	
        p1=(u8*)strstr((const char *)buf,"$GBGGA");
    
    if(Choose_mode==3)	
        p1=(u8*)strstr((const char *)buf,"$GNGGA");
	
    posx=NMEA_Comma_Pos(p1,6);								//�õ�GPS״̬
	
    if(posx!=0XFF)
        gpsx->gpssta=NMEA_Str2num(p1+posx,&dx);	
    
    posx=NMEA_Comma_Pos(p1,7);								//�õ����ڶ�λ��������
	
    if(posx!=0XFF)
        gpsx->posslnum=NMEA_Str2num(p1+posx,&dx); 
    
    posx=NMEA_Comma_Pos(p1,9);								//�õ����θ߶�
	
    if(posx!=0XFF)
        gpsx->altitude=NMEA_Str2num(p1+posx,&dx)/10;	
}

//����GPGSA��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void NMEA_GPGSA_Analysis(nmea_msg *gpsx,u8 *buf)
{
	u8 *p1,dx;			 
	u8 posx; 
	u8 i;   
	
    p1=(u8*)strstr((const char *)buf,"$GNGSA");
	
    posx=NMEA_Comma_Pos(p1,2);								//�õ���λ����
	
    if(posx!=0XFF)
        gpsx->fixmode=NMEA_Str2num(p1+posx,&dx);	
	
    for(i=0;i<12;i++)										//�õ���λ���Ǳ��
	{
		posx=NMEA_Comma_Pos(p1,3+i);					 
		if(posx!=0XFF)gpsx->possl[i]=NMEA_Str2num(p1+posx,&dx);
		else break; 
	}				  
	
    posx=NMEA_Comma_Pos(p1,15);								//�õ�PDOPλ�þ�������
	
    if(posx!=0XFF)
        gpsx->pdop=NMEA_Str2num(p1+posx,&dx);  
	
    posx=NMEA_Comma_Pos(p1,16);								//�õ�HDOPλ�þ�������
	
    if(posx!=0XFF)
        gpsx->hdop=NMEA_Str2num(p1+posx,&dx);  
	
    posx=NMEA_Comma_Pos(p1,17);								//�õ�VDOPλ�þ�������
	
    if(posx!=0XFF)
        gpsx->vdop=NMEA_Str2num(p1+posx,&dx);  
}

//����GPRMC��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void NMEA_GPRMC_Analysis(nmea_msg *gpsx,u8 *buf)
{
	u8 *p1,dx;			 
	u8 posx;     
	u32 temp;	
	float rs;


    if(Choose_mode==0|Choose_mode==1)	
        p1=(u8*)strstr((const char *)buf,"$GPRMC");
	
    if(Choose_mode==2)	
        p1=(u8*)strstr((const char *)buf,"$GBRMC");
    
    if(Choose_mode==3)	
        p1=(u8*)strstr((const char *)buf,"$GNRMC");  
        //p1=(u8*)strstr((const char *)buf,"GNRMC");//"$GPRMC",������&��GPRMC�ֿ������,��ֻ�ж�GPRMC.
	
    posx=NMEA_Comma_Pos(p1,3);								//�õ�γ��
    
    if(posx!=0XFF)
	{
		temp=NMEA_Str2num(p1+posx,&dx);		   //ex:30348257 	 
		gpsx->latitude=temp/NMEA_Pow(10,dx+2);	//�õ���  ex:31
		rs=temp%NMEA_Pow(10,dx+2);				//�õ�'	  ex:5560926
		gpsx->latitude=(gpsx->latitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60)/100000.0;//ת��Ϊ  .  �� 
	}	
	posx=NMEA_Comma_Pos(p1,4);								//��γ���Ǳ�γ 
	
    if(posx!=0XFF)
        gpsx->nshemi=*(p1+posx);
	
	
 	posx=NMEA_Comma_Pos(p1,5);								//�õ�����
	
    if(posx!=0XFF)
	{												  
		temp=NMEA_Str2num(p1+posx,&dx);	

		gpsx->longitude=temp/NMEA_Pow(10,dx+2);	//�õ���
		
		rs=temp%NMEA_Pow(10,dx+2);				//�õ�'		 
		gpsx->longitude=(gpsx->longitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60)/100000.0;//ת��Ϊ  .  �� 
	}
	
    posx=NMEA_Comma_Pos(p1,6);								//������������
	
    if(posx!=0XFF)
    gpsx->ewhemi=*(p1+posx);		 
  

	
	posx=NMEA_Comma_Pos(p1,10);								//�õ���ƫ��
	
    if(posx!=0XFF)
	{
		gpsx->cipianjiao=NMEA_Str2num(p1+posx,&dx);		 	 
	}	
	
    posx=NMEA_Comma_Pos(p1,11);
	
    if(posx!=0XFF)
        gpsx->Cewhemi=*(p1+posx);	
    //printf("��ƫ��:%f %1c\r\n",gpsx->cipianjiao,gpsx->Cewhemi);	//�õ���ƫ���ַ���	
	
	posx=NMEA_Comma_Pos(p1,7);	           	//�õ�����
	
    if(posx!=0XFF)
	temp=NMEA_Str2num(p1+posx,&dx);
	gpsx->speed=temp/1000.0;
    //printf("�ٶ�:%f knot\r\n",gpsx->speed);
}

//����GPVTG��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void NMEA_GPVTG_Analysis(nmea_msg *gpsx,u8 *buf)
{
	u8 *p1,dx;
	u8 posx;
	p1=(u8*)strstr((const char *)buf,"$GNVTG");					 
	posx=NMEA_Comma_Pos(p1,7);								//�õ���������
	if(posx!=0XFF)
	{
		gpsx->speed=NMEA_Str2num(p1+posx,&dx);
		if(dx<3)gpsx->speed*=NMEA_Pow(10,3-dx);	 	 		//ȷ������1000��
	}
}

//��ȡNMEA-0183��Ϣ
//gpsx:nmea��Ϣ�ṹ��
//buf:���յ���GPS���ݻ������׵�ַ
void GPS_Analysis(nmea_msg *gpsx,u8 *buf)
{
    //NMEA_GPGSV_Analysis(gpsx,buf);	//GPGSV����
	NMEA_GPGGA_Analysis(gpsx,buf);	    //GPGGA���� 	
    //NMEA_GPGSA_Analysis(gpsx,buf);	//GPGSA����
	NMEA_GPRMC_Analysis(gpsx,buf);	    //GPRMC����
    //NMEA_GPVTG_Analysis(gpsx,buf);	//GPVTG����
}

//GPSУ��ͼ���
//buf:���ݻ������׵�ַ
//len:���ݳ���
//cka,ckb:����У����.
void Ublox_CheckSum(u8 *buf,u16 len,u8* cka,u8*ckb)
{
	u16 i;
	*cka=0;*ckb=0;
	for(i=0;i<len;i++)
	{
		*cka=*cka+buf[i];
		*ckb=*ckb+*cka;
	}
}

/////////////////////////////////////////UBLOX ���ô���/////////////////////////////////////
//���CFG����ִ�����
//����ֵ:0,ACK�ɹ�
//       1,���ճ�ʱ����
//       2,û���ҵ�ͬ���ַ�
//       3,���յ�NACKӦ��
u8 Ublox_Cfg_Ack_Check(void)
{
	u16 len=0,i;
	u8 rval=0;
	while((USART1_RX_STA&0X8000)==0 && len<100)//�ȴ����յ�Ӧ��   
	{
		len++;
		delay_ms(5);
	}

    //��ʱ����.
	if(len<250)
	{
		len=USART1_RX_STA&0X7FFF;	//�˴ν��յ������ݳ��� 
		for(i=0;i<len;i++)if(USART1_RX_BUF[i]==0XB5)break;//����ͬ���ַ� 0XB5
		if(i==len)rval=2;						//û���ҵ�ͬ���ַ�
		else if(USART1_RX_BUF[i+3]==0X00)rval=3;//���յ�NACKӦ��
		else rval=0;	   						//���յ�ACKӦ��
	}else rval=1;								//���ճ�ʱ����
    USART1_RX_STA=0;							//�������
	return rval;  
}

//���ñ���
//����ǰ���ñ������ⲿEEPROM����
//����ֵ:0,ִ�гɹ�;1,ִ��ʧ��.
u8 Ublox_Cfg_Cfg_Save(void)
{
	u8 i;
	_ublox_cfg_cfg *cfg_cfg=(_ublox_cfg_cfg *)USART1_TX_BUF;
	cfg_cfg->header=0X62B5;		//cfg header
	cfg_cfg->id=0X0906;			//cfg cfg id
	cfg_cfg->dlength=13;		//����������Ϊ13���ֽ�.		 
	cfg_cfg->clearmask=0;		//�������Ϊ0
	cfg_cfg->savemask=0XFFFF; 	//��������Ϊ0XFFFF
	cfg_cfg->loadmask=0; 		//��������Ϊ0 
	cfg_cfg->devicemask=4; 		//������EEPROM����		 
	Ublox_CheckSum((u8*)(&cfg_cfg->id),sizeof(_ublox_cfg_cfg)-4,&cfg_cfg->cka,&cfg_cfg->ckb);
	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_cfg));	//ͨ��dma���ͳ�ȥ
	for(i=0;i<6;i++)if(Ublox_Cfg_Ack_Check()==0)break;		//EEPROMд����Ҫ�ȽϾ�ʱ��,���������ж϶��
	return i==6?1:0;
}

//����NMEA�����Ϣ��ʽ
//msgid:Ҫ������NMEA��Ϣ��Ŀ,���������Ĳ�����
//      00,GPGGA;01,GPGLL;02,GPGSA;
//		03,GPGSV;04,GPRMC;05,GPVTG;
//		06,GPGRS;07,GPGST;08,GPZDA;
//		09,GPGBS;0A,GPDTM;0D,GPGNS;
//uart1set:0,����ر�;1,�������.	  
//����ֵ:0,ִ�гɹ�;����,ִ��ʧ��.
u8 Ublox_Cfg_Msg(u8 msgid,u8 uart1set)
{
	_ublox_cfg_msg *cfg_msg=(_ublox_cfg_msg *)USART1_TX_BUF;
	cfg_msg->header=0X62B5;		//cfg header
	cfg_msg->id=0X0106;			//cfg msg id
	cfg_msg->dlength=8;			//����������Ϊ8���ֽ�.	
	cfg_msg->msgclass=0XF0;  	//NMEA��Ϣ
	cfg_msg->msgid=msgid; 		//Ҫ������NMEA��Ϣ��Ŀ
	cfg_msg->iicset=1; 			//Ĭ�Ͽ���
	cfg_msg->uart1set=uart1set; //��������
	cfg_msg->uart2set=1; 	 	//Ĭ�Ͽ���
	cfg_msg->usbset=1; 			//Ĭ�Ͽ���
	cfg_msg->spiset=1; 			//Ĭ�Ͽ���
	cfg_msg->ncset=1; 			//Ĭ�Ͽ���	  
	Ublox_CheckSum((u8*)(&cfg_msg->id),sizeof(_ublox_cfg_msg)-4,&cfg_msg->cka,&cfg_msg->ckb);
	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_msg));	//ͨ��dma���ͳ�ȥ
	return Ublox_Cfg_Ack_Check();
}

//����NMEA�����Ϣ������
//baudrate:������,4800/9600/19200/38400/57600/115200/230400	  
//����ֵ:0,ִ�гɹ�;����,ִ��ʧ��(���ﲻ�᷵��0��)
u8 Ublox_Cfg_Prt(u32 baudrate)
{
	_ublox_cfg_prt *cfg_prt=(_ublox_cfg_prt *)USART1_TX_BUF;
	cfg_prt->header=0X62B5;		//cfg header
	cfg_prt->id=0X0006;			//cfg prt id
	cfg_prt->dlength=20;		//����������Ϊ20���ֽ�.	
	cfg_prt->portid=1;			//��������1
	cfg_prt->reserved=0;	 	//�����ֽ�,����Ϊ0
	cfg_prt->txready=0;	 		//TX Ready����Ϊ0
	cfg_prt->mode=0X08D0; 		//8λ,1��ֹͣλ,��У��λ
	cfg_prt->baudrate=baudrate; //����������
	cfg_prt->inprotomask=0X0007;//0+1+2
	cfg_prt->outprotomask=0X0003;//0+1
 	cfg_prt->reserved4=0; 		//�����ֽ�,����Ϊ0
 	cfg_prt->reserved5=0; 		//�����ֽ�,����Ϊ0 
	DOG=~DOG;
	Ublox_CheckSum((u8*)(&cfg_prt->id),sizeof(_ublox_cfg_prt)-4,&cfg_prt->cka,&cfg_prt->ckb);
	DOG=~DOG;
	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_prt));	//ͨ��dma���ͳ�ȥ
	delay_ms(200);				//�ȴ�������� 
	uart_init( baudrate);	//���³�ʼ������2   
	DOG=~DOG;
	return Ublox_Cfg_Ack_Check();//���ﲻ�ᷴ��0,��ΪUBLOX��������Ӧ���ڴ������³�ʼ����ʱ���Ѿ���������.
}

//����UBLOX NEO-8M��ʱ���������
//interval:������(us)
//length:������(us)
//status:��������:1,�ߵ�ƽ��Ч;0,�ر�;-1,�͵�ƽ��Ч.
//����ֵ:0,���ͳɹ�;����,����ʧ��.
u8 Ublox_Cfg_Tp(u32 interval,u32 length,signed char status)
{
	_ublox_cfg_tp *cfg_tp=(_ublox_cfg_tp *)USART1_TX_BUF;
	cfg_tp->header=0X62B5;		//cfg header
	cfg_tp->id=0X0706;			//cfg tp id
	cfg_tp->dlength=20;			//����������Ϊ20���ֽ�.
	cfg_tp->interval=interval;	//������,us
	cfg_tp->length=length;		//������,us
	cfg_tp->status=status;	   	//ʱ����������
	cfg_tp->timeref=0;			//�ο�UTC ʱ��
	cfg_tp->flags=0;			//flagsΪ0
	cfg_tp->reserved=0;		 	//����λΪ0
	cfg_tp->antdelay=820;    	//������ʱΪ820ns
	cfg_tp->rfdelay=0;    		//RF��ʱΪ0ns
	cfg_tp->userdelay=0;    	//�û���ʱΪ0ns
	Ublox_CheckSum((u8*)(&cfg_tp->id),sizeof(_ublox_cfg_tp)-4,&cfg_tp->cka,&cfg_tp->ckb);
	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_tp));	//ͨ��dma���ͳ�ȥ
	return Ublox_Cfg_Ack_Check();
}

//����UBLOX NEO-8M�ĸ�������	    
//measrate:����ʱ��������λΪms�����ٲ���С��200ms��5Hz��
//reftime:�ο�ʱ�䣬0=UTC Time��1=GPS Time��һ������Ϊ1��
//����ֵ:0,���ͳɹ�;����,����ʧ��.
u8 Ublox_Cfg_Rate(u16 measrate,u8 reftime)
{
	_ublox_cfg_rate *cfg_rate=(_ublox_cfg_rate *)USART1_TX_BUF;
    //if(measrate<200)return 1;	//С��200ms��ֱ���˳�
 	cfg_rate->header=0X62B5;	//cfg header
	cfg_rate->id=0X0806;	 	//cfg rate id
	cfg_rate->dlength=6;	 	//����������Ϊ6���ֽ�.
	cfg_rate->navrate=1;		//�������ʣ����ڣ����̶�Ϊ1
	cfg_rate->measrate=measrate;//������,us

	cfg_rate->timeref=reftime; 	//�ο�ʱ��ΪGPSʱ��
	Ublox_CheckSum((u8*)(&cfg_rate->id),sizeof(_ublox_cfg_rate)-4,&cfg_rate->cka,&cfg_rate->ckb);

	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_rate));//ͨ��dma���ͳ�ȥ
	return Ublox_Cfg_Ack_Check();
}

u8 Ublox_Cfg_wx(u8 g,u8 s,u8 b)
{
	_ublox_cfg_wx *cfg_wx=(_ublox_cfg_wx *)USART1_TX_BUF;

 	cfg_wx->header=0X62B5;	//cfg header
	cfg_wx->id=0X3E06;	 	//cfg rate id
	cfg_wx->dlength=44; 	
	cfg_wx->msgver=0;
	cfg_wx->numTrkChHw=0;
	cfg_wx->numTrkChUse=32;
	cfg_wx->numConfigBlocks=5;	
	
	cfg_wx->GgnssId=0;           //gps������Ϣ
	cfg_wx->GresTrkCh=8;
    cfg_wx->GmaxTrkCh=16;
	cfg_wx->Greserved1=0;
	cfg_wx->Gflag1=1;
	cfg_wx->Gflag2=1;
	cfg_wx->Gflag3=0;
	cfg_wx->Gflag4=g;
	
	cfg_wx->SgnssId=1;           //SBAS������Ϣ
	cfg_wx->SresTrkCh=1;
    cfg_wx->SmaxTrkCh=3;
	cfg_wx->Sreserved1=0;	
	cfg_wx->Sflag1=1;
	cfg_wx->Sflag2=1;
	cfg_wx->Sflag3=0;
	cfg_wx->Sflag4=s;
	
	cfg_wx->BgnssId=3;            //����������Ϣ
	cfg_wx->BresTrkCh=8;
    cfg_wx->BmaxTrkCh=16;
	cfg_wx->Breserved1=0;
	cfg_wx->Bflag1=1;
	cfg_wx->Bflag2=1;
	cfg_wx->Bflag3=0;
	cfg_wx->Bflag4=b;
	
	cfg_wx->QgnssId=5;             //Qzss������Ϣ
	cfg_wx->QresTrkCh=0;
    cfg_wx->QmaxTrkCh=3;
	cfg_wx->Qreserved1=0;
	cfg_wx->Qflag1=1;
	cfg_wx->Qflag2=1;
	cfg_wx->Qflag3=0;
	cfg_wx->Qflag4=0;
	
	cfg_wx->LgnssId=6;              //Glonass������Ϣ
	cfg_wx->LresTrkCh=8;
    cfg_wx->LmaxTrkCh=14;
	cfg_wx->Lreserved1=0;
	cfg_wx->Lflag1=1;
	cfg_wx->Lflag2=1;
	cfg_wx->Lflag3=0;
	cfg_wx->Lflag4=0;
	Ublox_CheckSum((u8*)(&cfg_wx->id),sizeof(_ublox_cfg_wx)-4,&cfg_wx->cka,&cfg_wx->ckb);
	
	while(DMA1_Channel4->CNDTR!=0);	//�ȴ�ͨ��7�������   
	UART_DMA_Enable(DMA1_Channel4,sizeof(_ublox_cfg_wx));//ͨ��dma���ͳ�ȥ
	return Ublox_Cfg_Ack_Check();
}

//��λ����ѡ��
//MODE_GPS:        GPS
//MODE_GPSASBAS:   GPS��+SBAS��  ����ģʽ����ǿ��gps
//MODE_Beidou :    Beidou
//MODE_GPSABeidou: GPS(+SBAS)&Beidou
void satellites_choose(u8 MODE)
{
	u8 g,s,b;

	switch(MODE)
	{
		case MODE_GPS:
            {
                g=1;
                s=0;
                b=0;
            }
            break;

		case MODE_GPSASBAS:
            {
                g=1;
                s=1;
                b=0;
            }
            break;

		case MODE_Beidou:
            {
                g=0;
                s=0;
                b=1;
            }
            break;

		case MODE_GPSABeidou:
            {
                g=1;
                s=1;
                b=1;
            }
            break;

        default:
            break;
	}

    while( Ublox_Cfg_wx(g,s,b));

}

//��Ҫ������NMEA���ݣ�
// 00,GxGGA;01,GxGLL;02,GxGSA;
// 03,GxGSV;04,GxRMC;05,GxVTG;
// 06,GxGRS;07,GxGST;08,GxZDA;
// 09,GxGBS;0A,GxDTM;0D,GxGNS;
void MSG_choose()
{  
    Ublox_Cfg_Msg(0,1);        //��GGA
    Ublox_Cfg_Msg(1,0);        
    Ublox_Cfg_Msg(2,0);
    Ublox_Cfg_Msg(3,0);
    Ublox_Cfg_Msg(4,1);        //��RMC
    Ublox_Cfg_Msg(5,0);
    Ublox_Cfg_Msg(6,0);
    Ublox_Cfg_Msg(7,0);
    Ublox_Cfg_Msg(8,0);
    Ublox_Cfg_Msg(9,0);
    Ublox_Cfg_Msg(0xa,0);
    Ublox_Cfg_Msg(0xb,0);
    Ublox_Cfg_Msg(0xc,0);
    Ublox_Cfg_Msg(0xd,0);
	
}

//rate_5hz�� ����5hz
//rate_10hz������10hz
void Rate_chose(u8 rate)
{

    while(Ublox_Cfg_Rate(rate,1));
    
}

/* gps data check and parse */
void gps_datacheck(void)
{
    u16 rxlen;
    u8 send[6];

    if(USART1_RX_STA&0X8000)		//���յ�һ��������
    {
        rxlen=USART1_RX_STA&0X7FFF;	//�õ����ݳ���			
        GPS_Analysis(&gpsx,(u8*)USART1_RX_BUF);//�����ַ���


        
        rxlen=rxlen;       // ���ξ���
		
        stgpsdata.posslnum=gpsx.posslnum;
        stgpsdata.longitude = gpsx.longitude;
        stgpsdata.latitude = gpsx.latitude;
        stgpsdata.altitude = gpsx.altitude;
        stgpsdata.speed = gpsx.speed;
        stgpsdata.cipianjiao = gpsx.cipianjiao;
		
		
//        change(send,gpsx.posslnum);
//				
//        NRF24L01_TX_Mode();            //24L01�л����뷢��ģʽ
//        NRF24L01_TxPacket(send);       //����NRF24L01����һ������
        
        USART1_RX_STA=0;		   	//������һ�ν���
    }

    return;
}

/**
  * @brief  ���gps���ǿ���.
  * @param  None
  * @retval None
  */
u8 posslnum_check(void)
{
	gps_datacheck();
	if(gpsx.posslnum>=5)
		return 0;
	else
		return 1;
}
/**
  * @brief  gps init.
  * @param  None
  * @retval None
  */
void gps_init(void)
{

    Choose_mode=MODE_GPSABeidou ;
    Rate_chose(rate_5hz);               /* gpsģ������ѡ�� */

    satellites_choose(Choose_mode);     /* gpsģ�鵥˫Ƶģʽѡ�� */
		
    Ublox_Cfg_Prt(9600);				/* gpsģ�鲨�������� */

    MSG_choose();                       /* gpsģ������NMEA��Ϣ���� */


    return;
}
