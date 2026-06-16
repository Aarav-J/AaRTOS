#include "common-defines.h"
#include <libopencm3/stm32/memorymap.h>
#include <libopencm3/cm3/vector.h>

#define BOOTLOADER_SIZE (0x8000U) // 32KB
#define MAIN_APP_START_ADRESS (FLASH_BASE + BOOTLOADER_SIZE)


static void jump_to_main(void) { 
    
    vector_table_t* main_vector_table = (vector_table_t*) MAIN_APP_START_ADRESS;
    main_vector_table->reset(); // Call the reset handler of the main application, which is the entry point of the app
    
}


int main(void) { 

    
    jump_to_main();

    // Never Return!
    return 0; 
}