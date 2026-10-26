// GPL-3.0. Compact FORGE placement inputs from canonical Lc0 bitboards.
// No engine heuristics or chess scores are introduced here.
#include <cstdint>
#include <bit>
#include <algorithm>
#include <cstdlib>

static bool attacked(int s, const int* board, int side) {
  const int x=s%8, y=s/8, pawn=side*6;
  const int py=y+(side==0 ? -1:1);
  if(py>=0 && py<8) for(int dx: {-1,1})
    if(x+dx>=0 && x+dx<8 && board[py*8+x+dx]==pawn) return true;
  const int nx[]={1,2,2,1,-1,-2,-2,-1}, ny[]={2,1,-1,-2,-2,-1,1,2};
  for(int d=0;d<8;d++) {
    int a=x+nx[d],b=y+ny[d];
    if(a>=0&&a<8&&b>=0&&b<8&&board[b*8+a]==side*6+1) return true;
  }
  const int dx[]={1,1,0,-1,-1,-1,0,1},dy[]={0,1,1,1,0,-1,-1,-1};
  for(int d=0;d<8;d++) for(int k=1;k<8;k++) {
    int a=x+k*dx[d],b=y+k*dy[d];
    if(a<0||a>=8||b<0||b>=8) break;
    int p=board[b*8+a]; if(p<0) continue;
    if(p/6==side && (p%6==4 || (p%6==2 && d%2==1) ||
       (p%6==3 && d%2==0) || (p%6==5 && k==1))) return true;
    break;
  }
  return false;
}

// rights order: own queen/king, opponent queen/king; each is a rook-file mask.
// status: 0 accepted, 1 in check, 2 Chess960 rights, 3 clock >=100,
// negative means malformed input and must abort the producer.
extern "C" void pack_positions(int n,const uint64_t* bb,const uint64_t* oldp,
 const uint8_t* rights,const uint8_t* clocks,uint16_t* ids,uint8_t* meta,
 int8_t* boards,int8_t* status) {
  for(int i=0;i<n;i++) {
    const auto* p=bb+12*i; int b[64];std::fill(b,b+64,-1);
    uint64_t occ=0;int count=0,phase=0;bool bad=false;
    const int phase_value[]={0,1,1,2,4,0};
    for(int pc=0;pc<12;pc++) {
      if(occ&p[pc]) bad=true;occ|=p[pc];auto v=p[pc];
      phase+=std::popcount(v)*phase_value[pc%6];
      while(v) {int s=std::countr_zero(v);v&=v-1;b[s]=pc;count++;}
    }
    for(int s=0;s<64;s++) boards[i*64+s]=b[s];
    if(bad||count>32||std::popcount(p[5])!=1||std::popcount(p[11])!=1||
       ((p[0]|p[6])&0xff000000000000ffULL)) {status[i]=-1;continue;}
    int k=std::countr_zero(p[5]),opp=std::countr_zero(p[11]);
    if(attacked(opp,b,0)) {status[i]=-2;continue;}
    if(clocks[i]>=100) {status[i]=3;continue;}
    if(attacked(k,b,1)) {status[i]=1;continue;}
    bool nonstandard=false;
    for(int j=0;j<4;j++) {
      int r=rights[4*i+j],expected=j%2 ? 128:1;
      if(r && (r!=expected || (j<2 ? k!=4:opp!=60))) nonstandard=true;
      if(r && !(p[j<2 ? 3:9] & (uint64_t(r)<<(j<2 ? 0:56)))) bad=true;
    }
    if(bad) {status[i]=-3;continue;}
    if(nonstandard) {status[i]=2;continue;}
    auto* m=meta+8*i;
    m[0]=clocks[i]/2;m[1]=phase;
    m[2]=16*(bool(rights[4*i+1])+2*bool(rights[4*i]));
    m[3]=16*(bool(rights[4*i+3])+2*bool(rights[4*i+2]));
    m[4]=0;m[5]=count;m[6]=0;m[7]=32;
    // Previous opponent double push; retain pseudo-legal EP even when pinned.
    int ep_count=0;
    for(int f=0;f<8;f++) if((oldp[i]&(1ULL<<(48+f))) &&
      !(p[6]&(1ULL<<(48+f))) && !(oldp[i]&(1ULL<<(32+f))) &&
       (p[6]&(1ULL<<(32+f))) && b[40+f]<0 &&
       ((f>0 && b[32+f-1]==0)||(f<7 && b[32+f+1]==0))) {m[4]=41+f;ep_count++;}
    if(ep_count>1) {status[i]=-4;continue;}
    auto* out=ids+64*i;std::fill(out,out+64,uint16_t(24576));
    int kings[]={k,opp^56};
    for(int view=0;view<2;view++) {
      int king=kings[view],bucket=(king/8)*4+std::min(king%8,7-king%8),at=0;
      for(int s=0;s<64;s++) if(b[s]>=0) {
        int pc=view ? (b[s]+6)%12:b[s],sq=view ? s^56:s;
        out[view*32+at++]=bucket*768+pc*64+sq;
      }
      std::sort(out+view*32,out+view*32+count);
    }
    status[i]=0;
  }
}
