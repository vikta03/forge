/* Navigation and inspection arithmetic. Remmy Victor. GPL-3.0-or-later.
 * Pure helpers: usable without a DOM, WebGL context, or running engine. */
const AtlasMath = {
 orbit(camera, target, dx, dy) {
  const d = camera.position.clone().sub(target), s = new THREE.Spherical().setFromVector3(d);
  s.theta -= dx * .005;
  s.phi = THREE.MathUtils.clamp(s.phi - dy * .005, .045, Math.PI - .045);
  camera.position.copy(target).add(new THREE.Vector3().setFromSpherical(s));
  camera.lookAt(target); camera.updateMatrixWorld();
 },
 pan(camera, target, dx, dy, height) {
  camera.updateMatrixWorld();
  const scale = 2 * camera.position.distanceTo(target) * Math.tan(camera.fov * Math.PI / 360) / Math.max(1, height);
  const delta = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0).multiplyScalar(-dx * scale)
   .addScaledVector(new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 1), dy * scale);
  camera.position.add(delta); target.add(delta); camera.updateMatrixWorld();
 },
 zoom(camera, target, anchor, factor) {
  // Scale camera AND pivot about the chosen surface point. Its screen projection
  // stays fixed; subsequent orbit has a meaningful, increasingly local pivot.
  const distance = camera.position.distanceTo(anchor), radius = camera.position.distanceTo(target);
  factor = THREE.MathUtils.clamp(factor, Math.max(.12 / Math.max(.001, distance), .12 / Math.max(.001, radius)), 360 / Math.max(.001, radius));
  camera.position.sub(anchor).multiplyScalar(factor).add(anchor);
  target.sub(anchor).multiplyScalar(factor).add(anchor);
  camera.lookAt(target); camera.updateMatrixWorld();
 },
 wheelPixels(event, height) { return event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? height : 1); }
};

const AtlasInspect = {
 inputIndex(view, channel, us) { return (view === us ? 0 : 128) + channel; },
 channelTerms(result, view, channel) {
  return result.rows[view].map(row => ({...row, value: ForgeModel.tensors.placement[row.offset + channel]}));
 },
 hiddenTerms(result, head, unit) {
  const weights = ForgeModel.tensors[`h${head}.hidden`];
  return Array.from(result.x, (x, i) => ({i, x, w: weights[i * 32 + unit], product: x * weights[i * 32 + unit]}));
 },
 outputTerms(result, head, kind, index) {
  const width = kind === 'value' ? 2 : 16, weights = ForgeModel.tensors[`h${head}.${kind}`];
  return Array.from(result.heads[head].hidden, (x, i) => ({i, x, w: weights[i * width + index], product: x * weights[i * width + index]}));
 },
 moveTerms(result, head, move) {
  const t = ForgeModel.tensors, q = result.heads[head].query;
  return Array.from(q, (x, i) => {
   const from = t.from[move.fromRow * 16 + i], to = t.to[move.toRow * 16 + i], special = t.special[move.kind * 16 + i];
   return {i, q:x, from, to, special, sum:from + to + special, product:x * (from + to + special)};
  });
 },
 weightUses(result, selected) {
  return [0,1].flatMap(view => result.buckets[view] !== selected.bucket ? [] :
   result.rows[view].filter(r => r.row === selected.row).map(r => ({view, piece:r.piece})));
 },
 identities(records) {
  // Track physical pieces through the saved standard-chess games, including
  // captures, promotions, castling and en passant. Backtracking is then exact.
  let before = ForgeModel.parse(records[0].fen), ids = before.board.map(p => p ? `${p.color}:${p.type}:${p.sq}` : null);
  const maps = [ids.slice()];
  for (let n = 1; n < records.length; n++) {
   const after = ForgeModel.parse(records[n].fen), next = ids.slice(), move = records[n].last;
   if (move) {
    const [from,to] = move, piece = before.board[from];
    next[to] = ids[from]; next[from] = null;
    if (piece?.type === 0 && from % 8 !== to % 8 && !before.board[to]) next[to + (piece.color ? 8 : -8)] = null;
    if (piece?.type === 5 && Math.abs(to-from) === 2) {
     const rookFrom = Math.floor(from/8)*8 + (to>from ? 7 : 0), rookTo = to + (to>from ? -1 : 1);
     next[rookTo] = ids[rookFrom]; next[rookFrom] = null;
    }
   }
   for (let sq=0; sq<64; sq++) if (!after.board[sq]) next[sq]=null;
   maps.push(next); ids=next; before=after;
  }
  return maps;
 }
};
