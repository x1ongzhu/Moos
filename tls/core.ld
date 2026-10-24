SECTIONS {
 .image 0 : { *(.text) *(.text.*) *(.rodata) *(.rodata.*) *(.data) *(.data.*) *(.bss) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.note*) }
}
