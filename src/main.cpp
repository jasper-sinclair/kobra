#include "attacks.h"
#include "eval.h"
#include "uci.h"

int main() {
  attacks::init();
  zobrist::init();
  uci::loop();
}