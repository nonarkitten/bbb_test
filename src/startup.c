#include "main.h"

#include "arm.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "devicetree.h"
#include "m68k.h"
#include "hunkloader.h"
#include "duffcopy.h"
#include "emulogo.h"
#include "emufeatures.h"

extern uint8_t m68kcode[];

static const char bootstrapName_BE[] = "Emu68 runtime/ARM v7-a big endian";
static const char bootstrapName_LE[] = "Emu68 runtime/ARM v7-a little endian";

__attribute__ ((used, aligned (0x4000)))
static uint32_t mmu_table[4096] = { 0 }; 

static const char banner[] = 
"                                         ...                                    \n" 
"                  ....                 .:@@@:                                   \n" 
"             .:@@@@@@@@@@@:.          .@@@@ @:.                                 \n" 
"           .:@@@@@@@@@@@@@@@@:.       :@@@@@'':,                                \n" 
"         .:@@@@@@@@@@@@@@@@@@@@:.     :@@@@'                                    \n" 
"        :@@@@@@@@@@@@@@@@@@@@@@@@:.    '@@@.                                    \n" 
"       .@@@@@@@@@@@@@@@@@@@@@@@@@@@.    @@@@:                                   \n" 
"      .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:. .@@@@:                                   \n" 
"     :@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:                                   \n" 
"   .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'                                   \n" 
"  :@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'                                    \n" 
" .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'   .:@@@@@@@@:.      .:@@@@@@@@:.    \n" 
" :@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:'   .@@@@@@@@@@@@@@.  .@@@@@@@@@@@@@@.  \n" 
" '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@::''      :@@@@@@''@@@@@@:  :@@@@@@''@@@@@@:  \n" 
"  '@@@@@@@:'':@@@'  '@@@:''''''             @@@@@@'  '@@@@@'  @@@@@@'  '@@@@@@  \n" 
"   ':@@:'   .@@@'   .@@:                    @@@@@@            @@@@@@.  .@@@@@@  \n" 
"            :@@'   :@@@'                    @@@@@@ .:@@@:.    '@@@@@@..@@@@@@'  \n" 
"            :@:     :@@                     @@@@@@@@@@@@@@@.   .@@@@@@@@@@@@.   \n" 
"            :@'     '@@                     @@@@@@@''@@@@@@:  .@@@@@@''@@@@@@.  \n" 
"            @@       @@                     @@@@@@'  '@@@@@@  @@@@@@'  '@@@@@@  \n" 
"            @:       :@.                    @@@@@@    @@@@@@  @@@@@@    @@@@@@  \n" 
"           .@'       :@+                    @@@@@@    @@@@@@  @@@@@@    @@@@@@  \n" 
"           :@.       '@#                    @@@@@@.  .@@@@@@  @@@@@@.  .@@@@@@  \n" 
"          .@@:. .:.  .@@.                   :@@@@@@..@@@@@@:  :@@@@@@..@@@@@@:  \n" 
"          :#@@@@@':  :@@@@@::.              '@@@@@@@@@@@@@@'  '@@@@@@@@@@@@@@'  \n" 
"           ''''''     '''''' '                ':@@@@@@@@:'      ':@@@@@@@@:'     \n"
;

/* Linker variables */
extern char _stack_end, _stack_top, _bss_start, _bss_end;

/*  AM335x Physical Memory Map (1MB resolution)

    INTERNAL ADDRESSES                   M68K ADDRESSES
    =================================    ===================================================
    000 - 1FF GPMC                       000 - 0FF Amiga 16-bit memory, default do not cache
                                         002 - 009 Amiga Zorro-II space, cache optional
                                         00F - 00F Amiga Kickstart ROM, always cache
                                      
    800 - 97F 384MB User space           080 - 1FF Amiga 32-bit memory
    980 - AFF 384MB User space (1GB)     200 - 37F Amiga 32-bit memory
                                      
    400 - 5FF Internal Hardware          400 - 5FF Zorro-III Autoconfig Space
                                      
    980 - 9FF Application space       
    A00 - BFF Application space (1GB)
    
    
    1C0E    Set: 12, 11, 10, 4, 3, and 2
    
    
    31-20 BADR      Section Base Address. Must be 1MB aligned
    19    NS        Non-secure bit. This is used by the security extensions
    17    nG        Not global. Determines how this is marked in the TLB.
    18    0         Should be zero
    16    S         Shareable. 0 - non-shareable, 1 - shareable
    15    APX       Access Permissions Table
    14-12 TEX       
    11-10 AP[0:1]   
    9     IMP       Implementation defined
    8-5   Domain    Used as a memory protection mechanism. 16 possible domains
    4     XN        Execute never. Stops execution of page.
    3     C         Cacheable. 0 - inner non-cacheable, 1 - inner cacheable
    2     B         
    1     1         Should be one
    0     PXN       Privileged Execute Never - SBZ if PXN is not supported
    
    98000000 Entry point
    
*/

#define OFFSET                      0x44C00000

#define CM_DIV_M2_DPLL_MPU          0x002004A8
#define CM_CLKSEL_DPLL_MPU          0x0020042C
#define CM_CLKMODE_DPLL_MPU         0x00200488
#define CM_IDLEST_DPLL_MPU          0x00200420

#define DPLL_MN_BYP_MODE            4
#define DPLL_LOCK_MODE              7
#define ST_MN_BYPASS                1
#define ST_DPLL_CLK                 1
#define CLKINP                      24

typedef struct Dpll_mpu_struct {
	uint8_t N_divider;
	uint16_t M_multiplier;
	uint8_t M2_divider;
} Dpll_mpu;

//__attribute__((optimize("O0")))
//static void set_dpll(Dpll_mpu* dpll_mpu) {	
//    volatile uint8_t* map = (volatile uint8_t*)OFFSET;
//    bool pll_needs_lock = (dpll_mpu-> N_divider != (map[CM_CLKSEL_DPLL_MPU] & 0x7f))
//        || (dpll_mpu->M_multiplier != (map[CM_CLKSEL_DPLL_MPU + 1] | ((map[CM_CLKSEL_DPLL_MPU + 2] & 0x3f) << 8)));
//        
//    if(pll_needs_lock) {
//        // Switch PLL to bypass mode by setting CM_CLKMODE_DPLL_MPU.DPLL_EN to 0x4
//        // This also clears CM_CLKSEL_DPLL_MPU register to 0
//        map[CM_CLKMODE_DPLL_MPU] = (map[CM_CLKMODE_DPLL_MPU] & ~7) | DPLL_MN_BYP_MODE;
//        putchar('1');
//
//        // Wait for CM_IDLEST_DPLL_MPU.ST_MN_BYPASS = 1 to ensure PLL is in bypass
//        while ( !(map[CM_IDLEST_DPLL_MPU+1] & ST_MN_BYPASS) );
//        putchar('2');
//    
//        // Configure Multiply by setting CM_CLKSEL_DPLL_MPU.DPLL_MULT to the desired value.
//        map[CM_CLKSEL_DPLL_MPU + 1] = dpll_mpu->M_multiplier & 0xff;
//        putchar('3');
//        map[CM_CLKSEL_DPLL_MPU + 2] = (dpll_mpu->M_multiplier >> 8) & 0x07;
//        putchar('4');
// 
//        // Configure Divide by setting CM_CLKSEL_DPLL_MPU.DPLL_DIV to the desired value.
//        map[CM_CLKSEL_DPLL_MPU] = dpll_mpu->N_divider & 0x7f;
//        putchar('5');
//    }
//        
//    // M2 divider can also be changed on-the-fly; there is no need to put the PLL in bypass and back to lock mode
//    // Configure M2 divider by setting CM_DIV_M2_DPLL_MPU.DPLL_CLKOUT_DIV to the desired value.
//	if (dpll_mpu-> M2_divider !=  (map[CM_DIV_M2_DPLL_MPU] & 0x1f )) {
//        map[CM_DIV_M2_DPLL_MPU] = dpll_mpu->M2_divider & 0x1f;
//        putchar('6');
//    }
//
//    if(pll_needs_lock) {
//        // Switch over to lock mode by setting CM_CLKMODE_DPLL_MPU.DPLL_EN to 0x7.
//        map[CM_CLKMODE_DPLL_MPU] = (map[CM_CLKMODE_DPLL_MPU] & ~7) | DPLL_LOCK_MODE;
//        putchar('7');
//        
//        // Wait for CM_IDLEST_DPLL_MPU.ST_DPLL_CLK = 1 to ensure PLL is locked
//        while( !(map[CM_IDLEST_DPLL_MPU] & ST_DPLL_CLK) );
//        putchar('8'); 
//    }
//}

static void get_dpll_mpu(Dpll_mpu* dpll_mpu) {
    uint8_t* map = (uint8_t*)OFFSET;
    
	dpll_mpu->N_divider = map[CM_CLKSEL_DPLL_MPU] & 0x7f;
	dpll_mpu->M_multiplier = map[CM_CLKSEL_DPLL_MPU + 1] | ((map[CM_CLKSEL_DPLL_MPU + 2] & 0x07) << 8);
	dpll_mpu->M2_divider = map[CM_DIV_M2_DPLL_MPU] & 0x1f;
}

static double seconds(void) {
    uint32_t up = am335x_dmtimer1_get_counter();
    return (double)up / 24000000.0;
}

static inline char* ptime(double s) {
    static char buffer[32];
    uint32_t intPart = (uint32_t)s;
    double f = s - intPart;
    uint32_t fracPart = (uint32_t)(f * 1000000.0);
    sprintf(buffer, "%d.%06d seconds", intPart, fracPart);
    return buffer;
}

static inline char* psize(uint32_t size) {
    static char buffer[32];
    char *sfx;
    if(size < 1024) sfx = "B";
    else if(size < 1024 * 1024) { sfx = "KB"; size += 512; size /= 1024; }
    else if(size < 1024 * 1024 * 1024) { sfx = "MB"; size += 512 * 1024; size /= 1024 * 1024; }
    else { sfx = "GB"; size += 512 * 1024 * 1024; size /= 1024 * 1024 * 1024; }
    sprintf(buffer, "%d%s", size, sfx);
    return buffer;
}

static void print_context(struct M68KState *m68k) {
    printf("[JIT] M68K Context:\n");

    for (int i=0; i < 8; i++) {
        if (i==4) printf("\n");
        printf("    D%d = 0x%08x", i, m68k->D[i].u32);
    }
    printf("\n");

    for (int i=0; i < 8; i++) {
        if (i==4) printf("\n");
        printf("    A%d = 0x%08x", i, m68k->A[i].u32);
    }
    printf("\n");

    printf("    PC = 0x%08x    SR = %c%c%c%c%c", (int)m68k->PC,
        (m68k->SR & SR_X) ? 'X' : '.',
        (m68k->SR & SR_N) ? 'N' : '.',
        (m68k->SR & SR_Z) ? 'Z' : '.',
        (m68k->SR & SR_V) ? 'V' : '.',
        (m68k->SR & SR_C) ? 'C' : '.'
    );
    
    printf("\n    USP= 0x%08x    MSP= 0x%08x    ISP= 0x%08x\n", 
        m68k->USP.u32, m68k->MSP.u32, m68k->ISP.u32);

    for (int i=0; i < 8; i++) {
        union { double d; uint32_t u[2]; } u;
        if (i==4) printf("\n");
        u.d = m68k->FP[i];
        printf("    FP%d = %08x%08x", i, u.u[0], u.u[1]);
    }
    printf("\n");

    printf("    FPSR=0x%08x    FPIAR=0x%08x   FPCR=0x%04x\n", 
        m68k->FPSR, m68k->FPIAR, m68k->FPCR);
			
}

void main(void);
static const uint32_t top_of_ram = (uint32_t)main;
static const uint32_t jit_pool_base = (uint32_t)&_bss_end;
static const uint32_t jit_pool_end = 0x9fffffff;
extern struct M68KState *debug_m68k;
uint32_t last_PC = 0xffffffff;

//uint32_t data[128];
uint32_t stack[512];

#define DATA_SIZE 640*480

uint8_t chunky[DATA_SIZE];
uint8_t plane0[DATA_SIZE ];
uint8_t plane1[DATA_SIZE ];
uint8_t plane2[DATA_SIZE ];
uint8_t plane3[DATA_SIZE ];

uint8_t *bitmap[4] = {
    plane0, plane1, plane2, plane3
};


void start_emu(void *m68kcodeptr) {
    struct M68KTranslationUnit * unit = (void*)0;
	register struct M68KState * m68k asm("fp");
    struct M68KState __m68k;
    
    void (*arm_code)(); //(struct M68KState *ctx);
     
    M68K_InitializeCache();
	asm volatile ("mov %0, %1":"=r"(m68k):"r"(&__m68k));

    bitmap[0] = (uint8_t *)BE32((uintptr_t)bitmap[0]);
    bitmap[1] = (uint8_t *)BE32((uintptr_t)bitmap[1]);
    bitmap[2] = (uint8_t *)BE32((uintptr_t)bitmap[2]);
    bitmap[3] = (uint8_t *)BE32((uintptr_t)bitmap[3]);

	memset(&stack, 0xaa, sizeof(stack));

	bzero(m68k, sizeof(struct M68KState));
	m68k->PC = BE32((uint32_t)m68kcodeptr);
	
	m68k->A[0].u32 = BE32((uint32_t)chunky);
	m68k->A[1].u32 = BE32((uint32_t)chunky + DATA_SIZE);
	m68k->A[2].u32 = BE32((uint32_t)bitmap);
	m68k->A[7].u32 = BE32((uint32_t)&stack[511]);
	stack[511] = 0;
	
	print_context(m68k); 
	printf("[JIT] Let it go...\n");
	
    double t1=seconds();
	
	do {
		if (last_PC != (uint32_t)m68k->PC) {
			unit = M68K_GetTranslationUnit((uint16_t *)((uint32_t)m68k->PC));
			last_PC = (uint32_t)m68k->PC;
		}
		
		*(void**)(&arm_code) = unit->mt_ARMEntryPoint;
		arm_code(m68k);
	} while(m68k->PC != 0);
	
    double t2=seconds();
	
	printf("[JIT] Back from translated code\n");
	printf("[JIT] Time spent in m68k mode: %s\n", ptime(t2 - t1));

	print_context(m68k);
	
//#if 0
    printf("[JIT] --- Stack dump ---\n");
    for (int i=16; i > 0; --i) {
        printf("[JIT]   sp[%04d] = %08x\n", i, stack[i]);
        if (stack[i] == 0xaaaaaaaa)
            break;
    }
//#endif
    M68K_DumpStats();
}

/*     __  __       _         _____       _              
**    |  \/  | __ _(_)_ __   | ____|_ __ | |_ _ __ _   _ 
**    | |\/| |/ _` | | '_ \  |  _| | '_ \| __| '__| | | |
**    | |  | | (_| | | | | | | |___| | | | |_| |  | |_| |
**    |_|  |_|\__,_|_|_| |_| |_____|_| |_|\__|_|   \__, |
**                                                 |___/ 
*/ 
__attribute__((naked, section(".startup @")))
void main(void) { 
    // Brian Fraser's fix for UBoot and enable big endian mode
    asm volatile (
    "           sub     r0, r0, r0              \n" /* Brian Fraser's fix for UBoot            */
    "           mcr     p15, 0, r0, c1, c0, 0   \n" /* Disable MMU, instruction and data cache */
    "           setend  be                      \n" /* Switch to big endian (FTW!)             */
    );

    // Initialize system stacks
    asm volatile (
    "           msr     cpsr_c, #0xd1           \n" /* FIQ                                     */
    "           ldr     sp,=_stack_end + 0x10   \n"                               
    "           msr     cpsr_c, #0xd2           \n" /* IRQ                                     */
    "           ldr     sp,=_stack_end + 0x18   \n"                               
    "           msr     cpsr_c, #0xd7           \n" /* Abort                                   */
    "           ldr     sp,=_stack_end + 0x20   \n"                               
    "           msr     cpsr_c, #0xdb           \n" /* Undefined                               */
    "           ldr     sp,=_stack_end + 0x28   \n"                               
    "           msr     cpsr_c, #0xdf           \n" /* System                                  */
    "           ldr     sp,=_stack_end + 0x1000 \n"                               
    "           msr     cpsr_c, #0xd3           \n" /* Supervisor                              */
    "           ldr     sp,=_stack_top          \n"
    );

    // Invalidate cache and Enable Branch Prediction 
    // Allow unaligned access, effective only when MMU is enabled
    // Enable both instruction and data caches
    asm volatile (                                                                      
    "           mov r0, #0                      \n"                                     
    "           mcr p15, #0, r0, c7, c5, #6     \n"                                     
    "           mrc p15, 0, r0, c1, c0, 0       \n"
    "           orr r0, r0, #0x000004           \n" // Enable data cache
    "           orr r0, r0, #0x001800           \n" // Branch prediction and instruction cache
    "           orr r0, r0, #0x400000           \n" // Enable unaligned data
    "           bic r0, r0, #0x000002           \n" // Disable strict alignment check
    "           mcr p15, 0, r0, c1, c0, 0       \n"
    "           isb                             \n"                                     
    "           isb                             \n"                                     
    "           isb                             \n"                                     
    "           isb                             \n"                                     
    );                                                                                  
                                                                                         
    // Enable Neon/VFP Co-Processor
    asm volatile (                                                                      
    "           mrc     p15, #0, r1, c1, c0, #2 \n"    /* r1 = Access Control Register         */
    "           orr     r1, r1, #(0xf << 20)    \n"    /* enable full access for p10,11        */
    "           mcr     p15, #0, r1, c1, c0, #2 \n"    /* Access Control Register = r1         */
    "           mov     r1, #0                  \n"                                     
    "           mcr     p15, #0, r1, c7, c5, #4 \n"    /* flush prefetch buffer                */
    "           mov     r0,#0x40000000          \n"                                     
    "           fmxr    fpexc, r0               \n"    /* Set Neon/VFP Enable bit              */
    );                                                                                  
#if 0                                                                                        
    // disable am335x watchdog
    asm volatile (                                                                      
    "           ldr r0, =0x44e35000             \n"    /* load SOC_WDT_1_REGS                  */
    "           ldr r1, =0xaaaa0000             \n"                                     
    "           str r1, [r0, #0x48]             \n"    /* store 0xaaaa to WDT_WSPR             */
    "1:         ldr r1, [r0, #0x34]             \n"    /* loop until WDT_WWPS is 0             */
    "           cmp r1, #0x0                    \n"                                     
    "           bne 1b                          \n"                                     
    "           ldr r1, =0x55550000             \n"                                     
    "           str r1, [r0, #0x48]             \n"    /* store 0x5555 to WDT_WSPR             */
    "2:         ldr r1, [r0, #0x34]             \n"    /* loop until WDT_WWPS is 0             */
    "           cmp r1, #0x0                    \n"
    "           bne 2b                          \n"
    );
#endif
    // Clear the .bss section (zero init)
    asm volatile (                                                                      
    "           mov r0, #0                      \n"
    "           ldr r1, =_bss_start             \n"
    "           ldr r2, =_bss_end               \n"
    "1:         cmp r1, r2                      \n"
    "           strlo   r0, [r1], #4            \n"
    "           blo     1b                      \n"
    );

 	// Initialize bsp
    am335x_clock_enable_l3_l4wkup();
    am335x_dmtimer1_init();

    printf(banner);

    /* Check that we're running in big endian mode */
    { 
    uint32_t endian = 0x00000001;
    printf("[BOOT] Booting %s\n", (*(uint8_t*)&endian==0x01)?bootstrapName_LE:bootstrapName_BE);
    }
    printf("[BOOT] Built on %s at %s\n", __DATE__, __TIME__);     
    printf("[BOOT] Boot address is %p (%s)\n", top_of_ram, (top_of_ram==0x98000000)?"good":"bad");    
    printf("[BOOT] Stack %p-%p (size=%s)\n", &_stack_end, &_stack_top, psize(&_stack_top - &_stack_end));
    printf("[BOOT] BSS %p-%p (size=%s)\n", &_bss_end, 0x9FFFFFFF, psize((char*)0xA0000000 - &_bss_end));
    
    tlsf = tlsf_init();
    tlsf_add_memory(tlsf, &_bss_end, 0x9FFFFFFF - (uint32_t)&_bss_end);
    
    printf("[BOOT] Initializing MMU ... ");
    // 0x000 to 0x7FF, GPMC/PIO strongly ordered, no caching
    for (int i=0x000; i < 0x800; i++) mmu_table[i] = (i << 20) | 0x0c06;
    // 0x800 to 0x9FF, SDRAM, Caches write-through, write allocate, access for all
    for (int i=0x800; i < 0xA00; i++) mmu_table[i] = (i << 20) | 0x1c0e;
    // 0x400 to 0x403, internal ROM and SRAM, Caches write-through, write allocate, access for all
    for (int i=0x400; i < 0x403; i++) mmu_table[i] = (i << 20) | 0x1c0e;
    
    arm_flush_cache((uint32_t)mmu_table, sizeof(mmu_table));
    // Load new pointer to the mmu table
    
    asm volatile(
    "       dsb                         \n"
    "       mcr     p15,0,r0,c7,c5,0    \n" /* invalidate icache */
    "       isb                         \n"
    "       ldr     r4,=mmu_table       \n" /* Load MMU table pointer */
    "       mcr     p15,0,r4,c2,c0,0    \n" /* Write page_dir address to ttbr0 */
    "       mov     r8, #0              \n"
    "       mcr     p15,0,r8,c2,c0,2    \n" /* Write ttbr control N = 0 (use only ttbr0) */
    "       mov     r4, #1              \n"
    "       mcr     p15,0,r4,c3,c0,0    \n" /* Set domains - Dom0 is usable, rest is disabled */
    "       mrc     p15,0,r4,c1,c0,0    \n" /* Load control register */
    "       orr     r4,r4,#0x800000     \n" /* v6 page tables, subpages disabled */
    "       orr     r4,r4,#1            \n" /* MMU enabled */
    "       orr     r4,r4,#1<<25        \n" /* MMU tables in big endian */
    "       dsb                         \n"
    "       mcr     p15,0,r4,c1,c0,0    \n" /* Set control register and thus really enable mmu */
    "       isb                         \n"
    );
    printf("done\n");
         
    /* Get (and optionally set) CPU PLL */
    {
    Dpll_mpu mpu_pll;
    get_dpll_mpu(&mpu_pll);
    
	double MHz = (1.0 / ((double)mpu_pll.N_divider+1.0)) * \
        (double) mpu_pll.M_multiplier * CLKINP * (1.0 / (double)mpu_pll.M2_divider);
        
	printf("[BOOT] CPU Clock Speed = %d MHz (NDIV=%d, M1MULT=%d, M2DIV=%d)\n",  
        (int)MHz, mpu_pll.N_divider, mpu_pll.M_multiplier, mpu_pll.M2_divider);

    }
     
    printf("[BOOT] JIT Cache %p-%p (size=%s)\n", jit_pool_base, jit_pool_end, psize(1 + jit_pool_end - jit_pool_base));
    
	extern const unsigned char _acdhrystone[8836UL + 1];

    // fake malloc until we get kickstart going
    extern char * pool; 
    pool = (char*)0x82000000;
    // load our test program
    void *hunks = (void*)(4 + (uint32_t)LoadHunkFile((void*)_acdhrystone));
    
    //start_emu(m68kcode);
    printf("[BOOT] Hunk loaded, entry point at %p\n", hunks);
    printf("[BOOT] Startup took %s ... starting JIT\n", ptime(seconds()));
    start_emu(hunks); // <- if this is working we'll never go to the heartbeat
    
    init_led_output();
    heartbeat_forever();
    while(1);
}















