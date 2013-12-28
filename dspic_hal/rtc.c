/*
 * dspic_hal - Fat32lib hardware abstraction library for dspic MCUs
 * Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include "rtc.h"

static time_t __attribute__((boot, section(".rtc"))) rtc_time;
static uint32_t timer_period;

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])
#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')
#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[ 5])
#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])
#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])
#define BUILD_SEC_CH0 (__TIME__[6])
#define BUILD_SEC_CH1 (__TIME__[7])


#define CHAR_TO_INT(c) 			\
	(((c) == '0') ? 0 :			\
	((c) == '1') ? 1 :			\
	((c) == '2') ? 2 :			\
	((c) == '3') ? 3 :			\
	((c) == '4') ? 4 :			\
	((c) == '5') ? 5 :			\
	((c) == '6') ? 6 :			\
	((c) == '7') ? 7 :			\
	((c) == '8') ? 8 :			\
	((c) == '9') ? 9 : 0)

#define BUILD_MONTH				\
	(BUILD_MONTH_IS_JAN ? 1 :	\
	BUILD_MONTH_IS_FEB ? 2 :	\
	BUILD_MONTH_IS_MAR ? 3 :	\
	BUILD_MONTH_IS_APR ? 4 :	\
	BUILD_MONTH_IS_MAY ? 5 :	\
	BUILD_MONTH_IS_JUN ? 6 :	\
	BUILD_MONTH_IS_JUL ? 7 :	\
	BUILD_MONTH_IS_AUG ? 8 :	\
	BUILD_MONTH_IS_SEP ? 9 :	\
	BUILD_MONTH_IS_OCT ? 10 :	\
	BUILD_MONTH_IS_NOV ? 11 :	\
	BUILD_MONTH_IS_DEC ? 12 : 0)

#define BUILD_YEAR				\
	CHAR_TO_INT(BUILD_YEAR_CH0) * 1000 +	\
	CHAR_TO_INT(BUILD_YEAR_CH1) * 100 +		\
	CHAR_TO_INT(BUILD_YEAR_CH2) * 10 +		\
	CHAR_TO_INT(BUILD_YEAR_CH3)	

#define BUILD_DAY							\
	CHAR_TO_INT(BUILD_DAY_CH0) * 10	+		\
	CHAR_TO_INT(BUILD_DAY_CH1)

#define BUILD_HOUR							\
	CHAR_TO_INT(BUILD_HOUR_CH0) * 10	+	\
	CHAR_TO_INT(BUILD_HOUR_CH1)

#define BUILD_MIN							\
	CHAR_TO_INT(BUILD_MIN_CH0) * 10	+		\
	CHAR_TO_INT(BUILD_MIN_CH1)

#define BUILD_SEC							\
	CHAR_TO_INT(BUILD_SEC_CH0) * 10	+		\
	CHAR_TO_INT(BUILD_SEC_CH1)



void rtc_init(uint32_t fcy)
{
	struct tm* timeinfo;
	
	timer_period = fcy / 256;
	/*
	// create a time_t for compile time
	*/
	timeinfo = localtime (&rtc_time);
  	timeinfo->tm_year = BUILD_YEAR - 1900;
  	timeinfo->tm_mon = BUILD_MONTH - 1;
  	timeinfo->tm_mday = BUILD_DAY;
	timeinfo->tm_hour = BUILD_HOUR;
	timeinfo->tm_min = BUILD_MIN;
	timeinfo->tm_sec = BUILD_SEC;
	rtc_time = mktime(timeinfo);
	
	/*
	// set T2-T3 as a 32-bit timer
	*/
	T3CONbits.TON = 0;			/* Stop any 16-bit Timer3 operation */
	T2CONbits.TON = 0;			/* Stop any 16/32-bit Timer2 operation */
	T2CONbits.T32 = 1;			/* Enable 32-bit Timer mode */
	T2CONbits.TCS = 0;			/* Select internal instruction cycle clock */
	T2CONbits.TGATE = 0;		/* Disable Gated Timer mode */
	T2CONbits.TCKPS = 0x3;		/* Select 1:1 Prescaler */
	
	TMR3 = 0x00;				/* Clear 32-bit Timer (msw) */
	TMR2 = 0x00; 				/* Clear 32-bit Timer (lsw) */

	/* 156250 = 0x0002625A */
	PR3 = HI16(timer_period);	/* 0x0002; // Load 32-bit period value (msw) */
	PR2 = LO16(timer_period);	/* 0x625A; // Load 32-bit period value (lsw) */

	IPC2bits.T3IP = 0x01;		/* Set Timer3 Interrupt Priority Level */
	IFS0bits.T3IF = 0;			/* Clear Timer3 Interrupt Flag */
	IEC0bits.T3IE = 1;			/* Enable Timer3 interrupt */
	T2CONbits.TON = 1;			/* Start 32-bit Timer */
}

void rtc_change_fcy(uint32_t fcy)
{
	timer_period = fcy / 256;			/* Calculate new period */
	PR3 = HI16(timer_period); 			/* Load 32-bit period value (msw) */
	PR2 = LO16(timer_period); 			/* Load 32-bit period value (lsw) */
	TMR3 = 0x00;						/* Clear 32-bit Timer (msw) */
	TMR2 = 0x00; 						/* Clear 32-bit Timer (lsw) */
}

void rtc_set_time(time_t new_time)
{
	rtc_time = new_time;
}


void rtc_timer(uint32_t* s, uint32_t* ms)
{
	time_t now;
	uint16_t tmr2, tmr3;	
	INTERRUPT_PROTECT(
		tmr2 = TMR2;
		tmr3 = TMR3;
		now = rtc_time;
		);
		

	*s = (uint32_t) now;
	*ms = ((uint32_t) tmr3) << 16 | tmr2;
	if (*ms > timer_period)
		while(1);
	*ms = (*ms * 1000) / timer_period;
}

void rtc_timer_elapsed(uint32_t* s, uint32_t* ms)
{
	time_t now;
	uint16_t tmr2, tmr3;
	uint32_t ms_now;
		
	INTERRUPT_PROTECT(
		tmr2 = TMR2;
		tmr3 = TMR3;
		now = rtc_time;
		);
	
	ms_now = ((uint32_t) tmr3) << 16 | tmr2;
	ms_now = (ms_now * 1000) / timer_period;

	*s = ((uint32_t) now) - *s;
	
	if (ms_now >= *ms)
	{
		*ms = ms_now - *ms;	
	}
	else
	{
		(*s)--;
		*ms = 1000 - (*ms - ms_now);
	}
}


time_t time(time_t *pt)
{
	if (pt)
	{
		*pt = rtc_time;
	}
	return(rtc_time);
}

void __attribute__((__interrupt__, no_auto_psv)) _T3Interrupt(void)
{
	rtc_time++;
	T2CONbits.TON = 0;			/* Stop 32-bit Timer */
	TMR2 = 0;
	TMR3 = 0;
	T2CONbits.TON = 1;			/* Start 32-bit Timer */
	IFS0bits.T3IF = 0; 			/* Clear Timer3 Interrupt Flag */
}

