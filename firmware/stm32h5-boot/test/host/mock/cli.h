#ifndef CLI_H_
#define CLI_H_
#include "bsp.h"
// 호스트 테스트에서는 CLI 코드를 통째로 뺀다.
#undef  CLI_USE
#define CLI_USE(module)   0
typedef struct { int argc; } cli_args_t;
bool cliAdd(const char *cmd_str, void (*p_func)(cli_args_t *));
void cliPrintf(const char *fmt, ...);
#endif
