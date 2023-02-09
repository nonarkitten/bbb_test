#include "main.h"

static const char bootstrapName_BE[] = "Test runtime/ARM v7-a big endian";
static const char bootstrapName_LE[] = "Test runtime/ARM v7-a little endian";

/* Linker variables */
extern char _stack_end, _stack_top, _bss_start, _bss_end;

// #define LE(...)                  (__VA_ARGS__)
#define LE(...) __builtin_bswap32(__VA_ARGS__)

#define read32(ADDR)             (LE(*((uint32_t*)(ADDR))))
#define write32(ADDR, VAL)       *((int32_t*)(ADDR)) = (LE(VAL))
#define clrbits_le32(ADDR, MASK) *((int32_t*)(ADDR)) &= ~(LE(MASK))
#define setbits_le32(ADDR, MASK) *((int32_t*)(ADDR)) |= (LE(MASK))

#define WDT_WWPS                 (0x34)
#define WDT_WSPR                 (0x48)
#define WDT_WWPS_W_PEND_WSPR     (0x00000010u)
#define WDT_WRITE_PENDING_WSPR   (WDT_WWPS_W_PEND_WSPR)

static void WatchdogTimerDisable(unsigned int baseAdd) {
    static const uint32_t WDT_DISABLE_SEQ1 = (0x0000AAAAu);
    static const uint32_t WDT_DISABLE_SEQ2 = (0x00005555u);

    /* Write the first value of the watchdog timer disabling sequence */
    write32(baseAdd + WDT_WSPR, WDT_DISABLE_SEQ1);
    /* Wait until write is successful to WDT_WSPR register */
    while (read32(baseAdd + WDT_WWPS) & WDT_WRITE_PENDING_WSPR) {}
    /* Write the second value of the watchdog timer disabling sequence */
    write32(baseAdd + WDT_WSPR, WDT_DISABLE_SEQ2);
    /* Wait until write is successful to WDT_WSPR register */
    while (read32(baseAdd + WDT_WWPS) & WDT_WRITE_PENDING_WSPR) {}
}

void main(void) {
    // Brian Fraser's fix for UBoot and enable big endian mode
    asm volatile(
        "           sub     r0, r0, r0              \n" /* Brian Fraser's fix for UBoot            */
        "           mcr     p15, 0, r0, c1, c0, 0   \n" /* Disable MMU, instruction and data cache */
    );

    // Initialize system stacks
    asm volatile(
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
        "           ldr     sp,=_stack_top          \n");

    //  Invalidate cache and Enable Branch Prediction
    //  Allow unaligned access, effective only when MMU is enabled
    //  Enable both instruction and data caches
    asm volatile(
        "           mov r0, #0                      \n"
        "           mcr p15, #0, r0, c7, c5, #6     \n"
        "           mrc p15, 0, r0, c1, c0, 0       \n"
        "           orr r0, r0, #0x000004           \n"  // Enable data cache
        "           orr r0, r0, #0x001800           \n"  // Branch prediction and instruction cache
        "           orr r0, r0, #0x400000           \n"  // Enable unaligned data
        "           bic r0, r0, #0x000002           \n"  // Disable strict alignment check
        "           mcr p15, 0, r0, c1, c0, 0       \n"
        "           isb                             \n"
        "           isb                             \n"
        "           isb                             \n"
        "           isb                             \n");

    // Enable Neon/VFP Co-Processor
    asm volatile(
        "           mrc     p15, #0, r1, c1, c0, #2 \n" /* r1 = Access Control Register         */
        "           orr     r1, r1, #(0xf << 20)    \n" /* enable full access for p10,11        */
        "           mcr     p15, #0, r1, c1, c0, #2 \n" /* Access Control Register = r1         */
        "           mov     r1, #0                  \n"
        "           mcr     p15, #0, r1, c7, c5, #4 \n" /* flush prefetch buffer                */
        "           mov     r0,#0x40000000          \n"
        "           fmxr    fpexc, r0               \n" /* Set Neon/VFP Enable bit              */
    );

    // disable am335x watchdog
    WatchdogTimerDisable(0x44e35000);
    // asm volatile(
    //     "           ldr r0, =0x44e35000             \n" /* load SOC_WDT_1_REGS                  */
    //     "           ldr r1, =0xaaaa0000             \n"
    //     "           str r1, [r0, #0x48]             \n" /* store 0xaaaa to WDT_WSPR             */
    //     "1:         ldr r1, [r0, #0x34]             \n" /* loop until WDT_WWPS is 0             */
    //     "           cmp r1, #0x0                    \n"
    //     "           bne 1b                          \n"
    //     "           ldr r1, =0x55550000             \n"
    //     "           str r1, [r0, #0x48]             \n" /* store 0x5555 to WDT_WSPR             */
    //     "2:         ldr r1, [r0, #0x34]             \n" /* loop until WDT_WWPS is 0             */
    //     "           cmp r1, #0x0                    \n"
    //     "           bne 2b                          \n");

    // Clear the .bss section (zero init)
    asm volatile(
        "           mov r0, #0                      \n"
        "           ldr r1, =_bss_start             \n"
        "           ldr r2, =_bss_end               \n"
        "1:         cmp r1, r2                      \n"
        "           strlo   r0, [r1], #4            \n"
        "           blo     1b                      \n");

    // Initialize bsp
    am335x_clock_enable_l3_l4wkup();
    am335x_dmtimer1_init();
    // am335x_clock_enable_gpio_module(AM335X_CLOCK_GPIO0);
    // am335x_clock_enable_gpio_module(AM335X_CLOCK_GPIO1);
    // am335x_clock_enable_gpio_module(AM335X_CLOCK_GPIO2);
    // am335x_clock_enable_gpio_module(AM335X_CLOCK_GPIO3);
    // am335x_clock_enable_uart_module(AM335X_CLOCK_UART0);
    UART0Init(115200);

    /* Check that we're running in big endian mode */
    {
        uint32_t endian = 0x00000001;
        printf("[BOOT] Booting %s\n", (*(uint8_t*)&endian == 0x01) ? bootstrapName_LE : bootstrapName_BE);
    }
    printf("[BOOT] Built on %s at %s\n", __DATE__, __TIME__);

    while (1) {
        printf("Y");
    }
}
