#ifndef __SDB_WATCHPOINT_H__
#define __SDB_WATCHPOINT_H__

#include <common.h>

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[128];      // 监视的表达式
  word_t last_value;   // 上一次的值
  bool enabled;        // 是否启用
} WP;

#endif
