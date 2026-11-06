/* =====================================================================
   Pas à pas : rejoue un algorithme étape par étape, avec son pseudo-code.
   Les étapes sont calculées par la page (même méthode que le code C++ de la
   scène), puis dessinées dans le niveau par les calques « step… » de
   AGeomVisualizer. Le résultat final est comparé à celui renvoyé par le C++.
   Chaque étape : { line, msg, ...éléments à dessiner }.
   ===================================================================== */
'use strict';

const GeomSteps = (() => {
  const orient = (a, b, c) => (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  const d2 = (a, b) => (a.x - b.x) ** 2 + (a.y - b.y) ** 2;
  const L = (p) => `P${p.id}`;
  const num = (v) => { const a = Math.abs(v); return (v < 0 ? '−' : '') + (a >= 1e6 ? (a / 1e6).toFixed(1).replace('.', ',') + ' M' : a >= 1e3 ? (a / 1e3).toFixed(1).replace('.', ',') + ' k' : a.toFixed(0)); };
  const keyOf = (a, b) => (a.id < b.id ? a.id + '-' + b.id : b.id + '-' + a.id);

  const PSEUDO = {
    jarvis: [
      'i0 ← point le plus à gauche (puis le plus bas)',
      'i ← i0',
      'répéter',
      '  ajouter Pi à l\'enveloppe',
      '  inew ← un autre point',
      '  pour tout j : si orient(Pi, Pinew, Pj) < 0 alors inew ← j',
      '  i ← inew',
      'jusqu\'à Pi = Pi0',
    ],
    graham: [
      'B ← barycentre des points',
      'trier les points par angle polaire autour de B',
      'chaîner en liste circulaire (prec, suiv)',
      'pivot ← premier nœud ; Sinit ← pivot',
      'tant que pivot ≠ Sinit ou on vient de reculer',
      '  si orient(prec, pivot, suiv) > 0 : convexe, pivot ← suiv',
      '  sinon : supprimer pivot, Sinit ← prec, pivot ← prec',
    ],
    incr: [
      'trier les points par x puis y',
      'k ← premier point non aligné avec P0 P1',
      'éventail initial : triangles (Pi, Pi+1, Pk)',
      'pour chaque point suivant q',
      '  pour chaque arête du bord vue par q (orient < 0)',
      '    ajouter le triangle (arête, q)',
    ],
    delaunay: [
      'T ← triangulation incrémentale',
      'pour chaque arête intérieure [V1, V2]',
      'tant qu\'une arête a été basculée',
      '  OppLeft, OppRight ← sommets opposés des deux faces',
      '  si OppRight est dans le cercle de (V1, V2, OppLeft)',
      '    basculer : l\'arête devient [OppLeft, OppRight]',
    ],
  };
  const CALL = { jarvis: 'ComputeJarvisMarch', graham: 'ComputeGrahamScan', incr: 'IncrementalTriangulation2D', delaunay: 'FlipEdgesToDelaunay' };

  /* ---------------- Algorithmes instrumentés ---------------- */
  function allCollinear(P) {
    for (let i = 2; i < P.length; i++) if (orient(P[0], P[1], P[i]) !== 0) return false;
    return true;
  }

  function* jarvis(P) {
    let i0 = 0;
    for (let i = 1; i < P.length; i++) if (P[i].x < P[i0].x || (P[i].x === P[i0].x && P[i].y < P[i0].y)) i0 = i;
    yield { line: 0, msg: `Départ : <b>${L(P[i0])}</b>, le point le plus à gauche.`, hull: [], pivot: P[i0] };
    const hull = [];
    let i = i0, guard = 0;
    do {
      hull.push(P[i]);
      let inew = i === 0 ? 1 : 0;
      yield { line: 3, msg: `<b>${L(P[i])}</b> ajouté à l'enveloppe (${hull.length} sommet${hull.length > 1 ? 's' : ''}). Premier candidat : ${L(P[inew])}.`, hull: hull.slice(), pivot: P[i], candidate: P[inew] };
      for (let j = 0; j < P.length; j++) {
        if (j === i || j === inew) continue;
        const o = orient(P[i], P[inew], P[j]);
        const take = o < 0 || (o === 0 && d2(P[i], P[j]) > d2(P[i], P[inew]));
        yield {
          line: 5, msg: `orient(${L(P[i])}, ${L(P[inew])}, ${L(P[j])}) = <b>${num(o)}</b> : ${take ? `${L(P[j])} est à droite, il devient le candidat.` : `on garde ${L(P[inew])}.`}`,
          hull: hull.slice(), pivot: P[i], candidate: take ? P[j] : P[inew], probe: P[j], taken: take,
        };
        if (take) inew = j;
      }
      yield { line: 6, msg: `Aucun point à droite de ${L(P[i])} → ${L(P[inew])} : <b>${L(P[inew])}</b> est le sommet suivant.`, hull: hull.slice(), pivot: P[inew], edge: [P[i], P[inew]] };
      i = inew;
    } while (i !== i0 && ++guard <= P.length);
    yield { line: 7, msg: `Retour à ${L(P[i0])} : enveloppe de <b>${hull.length}</b> sommets.`, hull, done: true };
  }

  function* graham(P) {
    const n = P.length;
    if (allCollinear(P)) {
      const s = P.slice().sort((a, b) => a.x - b.x || a.y - b.y);
      yield { line: 0, msg: 'Tous les points sont alignés : l\'enveloppe se réduit aux deux extrémités.', hull: [s[0], s[s.length - 1]], done: true };
      return;
    }
    const B = { x: P.reduce((s, p) => s + p.x, 0) / n, y: P.reduce((s, p) => s + p.y, 0) / n };
    yield { line: 0, msg: `Barycentre B = (${num(B.x)}, ${num(B.y)}).`, bary: B };
    const ang = (p) => Math.atan2(p.y - B.y, p.x - B.x);
    const sorted = P.slice().sort((a, b) => ang(a) - ang(b) || d2(a, B) - d2(b, B));
    yield { line: 1, msg: `${n} points triés par angle autour de B.`, bary: B, rays: sorted };
    const nodes = sorted.map((p) => ({ p }));
    nodes.forEach((nd, i) => { nd.next = nodes[(i + 1) % n]; nd.prev = nodes[(i - 1 + n) % n]; });
    const ringFrom = (s) => { const out = []; let c = s, g = 0; do { out.push(c.p); c = c.next; } while (c !== s && ++g <= n); return out; };
    let pivot = nodes[0], sinit = pivot, avance = true, removed = 0, guard = 0;
    yield { line: 2, msg: `Liste circulaire de ${n} nœuds : un polygone étoilé autour de B.`, bary: B, ring: ringFrom(pivot) };
    yield { line: 3, msg: `On part de <b>${L(pivot.p)}</b>, le premier dans l'ordre des angles.`, bary: B, ring: ringFrom(pivot), pivot: pivot.p };
    do {
      const o = orient(pivot.prev.p, pivot.p, pivot.next.p);
      const tri3 = [pivot.prev.p, pivot.p, pivot.next.p];
      if (o > 0) {
        yield { line: 5, msg: `orient(${L(pivot.prev.p)}, ${L(pivot.p)}, ${L(pivot.next.p)}) = <b>${num(o)}</b> &gt; 0 : ${L(pivot.p)} est convexe, on avance.`, bary: B, ring: ringFrom(sinit), pivot: pivot.p, tri3 };
        pivot = pivot.next; avance = true;
      } else {
        yield { line: 6, msg: `orient(${L(pivot.prev.p)}, ${L(pivot.p)}, ${L(pivot.next.p)}) = <b>${num(o)}</b> ≤ 0 : ${L(pivot.p)} est concave, supprimé. On recule.`, bary: B, ring: ringFrom(sinit), pivot: pivot.p, tri3, reject: true };
        const prev = pivot.prev;
        prev.next = pivot.next; pivot.next.prev = prev; removed++;
        sinit = prev; pivot = sinit; avance = false;
      }
    } while ((pivot !== sinit || !avance) && ++guard < 20 * n);
    const h = ringFrom(sinit);
    yield { line: 4, msg: `Tour complet sans suppression : ${removed} sommets retirés, enveloppe de <b>${h.length}</b> sommets.`, hull: h, done: true };
  }

  function boundaryKeep(P) { // enveloppe en gardant les points alignés sur le bord
    const s = P.slice().sort((a, b) => a.x - b.x || a.y - b.y), lo = [], up = [];
    for (const p of s) { while (lo.length >= 2 && orient(lo[lo.length - 2], lo[lo.length - 1], p) < 0) lo.pop(); lo.push(p); }
    for (let i = s.length - 1; i >= 0; i--) { const p = s[i]; while (up.length >= 2 && orient(up[up.length - 2], up[up.length - 1], p) < 0) up.pop(); up.push(p); }
    up.pop(); lo.pop(); return lo.concat(up);
  }

  function* incremental(P, inner) {
    const s = P.slice().sort((a, b) => a.x - b.x || a.y - b.y), tris = [];
    yield { line: 0, msg: `${s.length} points triés par abscisse : ${s.slice(0, 4).map(L).join(', ')}…`, tris: [], order: s };
    let k = 2;
    while (k < s.length && orient(s[0], s[1], s[k]) === 0) k++;
    if (k === s.length) { yield { line: 1, msg: 'Tous les points sont alignés : aucune triangulation possible.', tris, done: true, degenerate: true }; return inner ? tris : undefined; }
    yield { line: 1, msg: k > 2 ? `${k} premiers points alignés : <b>${L(s[k])}</b> sert de sommet d'éventail.` : `<b>${L(s[k])}</b> n'est pas aligné avec ${L(s[0])} et ${L(s[1])}.`, tris: [], probe: s[k], chain: s.slice(0, k) };
    const ccw = (a, b, c) => (orient(a, b, c) > 0 ? [a, b, c] : [a, c, b]);
    for (let i = 0; i < k - 1; i++) tris.push(ccw(s[i], s[i + 1], s[k]));
    yield { line: 2, msg: `Éventail initial : <b>${tris.length}</b> triangle${tris.length > 1 ? 's' : ''}.`, tris: tris.slice(), probe: s[k] };
    for (let q = k + 1; q < s.length; q++) {
      const hull = boundaryKeep(s.slice(0, q)), seen = [];
      for (let i = 0; i < hull.length; i++) { const a = hull[i], b = hull[(i + 1) % hull.length]; if (orient(a, b, s[q]) < 0) seen.push([a, b]); }
      yield { line: 4, msg: `<b>${L(s[q])}</b> voit <b>${seen.length}</b> arête${seen.length > 1 ? 's' : ''} du bord.`, tris: tris.slice(), probe: s[q], seen, hullNow: hull };
      for (const [a, b] of seen) tris.push([a, s[q], b]);
      yield { line: 5, msg: `${seen.length} triangle${seen.length > 1 ? 's' : ''} ajouté${seen.length > 1 ? 's' : ''} autour de ${L(s[q])} (total ${tris.length}).`, tris: tris.slice(), probe: s[q], fresh: tris.slice(-seen.length) };
    }
    if (!inner) yield { line: 5, msg: `Triangulation terminée : <b>${tris.length}</b> triangles.`, tris, done: true };
    return tris;
  }

  function circumcircle(a, b, c) {
    const d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (Math.abs(d) < 1e-12) return null;
    const a2 = a.x * a.x + a.y * a.y, b2 = b.x * b.x + b.y * b.y, c2 = c.x * c.x + c.y * c.y;
    const x = (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d, y = (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d;
    return { x, y, r: Math.hypot(a.x - x, a.y - y) };
  }
  const inCircle = (a, b, c, p) => { // > 0 si p est dans le cercle de (a, b, c) pris dans le sens direct
    const ax = a.x - p.x, ay = a.y - p.y, bx = b.x - p.x, by = b.y - p.y, cx = c.x - p.x, cy = c.y - p.y;
    return (ax * ax + ay * ay) * (bx * cy - cx * by) - (bx * bx + by * by) * (ax * cy - cx * ay) + (cx * cx + cy * cy) * (ax * by - bx * ay);
  };

  function* delaunay(P) {
    const it = incremental(P, true); let r;
    yield { line: 0, msg: 'On part de la triangulation incrémentale.', tris: [] };
    while (!(r = it.next()).done) { if (r.value.degenerate) { yield { ...r.value, done: true }; return; } }
    const tris = r.value.map((t) => t.slice());
    yield { line: 1, msg: `Triangulation de départ : <b>${tris.length}</b> triangles. On teste chaque arête intérieure.`, tris: tris.map((t) => t.slice()), flips: 0 };
    let flips = 0, changed = true, guard = 0;
    while (changed && guard++ < 6000) {
      changed = false;
      const map = new Map();
      tris.forEach((t, ti) => { for (let e = 0; e < 3; e++) { const k = keyOf(t[e], t[(e + 1) % 3]); (map.get(k) || map.set(k, []).get(k)).push([ti, e]); } });
      for (const pair of map.values()) {
        if (pair.length !== 2) continue;
        const [[t1, e1], [t2, e2]] = pair, A = tris[t1], C = tris[t2];
        const u = A[e1], v = A[(e1 + 1) % 3], w1 = A[(e1 + 2) % 3], w2 = C[(e2 + 2) % 3];
        if (inCircle(A[0], A[1], A[2], w2) > 1e-6) {
          yield { line: 4, msg: `${L(w2)} est dans le cercle de (${L(A[0])}, ${L(A[1])}, ${L(A[2])}) : l'arête ${L(u)}–${L(v)} n'est pas de Delaunay.`, tris: tris.map((t) => t.slice()), flipEdge: [u, v], circle: circumcircle(A[0], A[1], A[2]), probe: w2, flips };
          tris[t1] = [w1, u, w2]; tris[t2] = [w1, w2, v];
          for (const ti of [t1, t2]) { const t = tris[ti]; if (orient(t[0], t[1], t[2]) < 0) [t[1], t[2]] = [t[2], t[1]]; }
          flips++;
          yield { line: 5, msg: `Bascule n°${flips} : ${L(u)}–${L(v)} devient <b>${L(w1)}–${L(w2)}</b>.`, tris: tris.map((t) => t.slice()), newEdge: [w1, w2], flips };
          changed = true; break;
        }
      }
    }
    yield { line: 2, msg: `Plus aucune arête à basculer : triangulation de Delaunay en <b>${flips}</b> bascule${flips > 1 ? 's' : ''}.`, tris, flips, done: true };
  }
  const GEN = { jarvis, graham, incr: incremental, delaunay };

  /* ---------------- Dessin dans le niveau (calques « step… ») ---------------- */
  const COLORS = { stepRing: '#5b6779', stepTris: '#5fa8ff', stepHull: '#ffb547', stepAccent: '#c58bff', stepProbe: '#e6ebf2', stepGood: '#6fdc95', stepBad: '#ff5d7a' };
  const NAMES = Object.keys(COLORS);
  const poly = (pts, closed) => { const s = []; for (let i = 0; i + 1 < pts.length; i++) s.push([pts[i], pts[i + 1]]); if (closed && pts.length > 2) s.push([pts[pts.length - 1], pts[0]]); return s; };
  const triEdges = (tris) => { const seen = new Set(), out = []; for (const t of tris) for (let e = 0; e < 3; e++) { const a = t[e], b = t[(e + 1) % 3], k = keyOf(a, b); if (!seen.has(k)) { seen.add(k); out.push([a, b]); } } return out; };
  const ring = (c, r, n = 20) => { const s = []; for (let i = 0; i < n; i++) { const a0 = (i / n) * 2 * Math.PI, a1 = ((i + 1) / n) * 2 * Math.PI; s.push([{ x: c.x + Math.cos(a0) * r, y: c.y + Math.sin(a0) * r }, { x: c.x + Math.cos(a1) * r, y: c.y + Math.sin(a1) * r }]); } return s; };
  const cross = (c, r) => [[{ x: c.x - r, y: c.y }, { x: c.x + r, y: c.y }], [{ x: c.x, y: c.y - r }, { x: c.x, y: c.y + r }]];

  function layersFor(st, size) {
    const out = Object.fromEntries(NAMES.map((n) => [n, []]));
    if (!st) return out;
    const R = size * 0.018;
    if (st.ring) out.stepRing.push(...poly(st.ring, true));
    if (st.hullNow) out.stepRing.push(...poly(st.hullNow, true));
    if (st.chain) out.stepRing.push(...poly(st.chain, false));
    if (st.rays && st.bary) for (const p of st.rays) out.stepRing.push([st.bary, p]);
    if (st.tris) out.stepTris.push(...triEdges(st.tris));
    if (st.fresh) out.stepGood.push(...triEdges(st.fresh));
    if (st.hull) out.stepHull.push(...poly(st.hull, !!st.done));
    if (st.edge) out.stepGood.push(st.edge);
    if (st.pivot) { out.stepHull.push(...ring(st.pivot, R)); }
    if (st.candidate && st.pivot) out.stepAccent.push([st.pivot, st.candidate]);
    if (st.probe) { out.stepProbe.push(...ring(st.probe, R * 0.8)); if (st.pivot && !st.tri3) out.stepProbe.push([st.pivot, st.probe]); }
    if (st.bary) out.stepProbe.push(...cross(st.bary, R));
    if (st.tri3) (st.reject ? out.stepBad : out.stepGood).push(...poly(st.tri3, false));
    if (st.seen) out.stepGood.push(...st.seen);
    if (st.flipEdge) out.stepBad.push(st.flipEdge);
    if (st.newEdge) out.stepGood.push(st.newEdge);
    if (st.circle && st.circle.r < 50000) out.stepAccent.push(...ring(st.circle, st.circle.r, 40));
    return out;
  }

  /* ---------------- Lecteur ---------------- */
  const $ = (id) => document.getElementById(id);
  let api = null, steps = [], idx = 0, timer = 0, speed = 10, key = '', active = false, size = 1000;

  function compute() {
    const algo = api.algo(), P = uniqueById(api.points());
    const k = algo + '|' + P.map((p) => `${p.id},${p.x},${p.y}`).join(';');
    if (k === key) return false;
    key = k; stop();
    steps = []; idx = 0;
    if (GEN[algo] && P.length >= 3) {
      const xs = P.map((p) => p.x), ys = P.map((p) => p.y);
      size = Math.max(200, Math.hypot(Math.max(...xs) - Math.min(...xs), Math.max(...ys) - Math.min(...ys)));
      let guard = 0;
      for (const s of GEN[algo](P)) { steps.push(s); if (++guard > 20000) break; }
    }
    return true;
  }
  function uniqueById(points) {
    const seen = new Set(), out = [];
    for (const p of points) { const k = p.x + ',' + p.y; if (!seen.has(k)) { seen.add(k); out.push({ id: p.id, x: +p.x, y: +p.y }); } }
    return out;
  }

  function show() {
    const algo = api.algo(), tot = steps.length, st = steps[idx];
    $('stepsPanel').hidden = !GEN[algo];
    $('stepsVoronoi').hidden = algo !== 'voronoi';
    api.hideResult(active && !!GEN[algo]);
    if (!GEN[algo]) { clearLayers(); return; }
    $('stCall').textContent = CALL[algo];
    $('stNo').textContent = tot ? idx + 1 : 0; $('stTot').textContent = tot;
    $('stScrub').max = Math.max(0, tot - 1); $('stScrub').value = idx;
    $('stMsg').innerHTML = st ? st.msg : api.points().length < 3 ? 'Ajoutez au moins 3 points dans le niveau.' : '';
    const pseudo = $('stPseudo');
    if (pseudo.dataset.algo !== algo) { pseudo.dataset.algo = algo; pseudo.innerHTML = PSEUDO[algo].map((l) => `<li>${l.replace(/&/g, '&amp;').replace(/</g, '&lt;')}</li>`).join(''); }
    [...pseudo.children].forEach((li, i) => li.classList.toggle('on', !!st && st.line === i));
    $('stPlay').textContent = timer ? 'Pause' : idx >= tot - 1 && tot ? 'Rejouer' : 'Lecture';
    const fin = steps[tot - 1];
    $('stCompare').innerHTML = fin && st === fin ? compareWithCpp(fin) : '';
    if (active) { const L = layersFor(st, size); for (const n of NAMES) api.overlay(n, L[n], COLORS[n]); }
  }
  function compareWithCpp(fin) {
    const r = api.result();
    if (!r) return '';
    if (fin.hull) {
      const same = r.hull && r.hull.length === fin.hull.length;
      return same ? `Même nombre de sommets que <code>${CALL[api.algo()]}</code> en C++ (${fin.hull.length}).` : `Le C++ renvoie ${r.hull ? r.hull.length : 0} sommets, l'animation ${fin.hull.length}.`;
    }
    if (fin.tris) {
      const n = r.tris ? r.tris.length : 0;
      return n === fin.tris.length ? `Même nombre de triangles que le C++ (${n}).` : `Le C++ renvoie ${n} triangles, l'animation ${fin.tris.length}.`;
    }
    return '';
  }
  function clearLayers() { for (const n of NAMES) api.overlay(n, [], COLORS[n]); }

  function stop() { clearInterval(timer); timer = 0; }
  function go(i) { stop(); idx = Math.max(0, Math.min(steps.length - 1, i)); show(); }
  function play() {
    if (timer) { stop(); show(); return; }
    if (!steps.length) return;
    if (idx >= steps.length - 1) idx = 0;
    timer = setInterval(() => { if (idx >= steps.length - 1) { stop(); show(); return; } idx++; show(); }, 1000 / speed);
    show();
  }

  function setActive(on) {
    if (on === active) return;
    active = on;
    // Le résultat C++ est masqué pendant l'animation, pour ne voir que l'étape en cours
    if (!on) { stop(); clearLayers(); api.hideResult(false); } else { compute(); show(); }
  }

  function init(a) {
    api = a;
    $('stFirst').onclick = () => go(0);
    $('stPrev').onclick = () => go(idx - 1);
    $('stPlay').onclick = play;
    $('stNext').onclick = () => go(idx + 1);
    $('stLast').onclick = () => go(steps.length - 1);
    $('stScrub').oninput = (e) => go(+e.target.value);
    document.querySelectorAll('[data-speed]').forEach((b) => (b.onclick = () => {
      speed = +b.dataset.speed;
      document.querySelectorAll('[data-speed]').forEach((x) => x.setAttribute('aria-pressed', String(x === b)));
      if (timer) { stop(); play(); }
    }));
  }
  // Appelé à chaque nouveau résultat : les points ou l'algorithme ont pu changer
  function refresh() { if (!api || !active) return; if (compute()) show(); }
  function key_(e) {
    if (!active) return false;
    if (e.key === ' ') { play(); return true; }
    if (e.key === 'ArrowRight') { go(idx + 1); return true; }
    if (e.key === 'ArrowLeft') { go(idx - 1); return true; }
    if (e.key === 'Home') { go(0); return true; }
    if (e.key === 'End') { go(steps.length - 1); return true; }
    return false;
  }
  return { init, refresh, setActive, key: key_, COLORS };
})();
window.GeomSteps = GeomSteps;
