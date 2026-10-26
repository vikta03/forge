/* Non-browser regression checks. No DOM rendering, browser access or engine job.
 * Remmy Victor. GPL-3.0-or-later. Run with node verify-interactions.cjs. */
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert/strict'),crypto=require('crypto');
const root=__dirname,read=n=>fs.readFileSync(path.join(root,n),'utf8');
const THREE=require(path.join(root,'../vendor/three.min.js'));
const data=JSON.parse(read('atlas-data.json')),checks=[];
const elements=new Map();
// Inert value holders used only by controller methods. They do not lay out or
// render HTML, fetch any URL or provide a browser surface.
function element(id){if(!elements.has(id))elements.set(id,{value:id==='weight-colour'?'16':'',hidden:false,scrollTop:0,textContent:'',setAttribute(){},classList:{add(){},remove(){},toggle(){}},insertAdjacentHTML(){}});return elements.get(id);}
const context=vm.createContext({THREE,Buffer,ATLAS_DATA:data,console,performance,document:{getElementById:element,querySelector:element,querySelectorAll:()=>[]},setTimeout,clearTimeout});
vm.runInContext(read('model.js')+'\n'+read('interaction.js')+'\n'+read('scene.js')+'\nglobalThis.TestAPI={M:ForgeModel,math:AtlasMath,inspect:AtlasInspect,Scene:AtlasScene};',context);
const {M,math,inspect,Scene}=context.TestAPI;
function check(name,fn){fn();checks.push(name);}
const vec=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z);
function camera(){const c=new THREE.PerspectiveCamera(42,1027/637,.1,550);c.position.set(19,43,79);c.lookAt(vec(0,3,0));c.updateMatrixWorld();return c;}
const screen=(p,c)=>{const q=p.clone().project(c);return [(q.x+1)*1027/2,(1-q.y)*637/2];};
check('Embedded model bytes retain the original deployed SHA256',()=>{
 assert.equal(crypto.createHash('sha256').update(Buffer.from(data.base64,'base64')).digest('hex'),data.modelHash);
 assert.equal(data.modelHash,'9430224f025568d8c20504e02775ac33c74a07fccdefd4b765c0a02ce5349512');
});
check('Both orbit axes follow a drag, without changing orbit radius',()=>{
 const marker=vec(0,3,13),target=vec(0,3,0),c=camera(),before=screen(marker,c),r=c.position.distanceTo(target);
 math.orbit(c,target,40,0);assert(screen(marker,c)[0]>before[0]);assert(Math.abs(c.position.distanceTo(target)-r)<1e-10);
 const d=camera();math.orbit(d,target,0,40);assert(screen(marker,d)[1]>before[1]);
});
check('Cursor-anchored zoom preserves off-centre projection at near/far scales',()=>{
 for(const pos of [vec(15,3,13.8),vec(-25,4,0),vec(7,-9.525,-2.58)])for(const factor of [.25,.7,1.5,3]){
  const c=camera(),t=vec(0,3,0),a=screen(pos,c);math.zoom(c,t,pos,factor);const b=screen(pos,c);assert(Math.hypot(a[0]-b[0],a[1]-b[1])<1e-8);
 }
 assert.equal(math.wheelPixels({deltaY:2,deltaMode:1},637),32);assert.equal(math.wheelPixels({deltaY:1,deltaMode:2},637),637);
});
check('Pan follows drag and preserves camera orientation',()=>{
 const c=camera(),t=vec(0,3,0),a=screen(t,c),marker=t.clone(),q=c.quaternion.clone();math.pan(c,t,40,25,637);const b=screen(marker,c);assert(Math.abs(b[0]-a[0]-40)<1e-8);assert(Math.abs(b[1]-a[1]-25)<1e-8);assert(q.equals(c.quaternion));
});
check('Resize updates projection without moving camera or pivot',()=>{
 const c=camera(),before=c.position.toArray(),target=vec(15,3,8),t=target.toArray();
 Scene.prototype.resize.call({canvas:{parentElement:{clientWidth:700,clientHeight:600}},camera:c,target,renderer:{setSize(){}},request(){},go(){throw Error('Resize tried to reframe');}});
 assert.deepEqual(c.position.toArray(),before);assert.deepEqual(target.toArray(),t);assert.equal(c.aspect,700/600);
});
check('Every value/query pick retains its head and exact coordinate',()=>{
 for(let head=0;head<4;head++)for(const kind of ['value','query'])for(let index=0;index<(kind==='value'?2:16);index++){
  const got=Scene.prototype.entityFor.call({}, {object:{userData:{pick:{type:kind+head}}},instanceId:index});
  assert.equal(got.type,kind);assert.equal(got.head,head);assert.equal(got.index,index);
 }
});
check('Unfold preserves and reconnects every head input/value/query route',()=>{
 const s=Object.create(Scene.prototype);s.heads=[];s.hiddenPositions=[];s.valuePositions=[];s.queryPositions=[];s.pauseTrace=()=>{};s.select=()=>{};s.request=()=>{};
 for(let h=0;h<4;h++){const z=(h-1.5)*9.2,route=()=>({geometry:new THREE.BufferGeometry(),visible:true});s.heads.push({g:new THREE.Group(),z,label:{pos:vec(17,7,z)},input:route(),vl:route(),ql:route()});s.hiddenPositions.push([vec(13,3.5,z)]);s.valuePositions.push([vec(21,5,z-.4)]);s.queryPositions.push([vec(21,1.75,z)]);}
 for(const on of [true,false]){s.unfold(on);for(let h=0;h<4;h++){const v=s.heads[h],dz=on?(h-1.5)*4:0;for(const key of ['input','vl','ql'])assert(v[key].visible);assert.equal(v.g.position.z,dz);assert.equal(v.vl.geometry.parameters.path.points[0].z,s.valuePositions[h][0].z+dz);assert.equal(v.input.geometry.parameters.path.points.at(-1).z,s.hiddenPositions[h][0].z+dz);}}
});
check('Universe hides the floor; view switches restore camera and targets',()=>{
 const s=Object.create(Scene.prototype);Object.assign(s,{mode:'architecture',camera:camera(),target:vec(0,3,0),keys:new Set(),root:new THREE.Group(),environment:new THREE.Group(),universe:new THREE.Group(),viewCameras:{},universePoints:{},chapter:0,reduce:{matches:true},setFlight(){},stopTrace(){},select(){},request(){},pivotText(){}});
 const initial=s.cameraState();s.selectUniverse(true);assert.equal(s.environment.visible,false);assert.equal(s.root.visible,false);assert.equal(s.universe.visible,true);
 s.camera.position.set(8,2,9);s.target.set(3,0,0);const u=s.cameraState();s.selectUniverse(false);assert.equal(s.environment.visible,true);assert.deepEqual(s.camera.position.toArray(),initial.position);s.selectUniverse(true);assert.deepEqual(s.camera.position.toArray(),u.position);assert.deepEqual(s.target.toArray(),u.target);
});
check('Selection focus follows a piece; changing views safely clears its move arrow',()=>{
 const s=Object.create(Scene.prototype),r=M.evaluate(data.games[0].positions[61]);
 Object.assign(s,{scene:new THREE.Scene(),root:new THREE.Group(),mode:'architecture',camera:camera(),target:vec(0,3,0),keys:new Set(),reduce:{matches:true},flight:false,result:r,request(){},canvas:{focus(){}},heads:Array.from({length:4},()=>({g:new THREE.Group(),internal:{material:{opacity:1}}}))});s.scene.add(s.root);
 s.select({type:'piece',sq:40,id:'bishop'});s.focusSelection('Bishop');assert(s.target.equals(s.pointFor(s.selection)));
 const from=s.camera.position.clone(),target=s.target.clone();s.select({type:'piece',sq:41,id:'bishop'});assert(Math.abs(s.camera.position.x-from.x-.9)<1e-10);assert(Math.abs(s.target.x-target.x-.9)<1e-10);
 const initial=s.cameraState();s.setFlight(true);s.camera.position.set(100,10,30);s.setFlight(false);assert.deepEqual(s.camera.position.toArray(),initial.position);assert.deepEqual(s.target.toArray(),initial.target);
 s.select({type:'move',head:r.headIndex,uci:r.active.quiet[0].uci});assert(s.moveArrow);s.mode='universe';s.select(s.selection);assert.equal(s.moveArrow,null);assert.equal(s.selectionRing.visible,false);
});
check('Weight field low/high channel positions remain pickable above the former floor',()=>{
 // Use the actual Points picker and ray intersection, with tiny known geometry.
 // No renderer is created; this is a geometry regression, not a visual test.
 const s=Object.create(Scene.prototype);Object.assign(s,{mode:'universe',bank:4,camera:new THREE.PerspectiveCamera(42,1,.1,550),scene:new THREE.Scene(),height:800,canvas:{getBoundingClientRect:()=>({left:0,top:0,width:800,height:800})}});
 const positions=new Float32Array([0,-9.525,0,0,9.525,0]),geo=new THREE.BufferGeometry();geo.setAttribute('position',new THREE.BufferAttribute(positions,3));s.universePoints=new THREE.Points(geo,new THREE.PointsMaterial());s.universePoints.userData.indices=new Uint32Array([0,127]);s.scene.add(s.universePoints);
 for(let i=0;i<2;i++){const p=vec().fromBufferAttribute(geo.attributes.position,i);s.camera.position.copy(p).add(vec(0,0,10));s.camera.lookAt(p);s.camera.updateMatrixWorld();const hit=s.hitAt(400,400);assert.equal(hit.entity.col,i?127:0);assert.equal(hit.entity.bucket,4);}
});
let receipt;
check('Original 128 native fixture comparisons still match',()=>{receipt=M.verify();assert(receipt.passed,receipt.errors.join('\n'));});
check('All inspector channel, hidden, output and policy sums reconstruct model outputs',()=>{
 for(const index of [0,61,62,63,100,163]){
  const r=M.evaluate(data.games[0].positions[index]);
  for(let view=0;view<2;view++)for(let k=0;k<128;k++)assert.equal(inspect.channelTerms(r,view,k).reduce((a,t)=>a+t.value,0),r.acc[view][k]);
  for(let h=0;h<4;h++){
   for(let j=0;j<32;j++)assert.equal(inspect.hiddenTerms(r,h,j).reduce((a,t)=>a+t.product,0),r.heads[h].sums[j]);
   for(const kind of ['value','query'])for(let k=0;k<(kind==='value'?2:16);k++)assert.equal(inspect.outputTerms(r,h,kind,k).reduce((a,t)=>a+t.product,0),kind==='value'?r.heads[h].rawValue[k]:r.heads[h].querySums[k]);
   for(const m of r.heads[h].quiet)assert.equal(inspect.moveTerms(r,h,m).reduce((a,t)=>a+t.product,0),m.raw);
  }
 }
});
check('Piece identities cover both games; captured bishop is absent, never replaced',()=>{
 for(const game of data.games){const maps=inspect.identities(game.positions);maps.forEach((map,i)=>{const p=M.parse(game.positions[i].fen);assert.equal(map.filter(Boolean).length,p.pieces.length);assert.equal(new Set(map.filter(Boolean)).size,p.pieces.length);p.pieces.forEach(q=>assert(map[q.sq]));});}
 const maps=inspect.identities(data.games[0].positions),id=maps[61][40];assert(id);assert.equal(maps[60].indexOf(id),19);assert.equal(maps[62].indexOf(id),-1);assert.notEqual(maps[62][40],id);
});
check('Stored weight reports only its real participating perspectives and pieces',()=>{
 const r=M.evaluate(data.games[0].positions[61]);for(const view of [0,1]){const row=r.rows[view].find(t=>t.piece.sq===40);const uses=inspect.weightUses(r,{bucket:r.buckets[view],row:row.row,col:0});assert(uses.some(u=>u.view===view&&u.piece.sq===40));}
 assert.equal(inspect.weightUses(r,{bucket:31,row:0,col:0}).length,0);
});

// Exercise the application's controller and generated inspector strings without
// its renderer, event binding, timers or a browser. Replace only render() with
// a no-op, keeping the selection/position/back logic exactly as shipped.
let app=read('app.js');app=app.slice(0,app.indexOf(' // Delegate inspector links:'));
const start=app.indexOf(' function render(){'),end=app.indexOf(' function showPanel()',start);
app=app.slice(0,start)+' function render(){}\n'+app.slice(end);
app+='\n globalThis.Controller={state,compute,choose,back,changePly,selectedPanel,panel,onPick,switchView,stepGuide,endGuide,openMatrix,cell:()=>matrixCell,result:()=>result,guide:()=>guide};\n})();';
vm.runInContext(app,context);const C=context.Controller;C.compute();
check('Inactive-head inspection stays on the clicked head across positions',()=>{
 C.onPick({type:'value',head:0,index:1});assert.equal(C.state.head,0);assert.equal(C.state.selection.index,1);assert.match(C.selectedPanel(),/COUNTERFACTUAL HEAD.*H0/);assert.match(C.selectedPanel(),new RegExp(String(C.result().heads[0].rawValue[1])));
 C.changePly(62);assert.equal(C.state.head,0);assert.equal(C.state.selection.head,0);assert.equal(C.state.selection.index,1);
 C.state.followHead=true;C.compute();assert.equal(C.state.selection.head,C.result().headIndex);
});
check('Channel identity and perspective survive turn changes; follow-turn is explicit',()=>{
 C.choose({type:'channel',view:0,index:43});C.changePly(63);assert.equal(C.state.selection.view,0);assert.equal(C.state.selection.index,43);assert.match(C.selectedPanel(),/raw sum/);
 C.state.followView=true;C.compute();assert.equal(C.state.selection.view,C.result().p.us);
});
check('Selection Back restores exact weight address and piece identity',()=>{
 C.changePly(61);C.choose({type:'piece',sq:40,view:0});const id=C.state.pieceId;C.choose({type:'weight',bucket:C.result().buckets[0],row:168,col:17});C.choose({type:'channel',view:0,index:17});C.back();assert.equal(C.state.selection.type,'weight');assert.equal(C.state.selection.row,168);assert.equal(C.state.selection.col,17);C.back();assert.equal(C.state.selection.type,'piece');assert.equal(C.state.pieceId,id);
 C.changePly(62);assert.equal(C.state.piece,null);assert.match(C.selectedPanel(),/absent here/);C.changePly(61);assert.equal(C.state.piece,40);
});
check('View switching restores selected head, unit and channel context',()=>{
 C.changePly(61);C.choose({type:'neuron',head:0,index:27});C.switchView(true);assert.equal(C.state.universe,true);C.choose({type:'weight',bucket:4,row:168,col:47});C.switchView(false);assert.equal(C.state.selection.type,'neuron');assert.equal(C.state.selection.head,0);assert.equal(C.state.selection.index,27);C.switchView(true);assert.equal(C.state.selection.type,'weight');assert.equal(C.state.selection.col,47);C.switchView(false);
});
check('Manual guide follows actual arithmetic and ends on a position change',()=>{
 C.changePly(61);C.choose({type:'piece',sq:40,view:0});const types=[];
 for(let n=0;n<8;n++){C.stepGuide(n);types.push(C.state.selection.type);assert(!C.guide().playing);}
 assert.deepEqual(types,['piece','weight','channel','input','neuron','value','query','move']);
 C.changePly(62);assert.equal(C.guide(),null);
});
check('Each inspector type renders a valid explanation string across game phases',()=>{
 for(const ply of [0,61,62,100,163]){C.changePly(ply);const r=C.result(),sq=r.p.pieces[0].sq;
  const entities=[{type:'piece',sq},{type:'weight',bucket:r.buckets[0],row:0,col:127},{type:'channel',view:1,index:127},{type:'input',index:0},{type:'input',index:280},{type:'input',index:326},{type:'neuron',head:0,index:31},{type:'value',head:0,index:0},{type:'query',head:3,index:15},{type:'head',head:1},{type:'field',bucket:4}];
  if(r.active.quiet.length)entities.push({type:'move',head:r.headIndex,uci:r.active.quiet[0].uci});
  for(const s of entities){C.choose(s);const html=C.selectedPanel()+C.panel();assert(!/undefined|NaN/.test(html),s.type);assert(html.length>100);}
 }
});
check('Every constant DOM ID reference has a matching template or generated element',()=>{
 const app=read('app.js'),scene=read('scene.js'),html=read('index.template.html'),all=html+app;
 const ids=[...app.matchAll(/\$\('([\w-]+)'\)/g),...scene.matchAll(/getElementById\('([\w-]+)'\)/g)].map(m=>m[1]);
 for(const id of new Set(ids))assert(all.includes(`id="${id}"`),`Missing element ${id}`);
 const declarations=[...html.matchAll(/\bid="([\w-]+)"/g)].map(m=>m[1]);assert.equal(declarations.length,new Set(declarations).size);
});
const result={passed:true,kind:'Non-browser arithmetic, geometry and controller regression checks',checks,fixture_comparison:receipt,browser_interaction:'NOT RUN: local-file browser access was denied; no alternate browser or server used',source_sha256:Object.fromEntries(['interaction.js','scene.js','app.js','model.js','index.template.html','style.css','verify-interactions.cjs'].map(n=>[n,crypto.createHash('sha256').update(read(n)).digest('hex')]))};
fs.writeFileSync(path.join(root,'ux-audit/repair-check.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({passed:true,checks:checks.length,native_positions:receipt.positions,integer_values:receipt.integerValues,policy_logits:receipt.policyLogits,browser_interaction:'not run'}));
