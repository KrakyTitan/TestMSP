#include <msp430.h>


/*
 * UART init
 */
void init_UART(void)
{
	  // Configure USCI_A0 for UART mode
	  UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
	  UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	  // Baud Rate calculation
	  // 8000000/(16*9600) = 52.083
	  // Fractional portion = 0.083
	  // User's Guide Table 21-4: UCBRSx = 0x04
	  // UCBRFx = int ( (52.083-52)*16) = 1
	  UCA0BR0 = 52;                             // 8000000/16/9600
	  UCA0BR1 = 0x00;
	  UCA0MCTLW |= UCOS16 | UCBRF_1;
	  // 115200
	  UCA0BR0 = 4;
	  UCA0MCTLW = 0x5500;                       // 8000000/115200 - INT(8000000/115200)=0.444
	                                            // UCBRSx value = 0x55 (See UG)
	  UCA0BR1 = 0;
	  UCA0MCTLW |= UCOS16 | UCBRF_5;

	  UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
	  UCA0IE |= UCRXIE;                         // Enable USCI_A0 RX interrupt
}

/*
 * GPIO init
 */
void init_GPIO(void)
{
	// P1.0 LED out "0"
    P1OUT &= ~BIT0;
    P1DIR |= BIT0;

	// P1.4 jako analog in A4
/*    P1DIR &= ~BIT4;
	P1SEL0 |= BIT4;
	P1SEL1 |= BIT4; */
	// P1.2 jako analog in A2
    P1DIR &= ~BIT2;
	P1SEL0 |= BIT2;
	P1SEL1 |= BIT2;

	// P1.5 jako digital out "0"
    P1OUT &= ~BIT5;
 //   P1OUT |= BIT5;
    P1DIR |= BIT5;
	// P3.0 jako digital out "0"
    P3OUT &= ~BIT0;
 //   P1OUT |= BIT5;
    P3DIR |= BIT0;

    // P2.0 a P2.1 jako UART (RxD a TxD)
    P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
    P2SEL0 &= ~(BIT0 | BIT1);

    // aktivuje nastaveni portu
    PM5CTL0 &= ~LOCKLPM5;
}

/*
 *  Clock source select (8MHZ DCO)
 */
void init_CS(void)
{
    // Set DCO to 8MHz, MCLK = SMCLK = 8MHz
	  // Clock System Setup
	  CSCTL0_H = CSKEY >> 8;                    // Unlock CS registers
	  CSCTL1 = DCOFSEL_6;                       // Set DCO to 8MHz
	  CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;  // Set SMCLK = MCLK = DCO,
	                                            // ACLK = LFXTCLK (32 768Hz)
	  CSCTL3 = DIVA__8 | DIVS__1 | DIVM__1;     // Set dividers (ACLK/8, SMCLK/1 and MCLK/1) - ACLK=4096Hz
	  CSCTL0_H = 0;                             // Lock CS registers
}

/*
 * TA0 pro ADC (1kHz)
 */
void init_Timer(void)
{
	//
	  // Configure Timer0_A3 to periodically trigger the ADC12
	TA0CCR0 = 79;                         // PWM Period 1ms (7999)
	TA0CCR0 = 159;                         // PWM Period 50kHz (159)
	TA0CCTL1 = OUTMOD_7;                      // TACCR1 reset/set
	TA0CCR1 = 20;                           // TACCR1 PWM Duty Cycle
	TA0CTL = TASSEL__SMCLK;           		// SMCLK, stop
}


/*
 * ADC init
 */
void init_ADC(void)
{
	// zapnout interni referenci, ale na zacatku je aktivni reference na AVCC (3.6V)
	// By default, REFMSTR=1 => REFCTL is used to configure the internal reference
	while(REFCTL0 & REFGENBUSY);              // If ref generator busy, WAIT
	REFCTL0 |= REFVSEL_0 | REFON;             // Select internal ref = 1.2V
	                                            // Internal Reference ON
	// single channel z P1.4, samle signal od TA0
	// reference AVCC
	// S&H na 16clock ticks, ADC on
	ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);
	// hodiny od ADC12OSC (5MHz), bez delice, single channel-single conversion, od èasovaèe TA0
	ADC12CTL1 = ADC12SHP | ADC12SHS_1 | ADC12CONSEQ_2;
	// 8bit resolution
	//ADC12CTL2 |= (ADC12RES__8BIT);
	// 12bit resolution
	ADC12CTL2 |= (ADC12RES__12BIT);
	//ADC12MEM0 zvolena
	ADC12CTL3 |= (0);
	// ref+=AVCC, single ended mode, input A4 (P1.4)
//	ADC12MCTL0 = ADC12INCH_4 | ADC12EOS;
	// ref+=AVCC, single ended mode, input A2 (P1.2)
	ADC12MCTL0 = ADC12INCH_2 | ADC12EOS;
	// zakaze IE od ADC0
	ADC12IER0 &= ~ADC12IE0;
}

/*
 * DMA init
 */
void init_DMA(volatile unsigned int abuffer[], unsigned int abuffer_size)
{
	// Configure DMA channel 0
	__data16_write_addr((unsigned short) &DMA0SA,(unsigned long) &ADC12MEM0);
	                                          // Source block address
	__data16_write_addr((unsigned short) &DMA0DA,(unsigned long) abuffer);
	                                          // Destination single address
	DMA0SZ = abuffer_size;                              // Block size
//	DMA0CTL = (DMADT_4 | DMASRCINCR_0 | DMADSTINCR_3 | DMADSTBYTE | DMASRCBYTE | DMAIE); // single transfer src no inc, dst inc, 8bit,
	DMA0CTL = (DMADT_4 | DMASRCINCR_0 | DMADSTINCR_3 | DMAIE); // single transfer src no inc, dst inc, 16bit,
	DMACTL0 |= DMA0TSEL__ADC12IFG;		//ADC select
    DMACTL4 = DMARMWDIS;
	DMA0CTL |= DMAEN;                   // Enable DMA0
}
