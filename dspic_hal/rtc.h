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
 
#include <time.h>
#include "common.h"

#ifndef RTC_H
#define RTC_H

/*
// initialize rtc
*/
void rtc_init(uint32_t fcy);

/*
// change the clock speed
*/
void rtc_change_fcy(uint32_t fcy);

/*
// set the time
*/
void rtc_set_time(time_t new_time);

/*
// get the current fcy setting
*/
uint32_t rtc_get_fcy();
void rtc_sleep(uint16_t ms_delay);
void rtc_timer(uint32_t* s, uint32_t* ms);
void rtc_timer_elapsed(uint32_t* s, uint32_t* ms);


#endif
