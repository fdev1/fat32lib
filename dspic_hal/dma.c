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
 
#include "dma.h"

/*
// constants to all available dma channels
*/
const DMA_CHANNEL ___dma_channel_0 = 
{ 
	/* .ControlBits = */ (DMA_CHANNEL_CONTROL_BITS*) &DMA0CON, 
	/* .BufferA = */ (volatile uint16_t*) &DMA0STA, 
	/* .BufferB = */ (volatile uint16_t*) &DMA0STB, 
	/* .PeripheralAddress = */ (volatile uint16_t*) &DMA0PAD, 
	/* .PeripheralIrq = */ (DMA_IRQ_BITS*) &DMA0REQ, 
	/* .TransferLength = */ (volatile uint16_t*) &DMA0CNT, 
	/* .InterruptFlag = */ ((BIT_POINTER) { (uint16_t*) &IFS0, BIT4 }), 
	/* .InterruptEnableBit = */ ((BIT_POINTER) { (uint16_t*) &IEC0, BIT4 }), 
	/* .InterruptPriority = */ ((BIT_POINTER) { (uint16_t*) &IPC1, BIT2 | BIT1 | BIT0 }),
	/* .Irq = */ 0x4
};

const DMA_CHANNEL ___dma_channel_1 = 
{ 
	/* .ControlBits = */ (DMA_CHANNEL_CONTROL_BITS*) &DMA1CON, 
	/* .BufferA = */ (volatile uint16_t*) &DMA1STA, 
	/* .BufferB = */ (volatile uint16_t*) &DMA1STB, 
	/* .PeripheralAddress = */ (volatile uint16_t*) &DMA1PAD, 
	/* .PeripheralIrq = */ (DMA_IRQ_BITS*) &DMA1REQ, 
	/* .TransferLength = */ (volatile uint16_t*) &DMA1CNT, 
	/* .InterruptFlag = */ ((BIT_POINTER) { (uint16_t*) &IFS0, BIT14 }), 
	/* .InterruptEnableBit = */ ((BIT_POINTER) { (uint16_t*) &IEC0, BIT14 }), 
	/* .InterruptPriority = */ ((BIT_POINTER) { (uint16_t*) &IPC3, BIT10 | BIT9 | BIT8 }),
	/* .Irq = */ 14
};

const DMA_CHANNEL ___dma_channel_2 = { 
	/* .ControlBits = */ (DMA_CHANNEL_CONTROL_BITS*) &DMA2CON, 
	/* .BufferA = */ (volatile uint16_t*) &DMA2STA, 
	/* .BufferB = */ (volatile uint16_t*) &DMA2STB, 
	/* .PeripheralAddress = */ (volatile uint16_t*) &DMA2PAD, 
	/* .PeripheralIrq = */ (DMA_IRQ_BITS*) &DMA2REQ, 
	/* .TransferLength = */ (volatile uint16_t*) &DMA2CNT, 
	/* .InterruptFlag = */ ((BIT_POINTER) { (uint16_t*) &IFS1, BIT8 }), 
	/* .InterruptEnableBit = */ ((BIT_POINTER) { (uint16_t*) &IEC1, BIT8 }), 
	/* .InterruptPriority = */ ((BIT_POINTER) { (uint16_t*) &IPC6, BIT2 | BIT1 | BIT0 }),
	/* .Irq = */ 14
};
