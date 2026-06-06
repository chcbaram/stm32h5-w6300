#ifndef CLI_NET_H_
#define CLI_NET_H_


#include "ap_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

bool cliNetInit(uint16_t port);   
void cliNetPoll(void);            
bool cliNetIsConnected(void);     

#ifdef __cplusplus
}
#endif

#endif