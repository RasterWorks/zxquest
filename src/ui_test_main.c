#include <arch/zxn.h>
extern int bank_ui_tests_main(void);
#ifdef STARTUP_TEST
extern unsigned char bank_startup_test(unsigned char,unsigned char,char *);
unsigned char launcher_test(unsigned char op,unsigned char arg,char *out){unsigned char r,m=ZXN_READ_REG(0x54);ZXN_WRITE_REG(0x54,92);r=bank_startup_test(op,arg,out);ZXN_WRITE_REG(0x54,m);return r;}
#endif
int main(void){ZXN_WRITE_REG(0x54,88);return bank_ui_tests_main();}
