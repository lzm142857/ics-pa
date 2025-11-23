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
#include <regex.h>
#include <string.h>
#include<stdlib.h>
#include<assert.h>

typedef struct token{
	int type;
	char str[32];
} Token;

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_NUM,    //十进制整数
  TK_MINUS,   //减号
  TK_MUL,
  TK_DIV,
  TK_LP,
  TK_RP,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"[0-9]+", TK_NUM},   // 十进制整数
  {"\\-", '-'},         // 减号
  {"\\*", '*'},         // 乘号
  {"/", '/'},           // 除号
  {"\\(", '('},         // 左括号
  {"\\)", ')'},         // 右括号
};

// 添加 ARRLEN 宏定义
#define ARRLEN(arr) (int)(sizeof(arr) / sizeof(arr[0]))
#define NR_REGEX ARRLEN(rules)
#define NR_TOKEN 1024


static regex_t re[NR_REGEX] = {};
static Token tokens[NR_TOKEN] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;
static int token_index = 0;

// 添加函数声明
static word_t expression(bool *success);
static word_t term(bool *success);
static word_t factor(bool *success);
static bool check_token(int type);
static void consume_token(int type);


/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}



static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NUM:
            // 对于数字token，需要记录字符串值
            tokens[nr_token].type = rules[i].token_type;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';  // 确保字符串结束
            nr_token++;
            break;
    
          case TK_NOTYPE:
            // 空格直接跳过，不记录到tokens数组
            break;
    
          case '+': case '-': case '*': case '/': 
          case '(': case ')': case TK_EQ:
            // 对于运算符，只需记录类型
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
    
          default:
            // 其他未处理的token类型
            printf("Unhandled token type: %d\n", rules[i].token_type);
            return false;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

// 检查当前 token 类型
static bool check_token(int type) {
  return token_index < nr_token && tokens[token_index].type == type;
}

// 消耗当前 token
static void consume_token(int type) {
  assert(check_token(type));
  token_index++;
}

// 因子：数字或括号表达式
static word_t factor(bool *success) {
  if (check_token('(')) {
    consume_token('(');
    word_t result = expression(success);
    if (!check_token(')')) {
      *success = false;
      return 0;
    }
    consume_token(')');
    return result;
  } else if (check_token(TK_NUM)) {
    word_t value = atoi(tokens[token_index].str);
    consume_token(TK_NUM);
    return value;
  } else {
    *success = false;
    return 0;
  }
}

// 项：处理 * 和 /
static word_t term(bool *success) {
  word_t result = factor(success);
  if (!*success) return 0;

  while (check_token('*') || check_token('/')) {
    if (check_token('*')) {
      consume_token('*');
      result *= factor(success);
    } else if (check_token('/')) {
      consume_token('/');
      word_t divisor = factor(success);
      if (divisor == 0) {
        *success = false;
        return 0;
      }
      result /= divisor;
    }
    if (!*success) return 0;
  }
  return result;
}

// 表达式：处理 + 和 -
static word_t expression(bool *success) {
  word_t result = term(success);
  if (!*success) return 0;

  while (check_token('+') || check_token('-')) {
    if (check_token('+')) {
      consume_token('+');
      result += term(success);
    } else if (check_token('-')) {
      consume_token('-');
      result -= term(success);
    }
    if (!*success) return 0;
  }
  return result;
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  token_index = 0;
  word_t result = expression(success);
  
  // 检查是否消耗了所有 token
  if (*success && token_index != nr_token) {
    *success = false;
    return 0;
  }
  
  return result;
}
