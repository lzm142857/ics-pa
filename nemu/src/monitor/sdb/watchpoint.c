/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"
#include "watchpoint.h"  // 添加这行
#include <stdio.h>       // 添加这行
#include<stdbool.h>

word_t expr(char *e, bool *success);
#define NR_WP 32


static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;


void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    // 初始化新增成员
    wp_pool[i].expr[0] = '\0';
    wp_pool[i].last_value = 0;
    wp_pool[i].enabled = false;
  }

  head = NULL;
  free_ = wp_pool;
}


// ========== 在这里插入 new_wp() 函数 ==========
WP* new_wp() {
  if (free_ == NULL) {
    printf("No free watchpoints available.\n");
    return NULL;
  }

  // 从 free_ 链表取出一个监视点
  WP *wp = free_;
  free_ = free_->next;

  // 初始化监视点
  wp->expr[0] = '\0';
  wp->last_value = 0;
  wp->enabled = true;

  // 添加到 head 链表头部
  wp->next = head;
  head = wp;

  return wp;
}

// ========== 在这里插入 free_wp() 函数 ==========
void free_wp(WP *wp) {
  if (wp == NULL) return;

  // 从 head 链表中移除
  if (head == wp) {
    head = head->next;
  } else {
    WP *prev = head;
    while (prev != NULL && prev->next != wp) {
      prev = prev->next;
    }
    if (prev != NULL) {
      prev->next = wp->next;
    }
  }

  // 添加到 free_ 链表头部
  wp->next = free_;
  free_ = wp;

  // 重置监视点状态
  wp->enabled = false;
}

/* TODO: Implement the functionality of watchpoint */

