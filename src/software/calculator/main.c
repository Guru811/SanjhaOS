#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_header() {
  printf("\033[38;2;255;140;0m");
  printf("\n  =========================================\n");
  printf("       SANJHA OS - CALCULATOR             \n");
  printf("  =========================================\n");
  printf("\033[0m");
  printf("  Operations: + - * / %%\n");
  printf("  Type 'exit' to quit\n\n");
}

int main() {
  print_header();

  char input[256];

  while (1) {
    printf("\033[38;2;0;220;255m  >> \033[0m");
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin))
      break;

    input[strcspn(input, "\n")] = 0;

    if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0)
      break;

    double a, b;
    char op;
    if (sscanf(input, "%lf %c %lf", &a, &op, &b) != 3) {
      printf("  Invalid input! Example: 10 + 5\n\n");
      continue;
    }

    double result;
    int valid = 1;

    switch (op) {
      case '+': result = a + b; break;
      case '-': result = a - b; break;
      case '*': result = a * b; break;
      case '/':
        if (b == 0) {
          printf("  Error: Division by zero!\n\n");
          valid = 0;
        } else {
          result = a / b;
        }
        break;
      case '%':
        if ((int)b == 0) {
          printf("  Error: Modulo by zero!\n\n");
          valid = 0;
        } else {
          result = (int)a % (int)b;
        }
        break;
      default:
        printf("  Unknown operator: %c\n\n", op);
        valid = 0;
    }

    if (valid)
      printf("  = %.6g\n\n", result);
  }

  printf("\n  Goodbye!\n\n");
  return 0;
}
