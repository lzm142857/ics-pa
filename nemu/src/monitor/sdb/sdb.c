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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "watchpoint.h"

// 添加监视点函数声明
WP* new_wp();
void free_wp(WP *wp);

extern WP *head;

word_t expr(char *e, bool *success);




static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPRESSION\n");
    return 0;
  }

  // 简单方法：如果第一个字符是引号，跳过它
  char *expr_str = args;
  if (expr_str[0] == '"') {
    expr_str++;  // 跳过开头的引号
  }
  
  // 如果最后一个字符是引号，去掉它
  size_t len = strlen(expr_str);
  if (len > 0 && expr_str[len-1] == '"') {
    expr_str[len-1] = '\0';
  }

  bool success;
  word_t result = expr(expr_str, &success);

  if (success) {
    printf("%s = %d\n", expr_str, result);
  } else {
    printf("Expression evaluation failed: %s\n", expr_str);
  }

  return 0;
}

// 设置监视点
static int cmd_w(char *args) {
  if (args == NULL) {
    printf("Usage: w EXPRESSION\n");
    return 0;
  }
  
  WP *wp = new_wp();
  if (wp == NULL) {
    printf("Failed to create watchpoint.\n");
    return 0;
  }
  
  // 去掉表达式两端的引号
  char *expr_str = args;
  if (expr_str[0] == '"') {
    expr_str++;
    size_t len = strlen(expr_str);
    if (len > 0 && expr_str[len-1] == '"') {
      expr_str[len-1] = '\0';
    }
  }
  
  strncpy(wp->expr, expr_str, sizeof(wp->expr) - 1);
  wp->expr[sizeof(wp->expr) - 1] = '\0';
  
  // 计算初始值
  bool success;
  wp->last_value = expr(wp->expr, &success);
  
  if (success) {
    printf("Watchpoint %d: %s\n", wp->NO, wp->expr);
  } else {
    printf("Invalid expression: %s\n", wp->expr);
    free_wp(wp);
  }
  
  return 0;
}

// 列出监视点
static int cmd_info_w(char *args) {
  WP *wp = head;
  if (wp == NULL) {
    printf("No watchpoints.\n");
    return 0;
  }
  
  printf("Num     Type           Disp Enb Address    What\n");
  while (wp != NULL) {
    if (wp->enabled) {
      printf("%-8dwatchpoint     keep y   <unknown>   %s = %d\n", 
             wp->NO, wp->expr, wp->last_value);
    }
    wp = wp->next;
  }
  return 0;
}

// 删除监视点
static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Usage: d NUM\n");
    return 0;
  }
  
  int no = atoi(args);
  WP *wp = head;
  while (wp != NULL) {
    if (wp->NO == no) {
      free_wp(wp);
      printf("Deleted watchpoint %d\n", no);
      return 0;
    }
    wp = wp->next;
  }
  
  printf("No watchpoint number %d.\n", no);
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info r - registers, info w - watchpoints\n");
    return 0;
  }
  
  if (strcmp(args, "w") == 0) {
    return cmd_info_w(args);  // 调用监视点列表函数
  }
  
  printf("Unknown info command: %s\n", args);
  return 0;
}




static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "p", "Evaluate expression", cmd_p },
  { "w", "Set watchpoint", cmd_w },
  { "info", "Print program info", cmd_info },
  { "d", "Delete watchpoint", cmd_d },

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
