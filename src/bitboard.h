#pragma once

#include<algorithm>
#include<cassert>
#include<cstdint>
#include<cstring>
#include<sstream>
#include<string>

#if defined(_MSC_VER)
#include<intrin.h>
#endif

using Color = bool;
using Piece = int32_t;
using PieceType = int32_t;
using File = int8_t;
using Rank = int8_t;
using Direction = int32_t;
using Square = uint8_t;
using Move = uint16_t;
using Key = uint64_t;

using Score = int16_t;
using Depth = int16_t;

constexpr int MAX_DEPTH = 256;
constexpr int MAX_PLY = MAX_DEPTH;
constexpr int CONTINUATION_PLY = 6;

enum Colors {
	WHITE,
	BLACK,
	N_COLORS
};

enum Pieces {
	NO_PIECE,
	WHITE_PAWN,
	WHITE_KNIGHT,
	WHITE_BISHOP,
	WHITE_ROOK,
	WHITE_QUEEN,
	WHITE_KING,
	BLACK_PAWN = WHITE_PAWN + 8,
	BLACK_KNIGHT,
	BLACK_BISHOP,
	BLACK_ROOK,
	BLACK_QUEEN,
	BLACK_KING,
	N_PIECES
};

enum PieceTypes {
	NO_PIECE_TYPE,
	PAWN,
	KNIGHT,
	BISHOP,
	ROOK,
	QUEEN,
	KING,
	N_PIECE_TYPES
};

enum Squares {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8,
	NO_SQUARE = 0,
	N_SQUARES = 64
};

enum Files {
	FILE_A,
	FILE_B,
	FILE_C,
	FILE_D,
	FILE_E,
	FILE_F,
	FILE_G,
	FILE_H,
	N_FILES
};

enum Ranks {
	RANK_1,
	RANK_2,
	RANK_3,
	RANK_4,
	RANK_5,
	RANK_6,
	RANK_7,
	RANK_8,
	N_RANKS
};

enum Directions {
	NORTH = 8,
	EAST = 1,
	SOUTH = -NORTH,
	WEST = -EAST,
	NORTHEAST = NORTH + EAST,
	NORTHWEST = NORTH + WEST,
	SOUTHEAST = SOUTH + EAST,
	SOUTHWEST = SOUTH + WEST,
	N_DIRECTIONS = 8
};

enum CastlingRights {
	NO_CASTLING,
	WHITE_QUEEN_SIDE = 1,
	WHITE_KING_SIDE = 1 << 1,
	BLACK_QUEEN_SIDE = 1 << 2,
	BLACK_KING_SIDE = 1 << 3,

	QUEEN_SIDE = WHITE_QUEEN_SIDE | BLACK_QUEEN_SIDE,
	KING_SIDE = WHITE_KING_SIDE | BLACK_KING_SIDE,
	WHITE_CASTLING = WHITE_KING_SIDE | WHITE_QUEEN_SIDE,
	BLACK_CASTLING = BLACK_KING_SIDE | BLACK_QUEEN_SIDE,
	ANY_CASTLING = WHITE_CASTLING | BLACK_CASTLING,
};

enum Scores :int16_t {
	DRAW_SCORE = 0,
	INFINITY_SCORE = 32001,
	MATE_SCORE = 32000,
	MIN_MATE_SCORE = MATE_SCORE - MAX_DEPTH,
	STOP_SCORE = 32002,
	MAX_EVAL_SCORE = 30000
};

namespace color {
	constexpr Color make(Piece pc) {
		return pc >> 3;
	}
}

namespace pieceType {
	constexpr PieceType make(Piece pc) {
		return pc & 7;
	}
}

namespace piece {
	constexpr Piece make(Color c, PieceType pt) {
		return (c << 3) + pt;
	}

	constexpr Piece relative(Piece pc) {
		return pc ? pc ^ 8 : pc;
	}

	const std::string PIECE_TO_CHAR = " PNBRQK  pnbrqk";
}

namespace file {
	constexpr File make(Square sq) {
		return sq & 7;
	}

	constexpr char CHAR_IDENTIFYERS[N_FILES] = { 'a','b','c','d','e','f','g','h' };
}

namespace rank {
	constexpr Rank make(Square sq) {
		return sq >> 3;
	}

	constexpr char CHAR_IDENTIFYERS[N_RANKS] = { '1','2','3','4','5','6','7','8' };
}

namespace direction {
	constexpr Direction pawnPush(Color c) {
		return c ? SOUTH : NORTH;
	}
}

namespace square {
	constexpr Square make(File file, Rank rank) {
		return (rank << 3) + file;
	}

	constexpr Square make(std::string_view sv) {
		return square::make(static_cast<File>(sv[0] - 'a'), static_cast<Rank>(sv[1] - '1'));
	}

	constexpr void mirror(Square& sq) { sq ^= A8; }

	constexpr Square relative(Color c, Square sq) { return c ? sq ^ A8 : sq; }

	inline std::string toString(Square sq) {
		return std::string(1, file::CHAR_IDENTIFYERS[file::make(sq)]) +
			std::string(1, rank::CHAR_IDENTIFYERS[rank::make(sq)]);
	}

	constexpr bool isValid(Square sq) {
		return sq >= A1 && sq <= H8;
	}

	inline uint8_t distance(Square s1, Square s2) {
		return std::max(
			std::abs(file::make(s1) - file::make(s2)), 
			std::abs(rank::make(s1) - rank::make(s2))
		);
	}
}

struct Bitboard {
	uint64_t data;

	Bitboard() = default;
	constexpr Bitboard(uint64_t data) :data(data) {}

	constexpr static Bitboard fromSquare(Square sq) { 
		return { static_cast<uint64_t>(1) << sq }; 
	}

	constexpr bool isSet(Square sq) const { 
		return data & static_cast<uint64_t>(1) << sq; 
	}

	constexpr void set(Square sq) { 
		data |= static_cast<uint64_t>(1) << sq; 
	}

	constexpr void clear(Square sq) { 
		data &= ~(static_cast<uint64_t>(1) << sq); 
	}

	constexpr void toggle(Square sq) { 
		data ^= static_cast<uint64_t>(1) << sq; 
	}

	template<Direction d>constexpr Bitboard shift() const;

#if defined(_MSC_VER)

	void mirror() { 
		data = _byteswap_uint64(data); 
	}

	Bitboard mirrored() const
   { 
		return _byteswap_uint64(data); 
	}

	int popcount() { 
		return static_cast<int>(_mm_popcnt_u64(data)); 
	}

	Square LSB() const
   {
		unsigned long idx;
		_BitScanForward64(&idx, data);
		return static_cast<Square>(idx);
	}

	Square MSB() const
   {
		unsigned long idx;
		_BitScanReverse64(&idx, data);
		return static_cast<Square>(idx);
	}

#else
	void mirror() {
		constexpr static uint64_t k1 = 0x00ff00ff00ff00ff;
		constexpr static uint64_t k2 = 0x0000ffff0000ffff;
		data = data >> 8 & k1 | (data & k1) << 8;
		data = data >> 16 & k2 | (data & k2) << 16;
		data = data >> 32 | data << 32;
	}

	Bitboard mirrored() {
		Bitboard copy(data);
		copy.mirror();
		return copy;
	}

	int popcount() {
		return __builtin_popcountll(data);
	}

	Square LSB() {
		return Square(__builtin_ctzll(data));
	}

	Square MSB() {
		return Square(__builtin_clzll(data) ^ 63);
	}

#endif

	Square popLSB() {
      const Square sq = LSB();
		data &= data - 1;
		return sq;
	}

	Square popMSB() {
      const Square sq = MSB();
		data &= data - 1;
		return sq;
	}

	constexpr friend Bitboard operator&(Bitboard a, Bitboard b) {
		return a.data & b.data;
	}

	constexpr friend Bitboard operator|(Bitboard a, Bitboard b) {
		return a.data | b.data;
	}

	constexpr friend Bitboard operator^(Bitboard a, Bitboard b) {
		return a.data ^ b.data;
	}

	constexpr friend Bitboard operator-(Bitboard a, Bitboard b) {
		return a.data & ~b.data;
	}

	constexpr friend Bitboard operator*(Bitboard a, Bitboard b) {
		return a.data * b.data;
	}

	constexpr void operator&=(Bitboard b) {
		data &= b.data;
	}

	constexpr void operator|=(Bitboard b) {
		data |= b.data;
	}

	constexpr void operator^=(Bitboard b) {
		data ^= b.data;
	}

	constexpr void operator-=(Bitboard b) {
		data &= ~b.data;
	}

	constexpr Bitboard operator~() const
   {
		return ~data;
	}

	constexpr explicit operator bool() {
		return static_cast<bool>(data);
	}

	constexpr friend Bitboard operator<<(Bitboard b, uint8_t shift) {
		return b.data << shift;
	}

	constexpr friend Bitboard operator>>(Bitboard b, uint8_t shift) {
		return b.data >> shift;
	}

	constexpr bool operator==(Bitboard b) const
   {
		return data == b.data;
	}

	friend std::ostream& operator<<(std::ostream& os, Bitboard b) {
		for (Rank r = RANK_8; r >= RANK_1; --r) {
			for (File f = FILE_A; f <= FILE_H; ++f) {
            const Square sq = square::make(f, r);
				os << (b.isSet(sq) ? 'X' : '.') << ' ';
			}
			os << '\n';
		}
		return os;
	}
};

constexpr Bitboard FILE_A_BB(0x101010101010101);
constexpr Bitboard FILE_B_BB = FILE_A_BB << 1;
constexpr Bitboard FILE_C_BB = FILE_A_BB << 2;
constexpr Bitboard FILE_D_BB = FILE_A_BB << 3;
constexpr Bitboard FILE_E_BB = FILE_A_BB << 4;
constexpr Bitboard FILE_F_BB = FILE_A_BB << 5;
constexpr Bitboard FILE_G_BB = FILE_A_BB << 6;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;

constexpr Bitboard RANK_1_BB(0xff);
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_3_BB = RANK_1_BB <<	16;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_6_BB = RANK_1_BB << 40;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

constexpr Bitboard DIAG_C2_H7(0x0080402010080400);

template<Direction d>
constexpr Bitboard Bitboard::shift() const
{
   const Bitboard b(*this);
	if (d == NORTH) return b << NORTH;
	else if (d == SOUTH) return b >> NORTH;
	else if (d == 2 * NORTH) return b << 2 * NORTH;
	else if (d == 2 * SOUTH) return b >> 2 * NORTH;
	else if (d == NORTHEAST) return (b << NORTHEAST) - FILE_A_BB;
	else if (d == NORTHWEST) return (b << NORTHWEST) - FILE_H_BB;
	else if (d == SOUTHEAST) return (b >> NORTHWEST) - FILE_A_BB;
	else if (d == SOUTHWEST) return (b >> NORTHEAST) - FILE_H_BB;
	else return {};
}

namespace move {
	// A move has the following format:

	// bit 0-5:   from square
	// bit 6-11:  to square
	// bit 12-13: promotion piece type
	// bit 14-15: move type

	enum MoveType {
		NORMAL,
		EN_PASSANT = 1 << 14,
		PROMOTION = 2 << 14,
		CASTLING = 3 << 14
	};

	constexpr Move make(Square from, Square to) {
		return from | to << 6;
	}
	constexpr Move make(Square from, Square to, MoveType moveType) {
		return from | to << 6 | moveType;
	}
	template<PieceType pieceType>
	constexpr Move make(Square from, Square to) {
		return from | to << 6 | PROMOTION | (pieceType - 2) << 12;
	}

	constexpr Square from(Move m) {
		return m & 0x3f;
	}

	constexpr Square to(Move m) {
		return m >> 6 & 0x3f;
	}

	constexpr MoveType moveType(Move m) {
		return static_cast<MoveType>(m & 0x3 << 14);
	}

	constexpr PieceType pieceType(Move m) {
		return (m >> 12 & 0x3) + 2;
	}

	inline std::string toString(Move m) {
		std::string s = square::toString(from(m)) + square::toString(to(m));

		if (moveType(m) == PROMOTION) {
			switch (pieceType(m)) {
			case KNIGHT:
				return s + 'n';
			case BISHOP:
				return s + 'b';
			case ROOK:
				return s + 'r';
			case QUEEN:
				return s + 'q';
      default: ;
      }
    }
		return s;
	}
};