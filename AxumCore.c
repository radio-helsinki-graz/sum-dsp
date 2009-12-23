#include "AxumCore.h"

extern void InitializeProcessing();
extern void Processing(signed int *InputBuffer, signed int *OutputBuffer);
			
void Delay_us(unsigned int us)
{
	//(25Mhz*0.000001)=25 -> 25/7 cycles per loop
	#define DELAY_1US	3.572f

	unsigned int cntDelay = DELAY_1US*us;
    while (cntDelay--) 
    {
        asm ("NOP");
    }
}

void ISR_dMAX_TransferComplete()
{
	//PingPong bit of Event5->ParameterEntry7[2]
	unsigned int PingPong = 0;
	volatile unsigned int *DTCR0 = (unsigned int *)0x60000080;

	*(unsigned int *)(0x45000018) |= 0x80000000;

	if (*DTCR0 & 0x08)
	{
		*DTCR0 = 0x08;	

		PingPong = 1;
		if ((*((unsigned int *)0x610081B0))&0x80000000)
		{
			PingPong = 0;
		}
		
		Processing(DMABuffer_RCV[PingPong], DMABuffer_XMIT[PingPong]);
	}
	
	*(unsigned int *)(0x45000018) &= 0x7FFFFFFF;
}

void InitializeUHPI()
{
//DeviceConfigurationRegisters
#define CFGHPI		0x0008 
#define CFGHPIAMSB	0x000C 
#define CFGHPIAUMB	0x0010 

//UHPIRegisters
#define PID			0x0000 
#define PWREMU		0x0004 
#define GPIOINT		0x0008 
#define GPIOEN		0x000C 
#define GPIODIR1	0x0010 
#define GPIODAT1	0x0014 
#define GPIODIR2	0x0018 
#define GPIODAT2	0x001C 
#define GPIODIR3	0x0020 
#define GPIODAT3	0x0024 
#define HPIC		0x0030 
#define HPIAW		0x0034 
#define HPIAR 		0x0038 

	unsigned int DeviceConfigurationRegisters = 0x40000000;
	unsigned int UHPIRegisters = 0x43000000;

	*(unsigned int *)(DeviceConfigurationRegisters+CFGHPI) = 0x00000010; //UHPI Byteadress, Halfword, Multiplex, Full 32 bit 
	*(unsigned int *)(DeviceConfigurationRegisters+CFGHPI) = 0x00000011; //UHPI Enable = 1

	*(unsigned int *)(UHPIRegisters+HPIC) &= 0xFFFFFF7F;	//Take the HPI out of reset
}

void InitializeC6727B_PLL()
{
	#define PLLPID		0x000
	#define PLLCSR		0x100
	#define PLLM		0x110
	#define PLLDIV0		0x114
	#define PLLDIV1		0x118
	#define PLLDIV2		0x11C
	#define PLLDIV3		0x120
	#define PLLCMD		0x138
	#define PLLSTAT		0x13C
	#define ALNCTL		0x140 
	#define CKEN		0x148 
	#define CKSTAT		0x14C 
	#define SYSTAT		0x150 

	unsigned int PLLControlRegistersBase = 0x41000000;

	*(unsigned int *)(PLLControlRegistersBase+PLLCSR) 	&= 0xFFFFFFEE;	//PLLPWRDN=0, PLLEN=0

	Delay_us(200);

	*(unsigned int *)(PLLControlRegistersBase+PLLCSR) 	|= 0x00000008;	//PLLRST=1

	*(unsigned int *)(PLLControlRegistersBase+PLLDIV0) 	= 0x00008000;	//1x25Mhz 
	*(unsigned int *)(PLLControlRegistersBase+PLLM)	 	= 0x0000000E;	//14x25Mhz=350Mhz

	Delay_us(200);

	while ((*(unsigned int *)(PLLControlRegistersBase+PLLSTAT)) & 0x00000001	)
	{
		Delay_us(1);
	}; //Wait while GO Stat in progress

	*(unsigned int *)(PLLControlRegistersBase+PLLDIV1) 	= 0x00008000;	//350Mhz/1 SysClk1 (CPU)
	*(unsigned int *)(PLLControlRegistersBase+PLLDIV2) 	= 0x00008001;	//350Mhz/2 SysClk2 (dMAX)
	*(unsigned int *)(PLLControlRegistersBase+PLLDIV3) 	= 0x00000000;	//Disabled (EMIF)

	*(unsigned int *)(PLLControlRegistersBase+ALNCTL)	 	= 0x00000007;	//ALIGN 1-3 enabled

	*(unsigned int *)(PLLControlRegistersBase+PLLCMD)	 	= 0x00000001;	//Start GO set

	while ((*(unsigned int *)(PLLControlRegistersBase+PLLSTAT)) & 0x00000001	)
	{
		Delay_us(1);
	}; //Wait while GO Stat in progress

	Delay_us(200);

	*(unsigned int *)(PLLControlRegistersBase+PLLCSR) 	&= 0xFFFFFFF7;	//PLLRST=0

	Delay_us(200);

	*(unsigned int *)(PLLControlRegistersBase+PLLCSR) 	|= 0x00000001;	//PLLEN=1
}

void InitializeMcASP0_dMAX_IRQ()
{
	#define PID 		0x00
	#define PWRDEMU 	0x04
	#define PFUNC 		0x10
	#define PDIR 		0x14
	#define PDOUT 		0x18
	#define PDIN 		0x1C
	#define PDSET 		0x1C
	#define PDCLR 		0x20
	#define GBLCTL 		0x44
	#define AMUTE 		0x48
	#define DLBCTL 		0x4C
	#define DITCTL 		0x50
	#define RGBLCTL 	0x60
	#define RMASK 		0x64
	#define RFMT 		0x68
	#define AFSRCTL 	0x6C
	#define ACLKRCTL 	0x70
	#define AHCLKRCTL 	0x74
	#define RTDM 		0x78
	#define RINTCTL 	0x7C
	#define RSTAT 		0x80
	#define RSLOT 		0x84
	#define RCLKCHK 	0x88
	#define REVTCTL 	0x8C
	#define XGBLCTL 	0xA0 //0xAC in Documentation?
	#define XMASK 		0xA4
	#define XFMT 		0xA8
	#define AFSXCTL 	0xAC
	#define ACLKXCTL 	0xB0
	#define AHCLKXCTL 	0xB4
	#define XTDM 		0xB8
	#define XINTCTL 	0xBC
	#define XSTAT 		0xC0
	#define XSLOT 		0xC4
	#define XCLKCHK 	0xC8
	#define XEVTCTL 	0xCC

	#define SRCTL		0x180

	volatile unsigned int McASP0ControlRegistersBase = 0x44000000;
	volatile unsigned int *McASP0Serializer = (unsigned int *)(0x44000000+SRCTL);


	*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+PWRDEMU) 	= 0x00000001;

	*(unsigned int *)(McASP0ControlRegistersBase+RMASK) 	= 0xFFFFFFFF;
	*(unsigned int *)(McASP0ControlRegistersBase+RFMT) 		= 0x000080F0;
	*(unsigned int *)(McASP0ControlRegistersBase+AFSRCTL) 	= 0x00000810; //<= 16 channel TDM, word
	*(unsigned int *)(McASP0ControlRegistersBase+ACLKRCTL) 	= 0x00000080; //rising edge
	*(unsigned int *)(McASP0ControlRegistersBase+AHCLKRCTL)	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+RTDM)	 	= 0xFFFFFFFF;
	*(unsigned int *)(McASP0ControlRegistersBase+RINTCTL) 	= 0x00000001;
	*(unsigned int *)(McASP0ControlRegistersBase+RCLKCHK) 	= 0x00000000;

	*(unsigned int *)(McASP0ControlRegistersBase+XMASK) 	= 0xFFFFFFFF;
	*(unsigned int *)(McASP0ControlRegistersBase+XFMT) 		= 0x000080F0;
	*(unsigned int *)(McASP0ControlRegistersBase+AFSXCTL) 	= 0x00000800; //<= 16 channel TDM, single bit
	*(unsigned int *)(McASP0ControlRegistersBase+ACLKXCTL) 	= 0x000000C0; //async, falling edge
	*(unsigned int *)(McASP0ControlRegistersBase+AHCLKXCTL)	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+XTDM)	 	= 0xFFFFFFFF;
	*(unsigned int *)(McASP0ControlRegistersBase+XINTCTL) 	= 0x00000001;
	*(unsigned int *)(McASP0ControlRegistersBase+XCLKCHK) 	= 0x00000000;

	McASP0Serializer[0] = 0x00000002;	//from module 1-16
	McASP0Serializer[1] = 0x00000002;	//from module 17-32
	McASP0Serializer[2] = 0x00000002;	//from module 33-48
	McASP0Serializer[3] = 0x00000002;	//from module 49-64
	McASP0Serializer[4] = 0x00000002;	//total buss 1-16 of 33-48
	McASP0Serializer[5] = 0x00000002;	//total buss 17-32 of 49-64
	McASP0Serializer[6] = 0x00000002;	//extern input 1-16	or FX input 1-16

	McASP0Serializer[7] = 0x00000009;	//buss out 1-16	or buss out 33-48 or FX out 1-16
	McASP0Serializer[8] = 0x00000009;	//buss out 17-32 or buss out 49-64 or not used
	McASP0Serializer[9] = 0x00000009;	//monitor buss out 1-8 (9-16 empty) or not used
	McASP0Serializer[10] = 0x00000009;   //N-1 out 1-16 or not used
	McASP0Serializer[11] = 0x00000009;   //N-1 out 17-32 or not used

/*	McASP0Serializer[0] = 0x00000009;	//buss 1-16
	McASP0Serializer[1] = 0x00000009;	//buss 17-32
	McASP0Serializer[2] = 0x00000009;	//monitor buss 1-8 (9-16 empty)
	McASP0Serializer[3] = 0x00000009;   //N-1 1-16
	McASP0Serializer[4] = 0x00000009;   //N-1 17-32
	//module 1-32
	McASP0Serializer[5] = 0x00000002;	//from module 1-16
	McASP0Serializer[6] = 0x00000002;	//from module 17-32
	McASP0Serializer[7] = 0x00000002;	//from module 33-48
	McASP0Serializer[8] = 0x00000002;	//from module 49-64
	McASP0Serializer[9] = 0x00000002;	//total buss 1-16
	McASP0Serializer[10] = 0x00000002;	//total buss 17-32
	McASP0Serializer[11] = 0x00000002;	//extern input 1-16*/

//	McASP0Serializer[10] = 0x00000000;
//	McASP0Serializer[11] = 0x00000000;
	McASP0Serializer[12] = 0x00000000;
	McASP0Serializer[13] = 0x00000000;
	McASP0Serializer[14] = 0x00000000;
	McASP0Serializer[15] = 0x00000000;

	*(unsigned int *)(McASP0ControlRegistersBase+PFUNC) 	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+PDIR) 		= 0x00000F80;
//	*(unsigned int *)(McASP0ControlRegistersBase+PDIR) 		= 0x00000007;
	*(unsigned int *)(McASP0ControlRegistersBase+DITCTL)	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+DLBCTL)	= 0x00000000;
	*(unsigned int *)(McASP0ControlRegistersBase+AMUTE)		= 0x00000000;

	//Start High-frequency clocks
	*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	= 0x00000202;
	while ((*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL)&0x00000202) != 0x00000202)
	{
		*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	= 0x00000202;
	}

	//Initialize dMAX
	{
#define DEPR	0x08
#define DEHPR	0x14
#define DEER	0x0C
#define	DTCR0	0x80
#define	DTCR1 	0xA0
		volatile unsigned int dMAXControlRegistersBase	= 0x60000000;
		volatile unsigned int *HiEventEntry4 				= (unsigned int *)0x61008010; //McASP0 Tx Event
		volatile unsigned int *LoEventEntry5 				= (unsigned int *)0x62008014; //McASP0 Rx Event
		volatile unsigned int *HiEventEntry26 				= (unsigned int *)0x61008068; //McASP0 Error

		volatile unsigned int *HiParameterEntry6	 		= (unsigned int *)0x610081A8; //McASP0 Tx Event
		volatile unsigned int *LoParameterEntry7	 		= (unsigned int *)0x620081D4; //McASP0 Rx Event

		*(unsigned int *)(dMAXControlRegistersBase+DEPR)	&= 0xFFFFFFCF;
		*(unsigned int *)(dMAXControlRegistersBase+DEPR)	|= 0x04000030;
		*(unsigned int *)(dMAXControlRegistersBase+DEHPR)	|= 0x04000010;
		*(unsigned int *)(dMAXControlRegistersBase+DEER)	|= 0x04000030;

		HiEventEntry4[0] = 0x82166A03;	//Tx, TCC = 2, NO TCINT
		LoEventEntry5[0] = 0x83567503;	//Rx, TCC = 3, TCINT.
		HiEventEntry26[0] = 0x00020007;	//McASP0 Error -> INT9

		HiParameterEntry6[0] = (unsigned int)DMABuffer_XMIT[0];
		HiParameterEntry6[1] = 0x54000000;
		HiParameterEntry6[2] = 0x01100005;	//bbssllll	bb=block, ss=slots, llll=lines	
		HiParameterEntry6[3] = 0x00010010;	//0001ssss -> number of slots
		HiParameterEntry6[4] = 0xFFFCFFC1; //-(lines-1)	| -(((lines-1)*slots)-1)
		HiParameterEntry6[5] = 0xFFFCFFB1; //-(lines-1)	| -((slots*lines)-1)
		HiParameterEntry6[6] = 0x01100005;											
		HiParameterEntry6[7] = (unsigned int)DMABuffer_XMIT[0];
		HiParameterEntry6[8] = 0x54000000;
		HiParameterEntry6[9] = (unsigned int)DMABuffer_XMIT[1];
		HiParameterEntry6[10] = 0x54000000;

		LoParameterEntry7[0] = 0x54000000;
		LoParameterEntry7[1] = (unsigned int)DMABuffer_RCV[0];
		LoParameterEntry7[2] = 0x01100007;	//bbssllll	bb=block, ss=slots, llll=lines (ss = 16, lines = 7)
		LoParameterEntry7[3] = 0x00100001;	//ssss0001 -> number of slots
		LoParameterEntry7[4] = 0xFFA1FFFA; //-(((lines-1)*slots)-1) | -(lines-1)
		LoParameterEntry7[5] = 0xFF91FFFA; //-((slots*lines)-1) | -(lines-1)
		LoParameterEntry7[6] = 0x01100007;
		LoParameterEntry7[7] = 0x54000000;
		LoParameterEntry7[8] = (unsigned int)DMABuffer_RCV[0];
		LoParameterEntry7[9] = 0x54000000;
		LoParameterEntry7[10] = (unsigned int)DMABuffer_RCV[1];

		//Clear all TCCs
		*(unsigned int *)(dMAXControlRegistersBase+DTCR0) = 0x000000FF;
		*(unsigned int *)(dMAXControlRegistersBase+DTCR1) = 0x000000FF;
	}

	//Enable interrupts
	IER |= 0x00000002;	//Enable NMI
	IER |= 0x00000100;	//Enable INT8 (dMAX Transfer Complete)
	IER |= 0x00000200;	//Enable INT9 (dMAX event 0x2, McASP0 Error)
	CSR |= 0x00000001;	//Enable GIE

	//Active serializers
	*(unsigned int *)(McASP0ControlRegistersBase+XSTAT)		= 0xFFFF;
	*(unsigned int *)(McASP0ControlRegistersBase+RSTAT)		= 0xFFFF;

	//Serializers out of reset
	*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00000404;
	while ((*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL)&0x00000404) != 0x0000404)
	{
		*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00000404;
	}

	//check if transmit serviced
	while ((*(unsigned int *)(McASP0ControlRegistersBase+XSTAT)&0x00000020) != 0x00);


	//State machines out of reset
	*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00000808;
	while ((*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL)&0x00000808) != 0x00000808)
	{
		*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00000808;
	}

	//Frame sync generators out of reset
	*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00001010;
	while ((*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL)&0x00001010) != 0x00001010)
	{
		*(unsigned int *)(McASP0ControlRegistersBase+GBLCTL) 	|= 0x00001010;
	}
}

int main()
{
	unsigned int McASP1ControlRegistersBase;
	unsigned int cntDebug;
	int cntChannel;

/*	InitializeProcessing();
	while (1)
	{
		Processing(DMABuffer_RCV[0], DMABuffer_XMIT[0]);
	}*/


	IER = 0x00000000;	//Disable all interrupts
	CSR &= 0xFFFFFFFE;	//Disable GIE

	InitializeC6727B_PLL();

	InitializeUHPI();

	InitializeProcessing();
	for (cntChannel = 0; cntChannel<96; cntChannel++)
	{
		DMABuffer_RCV[0][cntChannel] = 0x0000AAAA;
		DMABuffer_RCV[1][cntChannel] = 0x0000AAAA;
	}
	for (cntChannel = 0; cntChannel<64; cntChannel++)
	{
		DMABuffer_XMIT[0][cntChannel] = 0x00000000;
		DMABuffer_XMIT[1][cntChannel] = 0x00000000;
	}
	DMABuffer_XMIT[0][0] = 0x01000000;
	DMABuffer_XMIT[1][0] = 0x01000000;

	McASP1ControlRegistersBase = 0x45000000;
	*(unsigned int *)(McASP1ControlRegistersBase+PFUNC) 	= 0xA0000000;
	*(unsigned int *)(McASP1ControlRegistersBase+PDIR) 		= 0xA0000000;
	*(unsigned int *)(McASP1ControlRegistersBase+PDOUT)     = 0x20000000;

	InitializeMcASP0_dMAX_IRQ(); //This enabled Interrupts and GIE
	
	while (1)
	{
		if (cntDebug> ((350000000/23)*0.25))
		{
			cntDebug=0;	
//			*(unsigned int *)(McASP1ControlRegistersBase+PDOUT) ^= 0x20000000;
		}
		cntDebug++;
	}	
}

void McASP0Error()
{
	*(unsigned int *)(0x45000018) &= 0xDFFFFFFF;
}

