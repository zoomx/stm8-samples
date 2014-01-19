/*
 * zacwire.c - functions for zacwire sensor processing
 *
 * Copyright 2014 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "stm8l.h"
#include "zacwire.h"
#include "interrupts.h"

int Temperature_value = 0;     // measurement result
U8 ZW_data_ready = 0;          // flag - measurement is ready
U16 ZW_result = 0;             // data got by ZW protocol
U8 bit_cntr = 0;               // current bit number (in ZW proto - to omit start/crc bits)
U8 temp_measurement = 0;       // flag - temperature measurement in progress

/**
 * Turn on ZW & start measurement
 */
void ZW_on(){
	ZW_result = 0;
	bit_cntr = 0;
	PA_ODR = GPIO_PIN2; // power on zacwire sensor
	ZW_data_ready = 0;
	temp_measurement = 1;
	// Timer2: one-pulse mode + enable
	TIM2_CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

/**
 * Turn off Zacwire
 */
void ZW_off(){
	temp_measurement = 0;
	PA_ODR ^= GPIO_PIN2; // turn off ZW
}

/**
 * Get bit value
 */
void ZW_catch_bit(){
	long temper;
	// value of TIM2 CC2 (pulse width in us)
	U16 TIM2_cc_value = TIM2_CCR2H << 8 | TIM2_CCR2L;
	if(bit_cntr == 8){ // omit CRC bit
		bit_cntr++;
		goto restart_timer;
	}
	if(TIM2_cc_value > 150){ // error: pulse too long
		ZW_off();
		return;
	}else if(TIM2_cc_value < 50){ // this is 1
		ZW_result |= 1;
	}else if(TIM2_cc_value < 70){ // omit strobe bit
		goto restart_timer;
	} // else - zero bit
	if(bit_cntr == 16){ // we have processed all bits
		// check: result must be 12bit
		if(ZW_result > 2047){
			ZW_off();
			return;
		}
		// ((double)ans)/2047.*70. - 10.;
		// correct temperature is VAL/2047*70-10, but we have integer (float*10), so:
		// T = (VAL*700)/2047 - 100, result have only 12 bits, so we can do this
		temper = ((long)ZW_result) * 700L;
		Temperature_value = (int)(temper / 2047L - 100L);
		ZW_data_ready = 1;
		ZW_off();
		return;
	}
	bit_cntr++;
	ZW_result <<= 1; // prepare to fill next bit
restart_timer:
	TIM2_CR1 = TIM_CR1_OPM | TIM_CR1_CEN; // turn on timer
}
