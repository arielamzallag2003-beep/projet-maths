/* =====================================================================
   Géométrie de référence, en JavaScript, côté page uniquement.
   Sert d'oracle pour les vérifications et pour le calque « Référence ».
   N'a aucun lien avec le C++ : les algorithmes du projet ne sont pas touchés.
   ===================================================================== */
'use strict';

const Geo = (() => {
  const orient = (a, b, c) => (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

  function inCircle(a, b, c, d) { // a, b, c en sens trigo : > 0 si d est dans le cercle
    const ax = a.x - d.x, ay = a.y - d.y, bx = b.x - d.x, by = b.y - d.y, cx = c.x - d.x, cy = c.y - d.y;
    return (ax*ax + ay*ay) * (bx*cy - cx*by) - (bx*bx + by*by) * (ax*cy - cx*ay) + (cx*cx + cy*cy) * (ax*by - bx*ay);
  }

  function circumcircle(a, b, c) {
    const d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (Math.abs(d) < 1e-9) return null;
    const a2 = a.x*a.x + a.y*a.y, b2 = b.x*b.x + b.y*b.y, c2 = c.x*c.x + c.y*c.y;
    const x = (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d;
    const y = (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d;
    return { x, y, r: Math.hypot(a.x - x, a.y - y) };
  }

  // Enveloppe stricte (sans points alignés), sens trigonométrique
  function hull(P) { return monotone(P, true); }
  // Enveloppe qui garde les points alignés sur le bord (pour Euler et la triangulation)
  function hullWithBoundary(P) { return monotone(P, false); }
  function monotone(P, strict) {
    const s = P.slice().sort((a, b) => a.x - b.x || a.y - b.y), lo = [], up = [];
    const bad = (o) => strict ? o <= 0 : o < 0;
    for (const p of s) { while (lo.length >= 2 && bad(orient(lo[lo.length-2], lo[lo.length-1], p))) lo.pop(); lo.push(p); }
    for (let i = s.length - 1; i >= 0; i--) { const p = s[i]; while (up.length >= 2 && bad(orient(up[up.length-2], up[up.length-1], p))) up.pop(); up.push(p); }
    up.pop(); lo.pop(); return lo.concat(up);
  }

  const polyArea = (H) => Math.abs(H.reduce((s, p, i) => s + (p.x * H[(i+1) % H.length].y - H[(i+1) % H.length].x * p.y), 0)) / 2;
  const triArea = (t) => Math.abs(orient(t[0], t[1], t[2])) / 2;
  const allCollinear = (P) => P.length < 3 || P.every(p => Math.abs(orient(P[0], P[1], p)) < 1e-9);

  // Triangulation incrémentale (points triés par x), comme dans le cours
  function triangulate(P) {
    const s = P.slice().sort((a, b) => a.x - b.x || a.y - b.y), tris = [];
    if (s.length < 3) return tris;
    let k = 2;
    while (k < s.length && orient(s[0], s[1], s[k]) === 0) k++;
    if (k === s.length) return tris;
    for (let i = 0; i < k - 1; i++) tris.push(orient(s[i], s[i+1], s[k]) > 0 ? [s[i], s[i+1], s[k]] : [s[i], s[k], s[i+1]]);
    for (let q = k + 1; q < s.length; q++) {
      const H = hullWithBoundary(s.slice(0, q));
      for (let i = 0; i < H.length; i++) { const a = H[i], b = H[(i+1) % H.length]; if (orient(a, b, s[q]) < 0) tris.push([a, s[q], b]); }
    }
    return tris;
  }

  const key = (a, b) => { const ka = a.x + ',' + a.y, kb = b.x + ',' + b.y; return ka < kb ? ka + '|' + kb : kb + '|' + ka; };
  function edgeMap(tris) {
    const map = new Map();
    tris.forEach((t, ti) => { for (let e = 0; e < 3; e++) { const k = key(t[e], t[(e+1) % 3]); (map.get(k) || map.set(k, []).get(k)).push([ti, e]); } });
    return map;
  }

  // Flips de Lawson avec une pile d'arêtes à tester et une adjacence mise à jour à chaque flip
  // (O(flips) au lieu de reconstruire toutes les arêtes à chaque fois)
  function delaunay(P) {
    const ccw = (a, b, c) => (orient(a, b, c) > 0 ? [a, b, c] : [a, c, b]);
    const tris = triangulate(P).map((t) => ccw(t[0], t[1], t[2]));
    const map = new Map();
    const addT = (ti) => { const t = tris[ti]; for (let e = 0; e < 3; e++) { const k = key(t[e], t[(e + 1) % 3]); (map.get(k) || map.set(k, new Set()).get(k)).add(ti); } };
    const remT = (ti) => { const t = tris[ti]; for (let e = 0; e < 3; e++) { const set = map.get(key(t[e], t[(e + 1) % 3])); if (set) set.delete(ti); } };
    tris.forEach((_, i) => addT(i));
    const stack = [...map.keys()], queued = new Set(stack);
    const push = (a, b) => { const k = key(a, b); if (!queued.has(k)) { queued.add(k); stack.push(k); } };
    let flips = 0, guard = 0;
    while (stack.length && guard++ < 500000) {
      const k = stack.pop(); queued.delete(k);
      const set = map.get(k);
      if (!set || set.size !== 2) continue;
      const [t1, t2] = [...set], A = tris[t1], C = tris[t2];
      let e1 = 0; while (e1 < 3 && key(A[e1], A[(e1 + 1) % 3]) !== k) e1++;
      if (e1 === 3) continue;
      const u = A[e1], v = A[(e1 + 1) % 3], w1 = A[(e1 + 2) % 3], w2 = C.find((p) => p !== u && p !== v);
      if (!w2 || inCircle(A[0], A[1], A[2], w2) <= 1e-6 * scaleOf(A)) continue;
      remT(t1); remT(t2);
      tris[t1] = ccw(w1, u, w2); tris[t2] = ccw(w1, w2, v);
      addT(t1); addT(t2);
      flips++;
      push(w1, u); push(u, w2); push(w2, v); push(v, w1);
    }
    return { tris, flips };
  }
  const scaleOf = (t) => { const d = Math.max(1, Math.hypot(t[1].x - t[0].x, t[1].y - t[0].y)); return d * d * d * d * 1e-6; };

  // Arêtes non conformes au critère de Delaunay dans une triangulation donnée
  function nonDelaunayEdges(tris) {
    let bad = 0;
    for (const pair of edgeMap(tris).values()) {
      if (pair.length !== 2) continue;
      const A = tris[pair[0][0]], C = tris[pair[1][0]];
      const d = C.find(p => !A.some(q => q.x === p.x && q.y === p.y));
      if (!d) continue;
      const ccw = orient(A[0], A[1], A[2]) > 0 ? A : [A[0], A[2], A[1]];
      if (inCircle(ccw[0], ccw[1], ccw[2], d) > 1e-6 * scaleOf(A)) bad++;
    }
    return bad;
  }

  // Quatre points cocycliques ou plus : la triangulation de Delaunay n'est pas unique
  function hasCocircular(tris) {
    for (const pair of edgeMap(tris).values()) {
      if (pair.length !== 2) continue;
      const A = tris[pair[0][0]], C = tris[pair[1][0]];
      const d = C.find(p => !A.some(q => q.x === p.x && q.y === p.y));
      if (!d) continue;
      const ccw = orient(A[0], A[1], A[2]) > 0 ? A : [A[0], A[2], A[1]];
      if (Math.abs(inCircle(ccw[0], ccw[1], ccw[2], d)) <= 1e-6 * scaleOf(A)) return true;
    }
    return false;
  }

  function voronoiSegments(tris) {
    const segs = [];
    for (const pair of edgeMap(tris).values()) {
      if (pair.length === 2) {
        const c1 = circumcircle(...tris[pair[0][0]]), c2 = circumcircle(...tris[pair[1][0]]);
        if (c1 && c2) segs.push([c1, c2]);
      } else if (pair.length === 1) { // arête du bord : demi-droite vers l'extérieur
        const [ti, e] = pair[0], t = tris[ti], a = t[e], b = t[(e+1) % 3], c = circumcircle(...t);
        if (!c) continue;
        const ccw = orient(t[0], t[1], t[2]) > 0;
        let dx = b.y - a.y, dy = -(b.x - a.x); if (!ccw) { dx = -dx; dy = -dy; }
        const len = Math.hypot(dx, dy) || 1;
        segs.push([c, { x: c.x + dx / len * 8000, y: c.y + dy / len * 8000 }]);
      }
    }
    return segs;
  }

  function circleSegments(tris, n = 28) {
    const segs = [];
    for (const t of tris) {
      const c = circumcircle(...t); if (!c || c.r > 50000) continue;
      for (let i = 0; i < n; i++) {
        const a0 = i / n * Math.PI * 2, a1 = (i + 1) / n * Math.PI * 2;
        segs.push([{ x: c.x + Math.cos(a0) * c.r, y: c.y + Math.sin(a0) * c.r }, { x: c.x + Math.cos(a1) * c.r, y: c.y + Math.sin(a1) * c.r }]);
      }
    }
    return segs;
  }

  /* ---- Voronoï par intersection de demi-plans : même méthode que UVoronoiDiagram (C++),
     écrite à part. P sans doublons. Renvoie [{ site, poly: [{x, y}], nb: [indice du voisin
     de l'arête poly[k] -> poly[k+1], ou -1 pour le cadre] }], chaque poly en sens trigo. ---- */
  function voronoiBounds(P) {
    const xs = P.map((p) => p.x), ys = P.map((p) => p.y);
    const minX = Math.min(...xs), maxX = Math.max(...xs), minY = Math.min(...ys), maxY = Math.max(...ys);
    const m = Math.max(0.5 * Math.max(maxX - minX, maxY - minY), 100);
    return { minX: minX - m, minY: minY - m, maxX: maxX + m, maxY: maxY + m };
  }
  function voronoiCells(P, B) {
    const scale = Math.max(B.maxX - B.minX, B.maxY - B.minY), eps = 1e-9 * scale, merge = 1e-6 * scale;
    return P.map((pi, i) => {
      let poly = [{ x: B.minX, y: B.minY, nb: -1 }, { x: B.maxX, y: B.minY, nb: -1 }, { x: B.maxX, y: B.maxY, nb: -1 }, { x: B.minX, y: B.maxY, nb: -1 }];
      P.forEach((pj, j) => {
        if (j === i) return;
        const dx = pj.x - pi.x, dy = pj.y - pi.y, L = Math.hypot(dx, dy); if (!L) return;
        const nx = dx / L, ny = dy / L, mx = (pi.x + pj.x) / 2, my = (pi.y + pj.y) / 2;
        const f = (v) => nx * (v.x - mx) + ny * (v.y - my), out = [];
        poly.forEach((a, k) => {
          const b = poly[(k + 1) % poly.length], fa = f(a), fb = f(b), ina = fa <= eps, inb = fb <= eps;
          if (ina) out.push(a);
          if (ina !== inb) { const t = Math.min(1, Math.max(0, fa / (fa - fb))); out.push({ x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t, nb: ina ? j : a.nb }); }
        });
        poly = out;
        for (let k = 0; poly.length > 3 && k < poly.length;) { const q = poly[(k + 1) % poly.length]; if (Math.hypot(poly[k].x - q.x, poly[k].y - q.y) <= merge) poly.splice(k, 1); else k++; }
      });
      return { site: pi, poly: poly.map((v) => ({ x: v.x, y: v.y })), nb: poly.map((v) => v.nb) };
    });
  }
  // Arêtes d'un ensemble de régions : [[a, b, siteA, siteB]], une par paire de sites
  function voronoiEdgesOf(cells) {
    const seen = new Map();
    cells.forEach((c, i) => c.nb.forEach((j, k) => {
      if (j < 0) return;
      const a = c.poly[k], b = c.poly[(k + 1) % c.poly.length], kk = key(c.site, cells[j].site), len = Math.hypot(a.x - b.x, a.y - b.y);
      const old = seen.get(kk); if (!old || len > old.len) seen.set(kk, { seg: [a, b, c.site, cells[j].site], len });
    }));
    return [...seen.values()].map((v) => v.seg);
  }

  // Générateur pseudo-aléatoire reproductible (mulberry32) : même graine, même nuage
  function rng(seed) {
    let a = seed >>> 0;
    return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }

  // Nuages types. kind, n, seed -> [{x, y}] (sans id)
  function cloud(kind, n, seed) {
    const R = rng(seed), out = [];
    const gauss = () => { let u = 0, v = 0; while (!u) u = R(); while (!v) v = R(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); };
    const add = (x, y) => out.push({ x: Math.round(x), y: Math.round(y) });
    switch (kind) {
      case 'disc': for (let i = 0; i < n; i++) { const a = R() * 2 * Math.PI, r = Math.sqrt(R()) * 1300; add(Math.cos(a) * r * 1.4, Math.sin(a) * r * 0.9); } break;
      case 'gauss': for (let i = 0; i < n; i++) add(gauss() * 620, gauss() * 420); break;
      case 'clusters': { const C = [[-900, 400], [800, 500], [0, -600]]; for (let i = 0; i < n; i++) { const [cx, cy] = C[i % 3]; add(cx + gauss() * 230, cy + gauss() * 200); } break; }
      case 'circle': { const o = R() * Math.PI; for (let i = 0; i < n; i++) { const a = o + (i / n) * 2 * Math.PI; add(Math.cos(a) * 1200, Math.sin(a) * 1200); } break; }
      case 'grid': { const c = Math.max(2, Math.round(Math.sqrt(n))); for (let i = 0; i < c; i++) for (let j = 0; j < c; j++) add((i - (c - 1) / 2) * 2600 / c, (j - (c - 1) / 2) * 2000 / c); break; }
      case 'line': { const m = Math.max(2, n - 1); for (let i = 0; i < m; i++) add(-1500 + (i * 3000) / (m - 1), -300 + (i * 600) / (m - 1)); add(200, 700); break; }
      // Cas limites : petits, précis, ceux qui cassent les implémentations naïves
      case 'three': add(-800, -500); add(900, -300); add(100, 800); break;
      case 'square': add(-800, -800); add(800, -800); add(800, 800); add(-800, 800); add(0, 0); break;
      case 'collinear': for (let i = 0; i < Math.max(3, Math.min(n, 12)); i++) add(-1200 + i * 200, -400 + i * 70); break;
      case 'duplicates': for (let i = 0; i < Math.max(4, Math.min(n, 20)); i++) { const a = R() * 2 * Math.PI, r = Math.sqrt(R()) * 1000; add(Math.cos(a) * r, Math.sin(a) * r); } out.push({ ...out[0] }, { ...out[1] }); break;
      case 'vertical': for (let i = 0; i < 6; i++) add(-500, -1000 + i * 400); for (let i = 0; i < 6; i++) add(500, -1000 + i * 400); add(0, 0); break;
      case 'tiny': for (let i = 0; i < Math.max(4, Math.min(n, 30)); i++) out.push({ x: Math.round((R() * 6 - 3) * 100) / 100, y: Math.round((R() * 6 - 3) * 100) / 100 }); break;
    }
    return out;
  }

  // Écarts entre une sortie et la référence (pour le calque « Différences »)
  const pk = (p) => (+p.x).toFixed(2) + ',' + (+p.y).toFixed(2);
  function hullDiff(out, ref) {
    const so = new Set(out.map(pk)), sr = new Set(ref.map(pk));
    return { missing: ref.filter((p) => !so.has(pk(p))), extra: out.filter((p) => !sr.has(pk(p))) };
  }
  function trisDiff(out, ref) {
    const tk = (t) => t.map(pk).sort().join(' ');
    const so = new Set(out.map(tk)), sr = new Set(ref.map(tk));
    return { missing: ref.filter((t) => !so.has(tk(t))), extra: out.filter((t) => !sr.has(tk(t))) };
  }

  // Positions distinctes (les doublons exacts ne comptent qu'une fois)
  function unique(P) { const seen = new Set(); return P.filter((p) => { const k = pk(p); if (seen.has(k)) return false; seen.add(k); return true; }); }

  return { rng, cloud, unique, hullDiff, trisDiff, pk, orient, inCircle, circumcircle, hull, hullWithBoundary, polyArea, triArea, allCollinear, triangulate, delaunay, nonDelaunayEdges, hasCocircular, voronoiSegments, circleSegments, key, voronoiBounds, voronoiCells, voronoiEdgesOf };
})();

/* ---------- Vérifications d'un résultat renvoyé par le C++ ---------- */
function runChecks(res, rawPoints, refDelaunay) {
  // Les doublons exacts ne comptent qu'une fois pour les formules (Euler, nombre de sommets...)
  const points = Geo.unique(rawPoints);
  const out = [], n = points.length;
  if (!res || n < 3) return out;
  const add = (ok, label, value, hint) => out.push({ ok, label, value, hint });
  const near = (a, b) => Math.abs(a.x - b.x) < 0.01 && Math.abs(a.y - b.y) < 0.01;
  const H = Geo.hull(points), b = Geo.hullWithBoundary(points).length, collinear = Geo.allCollinear(points);

  const nonFinite = (res.counts && res.counts.nonFinite) || 0;
  if (rawPoints.length !== n) add(null, 'Doublons dans l\'entrée', `${rawPoints.length - n} ignoré${rawPoints.length - n > 1 ? 's' : ''}`, 'Les tests comptent chaque position une seule fois ; la fonction doit supporter les doublons sans planter.');
  if (nonFinite) add(false, 'Coordonnées valides (pas de NaN)', `${nonFinite} invalide${nonFinite > 1 ? 's' : ''}`, 'Une division par zéro ou un pointeur non initialisé a produit NaN ou l\'infini ; remplacé par 0 pour l\'affichage.');

  if (res.kind === 'voronoi') { checkVoronoi(res, points, add, refDelaunay); return out; }

  if (res.kind === 'hull') {
    const hull = res.hull;
    add(hull.length >= 3 || collinear, 'La fonction renvoie un polygone', `${hull.length} sommet${hull.length > 1 ? 's' : ''}`,
        hull.length === 0 ? 'Tableau vide : la fonction est sans doute encore à écrire.' : hull.length < 3 ? 'Moins de 3 sommets pour des points non alignés.' : '');
    if (hull.length >= 3) {
      const keys = hull.map((p) => Geo.pk(p)), dup = keys.length - new Set(keys).size;
      add(dup === 0, 'Aucun sommet en double', dup === 0 ? 'oui' : `${dup} doublon${dup > 1 ? 's' : ''}`, dup ? 'Souvent : la boucle ne s\'arrête pas en revenant au point de départ.' : '');
      const fromInput = hull.every(p => points.some(q => near(p, q)));
      add(fromInput, 'Sommets pris parmi les points d\'entrée', fromInput ? 'oui' : 'non', fromInput ? '' : 'Un sommet ne correspond à aucun point du nuage.');
      let neg = 0, zero = 0;
      hull.forEach((p, i) => { const o = Geo.orient(p, hull[(i+1) % hull.length], hull[(i+2) % hull.length]); if (o < -1e-6) neg++; else if (Math.abs(o) <= 1e-6) zero++; });
      const allCw = hull.every((p, i) => Geo.orient(p, hull[(i+1) % hull.length], hull[(i+2) % hull.length]) < 0);
      add(neg === 0 && zero === 0, 'Convexe, sens trigonométrique', neg === 0 && zero === 0 ? 'oui' : allCw ? 'sens horaire' : `${neg + zero} angle${neg + zero > 1 ? 's' : ''} faux`,
          allCw ? 'Le polygone est convexe mais parcouru dans le sens horaire.' : zero ? 'Des sommets alignés sont restés dans l\'enveloppe.' : '');
      const outside = points.filter(p => !hull.every((a, i) => Geo.orient(a, hull[(i+1) % hull.length], p) >= -1e-6) && !allCw).length;
      add(outside === 0, 'Contient tous les points', outside === 0 ? `${n}/${n}` : `${outside} dehors`, '');
      add(hull.length === H.length, 'Nombre de sommets (référence JS)', `${hull.length} / ${H.length}`, '');
    }
  } else {
    const T = res.tris, c = res.counts || {};
    add(T.length > 0 || collinear, 'La fonction renvoie des faces', `${T.length} face${T.length > 1 ? 's' : ''}`,
        T.length === 0 ? (c.edges ? 'Des arêtes mais aucune face lisible.' : 'Aucune face : la fonction est sans doute encore à écrire.') : '');
    const invalid = (c.invalidFaces || 0) + (c.invalidEdges || 0);
    if (c.faces || c.edges) add(invalid === 0, 'Pointeurs du graphe d\'incidence valides', invalid === 0 ? 'oui' : `${invalid} invalide${invalid > 1 ? 's' : ''}`,
        invalid ? 'Un pointeur sort du tableau Vertices : attention aux TArray qui se réallouent pendant un Add.' : '');
    if (T.length) {
      const degenerate = T.filter(t => Geo.triArea(t) < 1e-6).length;
      add(degenerate === 0, 'Aucun triangle plat', degenerate === 0 ? 'oui' : `${degenerate} plat${degenerate > 1 ? 's' : ''}`, '');
      add(T.length === 2 * n - 2 - b, 'Euler : T = 2n − 2 − b', `${T.length} = ${2 * n - 2 - b}`, 'b = nombre de points sur le bord de l\'enveloppe.');
      const sum = T.reduce((s, t) => s + Geo.triArea(t), 0), area = Geo.polyArea(H);
      const ratio = area ? sum / area : 0;
      add(Math.abs(ratio - 1) < 1e-6, 'Les triangles pavent l\'enveloppe', `${(ratio * 100).toFixed(1).replace('.', ',')} %`,
          ratio > 1 + 1e-6 ? 'Plus de 100 % : des triangles se chevauchent.' : ratio < 1 - 1e-6 ? 'Moins de 100 % : il manque des triangles.' : '');
      if (c.edges) { const E = (3 * T.length + b) / 2; add(c.edges === E, 'Nombre d\'arêtes : (3T + b) / 2', `${c.edges} / ${E}`, ''); }
      if (res.algo === 'delaunay') {
        const bad = Geo.nonDelaunayEdges(T);
        add(bad === 0, 'Critère de Delaunay (cercles vides)', bad === 0 ? 'respecté' : `${bad} arête${bad > 1 ? 's' : ''}`, '');
        if (!Geo.hasCocircular(T)) {
          const ref = refDelaunay ? refDelaunay() : Geo.delaunay(points).tris, kk = t => t.map(p => p.x + ',' + p.y).sort().join(' ');
          const set = new Set(ref.map(kk)), same = T.length === ref.length && T.every(t => set.has(kk(t)));
          add(same, 'Identique à la référence JS', same ? 'oui' : 'non', '');
        } else add(null, 'Identique à la référence JS', 'n/a', 'Points cocycliques : plusieurs triangulations de Delaunay sont correctes.');
      }
    }
  }
  return out;
}

/* ---------- Vérifications d'un diagramme de Voronoï renvoyé par le C++ ----------
   Les tests 1 à 5 vérifient la définition elle-même (site le plus proche), sans référence.
   Les suivants comparent avec la référence JS et avec la dualité de Delaunay. */
function checkVoronoi(res, points, add, refDelaunay) {
  const cells = res.cells || [], n = points.length, B = res.bounds;
  add(cells.length > 0, 'La fonction renvoie des régions', `${cells.length} région${cells.length > 1 ? 's' : ''}`,
      cells.length ? '' : 'Aucune région : la fonction est sans doute encore à écrire.');
  if (!cells.length || !B) return;
  const scale = Math.max(B.maxX - B.minX, B.maxY - B.minY), tol = 0.02 + 1e-6 * scale; // JSON arrondi à 0,01 cm
  const bySite = new Map(points.map((p, i) => [Geo.pk(p), i]));

  const unknown = cells.filter((c) => !bySite.has(Geo.pk(c.site))).length;
  const sites = new Set(cells.map((c) => Geo.pk(c.site)));
  add(cells.length === n && sites.size === n && !unknown, 'Une région par site', `${cells.length} / ${n}`,
      unknown ? `${unknown} région(s) pour un site absent de l'entrée.` : sites.size < cells.length ? 'Deux régions pour le même site.' : '');

  let notConvex = 0, siteOutside = 0, wrongNearest = 0, area = 0;
  const nearestDist = (q) => { let best = Infinity; for (const p of points) best = Math.min(best, Math.hypot(q.x - p.x, q.y - p.y)); return best; };
  for (const c of cells) {
    const P = c.poly, m = P.length;
    if (m < 3) { notConvex++; continue; }
    let signed = 0, convex = true;
    for (let k = 0; k < m; k++) {
      const a = P[k], b = P[(k + 1) % m], d = P[(k + 2) % m];
      signed += a.x * b.y - b.x * a.y;
      if (Geo.orient(a, b, d) < -tol * Math.hypot(b.x - a.x, b.y - a.y)) convex = false;
    }
    if (!convex || signed <= 0) notConvex++;
    area += Math.abs(signed) / 2;
    if (!P.every((a, k) => Geo.orient(a, P[(k + 1) % m], c.site) >= -tol * Math.hypot(P[(k + 1) % m].x - a.x, P[(k + 1) % m].y - a.y))) siteOutside++;
    // Définition : tout point de R_i a P_i comme site le plus proche. Testé sur les sommets
    // (à égalité près) et sur le centre de gravité (strictement)
    const own = (q) => Math.hypot(q.x - c.site.x, q.y - c.site.y);
    const g = { x: P.reduce((s, v) => s + v.x, 0) / m, y: P.reduce((s, v) => s + v.y, 0) / m };
    if (P.some((v) => own(v) - nearestDist(v) > tol) || own(g) - nearestDist(g) > tol) wrongNearest++;
  }
  add(notConvex === 0, 'Régions convexes, sens trigonométrique', notConvex ? `${notConvex} fausse${notConvex > 1 ? 's' : ''}` : 'oui',
      notConvex ? 'Une intersection de demi-plans est toujours convexe : erreur de découpe ou d\'orientation.' : '');
  add(siteOutside === 0, 'Chaque site est dans sa région', siteOutside ? `${siteOutside} dehors` : 'oui', siteOutside ? 'Souvent : demi-plan pris du mauvais côté de la médiatrice.' : '');
  add(wrongNearest === 0, 'Le site le plus proche est le bon', wrongNearest ? `${wrongNearest} région${wrongNearest > 1 ? 's' : ''}` : 'oui',
      wrongNearest ? 'Un point de la région est plus près d\'un autre site : un demi-plan manque.' : 'Vérifié sur chaque sommet et chaque centre de gravité.');
  const box = (B.maxX - B.minX) * (B.maxY - B.minY), ratio = box ? area / box : 0;
  add(Math.abs(ratio - 1) < 1e-4, 'Les régions pavent le cadre', `${(ratio * 100).toFixed(2).replace('.', ',')} %`,
      ratio > 1 + 1e-4 ? 'Plus de 100 % : des régions se chevauchent.' : ratio < 1 - 1e-4 ? 'Moins de 100 % : il manque des morceaux.' : '');

  // Référence JS : mêmes paires de voisins (mêmes arêtes)
  const pairKey = (e) => Geo.key(e[2], e[3]);
  const out = new Set((res.vedges || []).map(pairKey));
  const refEdges = Geo.voronoiEdgesOf(Geo.voronoiCells(points, B)), refSet = new Set(refEdges.map(pairKey));
  const missing = [...refSet].filter((k) => !out.has(k)).length, extra = [...out].filter((k) => !refSet.has(k)).length;
  add(missing === 0 && extra === 0, 'Mêmes arêtes que la référence JS', missing || extra ? `${missing} manquante${missing > 1 ? 's' : ''}, ${extra} en trop` : `${out.size} / ${refSet.size}`, '');

  // Dualité avec Delaunay (cours 5) : sommets <-> triangles, arêtes <-> arêtes. À titre indicatif :
  // un sommet hors du cadre ou quatre sites presque cocycliques changent légitimement les nombres.
  if (Geo.allCollinear(points)) {
    add(res.counts.vertices === 0, 'Sites alignés : aucun sommet', `${res.counts.vertices}`, 'Le diagramme est fait de droites parallèles.');
    return;
  }
  const T = refDelaunay ? refDelaunay() : Geo.delaunay(points).tris, b = Geo.hullWithBoundary(points).length;
  // Chaque triangle donne un sommet : le centre de son cercle circonscrit, s'il tombe dans le cadre
  const inside = T.map((t) => Geo.circumcircle(...t)).filter((c) => c && c.x > B.minX && c.x < B.maxX && c.y > B.minY && c.y < B.maxY).length;
  const cocircular = Geo.hasCocircular(T), outside = T.length - inside;
  const v = res.counts.vertices, e = res.counts.edges;
  add(v === inside ? true : cocircular ? null : false, 'Dualité : un sommet par triangle de Delaunay', `${v} / ${inside}`,
      cocircular ? 'Sites cocycliques : plusieurs triangles ont le même centre, donc moins de sommets.'
        : `${T.length} triangles (2n − 2 − b = ${2 * n - 2 - b})${outside ? `, dont ${outside} avec un centre hors du cadre` : ''}.`);
  if (!outside && !cocircular) add(e === 3 * n - 3 - b, 'Dualité : une arête par arête de Delaunay', `${e} / ${3 * n - 3 - b}`, '3n − 3 − b');
}
