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

  // Flips de Lawson jusqu'à ce que toutes les arêtes soient conformes
  function delaunay(P) {
    const tris = triangulate(P).map(t => t.slice());
    let flips = 0, changed = true, guard = 0;
    while (changed && guard++ < 20000) {
      changed = false;
      for (const pair of edgeMap(tris).values()) {
        if (pair.length !== 2) continue;
        const [[t1, e1], [t2, e2]] = pair, A = tris[t1], C = tris[t2];
        const u = A[e1], v = A[(e1+1) % 3], w1 = A[(e1+2) % 3], w2 = C[(e2+2) % 3];
        if (inCircle(A[0], A[1], A[2], w2) > 1e-6 * scaleOf(A)) {
          tris[t1] = [w1, u, w2]; tris[t2] = [w1, w2, v];
          for (const ti of [t1, t2]) { const t = tris[ti]; if (orient(t[0], t[1], t[2]) < 0) [t[1], t[2]] = [t[2], t[1]]; }
          flips++; changed = true; break;
        }
      }
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

  return { orient, inCircle, circumcircle, hull, hullWithBoundary, polyArea, triArea, allCollinear, triangulate, delaunay, nonDelaunayEdges, hasCocircular, voronoiSegments, circleSegments, key };
})();

/* ---------- Vérifications d'un résultat renvoyé par le C++ ---------- */
function runChecks(res, points) {
  const out = [], n = points.length;
  if (!res || n < 3) return out;
  const add = (ok, label, value, hint) => out.push({ ok, label, value, hint });
  const near = (a, b) => Math.abs(a.x - b.x) < 0.5 && Math.abs(a.y - b.y) < 0.5;
  const H = Geo.hull(points), b = Geo.hullWithBoundary(points).length, collinear = Geo.allCollinear(points);

  if (res.kind === 'hull') {
    const hull = res.hull;
    add(hull.length >= 3 || collinear, 'La fonction renvoie un polygone', `${hull.length} sommet${hull.length > 1 ? 's' : ''}`,
        hull.length === 0 ? 'Tableau vide : la fonction est sans doute encore à écrire.' : hull.length < 3 ? 'Moins de 3 sommets pour des points non alignés.' : '');
    if (hull.length >= 3) {
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
          const ref = Geo.delaunay(points).tris, kk = t => t.map(p => p.x + ',' + p.y).sort().join(' ');
          const set = new Set(ref.map(kk)), same = T.length === ref.length && T.every(t => set.has(kk(t)));
          add(same, 'Identique à la référence JS', same ? 'oui' : 'non', '');
        } else add(null, 'Identique à la référence JS', 'n/a', 'Points cocycliques : plusieurs triangulations de Delaunay sont correctes.');
      }
    }
  }
  return out;
}
