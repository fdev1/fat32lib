#include "common.h"
#include "clocksw.h"
#include "sd.h"
#include "storage_device.h"
#include "fat.h"

//
// configuration bits
//
//_FOSCSEL( FNOSC_FRCPLL & IESO_OFF );	
//_FOSC( POSCMD_XT & OSCIOFNC_ON & FCKSM_CSECMD );
//_FWDT( FWDTEN_OFF );				
//_FPOR( FPWRT_PWR128 );
_FOSCSEL(FNOSC_PRIPLL & IESO_OFF);
_FOSC(POSCMD_XT & OSCIOFNC_ON & FCKSM_CSDCMD);
_FWDT(FWDTEN_OFF);
_FPOR(FPWRT_PWR128);

//
// global variables
//
SD_CARD_INFO sd_card;
STORAGE_DEVICE storage_device;
FAT_VOLUME_DATA fat_volume;

unsigned char sd_buffer[SD_WORKING_BYTES];

//
// function prototypes
//
void init_cpu();
void test1();
void test2();

//
// entry point
//
int main()
{
	long i;
	uint16_t err;
	char hello[] = "Hello World.";
	char goodbye[] = "Goodbye Cruel World.";


	//
	// clock the cpu
	//
	init_cpu();
	//
	// set port a as output
	//
	IO_PORT_SET_AS_OUTPUT(A);
	IO_PIN_SET_AS_OUTPUT(F, 0);
	IO_PIN_WRITE(A, 0, 1);
	IO_PIN_WRITE(A, 7, 1);
	IO_PIN_WRITE(F, 0, 0);
	//
	// initialize SD card and mount the FAT filesystem
	//
	err= sd_init(&sd_card, sd_buffer, DMA_GET_CHANNEL(0), DMA_GET_CHANNEL(1), 0b0001010);
	sd_get_storage_device_interface(&sd_card, &storage_device);
	err= fat_mount_volume(&fat_volume, &storage_device);

	err= fat_create_directory(&fat_volume, "\\fldr");
	if (err != FAT_SUCCESS) while (1);

	//err = fat_volume.Device->ReadSector(fat_volume.Device->Media, 0x0, fat_volume.SectorCache);
	
	FAT_FILE_HANDLE file;

	err= fat_open_file(&fat_volume,
		"\\hey.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_close(&file);
	if (err != FAT_SUCCESS) while (1);

	err= fat_open_file(&fat_volume,
		"\\bye.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_close(&file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_open_file(&fat_volume,
		"\\My Long Filename Folder\\hello.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_close(&file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_open_file(&fat_volume,
		"\\MYLONG~1\\hello.txt", FAT_FILE_ACCESS_APPEND, &file);
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	if (err != FAT_SUCCESS) while (1);
	err= fat_file_close(&file);
	if (err != FAT_SUCCESS) while (1);	
	err = 1;
	while (1)
	{
		err = err == 1 ? 0 : 1;
		IO_PIN_WRITE( A, 7, err );

		for (i = 0; i < 0xFFFFF; i++)
			IO_PIN_WRITE( A, 6, IO_PIN_READ(F, 4) == 0 );
		//sd_timeslot(&sd_card);
	}

	return 0;
}

void test1()
{


}

void test2()
{
	
}

void init_cpu()
{
	//
	// Configure oscillator to operate the device at 40Mhz
	// using the primary oscillator with PLL and a 8 MHz crystal
	// on the explorer 16
	//
	// Fosc = Fin * M / ( N1 * N2 )
	// Fosc = 8M * 32 / ( 2 * 2 ) = 80 MHz
	// Fcy = Fosc / 2 
	// Fcy = 80 MHz / 2
	// Fcy = 40 MHz
	//
	PLLFBDbits.PLLDIV = 38;
	CLKDIVbits.PLLPOST = 0;		
	CLKDIVbits.PLLPRE = 0;	
	//
	// switch to primary oscillator
	//	
	//clock_sw( );
	//
	// wait for PLL to lock
	//
	while ( OSCCONbits.LOCK != 0x1 );

}
#define __SP					( *( ( unsigned int* ) 0x1e ) )
#define __PCH					( *( ( unsigned int* ) ( __SP - 28 ) ) )
#define __PCL					( *( ( unsigned int* ) ( __SP - 26 ) ) )
#define __PC( ) \
	( ( unsigned long ) __PCH ) | \
	( 0x7FFFFF & ( ( ( unsigned long ) __PCL ) << 0x10 ) )
unsigned long address;
void __attribute__ ( ( __interrupt__, __no_auto_psv__ ) ) _AddressError(void);
void __attribute__ ( ( __interrupt__, __no_auto_psv__ ) ) _AddressError(void)
{
	address = __PC();
	address -= 1;
	address += 1;

	while(1);
	//INTCON1bits.ADDRERR=0;

	return;
}
