#pragma once
#include <chrono>
#include <cstdint>
#include <random>

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#else
#endif

#define	SC	static_cast
#define	SCI	static_cast<int>
#define	SCB	static_cast<bool>
#define	SCC	static_cast<char>
#define	SCDO	static_cast<double>

#define	SCU8	static_cast<u8>
#define	SCU16	static_cast<u16>
#define	SCI32	static_cast<i32>
#define	SCU64	static_cast<u64>

#define	SCMT	static_cast<move_type>
#define	SCSZ	static_cast<size_t>
#define	SCTI	static_cast<thread_id>
#define	SCTP	static_cast<time_point>

#define SO std::cout
#define SE std::endl
#define NL "\n"
#define FL NL << std::flush

using i8=int8_t;
using i16=int16_t;
using i32=int32_t;
using i64=int64_t;
using u8=uint8_t;
using u16=uint16_t;
using u32=uint32_t;
using u64=uint64_t;

using hist_entry=i32;
using node_type=u8;
using thread_id=u32;
using std::chrono::milliseconds;
using std::log;
using time_point=milliseconds::rep;

inline constexpr int max_depth=256;
inline constexpr int max_ply=256;

enum colors : u8{
  white,black,n_colors
};

enum pieces : u8{
  no_piece=0,white_pawn=1,white_knight=2,white_bishop=3,white_rook=4,white_queen=5,white_king=6,black_pawn=9,
  black_knight=10,black_bishop=11,black_rook=12,black_queen=13,black_king=14,n_pieces=15
};

enum piece_types : u8{
  no_piece_type,pawn,knight,bishop,rook,queen,king,n_piece_types
};

enum squares : u8{
  a1=0,b1=1,c1=2,d1=3,e1=4,f1=5,g1=6,h1=7,
  a2=8,b2=9,c2=10,d2=11,e2=12,f2=13,g2=14,h2=15,
  a3=16,b3=17,c3=18,d3=19,e3=20,f3=21,g3=22,h3=23,
  a4=24,b4=25,c4=26,d4=27,e4=28,f4=29,g4=30,h4=31,
  a5=32,b5=33,c5=34,d5=35,e5=36,f5=37,g5=38,h5=39,
  a6=40,b6=41,c6=42,d6=43,e6=44,f6=45,g6=46,h6=47,
  a7=48,b7=49,c7=50,d7=51,e7=52,f7=53,g7=54,h7=55,
  a8=56,b8=57,c8=58,d8=59,e8=60,f8=61,g8=62,h8=63,
  no_sq=0,n_sqs=64
};

enum files : u8{
  file_a,file_b,file_c,file_d,file_e,file_f,file_g,file_h,
  n_files
};

enum ranks : u8{
  rank_1,rank_2,rank_3,rank_4,rank_5,rank_6,rank_7,rank_8,
  n_ranks
};

enum directions : i8{
  north=8,east=1,south=-north,west=-east,northeast=north+east,northwest=north+west,southeast=south+east,southwest=south+west,
  n_directions=8
};

enum castle_rights : u8{
  no_castle=0,white_qs=1,white_ks=1<<1,black_qs=1<<2,black_ks=1<<3,qs=white_qs|black_qs,ks=white_ks|black_ks,white_castle=white_ks|white_qs,
  black_castle=black_ks|black_qs,any_castle=white_castle|black_castle,
};

enum scores : i16{
  draw_score=0,infinite_score=32001,mate_score=32000,min_mate_score=mate_score-max_depth,stop_score=32002,max_score=30000
};

constexpr bool make(const i32 pc){
  return pc>>3;
}

constexpr i32 pawn_push(const bool c){
  return c?south:north;
}

constexpr i8 fmake(const u8 sq){
  return static_cast<i8>(sq&7);
}

constexpr char file_chars[n_files]={
'a','b','c','d','e','f','g','h'
};

constexpr i8 rmake(const u8 sq){
  return static_cast<i8>(sq>>3);
}

constexpr char rank_chars[n_ranks]={
'1','2','3','4','5','6','7','8'
};

constexpr i32 ptmake(const i32 pc){
  return pc&7;
}

constexpr i32 pmake(const bool c,const i32 pt){
  return (c<<3)+pt;
}

constexpr i32 relative(const i32 pc){
  return pc?pc^8:pc;
}

inline std::string piece_to_char=" PNBRQK  pnbrqk";

constexpr u8 make(const i8 file,const i8 rank){
  return SCU8((rank<<3)+file);
}

constexpr u8 make(const std::string_view sv){
  return make(static_cast<i8>(sv[0]-'a'),static_cast<i8>(sv[1]-'1'));
}

constexpr void mirror(u8& sq){
  sq^=a8;
}

constexpr u8 relative(const bool c,const u8 sq){
  return c?sq^a8:sq;
}

inline std::string sq_to_string(const u8 sq){
  return std::string(1,file_chars[fmake(sq)])+
    std::string(1,rank_chars[rmake(sq)]);
}

constexpr bool is_valid(const u8 sq){
  return sq>=a1&&sq<=h8;
}

inline u8 distance(const u8 s1,const u8 s2){
  const u8 dist=SCU8(std::max(std::abs(fmake(s1)-fmake(s2)),
    std::abs(rmake(s1)-rmake(s2))));
  return dist;
}

inline i32 curr_time(){
  return SCI(
    std::chrono::duration_cast<milliseconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count());
}

inline u32 rand_u32(const u32 low,const u32 high){
  std::mt19937 gen(curr_time());
  std::uniform_int_distribution dis(low,high);
  return dis(gen);
}

inline u64 rand_u64(){
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<u64> dis;
  return dis(gen);
}

namespace zobrist{
  inline u64 psq[16][n_sqs];
  inline u64 side;
  inline u64 castle[16];
  inline u64 en_passant[n_files];

  inline void init(){
    for(auto& i:psq){
      for(u64& j:i) j=rand_u64();
    }
    side=rand_u64();
    for(u64& i:castle) i=rand_u64();
    for(u64& i:en_passant) i=rand_u64();
  }
}
