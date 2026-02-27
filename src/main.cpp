#include "attack.h"
#include "eval.h"
#include "uci.h"

int main(){
  attack::init();
  eval::init();
  search::init();
  zobrist::init();
  uci::init();
}
