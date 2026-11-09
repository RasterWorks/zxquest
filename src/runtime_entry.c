#include <arch/zxn.h>
extern int bank_rt_main(void);
int main(void) { ZXN_WRITE_REG(0x54,52);return bank_rt_main(); }
