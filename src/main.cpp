#include"attack.h"
#include"eval.h"
#include"uci.h"

int main() {
	attacks::init();
	eval::init();
	Search::init();
	zobrist::init();
	uci::loop();
}