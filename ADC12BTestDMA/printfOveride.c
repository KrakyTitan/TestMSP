/*
 * redirecting PRINTF to UART_A0
 * source:	http://processors.wiki.ti.com/index.php/Printf_support_for_MSP430_CCSTUDIO_compiler?keyMatch=how%20to%20use%20printf%20to%20uart%20on%20MSP430&tisearch=Search-EN
 * 			http://43oh.com/2015/05/how-to-add-printf-support-for-the-msp432-launchpad-serial-port/
 */

#include <stdio.h>
#include <msp430.h>
#include <string.h>

int fputc(int _c, register FILE *_fp);
int fputs(const char *_ptr, register FILE *_fp);

int fputc(int _c, register FILE *_fp)
{
  while(!(UCA0IFG & UCTXIFG));
  UCA0TXBUF = (unsigned char) _c;

  return((unsigned char)_c);
}

int fputs(const char *_ptr, register FILE *_fp)
{
  unsigned int i, len;

  len = strlen(_ptr);

  for(i=0 ; i<len ; i++)
  {
    while(!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = (unsigned char) _ptr[i];
  }

  return len;
}
