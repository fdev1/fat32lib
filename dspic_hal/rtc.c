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

/*
// if the device has boot RAM and you want to store the
// time in boot RAM then define RTC_BOOT
*/
#if defined(__dsPIC33FJ128GP802__)
//#define RTC_BOOT __attribute__((boot, section(".rtc")))
#define RTC_BOOT
#else
#define RTC_BOOT
#endif

static time_t RTC_BOOT rtc_time;
static uint32_t timer_period;
static uint32_t current_fcy;

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
	
	current_fcy = fcy;
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
	T5CONbits.TON = 0;			/* Stop any 16-bit Timer3 operation */
	T4CONbits.TON = 0;			/* Stop any 16/32-bit Timer2 operation */
	T4CONbits.T32 = 1;			/* Enable 32-bit Timer mode */
	T4CONbits.TCS = 0;			/* Select internal instruction cycle clock */
	T4CONbits.TGATE = 0;		/* Disable Gated Timer mode */
	T4CONbits.TCKPS = 0x3;		/* Select 1:1 Prescaler */
	
	TMR4 = 0x00; 				/* Clear 32-bit Timer (lsw) */
	TMR5 = 0x00;				/* Clear 32-bit Timer (msw) */

	/* 156250 = 0x0002625A */
	PR4 = LO16(timer_period);	/* 0x625A; // Load 32-bit period value (lsw) */
	PR5 = HI16(timer_period);	/* 0x0002; // Load 32-bit period value (msw) */

	IPC7bits.T5IP = 0x01;		/* Set Timer3 Interrupt Priority Level */
	IFS1bits.T5IF = 0;			/* Clear Timer3 Interrupt Flag */
	IEC1bits.T5IE = 1;			/* Enable Timer3 interrupt */
	T4CONbits.TON = 1;			/* Start 32-bit Timer */
}

void rtc_time_increment(void)
{
	rtc_time++;	
}

uint32_t rtc_get_fcy()
{
	return current_fcy;
}

void rtc_change_fcy(uint32_t fcy)
{
	current_fcy = fcy;
	timer_period = fcy / 256;			/* Calculate new period */
	T4CONbits.TON = 0;
	PR5 = HI16(timer_period); 			/* Load 32-bit period value (msw) */
	PR4 = LO16(timer_period); 			/* Load 32-bit period value (lsw) */
	TMR4 = 0x00; 						/* Clear 32-bit Timer (lsw) */
	TMR5 = 0x00;						/* Clear 32-bit Timer (msw) */
	T4CONbits.TON = 1;
}

void rtc_set_time(time_t new_time)
{
	rtc_time = new_time;
}


void rtc_timer(uint32_t* s, uint32_t* ms)
{
	time_t now;
	uint16_t tmrb, tmrc;	
	tmrb = TMR4;
	tmrc = TMR5HLD;
	now = rtc_time;

	*s = (uint32_t) now;
	*ms = ((uint32_t) tmrc) << 16 | tmrb;
	if (*ms > timer_period)
		while(1);
	*ms = (*ms * 1000) / timer_period;
}

void rtc_timer_elapsed(uint32_t* s, uint32_t* ms)
{
	time_t now;
	uint16_t tmrb, tmrc;
	uint32_t ms_now;
		
	tmrb = TMR4;
	tmrc = TMR5HLD;
	now = rtc_time;
	
	ms_now = ((uint32_t) tmrc) << 16 | tmrb;
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

void rtc_sleep(uint16_t ms_delay)
{
	uint32_t s, ms, s_elapsed, ms_elapsed;
	rtc_timer(&s, &ms);
	s_elapsed = s;
	ms_elapsed = ms;
	while (1)
	{
		rtc_timer_elapsed(&s_elapsed, &ms_elapsed);
		if ((s_elapsed * 1000) + ms_elapsed >= ms_delay)
			break;
		s_elapsed = s;
		ms_elapsed = ms;
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

void __attribute__((__interrupt__, no_auto_psv)) _T5Interrupt(void)
{
	rtc_time++;
	//T4CONbits.TON = 0;			/* Stop 32-bit Timer */
	//TMR4 = 0;
	//TMR5 = 0;
	//T4CONbits.TON = 1;			/* Start 32-bit Timer */
	IFS1bits.T5IF = 0; 			/* Clear Timer3 Interrupt Flag */
}

