#include "test_common.h"

void testSlotLogic(void);
void testApply(void);
void testBootLog(void);
void testPowerLoss(void);


int main(int argc, char **argv)
{
  bool verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

  mockLogEnable(verbose);
  mockResetClear();

  printf("=== stm32h5-boot host unit tests ===\n\n");

  testSlotLogic();
  testApply();
  testBootLog();
  testPowerLoss();

  printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
