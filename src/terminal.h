#pragma once
#include <stddef.h>
#include <sys/types.h>

int term_init(void);
void term_cleanup(void);
void term_print(const char *buffer, size_t length);
ssize_t get_input(char *buffer, size_t size);
int term_get_width(void);
int term_get_height(void);