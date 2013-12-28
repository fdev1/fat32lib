#ifndef __DMA_H__
#define __DMA_H__

#include "common.h"

//
// dma constants
//
#define DMA_CHANNEL_WIDTH_WORD							0x0
#define DMA_CHANNEL_WIDTH_BYTE							0x1
#define DMA_DIR_PERIPHERAL_TO_RAM						0x0
#define DMA_DIR_RAM_TO_PERIPHERAL						0x1
#define DMA_INTERRUPT_MODE_HALF							0x1
#define DMA_INTERRUPT_MODE_FULL							0x0
#define DMA_ADDR_PERIPHERAL_INDIRECT					0x2
#define DMA_ADDR_REG_IND_NO_INCREMENT					0x1
#define DMA_ADDR_REG_IND_W_POST_INCREMENT				0x0
#define DMA_MODE_ONE_SHOT_W_PING_PONG					0x3
#define DMA_MODE_CONTINUOUS_W_PING_POINT				0x2
#define DMA_MODE_ONE_SHOT								0x1
#define DMA_MODE_CONTINUOUS								0x0

//
// DMA peripheral IRQs
//
#define DMA_PERIPHERAL_SPI1								( 0b0001010 )
#define DMA_PERIPHERAL_SPI2								( 0b0100001 )

//
// gets a dma channel
//
#define DMA_GET_CHANNEL( x )							( ( DMA_CHANNEL* ) &___dma_channel_##x )
#define DMA_CHANNEL_ENABLE( channel )					channel->ControlBits->CHEN = ( 0x1 )
#define DMA_CHANNEL_DISABLE( channel )					channel->ControlBits->CHEN = ( 0x0 )
#define DMA_SET_CHANNEL_WIDTH( channel, width )			channel->ControlBits->SIZE = ( width )
#define DMA_GET_CHANNEL_WIDTH( channel )				channel->ControlBits->SIZE
#define DMA_SET_CHANNEL_DIRECTION( channel, dir )		channel->ControlBits->DIR = ( dir )
#define DMA_SET_INTERRUPT_MODE( channel, mode )			channel->ControlBits->HALF = ( mode )
#define DMA_SET_NULL_DATA_WRITE_MODE( channel, mode )	channel->ControlBits->NULLW = ( mode )
#define DMA_SET_ADDRESSING_MODE( channel, mode )		channel->ControlBits->AMODE = ( mode )
#define DMA_SET_MODE( channel, mode )					channel->ControlBits->MODE = ( mode )
#define DMA_SET_PERIPHERAL( channel, peripheral )		channel->PeripheralIrq->IRQSEL = ( peripheral )
#define DMA_SET_PERIPHERAL_ADDRESS( channel, reg )		*channel->PeripheralAddress = ( volatile uint16_t ) ( reg )
#define DMA_SET_BUFFER_A( channel, address )			*channel->BufferA = ( volatile uint16_t ) ( ( address ) /*- DMA_BASE*/ )
#define DMA_SET_BUFFER_B( channel, address )			*channel->BufferB = ( volatile uint16_t ) ( ( address ) /*- DMA_BASE*/ )
#define DMA_SET_TRANSFER_LENGTH( channel, length )		*channel->TransferLength = ( length )
#define DMA_CHANNEL_ENABLE_INTERRUPT( channel )			BP_SET( channel->InterruptEnableBit )
#define DMA_CHANNEL_DISABLE_INTERRUPT( channel )		BP_CLR( channel->InterruptEnableBit )
#define DMA_CHANNEL_SET_INTERRUPT_FLAG( channel )		BP_SET( channel->InterruptFlag )
#define DMA_CHANNEL_CLEAR_INTERRUPT_FLAG( channel )		BP_CLR( channel->InterruptFlag )
#define DMA_CHANNEL_SET_INT_PRIORITY( channel, prty )	BP_WRITE( channel->InterruptPriority, prty )
//#define DMA_CHANNEL_SET_INTERRUPT( channel, isr )		system_register_isr( channel->Irq, ( WORD ) ( isr ) )
#define DMA_CHANNEL_FORCE_TRANSFER( channel )			channel->PeripheralIrq->FORCE = 0x1

//
// type definition for dma interrupts
//
typedef void DMA_INTERRUPT( void );

//
// structure of the DMA channel control bits
//	
typedef struct _DMA_CHANNEL_CONTROL_BITS 
{
	unsigned MODE:2;
    unsigned :2;
    unsigned AMODE:2;
    unsigned :5;
    unsigned NULLW:1;
    unsigned HALF:1;
    unsigned DIR:1;
    unsigned SIZE:1;
    unsigned CHEN:1;
}
DMA_CHANNEL_CONTROL_BITS, *PDMA_CHANNEL_CONTROL_BITS;

//
// DMA irl selection register structure
//
typedef struct _DMA_IRQSEL_BITS 
{
	unsigned IRQSEL:7;
	unsigned :8;
	unsigned FORCE:1;
}	
DMA_IRQ_BITS;

//
// defines a dma channel
//
typedef struct _DMA_CHANNEL 
{
	DMA_CHANNEL_CONTROL_BITS* const ControlBits;
	volatile uint16_t* const BufferA;
	volatile uint16_t* const BufferB;
	volatile uint16_t* const PeripheralAddress;
	DMA_IRQ_BITS* const PeripheralIrq;
	volatile uint16_t* const TransferLength;
	BIT_POINTER const InterruptFlag;
	BIT_POINTER const InterruptEnableBit;
	BIT_POINTER const InterruptPriority;
	unsigned char const Irq;
}
DMA_CHANNEL;	

//
// dma channel handles
//
extern const DMA_CHANNEL ___dma_channel_0;
extern const DMA_CHANNEL ___dma_channel_1;	
extern const DMA_CHANNEL ___dma_channel_2;

#endif
