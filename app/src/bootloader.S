.section .bootloader_section 
    .incbin "../bootloader/bootloader.bin"  @ The file path is relative to the linkerscript, not where the .S file actually is

