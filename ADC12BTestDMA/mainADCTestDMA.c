/*
 * Testy ADC12_B prevodniku na MSP430FR5969 LounchPadu
 * 12. 12. 2016
 * V1 	- prevod ADC z pinu P1.4 a ukladani do pole v pameti
 * 		- po kazdem prevodu zmeni hodnotu na pinu P1.5 (lze spojit s P1.4)
 * 		- behem prevodu sviti LED, po dokonceni zhasne
 * V2 	- jako V1, ale pro zapis do pole vyuzito DMA kanalu
 * 		- bez toglingu P1.5 po kazdem ADC samplu
 * 		- funguje do 100kHz pri 8MHz DCO
 * V2.1 - 19.12.2016
 * 		- prijem pokynu z UARTu (#I& - info - verze, a nastaveni
 * 								 #S& - single sample - odebere jednu serii ADC
 * 								 #Nnnn& - zmeni pocet odberu z ADC
 * 								 #Fnnnnnn& - zmeni samplovaci frekvenci [Hz] - krok 1, 2, 5, 10, ...)
 * V2.2 - 20.12.2016
 * 		- rozsiren pokyn o 		 #Annnn& - nastaveni reference (povolene hodnoty 3600(AVcc), 2500, 2000 a 1200 [mV])
 * V2.3 - 21.12.2016
 * 		- pridani "@" na konec vsech odpovedi
 * 		- rozsireni o moznost nastaveni ADC12 clock source (vcetne delicu) a S/H doby
 * 								 #Cn& - nastaveni zdroje hodin (0=ADC12OSC, 1=ACLK, 2=MCLK, 3=SMCLK)
 * 								 #Pn& - nastaveni preddelice (0=1x, 1=4x, 2=32x, 3=64x)
 * 								 #Dn& - nastaveni delice (0=1x - 7=8x)
 * 								 #Hnn& - nastaveni S/H (0=4x, 1=8x, 2=16x, 3=32x, 4=64x, 5=96x
 * 								 						6=128x, 7=192x, 8=256x, 9=384x, 10=512x)
 * 		- SW reset prikazem pres UART #R& - lepsi poslat &#R& (tim se prerusi ADC a provede se reset)
 * V2.4 - 22., 23.12.2016
 * 		- doplneni informaci posilanych z procesoru na dotaz "#I&"
 * V2.5 - 25.12.2016
 * 		- pridani testovani na pouzitelnost nastavenych parametru ADCcheck - ADC prevod s DMA prenosem musi byt kratsi, nezli nastaveni TIMER_A0
 * V2.6 - 28.12.2016
 * 		- oprava chyb
 * V2.7 - 26.02.2017
 * 		- pro 5 10 Hz pouzity hodiny SMCLK delene 32x (misto ACLK)
 *
 */

#define VERSION "V2.7"

#include <stdio.h>
#include <string.h>
#include <msp430.h>
#include <stdbool.h>

void init_GPIO(void);
void init_CS(void);
void init_Timer(void);
void init_ADC(void);
void init_DMA(volatile unsigned int abuffer[], unsigned int abuffer_size);
void init_UART(void);

void ADCsamples(void);	// naplni analog_buffer sadou ADC hodnot
signed char UART_Rx(void);	// zpracuje obsah prijimaciho bufferu (nastavi novou Fs z UART_Rx)
_Bool ADCcheck(void);	// zkontroluje parametry casovace a ADC, zda to nepretece

// look-up tabulky
static const char C_ADC_CLK_PDIV[4] = {1, 4, 32, 64};
static const int C_ADC_SH_CYCLES[11] = {4, 8, 16, 32, 64, 96, 128, 192, 256, 384, 512};

/*
 * promene
 */
#define ADC_BUF_SIZE 500	// pocet ADC samplu
#define ADC_REF 3600		// pouzita reference pro ADC [mV]
volatile unsigned int analog_reference = ADC_REF;
volatile unsigned int analog_buffer_size = ADC_BUF_SIZE;
volatile unsigned int analog_buffer_position = 0;

//#pragma DATA_SECTION (analog_buffer , "MYBUF");
volatile unsigned int analog_buffer[ADC_BUF_SIZE] = { 0 };
//volatile unsigned char analog_buffer[ADC_BUF_SIZE] = { 0 };

volatile unsigned char UART_buffer[16] = { 0 };	// UART Rx buffer
volatile signed char UART_buffer_position = -1;

unsigned long dTimer = 0;	// perioda casovace [us] (doba mezi dvema prevody)
unsigned long dADC = 0;		// doba odberu jendoho samplu vcetne DMA prenosu

/*
 * main.c
 */
int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    //printf(EUSCI_A0_MODULE, "ADC12B DMA Test!\n");
    init_GPIO();
    init_CS();
    init_UART();
    printf("\nADC12B DMA Test %s!\n",VERSION);
    printf("   Type: #?& for help\n");
    init_DMA(analog_buffer, analog_buffer_size);
 	init_ADC();
//    ADC12IER0 |= ADC12IE0;                    // Enable ADC interrupt
    init_Timer();

	__bis_SR_register(GIE);

	int j;

	//for(i=0;i<600;i++)
	while(1)
	{
		LPM0;
		__no_operation();

		signed char r = UART_Rx();
		if (r==-99) // sw restart
		{
			WDTCTL = 0;
			//PMMCTL0 = PMMPW | PMMSWBOR;   // This triggers a Software BOR
			//PMMCTL0 = PMMPW | PMMSWPOR;   // This triggers a Software POR
			//__bic_SR_register(GIE);
			//(*(void(**)(void))(0xfffe))();
		}
		if (r == 1)	// 1 - single trigger
		{
			P1OUT |= BIT0;	//LED on
			ADCsamples();
		    P1OUT &= ~BIT0; 	// LED off
			//zobrazeni/odeslani vzorku
			printf("ADC12B:\n");
			for (j=0; j<analog_buffer_size; j++)
				printf(" %d\n",analog_buffer[j]);

			printf("@@\n");
	//		P1OUT ^= BIT0;	// togle LED
		    P1OUT ^= BIT5; 	// togle P1.5
		    P3OUT ^= BIT0; 	// togle P3.0
		}
	    __delay_cycles(500);

	}
}
/*
 * END OF MAIN LOOP
 */


/*
 * ADC sample
 * nabere do bufferu vzorky z ADC prevodniku v intervalech nastavenych pomoci TIMER_A0
 */
void ADCsamples(void)
{
	DMA0SZ = analog_buffer_size;
	TA0CTL |= TACLR;	// clear timer
	TA0CTL |= MC__UP; 	// start timer
	ADC12CTL0 |= (ADC12ENC | ADC12SC);  // Start sampling/conversion
	DMA0CTL |= DMAEN;                   // Enable DMA0
    LPM0;
	__no_operation();

    ADC12CTL0 &= ~(ADC12ENC | ADC12SC);        // Stop sampling/conversion
	DMA0CTL &= ~(DMAEN);                       // Disable DMA0
	TA0CTL &= ~MC__UP;		// stop timer
	__no_operation();
}

/*
 * zpracuje obsah UART bufferu
 */
signed char UART_Rx(void)
{
	signed char result = -1;
	char i, n;
	unsigned int m;
	unsigned long f = 0;
	switch (UART_buffer[0]) {
		case '?':	// vypise napovedu
			printf("\n*** MSP430Scope %s ***\n",VERSION);
			printf(	"#I& - Setting information\n"
					"#S& - Samples capturing to buffer\n"
					"#N& - Setting length of ADC buffer (1-500)\n"
					"#Fn& - Sample frequency setting in 0.1Hz (n=1-10000000 in steps 1, 2 and 5)\n"
					"#Hn& - Setting sample and hold cycles (n=0-10 => 4-512 cycles\n"
					"#An& - Setting reference voltage (3600(AVcc), 2500, 2000 and 1200mV)\n"
					"#Cn& - Setting ADC clock source (n=0-3 => ADC12OSC, ACLK, MCLK, SMCLK)\n"
					"#Pn& - Setting ADC clock pre-divider (n=0-3 => 1, 4, 32, 64)\n"
					"#Dn& - Setting ADC clock divider (n=0-7 => 1-8)\n"
					"& - Interrupt ADC conversion\n"
					"#R& - MSP software restart\n@");
			break;
		case 'I':	// informace
			ADCcheck();	// aktualizuje promene dTimer a dADC
			printf("MSP430Scope %s\n",VERSION);
			if (dTimer >= 10000)
				printf("dTimer = %lu ms\n", dTimer/1000);
			else
				printf("dTimer = %lu us\n", dTimer);

			unsigned long fs;
			if (TA0CTL & TASSEL__ACLK)	// 0.1Hz - 2Hz
			{
				fs = 40960 / (TA0CCR0+1);
				printf("Fs = %lu mHz\n", fs*100);
			} else if (TA0CTL & ID__8)	// 5Hz - 100Hz
			{
				if (TA0EX0 > 0)
					fs = 250000 / (TA0CCR0+1);	// 5-10Hz
				else
					fs = 1000000 / (TA0CCR0+1); // 20-100Hz
				printf("Fs = %lu Hz\n", fs);
			} else
			{
				fs = 8000000 / (TA0CCR0+1);
				printf("Fs = %lu Hz\n", fs);
			}
			f = 0;
			printf("N = %d\n",analog_buffer_size);
			printf("Vref = %d mV\n", analog_reference);
			n=(ADC12CTL1 & 0x18)>>3; // aktualni clock source (0-3: ADC12OSC, ACLK, MCLK, SMCLK)
			switch(n) {
				case 0: f=5000000; break;
				case 1: f=4096; break;
				case 2: f=8000000; break;
				case 3: f=8000000; break;
			}
			printf("ADC_CLK = %d\n",n);
			n=(ADC12CTL1 & 0x6000)>>13; // aktualni pre-divider (0-3: 1, 4, 32, 64)
			f /= C_ADC_CLK_PDIV[n];
			printf("ADC_CLK_PDIV = %d\n",C_ADC_CLK_PDIV[n]);
			n=(ADC12CTL1 & 0x00E0)>>5; // aktualni divider (0-7: 1-8)
			f /= n+1;
			printf("ADC_CLK_DIV = %d\n", n+1);
			n=(ADC12CTL0 & 0x0F00)>>8; // aktualni S/H pro ADC12MEM0 (0-10: 4, 8, 16, 32, 64, 96, 128, 192, 256, 384, 512)
			printf("ADC_SH_CYCLES = %d\n", C_ADC_SH_CYCLES[n]);
			// doba prevodu 1ho a N vzorku
			printf("ADC real CLK = %lu Hz\n",f);
			if (dADC >= 10000)
				printf("ADC time = %lu ms\n",dADC/1000);
			else
				printf("ADC time = %lu us\n",dADC);
			printf("@\n");
			break;
		case 'S':	// jeden odmer
			result = ADCcheck();
			printf("\ndt = %luus --- ADCt = %luus@\n",dTimer,dADC);
			break;
		case 'N':	// zmena poctu odmeru (musi byt mensi nezli ADC_BUF_SIZE)
//			printf("\nCurrent buffer size = %d\n", analog_buffer_size);
			__no_operation();
			int NN=0;
			for(i=1;i<=UART_buffer_position;i++)
			{
				n = UART_buffer[i] - '0';
				NN *= 10;
				if (n <= 9) NN += n;
			}
			if (NN > 0 && NN <= ADC_BUF_SIZE)
			{
			    ADC12CTL0 &= ~(ADC12ENC | ADC12SC);        // Stop sampling/conversion
				DMA0CTL &= ~(DMAEN);                       // Disable DMA0
				analog_buffer_size = NN;
			}
			printf("\nanalog_buffer_size = %d@\n", analog_buffer_size);
			break;
		case 'A':
//			printf("\nCurrent analog reference = %dmV\n", analog_reference);
			__no_operation();
			int aref=0;
			for(i=1;i<=UART_buffer_position;i++)
			{
				n = UART_buffer[i] - '0';
				aref *= 10;
				if (n <= 9) aref += n;
			}
			if (aref >= 3600)
			{
				analog_reference = 3600;
				ADC12MCTL0 = ADC12INCH_2 | ADC12EOS;
			} else if (aref>=2500)
			{
				analog_reference = 2500;
				while(REFCTL0 & REFGENBUSY);              // If ref generator busy, WAIT
				REFCTL0 = REFVSEL_2 | REFON;             // Select internal ref = 2.5V
				ADC12MCTL0 = ADC12INCH_2 | ADC12EOS | ADC12VRSEL_1; // A2 ADC input select; Vref=2.5V
			} else if (aref>=2000)
			{
				analog_reference = 2000;
				while(REFCTL0 & REFGENBUSY);              // If ref generator busy, WAIT
				REFCTL0 = REFVSEL_1 | REFON;             // Select internal ref = 2.0V
				ADC12MCTL0 = ADC12INCH_2 | ADC12EOS | ADC12VRSEL_1; // A2 ADC input select; Vref=2.0V
			} else
			{
				analog_reference = 1200;
				while(REFCTL0 & REFGENBUSY);              // If ref generator busy, WAIT
				REFCTL0 = REFVSEL_0 | REFON;             // Select internal ref = 1.2V
				ADC12MCTL0 = ADC12INCH_2 | ADC12EOS | ADC12VRSEL_1; // A2 ADC input select; Vref=1.2V
			}
			while(!(REFCTL0 & REFGENRDY));            // Wait for reference generator to settle
			printf("\nanalog reference = %dmV@\n", analog_reference);
			break;
		case 'C':	// ADC clock source
			n = UART_buffer[1] - '0';
			if (n < 4)
			{
				n = n << 3;
				ADC12CTL1 &= ~0x18; // CLK source = ADC12OSC
				ADC12CTL1 |= n;	// CLK source = requested clock source
			}
			n=(ADC12CTL1 & 0x18)>>3; // aktualni clock source (0-3)
			printf("\nADC clock source = %d@\n", n);
			break;
		case 'P':	// ADC clock source pre-divider
			n = UART_buffer[1] - '0';
			if (n < 4)
			{
				m = n << 13;
				ADC12CTL1 &= ~(0x6000); // CLK pre divider = 1
				ADC12CTL1 |= m;	// CLK pre-divider = requested pre-divider
			}
			n=(ADC12CTL1 & 0x6000)>>13; // aktualni pre-divider (0-3)
			printf("\nADC clock pre-divider = %d@\n", C_ADC_CLK_PDIV[n]);
			break;
		case 'D':	// ADC clock source divider
			n = UART_buffer[1] - '0';
			if (n < 8)
			{
				n = n << 5;
				ADC12CTL1 &= ~(0x00E0); // CLK divider = 1
				ADC12CTL1 |= n;	// CLK divider = requested divider
			}
			n=(ADC12CTL1 & 0x00E0)>>5; // aktualni divider (0-7)
			printf("\nADC clock divider = %d@\n", n+1);
			break;
		case 'H':	// Sample/Hold cycles
			__no_operation();
			unsigned int H=0;
			for(i=1;i<=UART_buffer_position;i++) {
				n = UART_buffer[i] - '0';
				H *= 10;
				if (n <= 9) H += n;
			}
			if (H < 11 && (ADC12CTL0 & ADC12ENC)==0)
			{
				H = H << 8;
				//ADC12ENC=0;
				ADC12CTL0 &= ~(0xFF00); // S/H = 4 cycles
				ADC12CTL0 |= H;	// S/H = requested cycles for ADC12MEM0-7 and 24-31
				H = H << 4;
				ADC12CTL0 |= H;	// S/H = requested cycles for ADC12MEM8-23
			}
			n=(ADC12CTL0 & 0x0F00)>>8; // aktualni S/H (0-10) pro ADC12MEM0
			printf("\nADC SH time = %d cycles@\n", C_ADC_SH_CYCLES[n]);
			break;
		case 'F':	// nova fs
			if (UART_buffer_position>7) UART_buffer_position=7;
			for(i=1;i<=UART_buffer_position;i++) {
				n = UART_buffer[i] - '0';
				f *= 10;
				if (n < 9) f += n;
			}
			__no_operation();
			ADC12CTL0 &= ~(ADC12ENC | ADC12SC);        // Stop sampling/conversion
			DMA0CTL &= ~(DMAEN);                       // Disable DMA0
			TA0CTL &= ~MC__UP;		// stop timer
			TA0CTL = TASSEL__SMCLK;
			TA0EX0 = TAIDEX_0;	// pre-divider 1x
			n = 0;
			char s[10] = "";
			if (f>999999) {
				n=1;				// fs=100kHz
				strcpy(s,"100 000.0");
				ADC12CTL0 = (ADC12SHT10 + ADC12SHT00 + ADC12ON);	// S&H na 8clock ticks, ADC on
				TA0CCR0 = 79;                         // PWM Period 100kHz (79)
			} else if (f>499999) {
				n=2;				// fs=50kHz
				strcpy(s,"50 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 159;                         // PWM Period 50kHz (159)
			} else if (f>199999) {
				n=3;				// fs=20kHz
				strcpy(s,"20 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 399;                         // PWM Period 20kHz (399)
			} else if (f>99999) {
				n=4;				// fs=10kHz
				strcpy(s,"10 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 799;                         // PWM Period 10kHz (799)
			} else if (f>49999) {
				n= 5;				// fs=5kHz
				strcpy(s,"5 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 1599;                         // PWM Period 5kHz (1599)
			} else if (f>19999) {
				n=6;				// fs=2kHz
				strcpy(s,"2 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 3999;                         // PWM Period 2kHz (3999)
			} else if (f>9999) {
				n=7;				// fs=1kHz
				strcpy(s,"1 000.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 7999;                         // PWM Period 1kHz (7999)
			} else if (f>4999) {
				n=8;				// fs=500Hz
				strcpy(s,"500.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 15999;                         // PWM Period 500Hz (15 999)
			} else if (f>1999) {
				n=9;				// fs=200Hz
				strcpy(s,"200.0");
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT01 + ADC12ON);	// S&H na 16clock ticks, ADC on
				TA0CCR0 = 39999;                         // PWM Period 200Hz (39 999)
			} else if (f> 999) {
				n=10;				// fs=100Hz
				strcpy(s,"100.0");
				TA0CTL |= ID__8;
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 9999;                         // PWM Period 100Hz (9 999), predivider 8x
			} else if (f> 499) {
				n=11;				// fs=50Hz
				strcpy(s,"50.0");
				TA0CTL |= ID__8;
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 19999;                         // PWM Period 50Hz (19 999), predivider 8x
			} else if (f> 199) {
				n=12;				// fs=20Hz
				strcpy(s,"20.0");
				TA0CTL |= ID__8;
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 49999;                         // PWM Period 20Hz (49 999) predivider 8x
			} else if (f>  99) {
				n=13;				// fs=10Hz
				strcpy(s,"10.0");
				TA0CTL |= ID__8;
				TA0EX0 = TAIDEX_3;	// pre-divider 4x
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 24999;                         // PWM Period 10Hz (24 999), predivider 32x
			} else if (f>  49) {
				n=14;				// fs=5Hz
				strcpy(s,"5.0");
				TA0CTL |= ID__8;
				TA0EX0 = TAIDEX_3;	// pre-divider 4x
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 49999;                         // PWM Period 5Hz (49 999), predivider 32x
			} else if (f>  19) {
				n=15;				// fs=2Hz
				strcpy(s,"2.0");
				TA0CTL = TASSEL__ACLK;	// 4096Hz
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 2047;                         // PWM Period 2Hz (2 047), CLK = ACLK
			} else if (f>   9) {
				n=16;				// fs=1Hz
				strcpy(s,"1.0");
				TA0CTL = TASSEL__ACLK;	// 4096Hz
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 4095;                         // PWM Period 1Hz (4 095), CLK = ACLK
			} else if (f>   4) {
				n=17;				// fs=0.5Hz
				strcpy(s,"0.5");
				TA0CTL = TASSEL__ACLK;	// 4096Hz
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 8191;                         // PWM Period 0.5Hz (8 191), CLK = ACLK
			} else if (f>   1) {
				n=18;				// fs=0.2Hz
				strcpy(s,"0.2");
				TA0CTL = TASSEL__ACLK;	// 4096Hz
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 20479;                         // PWM Period 0.2Hz (20 479), CLK = ACLK
			} else {
				n=19;				// fs=0.1Hz
				strcpy(s,"0.1");
				TA0CTL = TASSEL__ACLK;	// 4096Hz
				ADC12CTL0 = (ADC12SHT11 + ADC12SHT10 + ADC12SHT01 + ADC12SHT00 + ADC12ON);	// S&H na 32clock ticks, ADC on
				TA0CCR0 = 40959;                         // PWM Period 0.1Hz (40 959), CLK = ACLK
			}
			printf("\nFs(%d) = %sHz@\n",n,s);
			break;
		case 'R':	// sw reset of MSP430
			printf("\nReset@\n");
			result = -99;
			break;
	}
	UART_buffer_position = -1;	// resetuje UART buffer
	return result;
}

/*
 * ADC check
 */
_Bool ADCcheck(void)
{
	_Bool result = false;
	unsigned long dt = 0;	// perioda casovace [us] (doba mezi dvema prevody)
	unsigned long ADCdt = 0;	// doba 1 cyklu hodin pro ADC [ns]
	unsigned long ADCt = 0; 	// doba jednoho samplu [us]


	if (TA0CTL & TASSEL__ACLK)	// 0.1Hz - 2Hz
	{
		dt = (TA0CCR0+1) / 4.096;
		dt *= 1000;
	} else if (TA0CTL & ID__8)	// 5Hz - 100Hz
	{
		if (TA0EX0 >0)	// 5-10Hz
			dt = (TA0CCR0+1)*4L;
		else
			dt = (TA0CCR0+1);	// 20-100Hz
	} else
	{
		dt = (TA0CCR0+1) / 8;	// 0.1-2Hz
	}
	unsigned long f = 0;	// frekvence hodin ADC [Hz]
	unsigned int n=(ADC12CTL1 & 0x18)>>3; // aktualni clock source (0-3: ADC12OSC, ACLK, MCLK, SMCLK)
	switch(n) {
		case 0: f=5000000; break;
		case 1: f=4096; break;
		case 2: f=8000000; break;
		case 3: f=8000000; break;
	}
	// doba prevodu 1ho vzorku ADC
	ADCdt = 1000000000/f;	//[ns]
	n=(ADC12CTL1 & 0x6000)>>13; // aktualni pre-divider (0-3: 1, 4, 32, 64)
	f /= C_ADC_CLK_PDIV[n];
	ADCdt *= C_ADC_CLK_PDIV[n];
	n=(ADC12CTL1 & 0x00E0)>>5; // aktualni divider (0-7: 1-8)
	f /= n+1;
	ADCdt *= n+1;
	n=(ADC12CTL0 & 0x0F00)>>8; // aktualni S/H pro ADC12MEM0 (0-10: 4, 8, 16, 32, 64, 96, 128, 192, 256, 384, 512)
	int tsh = C_ADC_SH_CYCLES[n];

	if (ADCdt > 1000000)
	{
		ADCdt /= 1000;	// v [us]
		ADCt = ADCdt * (tsh + 16 + 20); 	// doba prevodu = poèet SH cyklu + ADC prevod (14+2) + DMA aspol (cca 20) [us]
	} else {
		ADCt = ADCdt * (tsh + 16 + 20); 	// doba prevodu = poèet SH cyklu + ADC prevod (14+2) + DMA aspol (cca 20) [ns]
		ADCt /= 1000;
	}
	if (dt > ADCt) result = true;
	dTimer = dt;
	dADC = ADCt;

	return result;
}

/*
 * obsluha preruseni od DMA
 */
#pragma vector=DMA_VECTOR
__interrupt void DMA_ISR(void)
{
	__no_operation();
    // check interrupt for which channel
    switch (__even_in_range(DMAIV, DMAIV_DMA2IFG)) {
		case 0x00:  // None
			break;
		case DMAIV_DMA0IFG: // CHANNEL 0 - konec ADC samplovani
			// stop ADC (with preemption)
			ADC12CTL0 &= ~(ADC12ENC + ADC12SC);
			LPM0_EXIT;
			break;
		case DMAIV_DMA1IFG: // CHANNEL 1
			break;
		case DMAIV_DMA2IFG: // CHANNEL 2
			break;
		default:
			_never_executed();
    }
}

/*
 * obsluha prijmu znaku z UARTu
 */
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
	switch(__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG))
	{
		case USCI_NONE: break;
		case USCI_UART_UCRXIFG:
			while(!(UCA0IFG&UCTXIFG));
			switch(UCA0RXBUF)
			{
				case '#':	// zacatek zpravy
					UART_buffer_position = -1;
					break;
				case '&':	// konec zpravy
					LPM0_EXIT;
					break;
				default:
					UART_buffer[++UART_buffer_position] = UCA0RXBUF;
					break;
			}
			//UCA0TXBUF = UCA0RXBUF;		// echo
			__no_operation();
			break;
	    case USCI_UART_UCTXIFG: break;
	    case USCI_UART_UCSTTIFG: break;
	    case USCI_UART_UCTXCPTIFG: break;
	}
}
