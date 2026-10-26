/* FORGE integer evaluator, readable browser port of the delivered source.
 * Remmy Victor. GPL-3.0-or-later. Source and verification receipts travel with it.
 */
(() => {
 'use strict';
 const raw=typeof atob==='function'?Uint8Array.from(atob(ATLAS_DATA.base64),c=>c.charCodeAt(0)):new Uint8Array(Buffer.from(ATLAS_DATA.base64,'base64'));
 const weights=new Int8Array(raw.buffer,raw.byteOffset,raw.byteLength),tensors={};
 for(const m of ATLAS_DATA.manifest)tensors[m.name]=new Int8Array(weights.buffer,weights.byteOffset+m.offset,m.count);
 const clip=(n,a=0,b=127)=>Math.max(a,Math.min(b,n));
 const squareName=s=>'abcdefgh'[s%8]+(1+Math.floor(s/8));
 function parse(fen){const f=fen.split(' '),pieces=[],board=Array(64).fill(null),kings=[-1,-1];
  f[0].split('/').forEach((row,r)=>{let file=0;for(const p of row){if(/\d/.test(p)){file+=+p;continue;}const type='pnbrqk'.indexOf(p.toLowerCase()),color=p===p.toUpperCase()?0:1,sq=(7-r)*8+file++;const item={type,color,sq,char:p};board[sq]=item;pieces.push(item);if(type===5)kings[color]=sq;}});
  const us=f[1]==='w'?0:1;return {fen,pieces,board,kings,us,castling:f[2],ep:f[3]==='-'?-1:'abcdefgh'.indexOf(f[3][0])+(+f[3][1]-1)*8,clock:+f[4]||0};
 }
 const canonical=(s,v)=>s^(v?56:0);
 const plane=(p,s,v)=>(p.type+6*(p.color!==v))*64+canonical(s,v);
 const bucket=(s,v)=>{s=canonical(s,v);return Math.floor(s/8)*4+Math.min(s%8,7-s%8);};
 const logadd=(a,b)=>Math.max(a,b)+Math.log1p(Math.exp(-Math.abs(a-b)));
 const sig=x=>x<0?Math.exp(x)/(1+Math.exp(x)):1/(1+Math.exp(-x));
 function evaluate(record){
  const p=parse(record.fen),buckets=p.kings.map((k,v)=>bucket(k,v)),acc=[new Int32Array(128),new Int32Array(128)],rows=[[],[]];
  for(let v=0;v<2;v++)for(const piece of p.pieces){const row=plane(piece,piece.sq,v),off=(buckets[v]*768+row)*128;rows[v].push({piece,row,offset:off});for(let i=0;i<128;i++)acc[v][i]+=tensors.placement[off+i];}
  const x=new Int32Array(328);for(let k=0;k<128;k++){x[k]=clip(Math.trunc(acc[p.us][k]/2)+32);x[128+k]=clip(Math.trunc(acc[1-p.us][k]/2)+32);}
  const phase=p.pieces.reduce((a,q)=>a+[0,1,1,2,4,0][q.type],0);
  const rights=v=>(p.castling.includes(v?'k':'K')?1:0)+(p.castling.includes(v?'q':'Q')?2:0);
  x.set([Math.trunc(clip(p.clock,0,255)/2),phase,rights(p.us)*16,rights(1-p.us)*16,p.ep<0?0:canonical(p.ep,p.us)+1,p.pieces.length,record.check?1:0,32],320);
  const headIndex=clip(Math.trunc((p.pieces.length-1)/8),0,3),heads=[];
  for(let h=0;h<4;h++){
   const hw=tensors[`h${h}.hidden`],vw=tensors[`h${h}.value`],qw=tensors[`h${h}.query`];
   const sums=new Int32Array(32),hidden=new Int32Array(32),rawValue=new Int32Array(2),query=new Int32Array(16),querySums=new Int32Array(16);
   for(let i=0;i<328;i++)if(x[i])for(let j=0;j<32;j++)sums[j]+=x[i]*hw[i*32+j];
   for(let j=0;j<32;j++){hidden[j]=clip(Math.trunc(sums[j]/64));for(let k=0;k<2;k++)rawValue[k]+=hidden[j]*vw[j*2+k];for(let k=0;k<16;k++)querySums[k]+=hidden[j]*qw[j*16+k];}
   for(let k=0;k<16;k++)query[k]=clip(Math.trunc(querySums[k]/64),-127,127);
   const a=rawValue[0]/256,c=rawValue[1]/256,t=logadd(0,a),logTwo=Math.log(2);
   const floatScore=187*(logadd(t,logTwo+a+c)-logadd(t,logTwo+c));
   const score=clip(Math.sign(floatScore)*Math.floor(Math.abs(floatScore)+.5),-12000,12000),decisive=sig(c),conditional=sig(a),wdl=[decisive*conditional,1-decisive,decisive*(1-conditional)];
   const quiet=record.quiet.map(m=>{const piece=p.board[m.fromSq],from=plane(piece,m.fromSq,p.us),to=plane(piece,m.toSq,p.us);let raw=0;for(let k=0;k<16;k++)raw+=query[k]*(tensors.from[from*16+k]+tensors.to[to*16+k]+tensors.special[m.kind*16+k]);return {...m,fromRow:from,toRow:to,raw};});
   const mean=quiet.length?Math.trunc(quiet.reduce((s,m)=>s+m.raw,0)/quiet.length):0;
   for(const move of quiet){const d=move.raw-mean;move.bonus=Math.trunc(1024*d/(256+Math.abs(d)));}
   heads.push({sums,hidden,rawValue,query,querySums,score,wdl,quiet:quiet.sort((a,b)=>b.raw-a.raw),a,c});
  }
  return {p,buckets,rows,acc,x,headIndex,heads,active:heads[headIndex],record};
 }
 function verify(){let values=0,policy=0;const errors=[];
  for(let i=0;i<ATLAS_DATA.fixtures.length;i++){
   const f=ATLAS_DATA.fixtures[i],out=evaluate(f).active,e=f.expected;
   if(out.score!==e.score)errors.push(`fixture ${i}: score ${out.score} != ${e.score}`);
   if(out.hidden.some((v,k)=>v!==e.hidden[k]))errors.push(`fixture ${i}: hidden mismatch`);
   if(out.rawValue.some((v,k)=>v!==e.raw_value_logits[k]))errors.push(`fixture ${i}: value logits mismatch`);
   const policies=Object.fromEntries(out.quiet.map(m=>[m.native,m.raw]));
   if(Object.keys(policies).length!==Object.keys(e.quiet_policy).length)errors.push(`fixture ${i}: policy move set`);
   for(const [m,v] of Object.entries(e.quiet_policy)){if(policies[m]!==v)errors.push(`fixture ${i}: policy ${m}`);policy++;}values+=35;
  }
  return {positions:ATLAS_DATA.fixtures.length,integerValues:values,policyLogits:policy,errors,passed:!errors.length,source:'Existing J46 native-engine log; no new engine job'};
 }
 globalThis.ForgeModel={weights,tensors,parse,plane,bucket,canonical,squareName,evaluate,verify,clip};
})();
