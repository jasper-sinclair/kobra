#pragma once
#include <bit>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <ostream>
#include "main.h"

struct bitboard{
  u64 data;
  bitboard() = default;
  constexpr bitboard(const u64 data) : data(data){}

  constexpr static bitboard from_sq(const u8 sq){
    return {SCU64(1)<<sq};
  }

  constexpr void set(const u8 sq){
    data|=SCU64(1)<<sq;
  }

  constexpr void clear(const u8 sq){
    data&=~(SCU64(1)<<sq);
  }

  constexpr void toggle(const u8 sq){
    data^=SCU64(1)<<sq;
  }

  [[nodiscard]] constexpr bool is_set(const u8 sq) const{
    return data&SCU64(1)<<sq;
  }

  template<i32 D> [[nodiscard]] constexpr bitboard shift() const;

  constexpr friend bitboard operator&(const bitboard a,const bitboard b){
    return a.data&b.data;
  }

  constexpr friend bitboard operator|(const bitboard a,const bitboard b){
    return a.data|b.data;
  }

  constexpr friend bitboard operator^(const bitboard a,const bitboard b){
    return a.data^b.data;
  }

  constexpr friend bitboard operator-(const bitboard a,const bitboard b){
    return a.data&~b.data;
  }

  constexpr friend bitboard operator*(const bitboard a,const bitboard b){
    return a.data*b.data;
  }

  constexpr void operator&=(const bitboard b){
    data&=b.data;
  }

  constexpr void operator|=(const bitboard b){
    data|=b.data;
  }

  constexpr void operator^=(const bitboard b){
    data^=b.data;
  }

  constexpr void operator-=(const bitboard b){
    data&=~b.data;
  }

  constexpr bitboard operator~() const{
    return ~data;
  }

  constexpr explicit operator bool() const{
    return SCB(data);
  }

  constexpr friend bitboard operator<<(const bitboard b,const u8 shift){
    return b.data<<shift;
  }

  constexpr friend bitboard operator>>(const bitboard b,const u8 shift){
    return b.data>>shift;
  }

  constexpr bool operator==(const bitboard b) const{
    return data==b.data;
  }

  friend std::ostream& operator<<(std::ostream& os,const bitboard b){
    for(i8 r=rank_8;r>=rank_1;--r){
      for(i8 f=file_a;f<=file_h;++f){
        const u8 sq=make(f,r);
        os<<(b.is_set(sq)?'X':'.')<<' ';
      }
      os<<'\n';
    }
    return os;
  }
};

constexpr bitboard filea(0x101010101010101);
constexpr bitboard fileb=filea<<1;
constexpr bitboard filec=filea<<2;
constexpr bitboard filed=filea<<3;
constexpr bitboard filee=filea<<4;
constexpr bitboard filef=filea<<5;
constexpr bitboard fileg=filea<<6;
constexpr bitboard fileh=filea<<7;

constexpr bitboard rank1(0xff);
constexpr bitboard rank2=rank1<<8;
constexpr bitboard rank3=rank1<<16;
constexpr bitboard rank4=rank1<<24;
constexpr bitboard rank5=rank1<<32;
constexpr bitboard rank6=rank1<<40;
constexpr bitboard rank7=rank1<<48;
constexpr bitboard rank8=rank1<<56;

constexpr bitboard white_qs_path(0xe);
constexpr bitboard white_ks_path(0x60);
constexpr bitboard black_qs_path(0xE00000000000000);
constexpr bitboard black_ks_path(0x6000000000000000);
constexpr bitboard diag_c2_h7(0x0080402010080400);

template<i32 D> constexpr bitboard bitboard::shift() const{
  const bitboard b(*this);
  if constexpr(D==north) return b<<north;
  else if constexpr(D==south) return b>>north;
  else if constexpr(D==2*north) return b<<2*north;
  else if constexpr(D==2*south) return b>>2*north;
  else if constexpr(D==northeast) return (b<<northeast)-filea;
  else if constexpr(D==northwest) return (b<<northwest)-fileh;
  else if constexpr(D==southeast) return (b>>northwest)-filea;
  else if constexpr(D==southwest) return (b>>northeast)-fileh;
  else return {};
}

namespace move{
  enum move_type : u16{
    normal=0,en_passant=1<<14,promotion=2<<14,castle=3<<14
  };

  constexpr u16 make(const u8 from,const u8 to){
    return SCU16(from|to<<6);
  }

  constexpr u16 make(const u8 from,const u8 to,
    const move_type mt){
    return SCU16(from|to<<6|mt);
  }

  template<i32 Pt> constexpr u16 make(const u8 from,const u8 to){
    return from|to<<6|promotion|(Pt-2)<<12;
  }

  constexpr u8 from(const u16 m){
    return m&0x3f;
  }

  constexpr u8 to(const u16 m){
    return m>>6&0x3f;
  }

  constexpr move_type mt(const u16 m){
    return SCMT(m&0x3<<14);
  }

  constexpr i32 get_piece_type(const u16 m){
    return (m>>12&0x3)+2;
  }

  inline std::string move_to_string(const u16 m){
    std::string s=sq_to_string(from(m))+sq_to_string(to(m));
    if(mt(m)==promotion){
      switch(get_piece_type(m)){
      case knight: return s+'n';
      case bishop: return s+'b';
      case rook: return s+'r';
      case queen: return s+'q';
      default:;
      }
    }
    return s;
  }
}

struct castle{
  int data;

  void set(const int cr){
    data|=cr;
  }

  void reset(const int cr){
    data&=~cr;
  }

  [[nodiscard]] bool can_castle(const int cr) const{
    return data&cr;
  }

  [[nodiscard]] bool cannot_castle() const{
    return data==0;
  }
};

struct king_attack_info{
  bitboard pinned{};
  bitboard atts{};
  bool double_check=false;
  bool computed=false;

  [[nodiscard]] bool check() const{
    return SCB(atts);
  }
};

struct board_state{
  castle castles{};
  int fifty_move_count=0;
  int ply_count=0;
  int repetitions=0;
  u64 zobrist=0;
  king_attack_info king_attacinfo{};
  u16 move=0;
  i32 captured=0;
  u8 ep_sq=0;
};

struct board{
  ~board() = default;
  bitboard color_bb[n_colors]{};
  bitboard least_valuable_piece(bitboard attacking,bool attacker,i32& pc) const;
  bitboard occupied_bb{};
  bitboard piece_bb[n_piece_types]{};
  board_state* bs(int idx);
  board_state* get_board_status();
  board_state* st=nullptr;

  board() = default;
  board(board&& other) noexcept = default;
  board(const board& other);

  board& operator=(board&& other) noexcept = default;
  board& operator=(const board& other);

  bool is_legal(u16 m);
  bool side_to_move=white;

  explicit board(const std::string& fen);
  friend std::ostream& operator<<(std::ostream& os,const board& pos);
  i32 pos[n_sqs]{};
  static bool is_promotion(u16 m);
  std::vector<board_state> board_status{};

  template<bool UpdateZobrist=true> void move_piece(u8 from,u8 to);
  template<bool UpdateZobrist=true> void remove_piece(u8 sq);
  template<bool UpdateZobrist=true> void set_piece(i32 pc,u8 sq);
  template<i32 Pt> bitboard atts_by(bool c);

  void apply_move(u16 m);
  void apply_null_move();
  void gen_king_attack_info(king_attack_info& k) const;
  void undo_move();
  void undo_null_move();

  [[nodiscard]] bitboard attackers_to(u8 sq,bitboard occupied) const;
  [[nodiscard]] bitboard get_color(bool c) const;
  [[nodiscard]] bitboard get_pieces(bool c,i32 pt) const;
  [[nodiscard]] bitboard get_pieces(i32 pt) const;
  [[nodiscard]] bitboard non_pawn_material(bool c) const;
  [[nodiscard]] bitboard occupied() const;
  [[nodiscard]] bool can_castle(int cr) const;
  [[nodiscard]] bool cannot_castle() const;
  [[nodiscard]] bool gives_check(u16 m) const;
  [[nodiscard]] bool is_capture(u16 m) const;
  [[nodiscard]] bool is_draw() const;
  [[nodiscard]] bool is_in_check() const;
  [[nodiscard]] bool is_pseudo_legal(u16 m) const;
  [[nodiscard]] bool is_under_attack(bool us,u8 sq) const;
  [[nodiscard]] i32 piece_on(u8 sq) const;
  [[nodiscard]] int see(u16 m) const;
  [[nodiscard]] std::string fen() const;
  [[nodiscard]] u64 key() const;
  [[nodiscard]] u8 ksq(bool c) const;
};

inline i32 board::piece_on(const u8 sq) const{
  return pos[sq];
}

inline board_state* board::get_board_status(){
  return &board_status.back();
}

inline board_state* board::bs(const int idx){
  return &board_status[board_status.size()-idx-1];
}

inline bool board::can_castle(const int cr) const{
  return st->castles.can_castle(cr);
}

inline bool board::cannot_castle() const{
  return st->castles.cannot_castle();
}

inline bitboard board::get_color(const bool c) const{
  return color_bb[c];
}

inline bitboard board::get_pieces(const i32 pt) const{
  return piece_bb[pt];
}

inline bitboard board::get_pieces(const bool c,const i32 pt) const{
  return get_color(c)&get_pieces(pt);
}

inline bitboard board::occupied() const{
  return occupied_bb;
}

inline u64 board::key() const{
  return st->zobrist;
}

inline bool board::is_draw() const{
  return st->repetitions>=2;
}

inline bool board::is_capture(const u16 m) const{
  return piece_on(move::to(m))||move::mt(m)==move::en_passant;
}

inline bool board::is_promotion(const u16 m){
  return move::mt(m)==move::promotion;
}

inline bitboard board::non_pawn_material(const bool c) const{
  return get_color(c)-get_pieces(pawn)-get_pieces(king);
}
#if defined(_MSC_VER)
constexpr int popcnt(const bitboard b){
  return std::popcount(b.data);
}

constexpr u8 lsb(const bitboard b){
  return SCU8(std::countr_zero(b.data));
}

inline void mirror(bitboard& b){
  b.data=_byteswap_uint64(b.data);
}

inline bitboard mirrored(const bitboard b){
  return {_byteswap_uint64(b.data)};
}
#else
inline void mirror(bitboard& b) {
  constexpr static u64 k1 = 0x00ff00ff00ff00ff;
  constexpr static u64 k2 = 0x0000ffff0000ffff;
  b.data = b.data >> 8 & k1 | (b.data & k1) << 8;
  b.data = b.data >> 16 & k2 | (b.data & k2) << 16;
  b.data = b.data >> 32 | b.data << 32;
}
inline bitboard mirrored(const bitboard b) {
  bitboard copy(b.data);
  mirror(copy);
  return copy;
}
inline int popcnt(const bitboard b) { return __builtin_popcountll(b.data); }
inline u8 lsb(const bitboard b) { return u8(__builtin_ctzll(b.data)); }
#endif
inline u8 pop_lsb(bitboard& b){
  const u8 sq=lsb(b);
  b.data&=b.data-1;
  return sq;
}

inline u8 board::ksq(const bool c) const{
  return lsb(get_pieces(c,king));
}

extern template bitboard board::atts_by<pawn>(bool c);
extern template bitboard board::atts_by<knight>(bool c);
extern template bitboard board::atts_by<bishop>(bool c);
extern template bitboard board::atts_by<rook>(bool c);
extern template bitboard board::atts_by<queen>(bool c);
