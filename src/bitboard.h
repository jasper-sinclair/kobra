#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "main.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4702)
#else
#endif

namespace color {
  constexpr Color make(const Piece pc) { return pc >> 3; }
} // namespace color

namespace piece_type {
  constexpr PieceType make(const Piece pc) { return pc & 7; }
} // namespace piece_type

namespace piece {
  constexpr Piece make(const Color c, const PieceType pt) {
    return (c << 3) + pt;
  }

  constexpr Piece relative(const Piece pc) { return pc ? pc ^ 8 : pc; }
  inline std::string piece_to_char = " PNBRQK  pnbrqk";
} // namespace piece

namespace file {
  constexpr File make(const Square sq) { return static_cast<int8_t>(sq & 7); }

  constexpr char kCharIdentifiers[N_FILES] = {
    'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h'
  };
} // namespace file

namespace rank {
  constexpr Rank make(const Square sq) { return static_cast<int8_t>(sq >> 3); }

  constexpr char kCharIdentifiers[N_RANKS] = {
    '1', '2', '3', '4',
    '5', '6', '7', '8'
  };
} // namespace rank

namespace direction {
  constexpr Direction PawnPush(const Color c) { return c ? SOUTH : NORTH; }
} // namespace direction

namespace square {
  constexpr Square make(const File file, const Rank rank) {
    return static_cast<Square>((rank << 3) + file);
  }

  constexpr Square make(const std::string_view sv) {
    return make(static_cast<File>(sv[0] - 'a'), static_cast<Rank>(sv[1] - '1'));
  }

  constexpr void mirror(Square& sq) { sq ^= A8; }

  constexpr Square relative(const Color c, const Square sq) {
    return c ? sq ^ A8 : sq;
  }

  inline std::string ToString(const Square sq) {
    return std::string(1, file::kCharIdentifiers[file::make(sq)]) +
      std::string(1, rank::kCharIdentifiers[rank::make(sq)]);
  }

  constexpr bool IsValid(const Square sq) { return sq >= A1 && sq <= H8; }

  inline uint8_t distance(const Square s1, const Square s2) {
#define GETMAX(a, b) (((a) > (b)) ? (a) : (b))
    return static_cast<uint8_t>(
      GETMAX(std::abs(file::make(s1) - file::make(s2)),
             std::abs(rank::make(s1) - rank::make(s2))));
  }
} // namespace square

struct Bitboard {
  uint64_t data;
  Bitboard() = default;

  constexpr Bitboard(const uint64_t data) : data(data) {}

  constexpr static Bitboard FromSquare(const Square sq) {
    return {static_cast<uint64_t>(1) << sq};
  }

  constexpr void set(const Square sq) {
    data |= static_cast<uint64_t>(1) << sq;
  }

  constexpr void clear(const Square sq) {
    data &= ~(static_cast<uint64_t>(1) << sq);
  }

  constexpr void toggle(const Square sq) {
    data ^= static_cast<uint64_t>(1) << sq;
  }

  [[nodiscard]] constexpr bool IsSet(const Square sq) const {
    return data & static_cast<uint64_t>(1) << sq;
  }

  template <Direction D>
  [[nodiscard]] constexpr Bitboard shift() const;

#if defined(_MSC_VER)
  void mirror() { data = _byteswap_uint64(data); }

  [[nodiscard]] Bitboard mirrored() const { return _byteswap_uint64(data); }

  [[nodiscard]] int popcount() const {
    return static_cast<int>(_mm_popcnt_u64(data));
  }

  [[nodiscard]] Square Lsb() const {
    unsigned long idx;
    _BitScanForward64(&idx, data);
    return static_cast<Square>(idx);
  }

  [[nodiscard]] Square Msb() const {
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

  int popcount() { return __builtin_popcountll(data); }

  Square Lsb() { return Square(__builtin_ctzll(data)); }

  Square Msb() { return Square(__builtin_clzll(data) ^ 63); }
#endif

  Square PopLsb() {
    const Square sq = Lsb();
    data &= data - 1;
    return sq;
  }

  Square PopMsb() {
    const Square sq = Msb();
    data &= data - 1;
    return sq;
  }

  constexpr friend Bitboard operator&(const Bitboard a, const Bitboard b) {
    return a.data & b.data;
  }

  constexpr friend Bitboard operator|(const Bitboard a, const Bitboard b) {
    return a.data | b.data;
  }

  constexpr friend Bitboard operator^(const Bitboard a, const Bitboard b) {
    return a.data ^ b.data;
  }

  constexpr friend Bitboard operator-(const Bitboard a, const Bitboard b) {
    return a.data & ~b.data;
  }

  constexpr friend Bitboard operator*(const Bitboard a, const Bitboard b) {
    return a.data * b.data;
  }

  constexpr void operator&=(const Bitboard b) { data &= b.data; }
  constexpr void operator|=(const Bitboard b) { data |= b.data; }
  constexpr void operator^=(const Bitboard b) { data ^= b.data; }
  constexpr void operator-=(const Bitboard b) { data &= ~b.data; }
  constexpr Bitboard operator~() const { return ~data; }
  constexpr explicit operator bool() const { return static_cast<bool>(data); }

  constexpr friend Bitboard operator<<(const Bitboard b, const uint8_t shift) {
    return b.data << shift;
  }

  constexpr friend Bitboard operator>>(const Bitboard b, const uint8_t shift) {
    return b.data >> shift;
  }

  constexpr bool operator==(const Bitboard b) const { return data == b.data; }

  friend std::ostream& operator<<(std::ostream& os, const Bitboard b) {
    for (Rank r = RANK_8; r >= RANK_1; --r) {
      for (File f = FILE_A; f <= FILE_H; ++f) {
        const Square sq = square::make(f, r);
        os << (b.IsSet(sq) ? 'X' : '.') << ' ';
      }
      os << '\n';
    }
    return os;
  }
};

constexpr Bitboard kFileABb(0x101010101010101);
constexpr Bitboard kFileBBb = kFileABb << 1;
constexpr Bitboard kFileCBb = kFileABb << 2;
constexpr Bitboard kFileDBb = kFileABb << 3;
constexpr Bitboard kFileEBb = kFileABb << 4;
constexpr Bitboard kFileFBb = kFileABb << 5;
constexpr Bitboard kFileGBb = kFileABb << 6;
constexpr Bitboard kFileHBb = kFileABb << 7;

constexpr Bitboard kRank1Bb(0xff);
constexpr Bitboard kRank2Bb = kRank1Bb << 8;
constexpr Bitboard kRank3Bb = kRank1Bb << 16;
constexpr Bitboard kRank4Bb = kRank1Bb << 24;
constexpr Bitboard kRank5Bb = kRank1Bb << 32;
constexpr Bitboard kRank6Bb = kRank1Bb << 40;
constexpr Bitboard kRank7Bb = kRank1Bb << 48;
constexpr Bitboard kRank8Bb = kRank1Bb << 56;

constexpr Bitboard kDiagC2H7(0x0080402010080400);

template <Direction D>
constexpr Bitboard Bitboard::shift() const {
  const Bitboard b(*this);
  if constexpr (D == NORTH)
    return b << NORTH;
  else if constexpr (D == SOUTH)
    return b >> NORTH;
  else if constexpr (D == 2 * NORTH)
    return b << 2 * NORTH;
  else if constexpr (D == 2 * SOUTH)
    return b >> 2 * NORTH;
  else if constexpr (D == NORTHEAST)
    return (b << NORTHEAST) - kFileABb;
  else if constexpr (D == NORTHWEST)
    return (b << NORTHWEST) - kFileHBb;
  else if constexpr (D == SOUTHEAST)
    return (b >> NORTHWEST) - kFileABb;
  else if constexpr (D == SOUTHWEST)
    return (b >> NORTHEAST) - kFileHBb;
  else
    return {};
}

namespace move {
  enum MoveType {
    NORMAL,
    EN_PASSANT = 1 << 14,
    PROMOTION = 2 << 14,
    CASTLING = 3 << 14
  };

  constexpr Move make(const Square from, const Square to) {
    return static_cast<Move>(from | to << 6);
  }

  constexpr Move make(const Square from, const Square to,
                      const MoveType move_type) {
    return static_cast<Move>(from | to << 6 | move_type);
  }

  template <PieceType Pt>
  constexpr Move make(const Square from, const Square to) {
    return from | to << 6 | PROMOTION | (Pt - 2) << 12;
  }

  constexpr Square from(const Move m) { return m & 0x3f; }
  constexpr Square to(const Move m) { return m >> 6 & 0x3f; }

  constexpr MoveType move_type(const Move m) {
    return static_cast<MoveType>(m & 0x3 << 14);
  }

  constexpr PieceType GetPieceType(const Move m) { return (m >> 12 & 0x3) + 2; }

  inline std::string ToString(const Move m) {
    std::string s = square::ToString(from(m)) + square::ToString(to(m));
    if (move_type(m) == PROMOTION) {
      switch (GetPieceType(m)) {
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
} // namespace move

const std::string kStartFen =
  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr Bitboard kWhiteQueenSidePath(0xe);
constexpr Bitboard kWhiteKingSidePath(0x60);
constexpr Bitboard kBlackQueenSidePath(0xE00000000000000);
constexpr Bitboard kBlackKingSidePath(0x6000000000000000);

struct Castling {
  int data;
  void set(const int cr) { data |= cr; }
  void reset(const int cr) { data &= ~cr; }

  [[nodiscard]] bool CanCastle(const int cr) const { return data & cr; }
  [[nodiscard]] bool CannotCastle() const { return data == 0; }
};

struct KingAttackInfo {
  Bitboard pinned{};
  Bitboard attacks{};
  bool double_check = false;
  bool computed = false;
  [[nodiscard]] bool check() const { return static_cast<bool>(attacks); }
};

struct BoardStatus {
  Castling castlings{};
  int fifty_move_count = 0;
  int ply_count = 0;
  int repetitions = 0;
  Key zobrist = 0;
  KingAttackInfo king_attack_info{};
  Move move = 0;
  Piece captured = 0;
  Square ep_square = 0;
};

struct Board {
  [[nodiscard]] Bitboard AttackersTo(Square sq, Bitboard occupied) const;
  [[nodiscard]] Bitboard color(Color c) const;
  [[nodiscard]] Bitboard NonPawnMaterial(Color c) const;
  [[nodiscard]] Bitboard occupied() const;
  [[nodiscard]] Bitboard pieces(Color c, PieceType pt) const;
  [[nodiscard]] Bitboard pieces(PieceType pt) const;
  [[nodiscard]] bool CanCastle(int cr) const;
  [[nodiscard]] bool GivesCheck(Move m) const;
  [[nodiscard]] bool IsCapture(Move m) const;
  [[nodiscard]] bool IsDraw() const;
  [[nodiscard]] bool IsInCheck() const;
  [[nodiscard]] bool IsPseudoLegal(Move m) const;
  [[nodiscard]] bool IsUnderAttack(Color us, Square sq) const;
  [[nodiscard]] bool CannotCastle() const;
  [[nodiscard]] Key key() const;
  [[nodiscard]] Piece PieceOn(Square sq) const;
  [[nodiscard]] Score see(Move m) const;
  [[nodiscard]] Square ksq(Color c) const;
  [[nodiscard]] std::string fen() const;

  Bitboard color_bb[N_COLORS];
  Bitboard LeastValuablePiece(Bitboard attacking, Color attacker,
                              Piece& pc) const;
  Bitboard occupied_bb = 0;
  Bitboard piece_bb[N_PIECE_TYPES]{};

  Board() = default;
  Board(const Board& other);
  Board(const std::string& fen);

  Board& operator=(const Board& other);
  BoardStatus* Bs(int idx);
  BoardStatus* GetBoardStatus();
  BoardStatus* st;

  bool IsLegal(Move m);
  Color side_to_move;
  friend std::ostream& operator<<(std::ostream& os, const Board& board);
  Piece board[N_SQUARES];
  static bool IsPromotion(Move m);
  std::vector<BoardStatus> history;

  template <bool UpdateZobrist = true>
  void MovePiece(Square from, Square to);
  template <bool UpdateZobrist = true>
  void RemovePiece(Square sq);
  template <bool UpdateZobrist = true>
  void SetPiece(Piece pc, Square sq);
  template <PieceType Pt>
  Bitboard AttacksBy(Color c);

  void ApplyMove(Move m);
  void ApplyNullMove();
  void GenKingAttackInfo(KingAttackInfo& k) const;
  void UndoMove();
  void UndoNullMove();
};

inline Piece Board::PieceOn(const Square sq) const { return board[sq]; }
inline BoardStatus* Board::GetBoardStatus() { return &history.back(); }

inline BoardStatus* Board::Bs(const int idx) {
  return &history[history.size() - idx - 1];
}

inline bool Board::CanCastle(const int cr) const {
  return st->castlings.CanCastle(cr);
}

inline bool Board::CannotCastle() const { return st->castlings.CannotCastle(); }
inline Bitboard Board::color(const Color c) const { return color_bb[c]; }
inline Bitboard Board::pieces(const PieceType pt) const { return piece_bb[pt]; }

inline Bitboard Board::pieces(const Color c, const PieceType pt) const {
  return color(c) & pieces(pt);
}

inline Bitboard Board::occupied() const { return occupied_bb; }
inline Square Board::ksq(const Color c) const { return pieces(c, KING).Lsb(); }
inline Key Board::key() const { return st->zobrist; }
inline bool Board::IsDraw() const { return st->repetitions >= 2; }

inline bool Board::IsCapture(const Move m) const {
  return PieceOn(move::to(m)) || move::move_type(m) == move::EN_PASSANT;
}

inline bool Board::IsPromotion(const Move m) {
  return move::move_type(m) == move::PROMOTION;
}

inline Bitboard Board::NonPawnMaterial(const Color c) const {
  return color(c) - pieces(PAWN) - pieces(KING);
}

using std::chrono::milliseconds;

using TimeMs = long;

inline TimeMs curr_time() {
  return static_cast<long>(
    std::chrono::duration_cast<milliseconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count());
}

inline std::uint32_t RandU32(const std::uint32_t low, const std::uint32_t high) {
  std::mt19937 gen(curr_time());
  std::uniform_int_distribution dis(low, high);
  return dis(gen);
}

inline uint64_t RandU64() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;
  return dis(gen);
}

namespace zobrist {
  inline Key psq[16][N_SQUARES];
  inline Key side;
  inline Key castling[16];
  inline Key en_passant[N_FILES];

  inline void init() {
    for (auto& i : psq) {
      for (unsigned long long& j : i) j = RandU64();
    }
    side = RandU64();
    for (unsigned long long& i : castling) i = RandU64();
    for (unsigned long long& i : en_passant) i = RandU64();
  }
} // namespace zobrist
