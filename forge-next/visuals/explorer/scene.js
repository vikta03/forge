/* The spatial design is illustrative; every displayed activation and weight
 * is supplied by the verified integer evaluator. Remmy Victor. */
class AtlasScene {
 constructor(onPick){
  this.onPick=onPick;this.canvas=document.getElementById('world');this.layer=document.getElementById('labels');this.reduce=matchMedia('(prefers-reduced-motion: reduce)');
  this.labels=[];this.picks=[];this.viewCameras={};this.selection=null;this.edgeMode='top';this.focusKey=null;this.keys=new Set();this.pointers=new Map();this.frame=0;this.draws=0;this.target=new THREE.Vector3(0,3,0);this.chapter=0;this.mode='architecture';this.flight=false;this.unfolded=false;this.trace=null;this.last=0;this.connections=null;this._init();this._controls();
 }
 v(x=0,y=0,z=0){return new THREE.Vector3(x,y,z);}
 material(color,emissive=0){return new THREE.MeshStandardMaterial({color,roughness:.42,metalness:.28,emissive,emissiveIntensity:.3});}
 mesh(geo,mat,x,y,z,parent=this.root,pick=null){const m=new THREE.Mesh(geo,mat);m.position.set(x,y,z);parent.add(m);if(pick){m.userData.pick=pick;this.picks.push(m);}return m;}
 box(w,h,d,mat,x,y,z,parent=this.root,pick=null){return this.mesh(new THREE.BoxGeometry(w,h,d),mat,x,y,z,parent,pick);}
 line(points,color=0x63968e,opacity=.5,parent=this.root){const geo=new THREE.BufferGeometry().setFromPoints(points);const o=new THREE.Line(geo,new THREE.LineBasicMaterial({color,transparent:true,opacity,depthWrite:false}));parent.add(o);return o;}
 tube(points,color,radius=.035,parent=this.root){const curve=new THREE.CatmullRomCurve3(points);return this.mesh(new THREE.TubeGeometry(curve,48,radius,6,false),new THREE.MeshBasicMaterial({color,transparent:true,opacity:.5}),0,0,0,parent);}
 label(text,pos,pick,priority=1,parent='architecture'){const e=document.createElement('button');e.className='label';e.innerHTML=text;e.onclick=()=>this.onPick(pick);e.ondblclick=()=>this.onPick({...pick,focus:true});this.layer.append(e);const l={e,pos:this.v(...pos),pick,priority,parent};this.labels.push(l);return l;}
 text(text,pos,width=5,color='#84a9ac',parent=this.root){const c=document.createElement('canvas');c.width=768;c.height=96;const ctx=c.getContext('2d');ctx.font='500 36px system-ui';ctx.fillStyle=color;ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText(text,384,48);const t=new THREE.CanvasTexture(c);t.colorSpace=THREE.SRGBColorSpace;const p=this.mesh(new THREE.PlaneGeometry(width,width/8),new THREE.MeshBasicMaterial({map:t,transparent:true,depthWrite:false,side:THREE.DoubleSide}),...pos,parent);return p;}
 halo(color,size,x,y,z,parent=this.root){if(!this.haloTexture){const c=document.createElement('canvas');c.width=c.height=64;const t=c.getContext('2d'),g=t.createRadialGradient(32,32,0,32,32,32);g.addColorStop(0,'rgba(255,255,255,.8)');g.addColorStop(.18,'rgba(255,255,255,.30)');g.addColorStop(1,'rgba(255,255,255,0)');t.fillStyle=g;t.fillRect(0,0,64,64);this.haloTexture=new THREE.CanvasTexture(c);}const m=new THREE.SpriteMaterial({map:this.haloTexture,color,transparent:true,blending:THREE.AdditiveBlending,depthWrite:false,opacity:.35});const s=new THREE.Sprite(m);s.scale.set(size,size,1);s.position.set(x,y,z);parent.add(s);return s;}
 color(n,scale=16){const p=Math.pow(Math.min(1,Math.abs(n)/scale),.72),base=[.045,.095,.13],end=n<0?[.25,.72,.89]:[1,.63,.31];return base.map((v,i)=>v+(end[i]-v)*p);}
 _init(){
  this.scene=new THREE.Scene();this.scene.background=new THREE.Color(0x080e17);this.scene.fog=new THREE.FogExp2(0x080e17,.0045);
  this.camera=new THREE.PerspectiveCamera(42,1,.1,550);
  this.renderer=new THREE.WebGLRenderer({canvas:this.canvas,antialias:true,powerPreference:'low-power'});this.renderer.setPixelRatio(Math.min(devicePixelRatio,1.6));this.renderer.outputColorSpace=THREE.SRGBColorSpace;this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1.15;
  this.scene.add(new THREE.HemisphereLight(0xbad8ec,0x111a25,1.75));const warm=new THREE.DirectionalLight(0xffd4a0,2.4);warm.position.set(-12,25,35);this.scene.add(warm);const cool=new THREE.DirectionalLight(0x79c9d8,1.2);cool.position.set(15,12,-18);this.scene.add(cool);
  this.root=new THREE.Group();this.scene.add(this.root);this.universe=new THREE.Group();this.scene.add(this.universe);this.universe.visible=false;
  this.base=this.material(0x192733);this.rim=this.material(0x42616a);this.copper=this.material(0xc68f58,0x6e3514);this.white=this.material(0xece2ca);this.black=this.material(0x203843);this.teal=this.material(0x55baab,0x16433d);
  this.environment=new THREE.Group();this.scene.add(this.environment);const floor=new THREE.Mesh(new THREE.PlaneGeometry(380,220),new THREE.MeshBasicMaterial({color:0x09111b}));floor.rotation.x=-Math.PI/2;floor.position.y=-1.9;this.environment.add(floor);
  const grid=new THREE.GridHelper(240,60,0x1e3542,0x182936);grid.position.y=-1.87;grid.material.transparent=true;grid.material.opacity=.28;this.environment.add(grid);
  for(const r of [15,28,43,61]){const ring=this.mesh(new THREE.RingGeometry(r,r+.025,128),new THREE.MeshBasicMaterial({color:0x244451,side:THREE.DoubleSide,transparent:true,opacity:.25}),0,-1.8,0,this.environment);ring.rotation.x=-Math.PI/2;}
  this._board();this._vault();this._accumulators();this._inputs();this._heads();this._outputs();this._roads();
  this.orb=this.mesh(new THREE.SphereGeometry(.3,16,12),new THREE.MeshBasicMaterial({color:0xffdca4}),0,0,0);this.orb.visible=false;this.orbHalo=this.halo(0xffbc73,2,0,0,0);this.orbHalo.visible=false;
  this._makeUniverse();new ResizeObserver(()=>this.resize()).observe(this.canvas.parentElement);this.resize();this.go(0,false);
 }
 _board(){
  this.box(8.1,.5,8.1,this.base,-40,-.7,0,this.root,{type:'chapter',id:1});this.box(7.7,.1,7.7,this.copper,-40,-.4,0);
  const light=this.material(0xc3c4ae),dark=this.material(0x365555);
  for(let r=0;r<8;r++)for(let f=0;f<8;f++)this.box(.9,.13,.9,(r+f)%2?dark:light,-40+(f-3.5)*.9,-.26,(r-3.5)*.9,this.root,{type:'square',sq:(7-r)*8+f});
  this.pieceRoot=new THREE.Group();this.root.add(this.pieceRoot);
  const route=this.mesh(new THREE.RingGeometry(5.3,5.35,96),new THREE.MeshBasicMaterial({color:0xb68c59,side:THREE.DoubleSide,transparent:true,opacity:.55}),-40,-.9,0);route.rotation.x=-Math.PI/2;
  this.text('THE POSITION',[-40,-.8,5.2],5);this.label('<em>01</em> A POSITION',[-40,3.2,0],{type:'chapter',id:1},8);
 }
 _piece(piece){const f=piece.sq%8,r=7-Math.floor(piece.sq/8),g=new THREE.Group();g.position.set(-40+(f-3.5)*.9,-.14,(r-3.5)*.9);this.pieceRoot.add(g);const m=piece.color?this.black:this.white,pick={type:'piece',sq:piece.sq};
  const type=piece.type;const profile=[[.26,0],[.29,.08],[.26,.16],[.20,.2],[.16,.27],[.12,.44],[.11,.6],[.18,.65],[.18,.72]].map(([x,y])=>new THREE.Vector2(x,y*(type===0?.65:1)));
  this.mesh(new THREE.LatheGeometry(profile,16),m,0,0,0,g,pick);
  if(type===0)this.mesh(new THREE.SphereGeometry(.18,16,12),m,0,.6,0,g,pick);
  if(type===1){const neck=this.box(.24,.4,.27,m,0,.76,0,g,pick);neck.rotation.x=piece.color?-.25:.25;this.box(.25,.18,.38,m,0,.96,piece.color?.10:-.1,g,pick);this.box(.07,.17,.1,m,.07,1.1,0,g,pick);}
  if(type===2){const b=this.mesh(new THREE.SphereGeometry(.19,16,10),m,0,.91,0,g,pick);b.scale.y=1.45;this.mesh(new THREE.SphereGeometry(.055,10,8),this.copper,0,1.21,0,g,pick);}
  if(type===3){this.mesh(new THREE.CylinderGeometry(.24,.18,.25,16),m,0,.81,0,g,pick);for(let i=0;i<4;i++)this.box(.14,.15,.14,m,Math.cos(i*Math.PI/2)*.17,1.0,Math.sin(i*Math.PI/2)*.17,g,pick);}
  if(type===4){this.mesh(new THREE.CylinderGeometry(.25,.13,.22,16),m,0,.83,0,g,pick);for(let i=0;i<6;i++)this.mesh(new THREE.SphereGeometry(.06,10,6),this.copper,Math.cos(i*Math.PI/3)*.22,.99,Math.sin(i*Math.PI/3)*.22,g,pick);this.mesh(new THREE.SphereGeometry(.1,12,8),m,0,1.05,0,g,pick);}
  if(type===5){this.mesh(new THREE.CylinderGeometry(.15,.13,.2,16),m,0,.84,0,g,pick);this.box(.075,.38,.075,this.copper,0,1.13,0,g,pick);this.box(.26,.075,.075,this.copper,0,1.2,0,g,pick);}
 }
 _vault(){
  this.banks=[];this.bankMaterials=[];this.box(12.4,.3,12.8,this.base,-25,-.8,0,this.root,{type:'chapter',id:2});
  for(let b=0;b<32;b++){
   const x=-25+(b%8-3.5)*1.45,z=(Math.floor(b/8)-1.5)*3.1,g=new THREE.Group();g.position.set(x,0,z);this.root.add(g);
   const bytes=new Uint8Array(128*768*4);const data=ForgeModel.tensors.placement;for(let i=0;i<128*768;i++){const c=this.color(data[b*128*768+i]);for(let k=0;k<3;k++)bytes[i*4+k]=Math.round(c[k]*255);bytes[i*4+3]=255;}
   const texture=new THREE.DataTexture(bytes,128,768,THREE.RGBAFormat);texture.colorSpace=THREE.SRGBColorSpace;texture.magFilter=THREE.NearestFilter;texture.minFilter=THREE.LinearFilter;texture.needsUpdate=true;
   const face=new THREE.MeshBasicMaterial({map:texture,side:THREE.DoubleSide});this.box(1.15,7.3,.18,this.base,0,3.1,0,g,{type:'bank',index:b});this.mesh(new THREE.PlaneGeometry(1.04,7.1),face,0,3.1,.1,g,{type:'bank',index:b});
   const edge=new THREE.MeshBasicMaterial({color:0x395562});this.box(.025,7.4,.23,edge,-.59,3.1,0,g);this.box(.025,7.4,.23,edge,.59,3.1,0,g);this.box(1.18,.04,.23,edge,0,6.8,0,g);
   this.banks.push(g);this.bankMaterials.push(edge);
  }
  this.label('<em>02</em> SHARED MEMORY · 3,145,728',[-25,9.3,0],{type:'chapter',id:2},9);
  this.text('32 KING BUCKETS  /  768 × 128',[-25,-.25,8],10);this.halo(0x2b7f94,21,-25,3,-2);
 }
 _instanced(count,geometry,material,positions,parent=this.root,type='input'){
  const o=new THREE.InstancedMesh(geometry,material,count),d=new THREE.Object3D();positions.forEach((p,i)=>{d.position.copy(p);d.updateMatrix();o.setMatrixAt(i,d.matrix);});o.userData.pick={type};o.userData.positions=positions;o.instanceMatrix.setUsage(THREE.DynamicDrawUsage);parent.add(o);this.picks.push(o);return o;
 }
 _accumulators(){
  this.accPositions=[[],[]];this.accMeshes=[];
  for(let v=0;v<2;v++){
   const z=(v?1:-1)*3.3;
   for(let k=0;k<128;k++){const row=Math.floor(k/8),col=k%8;this.accPositions[v].push(this.v(-9+(col-3.5)*.34,1+row*.32,z));}
   const m=this._instanced(128,new THREE.SphereGeometry(.105,10,8),this.material(0x79c4bf,0x275e60),this.accPositions[v],this.root,'acc'+v);this.accMeshes.push(m);
   this.box(4,.18,2,this.base,-9,-.4,z);this.text(v?'OPPONENT VIEW':'SIDE-TO-MOVE VIEW',[-9,.0,z+1.25],4);
  }
  this.label('<em>03</em> 128 + 128 CHANNELS',[-9,8.6,0],{type:'chapter',id:3},8);this.halo(0x369eae,9,-9,3,0);
 }
 _inputs(){
  this.inputPositions=[];for(let i=0;i<328;i++){const row=i%41,col=Math.floor(i/41);this.inputPositions.push(this.v(.3,(row-20)*.17+3.8,(col-3.5)*.32));}
  this.inputMesh=this._instanced(328,new THREE.SphereGeometry(.065,8,6),this.material(0x87b7af,0x244a44),this.inputPositions,this.root,'input');
  this.box(2,.2,4.2,this.base,.3,-.3,0);this.text('328 PACKED INPUTS',[.3,.4,2.6],5);this.router=this.mesh(new THREE.OctahedronGeometry(.7,0),this.copper,4,3.4,0,this.root,{type:'chapter',id:4});this.halo(0xeb9b50,4,4,3.4,0);
  this.label('328 INPUTS / HARD ROUTE',[1,8.4,0],{type:'chapter',id:4},5);
 }
 _heads(){
  this.heads=[];this.headHidden=[];this.hiddenPositions=[];this.queryPositions=[];this.valuePositions=[];
  const palette=[0x78b4db,0x65c6ac,0xb1a2d9,0xe3aa72];
  for(let h=0;h<4;h++){
   const z=(h-1.5)*9.2,g=new THREE.Group();this.root.add(g);const positions=[];
   for(let j=0;j<32;j++){const a=(j%8)/8*Math.PI*2,r=1.35+(Math.floor(j/8)*.04);positions.push(this.v(13+Math.floor(j/8)*.48,3.5+Math.sin(a)*r,z+Math.cos(a)*r));}
   const sm=this.material(0xffffff),hidden=this._instanced(32,new THREE.SphereGeometry(.20,12,8),sm,positions,g,'hidden'+h);this.headHidden.push(hidden);this.hiddenPositions.push(positions);
   const qp=[];for(let k=0;k<16;k++){const a=k*Math.PI/8;qp.push(this.v(21,1.75+Math.sin(a)*.85,z+Math.cos(a)*.85));}this.queryPositions.push(qp);
   const query=this._instanced(16,new THREE.SphereGeometry(.13,10,8),sm,qp,g,'query'+h);
   const vp=[this.v(21,5.0,z-.4),this.v(21,5.0,z+.4)];this.valuePositions.push(vp);const value=this._instanced(2,new THREE.SphereGeometry(.25,16,12),sm,vp,g,'value'+h);
   this.box(12,.18,5.5,this.base,17,-.4,z,g,{type:'head',index:h});const edge=new THREE.MeshBasicMaterial({color:palette[h],transparent:true,opacity:.55});this.box(12,.035,.05,edge,17,-.28,z+2.76,g);
   this.text(`H${h}  /  ${['2–8','9–16','17–24','25–32'][h]} PIECES`,[17,-.15,z+2.1],6.7,'#80a0aa',g);
   const internal=this._headEdges(h,g);const input=this.tube([this.v(4,3.4,0),this.v(7,3.4,z*.7),positions[0]],palette[h],.03);
   const vl=this.tube([vp[0],this.v(27,5,z),this.v(31,5,-7),this.v(35,5,-7)],palette[h],.025);
   const ql=this.tube([qp[0],this.v(27,1.8,z),this.v(31,1.8,8),this.v(35,1.8,8)],palette[h],.025);
   const l=this.label(`H${h} · ${['2–8','9–16','17–24','25–32'][h]}`,[17,7.3,z],{type:'head',index:h},9-h);
   this.heads.push({g,sm,query,value,edge,input,vl,ql,label:l,internal,z});
  }
 }
 _headEdges(h,g){const pos=[],color=[];const wv=ForgeModel.tensors[`h${h}.value`],wq=ForgeModel.tensors[`h${h}.query`];
  for(let j=0;j<32;j++)for(let o=0;o<18;o++){const value=o<2?wv[j*2+o]:wq[j*16+o-2];const end=o<2?this.valuePositions[h][o]:this.queryPositions[h][o-2],start=this.hiddenPositions[h][j];pos.push(...start,...end);const c=this.color(value,32);color.push(...c,...c);}
  const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.Float32BufferAttribute(pos,3));geometry.setAttribute('color',new THREE.Float32BufferAttribute(color,3));const lines=new THREE.LineSegments(geometry,new THREE.LineBasicMaterial({vertexColors:true,transparent:true,opacity:.17,depthWrite:false}));g.add(lines);return lines;
 }
 _outputs(){
  this.probMeshes=[];this.box(11,.3,7,this.base,37,-.5,-7,this.root,{type:'chapter',id:5});
  for(let i=0;i<3;i++){
   const x=34.3+i*2.5,colour=[0x65d1b6,0x8ba0b2,0xe5a06f][i];
   this.mesh(new THREE.CylinderGeometry(.68,.68,5.4,24,1,true),new THREE.MeshBasicMaterial({color:colour,transparent:true,opacity:.07,side:THREE.DoubleSide,depthWrite:false}),x,2.7,-7);
   const ring=this.mesh(new THREE.TorusGeometry(.69,.022,6,32),new THREE.MeshBasicMaterial({color:colour}),x,0,-7);ring.rotation.x=Math.PI/2;
   const fill=this.mesh(new THREE.CylinderGeometry(.55,.55,1,24),this.material(colour,colour),x,.5,-7,this.root,{type:'chapter',id:5});this.probMeshes.push(fill);this.halo(colour,4,x,2,-7);this.text(['WIN','DRAW','LOSS'][i],[x,-.1,-5.5],2.3);
  }
  this.scalarText=this.text('EVALUATION',[37,6.5,-7],9,'#d6bc94');this.label('<em>05</em> W / D / L → VALUE',[37,8,-7],{type:'chapter',id:5},8);
  this.box(11,.3,7,this.base,37,-.5,8,this.root,{type:'chapter',id:6});
  for(let i=0;i<3;i++){this.box(.24,3.8,2.2,[this.teal,this.copper,this.rim][i],33.5+i*1.3,2,8,this.root,{type:'chapter',id:6});for(let r=0;r<6;r++)this.box(.26,.02,2.25,this.rim,33.5+i*1.3,.5+r*.58,8);}
  this.policyBars=[];for(let i=0;i<5;i++){const bar=this.box(3,.14,.24,this.teal,40,1+i*.68,8,this.root,{type:'chapter',id:6});this.policyBars.push(bar);}
  this.text('FROM  /  TO  /  SPECIAL',[35,.1,10.4],5.4);this.text('QUIET ORDER',[40,.1,10.4],3.5);this.label('<em>06</em> A POLICY HINT',[37,6.5,8],{type:'chapter',id:6},7);
  this.halo(0x3b938e,10,37,2,8);
 }
 _roads(){
  this.tube([this.v(-36,1,0),this.v(-33,2,0),this.v(-30,2.5,0)],0xc09261,.05);
  for(let v=0;v<2;v++){const z=v?3.3:-3.3;this.tube([this.v(-18.7,3,0),this.v(-15,3,z),this.v(-10.6,3,z)],v?0xa78ad1:0x58cbb5,.045);this.tube([this.v(-7.3,3,z),this.v(-4,3.3,z),this.v(0,3.8,0)],v?0xa78ad1:0x58cbb5,.04);}
  this.tube([this.v(.5,3.8,0),this.v(2.5,3.8,0),this.v(4,3.4,0)],0xc49a64,.055);
 }
 _makeUniverse(){this.universeBox=new THREE.Box3(this.v(-19,-10,-10),this.v(19,10,10));const helper=new THREE.Box3Helper(this.universeBox,0x345964);this.universe.add(helper);this.universePoints=null;
  this.text('64 CANONICAL SQUARES',[0,-11,11],14,'#86a6b4',this.universe);this.text('128 EMBEDDING CHANNELS',[-22,11,0],14,'#86a6b4',this.universe);this.text('12 RELATIVE PIECE IDENTITIES',[0,-11,-12],17,'#86a6b4',this.universe);
  this.universeLabel=this.label('ALL 98,304 WEIGHTS IN THIS BUCKET',[0,13,0],{type:'universe-info'},2,'universe');
 }
 setUniverse(bucket=0,layer='all',scale=16){this.bank=bucket;this.pieceLayer=layer;this.weightScale=scale;
  if(this.universePoints){this.universe.remove(this.universePoints);this.universePoints.geometry.dispose();this.universePoints.material.dispose();}
  const count=layer==='all'?98304:8192,p=new Float32Array(count*3),c=new Float32Array(count*3),indices=new Uint32Array(count);let n=0;
  this.universeLabel.e.textContent=layer==='all'?'ALL 98,304 WEIGHTS IN THIS BUCKET':'ALL 8,192 WEIGHTS IN THIS PIECE LAYER';
  for(let type=0;type<12;type++){if(layer!=='all'&&type!==Number(layer))continue;for(let sq=0;sq<64;sq++)for(let channel=0;channel<128;channel++){
   const index=(type*64+sq)*128+channel,value=ForgeModel.tensors.placement[bucket*98304+index];p.set([(sq-31.5)*.57,(channel-63.5)*.15,(type-5.5)*1.72],n*3);c.set(this.color(value,scale),n*3);indices[n++]=index;
  }}
  const geo=new THREE.BufferGeometry();geo.setAttribute('position',new THREE.BufferAttribute(p,3));geo.setAttribute('color',new THREE.BufferAttribute(c,3));this.universePoints=new THREE.Points(geo,new THREE.PointsMaterial({size:.115,vertexColors:true,transparent:true,opacity:.86,depthWrite:false,sizeAttenuation:true}));this.universePoints.userData.indices=indices;this.universe.add(this.universePoints);this.request();
 }
 setData(r){this.result=r;
  const old=[];this.pieceRoot.traverse(o=>{if(o.isMesh)old.push(o);});old.forEach(o=>{o.geometry.dispose();const i=this.picks.indexOf(o);if(i>=0)this.picks.splice(i,1);});this.pieceRoot.clear();r.p.pieces.forEach(p=>this._piece(p));
  this.bankMaterials.forEach((m,i)=>m.color.setHex(r.buckets[r.p.us]===i?0xf4ba76:r.buckets[1-r.p.us]===i?0x7e9edf:0x395562));
  const col=new THREE.Color();for(let v=0;v<2;v++){const mesh=this.accMeshes[v];for(let k=0;k<128;k++){const n=r.x[v*128+k]/127;col.setRGB(.10+n*.25,.22+n*.62,.28+n*.5);mesh.setColorAt(k,col);}mesh.instanceColor.needsUpdate=true;}
  for(let i=0;i<328;i++){const n=r.x[i]/127;col.setRGB(.10+n*.40,.18+n*.6,.23+n*.35);if(i>=256&&i<320)col.setRGB(.12,.15,.18);if(i>=320)col.setRGB(.35+n*.5,.22+n*.4,.10);this.inputMesh.setColorAt(i,col);}this.inputMesh.instanceColor.needsUpdate=true;
  for(let h=0;h<4;h++){const on=h===r.headIndex,o=r.heads[h],visual=this.heads[h],mesh=this.headHidden[h];for(let j=0;j<32;j++){const n=Math.pow(o.hidden[j]/127,.55);col.setRGB(on?.035+n*.95:.035+n*.24,on?.045+n*.62:.05+n*.38,on?.055+n*.26:.065+n*.46);mesh.setColorAt(j,col);}mesh.instanceColor.needsUpdate=true;for(let k=0;k<16;k++){col.setRGB(...this.color(o.query[k],127));visual.query.setColorAt(k,col);}visual.query.instanceColor.needsUpdate=true;for(let k=0;k<2;k++){col.setRGB(...this.color(o.rawValue[k]/256,10));visual.value.setColorAt(k,col);}visual.value.instanceColor.needsUpdate=true;
   visual.input.material.opacity=on?.9:.13;visual.vl.material.opacity=on?.7:.07;visual.ql.material.opacity=on?.7:.07;visual.internal.material.opacity=on?.24:.035;visual.sm.emissiveIntensity=on?.55:.05;visual.label.e.classList.toggle('active',on);visual.label.e.innerHTML=`<em>H${h}</em> ${['2–8','9–16','17–24','25–32'][h]}${on?' · RUNNING':''}`;
  }
  r.active.wdl.forEach((n,i)=>{const o=this.probMeshes[i];o.scale.y=n*5.3;o.position.y=o.scale.y/2;});const top=r.active.quiet.slice(0,5),lo=top.length?top.at(-1).raw-100:0,hi=top.length?top[0].raw+100:1;this.policyBars.forEach((o,i)=>{o.visible=!!top[i];if(top[i])o.userData.pick={type:'move',head:r.headIndex,uci:top[i].uci};o.scale.x=top[i]?.raw!==undefined?.2+.8*(top[i].raw-lo)/(hi-lo):.05;});
  if(this.selection)this.select(this.selection);
  this.request();
 }
 // Selection is an exact entity, shared with the application and tensor ledger.
 pointFor(s) {
  if(!s) return null;
  if(this.mode==='universe'&&s.type!=='weight')return null;
  if(s.type==='weight'&&(s.bucket!==this.bank||(this.pieceLayer!=='all'&&Math.floor(s.row/64)!==+this.pieceLayer)))return null;
  const h=s.head??this.inspectedHead??this.result?.headIndex??3, shift=this.heads[h].g.position.z;
  if(s.type==='piece')return s.sq==null?null:this.v(-40+(s.sq%8-3.5)*.9,.8,(7-Math.floor(s.sq/8)-3.5)*.9);
  if(s.type==='weight')return this.mode==='universe'?this.v((s.row%64-31.5)*.57,(s.col-63.5)*.15,(Math.floor(s.row/64)-5.5)*1.72):null;
  if(s.type==='channel')return this.accPositions[s.view===this.result.p.us?0:1][s.index].clone();
  if(s.type==='input')return this.inputPositions[s.index].clone();
  if(s.type==='neuron')return this.hiddenPositions[h][s.index].clone().add(this.v(0,0,shift));
  if(s.type==='query'||s.type==='value')return (s.type==='query'?this.queryPositions:this.valuePositions)[h][s.index].clone().add(this.v(0,0,shift));
  if(s.type==='head')return this.v(16,3.5,this.heads[h].z+shift);
  if(s.type==='bank')return this.banks[s.index].position.clone().add(this.v(0,3,0));
  if(s.type==='move')return this.v(-40,1,0);
  return null;
 }
 clearConnections(){if(this.connections){this.root.remove(this.connections);this.connections.geometry.dispose();this.connections.material.dispose();this.connections=null;}}
 selectNeuron(h,j){this.select({type:'neuron',head:h,index:j});}
 selectionKey(s){if(!s)return null;return s.type==='piece'?'piece:'+(s.id??s.sq):s.type+':'+[s.head,s.view,s.index,s.bucket,s.row,s.col,s.uci].join(':');}
 select(s) {
  this.selection=s?{...s}:null;this.clearConnections();
  if(this.neuronRing)this.neuronRing.visible=false;
  if(!this.selectionRing){this.selectionRing=this.mesh(new THREE.TorusGeometry(.31,.035,8,32),new THREE.MeshBasicMaterial({color:0xffe4ae,depthTest:false,depthWrite:false}),0,0,0,this.scene);this.selectionRing.userData.overlay=true;this.selectionRing.renderOrder=10;}
  const pos=this.pointFor(s);this.selectionRing.visible=!!pos;
  if(pos&&this.focusKey===this.selectionKey(s)&&this.focusPoint){const shift=pos.clone().sub(this.focusPoint);this.camera.position.add(shift);this.target.add(shift);if(this.transition)for(const key of ['from','to','tf','tt'])this.transition[key].add(shift);this.focusPoint.copy(pos);this._syncOrbit();}
  if(pos){this.selectionRing.position.copy(pos);this.selectionRing.scale.setScalar(s.type==='weight'?.6:s.type==='head'?2:1);}
  if(this.moveArrow){this.root.remove(this.moveArrow);this.moveArrow.geometry.dispose();this.moveArrow.material.dispose();this.moveArrow=null;}
  this.heads.forEach((o,h)=>{o.internal.material.opacity=s?.head===h?.07:.025;});
  if(!s||(this.mode==='universe'&&s.type!=='weight')){this.request();return;}
  if(s.head!==undefined)this.inspectedHead=s.head;
  const edges=[];
  if(s.type==='neuron') {
   this.neuron=s.index;const terms=AtlasInspect.hiddenTerms(this.result,s.head,s.index).sort((a,b)=>Math.abs(b.product)-Math.abs(a.product));
   for(const t of (this.edgeMode==='all'?terms:terms.slice(0,12))) edges.push([this.inputPositions[t.i],pos,t.product]);
  } else if(s.type==='value'||s.type==='query') {
   const terms=AtlasInspect.outputTerms(this.result,s.head,s.type,s.index).sort((a,b)=>Math.abs(b.product)-Math.abs(a.product));
   for(const t of (this.edgeMode==='all'?terms:terms.slice(0,12)))edges.push([this.pointFor({type:'neuron',head:s.head,index:t.i}),pos,t.product]);
  } else if(s.type==='channel') {
   const terms=AtlasInspect.channelTerms(this.result,s.view,s.index).sort((a,b)=>Math.abs(b.value)-Math.abs(a.value));
   for(const t of (this.edgeMode==='all'?terms:terms.slice(0,12)))edges.push([this.pointFor({type:'piece',sq:t.piece.sq}),pos,t.value]);
   edges.push([pos,this.inputPositions[AtlasInspect.inputIndex(s.view,s.index,this.result.p.us)],1]);
  } else if(s.type==='input') {
   if(s.index<256)edges.push([this.pointFor({type:'channel',view:s.index<128?this.result.p.us:1-this.result.p.us,index:s.index%128}),pos,1]);
   const h=this.inspectedHead??this.result.headIndex,w=ForgeModel.tensors[`h${h}.hidden`],terms=Array.from({length:32},(_,j)=>({j,v:this.result.x[s.index]*w[s.index*32+j]})).sort((a,b)=>Math.abs(b.v)-Math.abs(a.v));
   for(const t of (this.edgeMode==='all'?terms:terms.slice(0,12)))edges.push([pos,this.pointFor({type:'neuron',head:h,index:t.j}),t.v]);
  } else if(s.type==='move') {
   const move=this.result.heads[s.head].quiet.find(m=>m.uci===s.uci);
   if(move){const a=this.pointFor({type:'piece',sq:move.fromSq}),b=this.pointFor({type:'piece',sq:move.displayTo??move.toSq}),d=b.clone().sub(a).normalize(),side=this.v(-d.z,0,d.x);
    const path=[a,b,b.clone().addScaledVector(d,-.45).addScaledVector(side,.25),b,b.clone().addScaledVector(d,-.45).addScaledVector(side,-.25)];
    this.moveArrow=this.line(path,0xffd39b,1);this.moveArrow.material.depthTest=false;this.moveArrow.renderOrder=8;
   }
  }
  if(edges.length&&this.mode==='architecture') {
   const p=[],c=[],max=Math.max(1,...edges.map(e=>Math.abs(e[2])));
   for(const [a,b,n] of edges){p.push(...a,...b);const col=this.color(n,max);c.push(...col,...col);}
   const geo=new THREE.BufferGeometry();geo.setAttribute('position',new THREE.Float32BufferAttribute(p,3));geo.setAttribute('color',new THREE.Float32BufferAttribute(c,3));
   this.connections=new THREE.LineSegments(geo,new THREE.LineBasicMaterial({vertexColors:true,transparent:true,opacity:.85,depthWrite:false}));this.root.add(this.connections);
  }
  this.request();
 }
 highlightPiece(sq){if(!this.pieceHalo){this.pieceHalo=this.mesh(new THREE.RingGeometry(.38,.43,32),new THREE.MeshBasicMaterial({color:0xffce80,side:THREE.DoubleSide}),0,0,0);this.pieceHalo.rotation.x=-Math.PI/2;}this.pieceHalo.visible=sq!=null;if(sq!=null)this.pieceHalo.position.set(-40+(sq%8-3.5)*.9,-.10,(7-Math.floor(sq/8)-3.5)*.9);this.request();}
 setEdgeMode(mode){this.edgeMode=mode;this.select(this.selection);}
 unfold(on) {
  this.pauseTrace();this.unfolded=on;
  this.heads.forEach((h,i)=>{
   const shift=on?(i-1.5)*4:0;h.g.position.z=shift;h.label.pos.z=h.z+shift;
   const vp=this.valuePositions[i][0].clone().add(this.v(0,0,shift)),qp=this.queryPositions[i][0].clone().add(this.v(0,0,shift)),hp=this.hiddenPositions[i][0].clone().add(this.v(0,0,shift));
   for(const [key,points] of [['input',[this.v(4,3.4,0),this.v(7,3.4,(h.z+shift)*.7),hp]],['vl',[vp,this.v(27,5,h.z+shift),this.v(31,5,-7),this.v(35,5,-7)]],['ql',[qp,this.v(27,1.8,h.z+shift),this.v(31,1.8,8),this.v(35,1.8,8)]]]){
    h[key].geometry.dispose();h[key].geometry=new THREE.TubeGeometry(new THREE.CatmullRomCurve3(points),48,key==='input'?.03:.025,6,false);h[key].visible=true;
   }
  });
  this.select(this.selection);this.request();
 }
 resize(){this.width=Math.max(1,this.canvas.parentElement.clientWidth);this.height=Math.max(1,this.canvas.parentElement.clientHeight);this.camera.aspect=this.width/this.height;this.camera.updateProjectionMatrix();this.renderer.setSize(this.width,this.height,false);this.request();}
 _destination(chapter){const h=this.inspectedHead??this.result?.headIndex??3,z=this.heads[h].z+this.heads[h].g.position.z,ratio=this.camera.aspect;
  const views=[{t:[0,3,0],d:[19,43,79]},{t:[-40,.6,0],d:[-32,10,14]},{t:[-25,3.5,0],d:[-9,14,22]},{t:[-7,3,0],d:[7,12,21]},{t:[15,3,z],d:[29,12,z+20]},{t:[37,2.7,-7],d:[47,12,9]},{t:[37,2.4,8],d:[49,12,23]}];
  const view=views[chapter],t=this.v(...view.t),d=this.v(...view.d);if(chapter===0){const f=Math.max(1,1.35/ratio);d.sub(t).multiplyScalar(f).add(t);}else if(ratio<.8)d.sub(t).multiplyScalar(1.25).add(t);return {t,d};
 }
 cameraState(){return {position:this.camera.position.toArray(),target:this.target.toArray(),mode:this.mode,focusKey:this.focusKey,focusPoint:this.focusPoint?.toArray(),pivot:this.pivotLabel||'Scene centre'};}
 restoreCamera(saved){if(!saved)return;this.setFlight(false);this.focusKey=saved.focusKey;this.focusPoint=saved.focusPoint?this.v(...saved.focusPoint):null;this.pivotLabel=saved.pivot;this.travel(this.v(...saved.target),this.v(...saved.position),false);this.pivotText();}
 pivotText(){document.getElementById('pivot-name').textContent='Orbit centre: '+(this.pivotLabel||'Scene centre');}
 selectUniverse(on,restore=true) {
  const mode=on?'universe':'architecture';if(this.mode===mode)return;
  this.setFlight(false);this.viewCameras[this.mode]=this.cameraState();this.mode=mode;
  this.root.visible=!on;this.environment.visible=!on;this.universe.visible=on;this.stopTrace();
  if(on&&!this.universePoints)this.setUniverse(this.result?.buckets[this.result.p.us]||0);
  this.focusKey=null;
  if(restore&&this.viewCameras[mode])this.restoreCamera(this.viewCameras[mode]);
  else {const d=on?{t:this.v(),d:this.v(35,24,50)}:this._destination(this.chapter);this.pivotLabel=on?'Weight field':`Chapter ${this.chapter}`;this.travel(d.t,d.d);this.pivotText();}
  this.select(this.selection);this.request();
 }
 go(chapter,animate=true){this.chapter=chapter;if(this.mode!=='architecture')this.selectUniverse(false,false);this.setFlight(false);this.focusKey=null;this.pivotLabel=chapter===0?'Whole architecture':`Chapter ${chapter}`;const p=this._destination(chapter);this.travel(p.t,p.d,animate);this.pivotText();}
 overview(){this.focusKey=null;this.pivotLabel=this.mode==='universe'?'Weight field':'Whole architecture';if(this.mode==='universe')this.travel(this.v(),this.v(35,24,50));else this.go(0);this.pivotText();}
 focusSelection(label='Selected component') {
  const p=this.pointFor(this.selection);if(!p)return false;
  this.pauseTrace();this.setFlight(false);
  const distance={piece:4,weight:3,channel:4,input:4,neuron:5,value:5,query:5,head:17,bank:15,move:12}[this.selection.type]||8;
  const direction=this.camera.position.clone().sub(this.target).normalize();if(!direction.lengthSq())direction.set(.5,.4,1).normalize();
  this.focusKey=this.selectionKey(this.selection);this.focusPoint=p.clone();this.pivotLabel=label;
  this.travel(p,p.clone().addScaledVector(direction,distance*Math.max(1,.8/this.camera.aspect)));this.pivotText();return true;
 }
 travel(t,p,animate=true){this.keys.clear();if(!animate||this.reduce.matches){this.target.copy(t);this.camera.position.copy(p);this.camera.lookAt(this.target);this._syncOrbit();this.transition=null;}else this.transition={start:performance.now(),from:this.camera.position.clone(),to:p.clone(),tf:this.target.clone(),tt:t.clone()};this.request();}
 _syncOrbit(){const d=this.camera.position.clone().sub(this.target);this.radius=d.length();this.phi=Math.acos(Math.max(-1,Math.min(1,d.y/Math.max(.001,this.radius))));this.theta=Math.atan2(d.x,d.z);}
 _orbit(){const {radius:r,phi:p,theta:t}=this;this.camera.position.copy(this.target).add(this.v(r*Math.sin(p)*Math.sin(t),r*Math.cos(p),r*Math.sin(p)*Math.cos(t)));this.camera.lookAt(this.target);}
 setFlight(value){const was=this.flight;if(value===was)return;this.pauseTrace();this.flight=value;this.transition=null;this.keys.clear();if(value){this.beforeFlight=this.cameraState();const e=new THREE.Euler().setFromQuaternion(this.camera.quaternion,'YXZ');this.yaw=e.y;this.pitch=e.x;this.canvas.focus({preventScroll:true});}else if(was){const saved=this.beforeFlight;this.target.fromArray(saved.target);this.camera.position.fromArray(saved.position);this.camera.lookAt(this.target);this._syncOrbit();this.focusKey=saved.focusKey;this.focusPoint=saved.focusPoint?this.v(...saved.focusPoint):null;this.pivotLabel=saved.pivot;this.pivotText();}document.getElementById('flight').setAttribute('aria-pressed',String(value));document.getElementById('gesture').textContent=value?'Drag to look · W/A/S/D move · Q/E down/up · Esc returns to your previous view':'Drag to orbit · wheel at pointer · Shift/right-drag to pan · double-click to focus';this.request();}
 _controls(){const canvas=this.canvas;canvas.oncontextmenu=e=>e.preventDefault();
  canvas.addEventListener('pointerdown',e=>{this.pauseTrace();this.transition=null;canvas.focus({preventScroll:true});canvas.setPointerCapture(e.pointerId);this.pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});if(this.pointers.size===1){this.drag=0;this.multiTouch=false;}else this.multiTouch=true;document.getElementById('hover-hint').hidden=true;});
  canvas.addEventListener('pointermove',e=>{const old=this.pointers.get(e.pointerId);if(!old){if(performance.now()-(this.lastHover||0)>90){this.lastHover=performance.now();this.hover(e);}return;}
   const dx=e.clientX-old.x,dy=e.clientY-old.y;this.drag+=Math.abs(dx)+Math.abs(dy);
   if(this.pointers.size===2){const other=[...this.pointers.entries()].find(([id])=>id!==e.pointerId)[1],before=Math.hypot(old.x-other.x,old.y-other.y),after=Math.hypot(e.clientX-other.x,e.clientY-other.y),cx=(old.x+other.x)/2,cy=(old.y+other.y)/2;
    const anchor=this.zoomAnchor(cx,cy);AtlasMath.zoom(this.camera,this.target,anchor,before/Math.max(1,after));AtlasMath.pan(this.camera,this.target,dx*.5,dy*.5,this.height);
   }else if(this.flight){this.yaw-=dx*.004;this.pitch=Math.max(-1.5,Math.min(1.5,this.pitch-dy*.004));this.camera.quaternion.setFromEuler(new THREE.Euler(this.pitch,this.yaw,0,'YXZ'));}
   else if(e.buttons===2||e.shiftKey){AtlasMath.pan(this.camera,this.target,dx,dy,this.height);this.focusKey=null;this.pivotLabel='Panned view';this.pivotText();}
   else AtlasMath.orbit(this.camera,this.target,dx,dy);
   this._syncOrbit();this.pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});this.request();
  });
  const up=e=>{if(this.pointers.has(e.pointerId)&&this.drag<6&&!this.multiTouch&&e.button===0)this._pick(e);this.pointers.delete(e.pointerId);if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);};
  canvas.addEventListener('pointerup',up);canvas.addEventListener('pointercancel',e=>this.pointers.delete(e.pointerId));canvas.addEventListener('lostpointercapture',e=>this.pointers.delete(e.pointerId));
  canvas.addEventListener('pointerleave',()=>{document.getElementById('hover-hint').hidden=true;});
  canvas.addEventListener('dblclick',e=>{const hit=this.hitAt(e.clientX,e.clientY);if(hit?.entity)this.onPick({...hit.entity,focus:true});});
  canvas.addEventListener('wheel',e=>{e.preventDefault();this.pauseTrace();this.transition=null;const delta=AtlasMath.wheelPixels(e,this.height);if(this.flight){const d=this.v();this.camera.getWorldDirection(d);this.camera.position.addScaledVector(d,-Math.sign(delta)*1.5);}else{const anchor=this.zoomAnchor(e.clientX,e.clientY);AtlasMath.zoom(this.camera,this.target,anchor,Math.exp(Math.max(-1,Math.min(1,delta*.0015))));this.focusKey=null;this.pivotLabel='Pointer zoom target';this.pivotText();this._syncOrbit();}this.request();},{passive:false});
  document.addEventListener('keydown',e=>{if(document.querySelector('dialog[open]')||/INPUT|SELECT|TEXTAREA/.test(e.target.tagName))return;const k=e.key==='Shift'?'Shift':e.key.toLowerCase();if(k==='escape'){this.pauseTrace();this.setFlight(false);return;}if(k==='home'){e.preventDefault();this.onPick({type:'overview'});return;}if(k==='f'&&!this.flight){e.preventDefault();this.onPick({type:'focus'});return;}if(this.flight&&['w','a','s','d','q','e','Shift'].includes(k)){e.preventDefault();this.keys.add(k);this.request();}});
  document.addEventListener('keyup',e=>this.keys.delete(e.key==='Shift'?'Shift':e.key.toLowerCase()));window.addEventListener('blur',()=>{this.keys.clear();this.pointers.clear();});document.addEventListener('visibilitychange',()=>{this.keys.clear();if(document.hidden){this.pauseTrace();cancelAnimationFrame(this.frame);this.frame=0;}else this.request();});
 }
 rayAt(x,y){this.camera.updateMatrixWorld();this.scene.updateMatrixWorld(true);const r=this.canvas.getBoundingClientRect(),ray=new THREE.Raycaster();ray.setFromCamera(new THREE.Vector2((x-r.left)/r.width*2-1,-(y-r.top)/r.height*2+1),this.camera);return ray;}
 zoomAnchor(x,y){const hit=this.hitAt(x,y);if(hit)return hit.point;const ray=this.rayAt(x,y),normal=this.v();this.camera.getWorldDirection(normal);return ray.ray.intersectPlane(new THREE.Plane().setFromNormalAndCoplanarPoint(normal,this.target),this.v())||this.target.clone();}
 entityFor(hit){const p=hit.object.userData.pick;if(!p)return null;
  if(p.type.startsWith('hidden'))return {type:'neuron',head:+p.type.slice(6),index:hit.instanceId};
  if(p.type.startsWith('acc'))return {type:'channel',view:+p.type.slice(3)?1-this.result.p.us:this.result.p.us,index:hit.instanceId};
  if(p.type==='input')return {type:'input',index:hit.instanceId};
  if(p.type.startsWith('query')||p.type.startsWith('value'))return {type:p.type.slice(0,5),head:+p.type.slice(5),index:hit.instanceId};
  if(p.type==='head')return {type:'head',head:p.index};return {...p};
 }
 hitAt(x,y){const ray=this.rayAt(x,y);
  if(this.mode==='universe'){
   if(!this.universePoints)return null;
   const scale=2*Math.tan(this.camera.fov*Math.PI/360)/this.height;
   ray.params.Points.threshold=Math.max(.025,scale*(this.camera.position.length()+26)*4);
   const r=this.canvas.getBoundingClientRect();
   for(const hit of ray.intersectObject(this.universePoints)){
    const p=this.v().fromBufferAttribute(this.universePoints.geometry.attributes.position,hit.index),q=p.clone().project(this.camera);
    if(q.z<-1||q.z>1||Math.hypot((q.x+1)*r.width/2+r.left-x,(1-q.y)*r.height/2+r.top-y)>4)continue;
    const index=this.universePoints.userData.indices[hit.index];return {point:p,entity:{type:'weight',bucket:this.bank,row:Math.floor(index/128),col:index%128}};
   }return null;
  }
  const objects=[];
  const collect=o=>{if(!o.isMesh||o.userData.overlay||!o.visible)return;let a=o;while(a){if(!a.visible)return;a=a.parent;}
   const mats=Array.isArray(o.material)?o.material:[o.material];if(o.userData.pick||mats.some(m=>!m.transparent&&m.depthWrite!==false))objects.push(o);};
  this.root.traverse(collect);this.environment.traverse(collect);
  const hit=ray.intersectObjects(objects,false)[0];return hit?{point:hit.point,entity:this.entityFor(hit)}:null;
 }
 _pick(e){const hit=this.hitAt(e.clientX,e.clientY);if(hit?.entity)this.onPick(hit.entity);}
 hover(e){if(this.flight||this.transition)return;const hit=this.hitAt(e.clientX,e.clientY),p=hit?.entity,box=document.getElementById('hover-hint');this.canvas.style.cursor=p?'pointer':'grab';box.hidden=!p;if(!p)return;
  box.textContent=p.type==='weight'?`Bucket ${p.bucket} · row ${p.row} · channel ${p.col} = ${ForgeModel.tensors.placement[(p.bucket*768+p.row)*128+p.col]}`:p.type==='neuron'||p.type==='query'||p.type==='value'?`H${p.head} · ${p.type} ${p.index} · click to inspect, double-click to focus`:p.type==='piece'||p.type==='square'?`${ForgeModel.squareName(p.sq)} · click to inspect`:p.type==='channel'?`${p.view?'Black':'White'} channel ${p.index}`:p.type==='input'?`Input ${p.index}`:p.type==='bank'?`King bucket ${p.index}`:p.type==='head'?`Specialist H${p.head}`:'Open this chapter';
 }
 // The walkthrough is controlled by the application; scene animation never
 // owns a stale copy of a game position or head route.
 startTrace(){this.onPick({type:'guide'});}
 pauseTrace(){if(this.trace){this.trace=false;this.onPick({type:'pause-guide'});}}
 stopTrace(){this.pauseTrace();if(this.orb)this.orb.visible=this.orbHalo.visible=false;}
 request(){if(!this.frame&&!document.hidden)this.frame=requestAnimationFrame(t=>this._draw(t));}
 _draw(now){this.frame=0;const dt=Math.min(.05,(now-(this.last||now))/1000);this.last=now;let moving=false;
  if(this.transition){const a=this.transition,t=Math.max(0,Math.min(1,(now-a.start)/650)),s=t*t*(3-2*t);this.camera.position.lerpVectors(a.from,a.to,s);this.target.lerpVectors(a.tf,a.tt,s);this.camera.lookAt(this.target);this._syncOrbit();if(t===1)this.transition=null;else moving=true;}
  if(this.flight&&this.keys.size){const f=this.v();this.camera.getWorldDirection(f);const right=this.v().crossVectors(f,this.v(0,1,0)).normalize(),delta=this.v();if(this.keys.has('w'))delta.add(f);if(this.keys.has('s'))delta.sub(f);if(this.keys.has('a'))delta.sub(right);if(this.keys.has('d'))delta.add(right);if(this.keys.has('q'))delta.y--;if(this.keys.has('e'))delta.y++;if(delta.lengthSq()){this.camera.position.addScaledVector(delta.normalize(),dt*(this.keys.has('Shift')?32:9));moving=true;}}
  if(this.selectionRing)this.selectionRing.quaternion.copy(this.camera.quaternion);this.camera.updateMatrixWorld();this.renderer.render(this.scene,this.camera);this.draws++;this._labels();document.getElementById('frame-status').textContent=moving?'Moving through the model':'Render on demand';if(moving)this.request();
 }
 _labels(){const occupied=[],v=this.v(),canvasRect=this.canvas.getBoundingClientRect(),blockers=['.stage-title','#universe-controls','#guide-controls','.view-tools','#hover-hint'].map(s=>document.querySelector(s)).filter(e=>e&&!e.hidden).map(e=>{const r=e.getBoundingClientRect();return {x:r.left-canvasRect.left,y:r.top-canvasRect.top,w:r.width,h:r.height};});
  for(const label of [...this.labels].sort((a,b)=>b.priority-a.priority)){
   const el=label.e;if(label.parent!==this.mode){el.style.visibility='hidden';continue;}v.copy(label.pos).project(this.camera);const x=(v.x+1)*this.width/2,y=(1-v.y)*this.height/2,w=el.offsetWidth||130,h=el.offsetHeight||28,r={x:x-w/2,y:y-h,w,h};
   let ok=v.z<1&&v.z>-1&&r.x>7&&r.x+w<this.width-7&&r.y>12&&y<this.height-130;
   if([...occupied,...blockers].some(a=>r.x<a.x+a.w+7&&r.x+w>a.x-7&&r.y<a.y+a.h+4&&r.y+h>a.y-4))ok=false;
   el.style.visibility=ok?'visible':'hidden';el.style.left=x+'px';el.style.top=y+'px';if(ok)occupied.push(r);
  }
 }
}
