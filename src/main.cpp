#include "attack.h"
#include "uci.h"

int main() {
  attack::init();
  Search::init();
  zobrist::init();
  uci::loop();
}
