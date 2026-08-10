// RUN: %clang_cc1 -triple tms9900-unknown-none -fsyntax-only -verify %s

__attribute__((interrupt)) int object;
// expected-warning@-1 {{'interrupt' attribute only applies to functions}}

__attribute__((interrupt(1))) void has_attribute_argument(void);
// expected-error@-1 {{'interrupt' attribute takes no arguments}}

__attribute__((interrupt)) int has_result(void);
// expected-warning@-1 {{TMS9900 'interrupt' attribute only applies to functions that have a 'void' return type}}

__attribute__((interrupt)) void has_parameter(int value);
// expected-warning@-1 {{TMS9900 'interrupt' attribute only applies to functions that have no parameters}}

__attribute__((interrupt)) void handler(void) {}

void caller(void) {
  handler();
  // expected-error@-1 {{TMS9900 interrupt service routine cannot be called directly}}
}
