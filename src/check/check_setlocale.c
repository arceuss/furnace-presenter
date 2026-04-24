#include <locale.h>

int main(int argc, char** argv) {
  setlocale(LC_CTYPE, "");
  return 0;
}
