#ifndef ASSERT_DEF_H_
#define ASSERT_DEF_H_

#ifdef __cplusplus
 extern "C" {
#endif


#include "hw_def.h"


#ifdef  USE_FULL_ASSERT
  #define assert(expr) ((expr) ? (void)0U : assertFailed((uint8_t *)__FILE__, __LINE__, (uint8_t *)#expr))  
#else
  #define assert(expr) ((void)0U)
#endif 

bool assertInit(void);
void assertFailed(uint8_t *file, uint32_t line, uint8_t *expr);


#ifdef __cplusplus
 }
#endif


#endif 