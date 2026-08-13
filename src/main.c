#include <stdio.h>

constexpr int WORDSZ = 8;

int main() {
  printf("Hello World - %d\n", WORDSZ);
  static_assert(WORDSZ <= 10);
}
