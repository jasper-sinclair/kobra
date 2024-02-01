#pragma once

#include<vector>

#include"bitboard.h"
#include"random.h"

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr Bitboard WHITE_QUEEN_SIDE_PATH(0xe);
constexpr Bitboard WHITE_KING_SIDE_PATH(0x60);
constexpr Bitboard BLACK_QUEEN_SIDE_PATH(0xE00000000000000);
constexpr Bitboard BLACK_KING_SIDE_PATH(0x6000000000000000);

struct Castlings {
	int data;

	void set(int cr) { data |= cr; }
	void reset(int cr) { data &= ~cr; }
	bool canCastle(int cr) { return data & cr; }
	bool noCastling() { return data == 0; }
};

struct KingAttackInfo {
	Bitboard pinned;
	Bitboard attacks;
	bool doubleCheck;
	bool computed;

	bool check() { return bool(attacks); }
};

struct BoardStatus {
	int plyCount;
	int fiftyMoveCount;
	Castlings castlings;
	Key zobrist;

	int repetitions;
	Square epSquare;
	Piece captured;
	Move move;
	KingAttackInfo kingAttackInfo;

	friend std::ostream& operator<<(std::ostream& os, BoardStatus& bs);
};

struct Board {
	Piece board[N_SQUARES];
	Bitboard pieceBB[N_PIECE_TYPES];
	Bitboard colorBB[N_COLORS];
	Bitboard occupiedBB;
	Color sideToMove;
	std::vector<BoardStatus>history;
	BoardStatus* st;

	Board() = default;
	Board(const std::string& FEN);

	Board(const Board& other);
	Board& operator=(const Board& other);

	friend std::ostream& operator<<(std::ostream& os, Board& board);

	std::string fen();

	Piece pieceOn(Square sq);
	template<bool updateZobrist = true>
	void setPiece(Piece pc, Square sq);
	template<bool updateZobrist = true>
	void removePiece(Square sq);
	template<bool updateZobrist = true>
	void movePiece(Square from, Square to);

	BoardStatus* getBoardStatus();
	BoardStatus* boardStatus(int idx);
	Key key();

	bool canCastle(int cr);
	bool noCastling();

	void applyMove(Move m);
	void undoMove();
	void applyNullMove();
	void undoNullMove();

	Bitboard color(Color c);
	Bitboard pieces(PieceType pt);
	Bitboard pieces(Color c, PieceType pt);
	Bitboard occupied();
	// King square
	Square ksq(Color c);

	// Draw by fifty-move rule or threefold repetition
	bool isDraw();
	
	bool isCapture(Move m);
	bool isPromotion(Move m);
	bool givesCheck(Move m);
	// Tests whether a move from the transposition table or the principal variation is pseudo-legal
	bool isPseudoLegal(Move m);
	// Tests whether a pseudo-legal move is legal
	bool isLegal(Move m);

	bool isUnderAttack(Color us, Square sq);
	bool isInCheck();
	void generateKingAttackInfo(KingAttackInfo& k);
	
	Score evaluate();
	// Static Exchange Evaluation
	Score see(Move m);
	Bitboard attackersTo(Square sq, Bitboard occupied);
	Bitboard leastValuablePiece(Bitboard attadef, Color attacker, Piece& pc);

	bool nonPawnMaterial(Color c);

};

inline Piece Board::pieceOn(Square sq) {
	return board[sq];
}

inline BoardStatus* Board::getBoardStatus() {
	return &history.back();
}

inline BoardStatus* Board::boardStatus(int idx) {
	return &history[history.size() - idx - 1];
}

inline bool Board::canCastle(int cr) {
	return st->castlings.canCastle(cr);
}

inline bool Board::noCastling() {
	return st->castlings.noCastling();
}

inline Bitboard Board::color(Color c) {
	return colorBB[c];
}

inline Bitboard Board::pieces(PieceType pt) {
	return pieceBB[pt];
}

inline Bitboard Board::pieces(Color c, PieceType pt) {
	return color(c) & pieces(pt);
}

inline Bitboard Board::occupied() {
	return occupiedBB;
}

inline Square Board::ksq(Color c) {
	return pieces(c, KING).LSB();
}

inline Key Board::key() {
	return st->zobrist;
}

inline bool Board::isDraw() {
	return st->repetitions >= 2 || st->fiftyMoveCount >= 2 * 50;
}

inline bool Board::isCapture(Move m) {
	return pieceOn(move::to(m)) || move::moveType(m) == move::EN_PASSANT;
}

inline bool Board::isPromotion(Move m) {
	return move::moveType(m) == move::PROMOTION;
}

inline bool Board::nonPawnMaterial(Color c) {
	return bool(color(c) - pieces(PAWN) - pieces(KING));
}

inline Score Board::evaluate() {
	return evaluate();
}

namespace zobrist {
	inline Key psq[16][N_SQUARES];
	inline Key side;
	inline Key castling[16];
	inline Key enPassant[N_FILES];

	inline void init() {
		for (int i = 0; i < 16; ++i) {
			for (int j = 0; j < N_SQUARES; ++j)
				psq[i][j] = rng::getULL();
		}
		side = rng::getULL();
		for (int i = 0; i < 16; ++i)
			castling[i] = rng::getULL();
		for (int i = 0; i < N_FILES; ++i)
			enPassant[i] = rng::getULL();
	}
}