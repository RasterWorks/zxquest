#include <arch/zxn.h>
extern int bank_ui_tests_main(void);
int main(void){ZXN_WRITE_REG(0x54,88);return bank_ui_tests_main();}
