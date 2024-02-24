#include "attack.h"
#include "eval.h"
#include "uci.h"

int main() {
  attack::init();
  eval::init();
  Search::init();
  zobrist::init();
  uci::loop();
}
