#pragma once

void findCompiler(char** c, char** cxx);
int genRandomNumber(const int num1, const int num2 = 0);

#define PRETTYF(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
