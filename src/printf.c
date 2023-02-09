#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

/* Address of the BBB primary UART */
#define  UART0            (0x44E09000)

#define UART0_DLL  *(volatile uint8_t*)(UART0 + 0x00)
#define UART0_DLH  *(volatile uint8_t*)(UART0 + 0x04)

#define UART0_IER  *(volatile uint8_t*)(UART0 + 0x04)
#define UART0_IIR  *(volatile uint8_t*)(UART0 + 0x08)
#define UART0_LCR  *(volatile uint8_t*)(UART0 + 0x0C)
#define UART0_MCR  *(volatile uint8_t*)(UART0 + 0x10)
#define UART0_MDR1 *(volatile uint8_t*)(UART0 + 0x20)
#define UART0_SYSC *(volatile uint8_t*)(UART0 + 0x54)
#define UART0_SYSS *(volatile uint8_t*)(UART0 + 0x58)

#define CM_WKUP_UART0_CLKCTRL *(volatile uint8_t*)(0x44E004B4)
#define CM_WKUP_UART0_CLKCTRL_S *(volatile uint8_t*)(0x44E004B6)
#define CONF_UART0_RXD *(volatile uint8_t*)(0x44E10970)
#define CONF_UART0_TXD *(volatile uint8_t*)(0x44E10974)

void UART0Init(int baud) {
    /*Enable Func Clock*/
    CM_WKUP_UART0_CLKCTRL = 0x02;
    /*Wait clocks to get active*/
    while(CM_WKUP_UART0_CLKCTRL_S);

    // PAD_CONTROL_RXACTIVE | PAD_CONTROL_PULLUP
    CONF_UART0_RXD = 0x70;
    CONF_UART0_TXD = 0x58;

    /*Reset UART*/
	UART0_SYSC = 0x01;
	while(!(UART0_SYSS & 0x01));

    float div = 48000000.0f/(16.0f*(float)baud);
    unsigned int intdiv = (unsigned int) div;

	UART0_IER  = 0x00;         // Disable interrupts
    UART0_MDR1 = 0x07;         // Disable mode select

    UART0_LCR =  0xBF;         // Divisor latch enable, access DLL DHL, set UART as 8bit
    UART0_DLL =  0x00;         // DLL = 0
    UART0_DLH =  0x00;         // DLH = 0
    UART0_LCR =  0x03;         // Set UART 8-bit

    UART0_MCR =  0x03;         // Force /rts & /drt to active (low)
    UART0_IIR =  0x07;         // Clear FIFOs and enable them

    UART0_LCR =  0xBF;         // Divisor latch enable, access DLL DHL, set UART as 8bit
    UART0_DLL =  (intdiv & 0xFF);         // DLL = 0
    UART0_DLH =  ((intdiv >> 8) & 0x3F);         // DLH = 0
    UART0_LCR =  0x03;         // Set UART 8-bit

    UART0_MDR1 = 0;            // UART 16x oversampleing
}

static const char* l_base_chars = "0123456789abcdef";
static const char* u_base_chars = "0123456789ABCDEF";

static void writeUART(int c) {
	while(*((volatile uint8_t*)(UART0 + 0x68)) >= 63) continue;	
	*(volatile uint8_t*)UART0 = (uint8_t)c;
}

static void printUART(int c) {
	if(c == '\n') writeUART('\r');
	writeUART(c);
}

static int printchar(char **str, int c) {
	if(str) *(*str)++ = c; 
	else printUART(c);
	return 1; 
}

static inline int __attribute__ ((optimize("Os"))) _strlen(char *buf) {
    if (buf) {
        char *end = buf++;
        while(*end++);
        return end - buf;
    } 
    else return 0;
}

#define PAD_RIGHT 1
#define PAD_ZERO 2 

static inline int prints(char **out, char *str, int width, int pad) {
	if(str == NULL) return prints(out, "(null)", width, pad);
	
	int pad_char = (pad&PAD_ZERO)?'0':' ';
	int len = _strlen(str);
	int pc = 0;
	
	if(len > width) width = 0; else width -= len;
	if(!(pad&PAD_RIGHT)) for(;width;--width) pc += printchar(out, pad_char);
	/* dump string    */ for(;*str; ++str)   pc += printchar(out, *str);
	/* left padding   */ for(;width;--width) pc += printchar(out, pad_char);
	
	return pc;
}
 
static int printu(char **out, uint32_t x, uint32_t base, const char* base_chars, int width, int pad) {
    static char buffer[33];
	char *b = &buffer[32];

	*b = 0;
	do {
		uint32_t q = x / base;
        uint32_t r = x % base;
        
		*--b = base_chars[r];		
		x = q;
	} while(x);
	
    return prints(out, b, width, pad);
}

static inline int printi(char **out, int32_t x, int base, const char* base_chars, int width, int pad) {
	int pc = 0;
	
	if((x < 0) && (base == 10)) {
		pc += printchar(out, '-');
		x = -x;
	}

	return pc + printu(out, (uint32_t)x, base, base_chars, width, pad);
}

static int print(char **out, int *varg) {
	int width, pad;
	int pc = 0;
	char *format = (char *)(*varg++);

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format) {
				width = (width * 10) + (*format - '0');
			}
			
			/**/ if( *format == 's' ) pc += prints (out, *((char **)varg++), width, pad);
			else if( *format == 'd' ) pc += printi (out, *(int32_t*)varg++,  10, l_base_chars, width, pad);
			else if( *format == 'o' ) pc += printu (out, *(uint32_t*)varg++,  8, l_base_chars, width, pad);
			else if( *format == 'x' ) pc += printu (out, *(uint32_t*)varg++, 16, l_base_chars, width, pad);
			else if( *format == 'X' ) pc += printu (out, *(uint32_t*)varg++, 16, u_base_chars, width, pad);
			else if( *format == 'p' ) pc += printu (out, *(uint32_t*)varg++, 16, u_base_chars, 8, pad | PAD_ZERO);
			else if( *format == 'u' ) pc += printu (out, *(uint32_t*)varg++, 10, l_base_chars, width, pad);
			else if( *format == 'c' ) pc += printchar (out, *varg++);
			else /*                */ pc += printchar (out, *format);
			
		} else {
			pc += printchar (out, *format);
		}
	} 
	if (out) **out = '\0';
	return pc;
}

int printf(const char *format, ...) {
	return print(0, (int *)(&format));
}

int sprintf(char *out, const char *format, ...) {
	return print(&out, (int *)(&format));
}

int puts ( const char * str ) {
	return prints(0, (char*)str, 0, 0) + printchar(0, '\n');
} 

int putchar ( int c ) {
	writeUART( c );
	return c;
}


