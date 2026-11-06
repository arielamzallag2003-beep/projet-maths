/* =====================================================================
   Simulation du niveau, pour travailler sur la page dans Chrome sans Unreal.
   Elle reproduit le contrat de UGeomBridge (mêmes noms de fonctions, mêmes
   rappels) et dessine dans un canvas ce que AGeomVisualizer dessine en jeu.
   Les « résultats C++ » sont ceux de la référence JS.
   Ouvrir index.html#vide pour simuler des fonctions pas encore écrites.
   Dans Unreal ce fichier ne fait rien : window.ue.geom existe déjà.
   ===================================================================== */
'use strict';

(function () {
  const fromUnreal = new URLSearchParams(location.search).get('host') === 'ue';
  const hasBridge = () => !!(window.ue && window.ue.geom);

  // Ouverte par Unreal (?host=ue) : la liaison window.ue.geom peut arriver un peu après
  // les scripts, on l'attend. Jamais de simulation dans ce cas : une erreur visible vaut mieux.
  function waitForBridge(tries) {
    if (hasBridge()) { GeomApp.boot(window.ue.geom, true); return; }
    if (tries <= 0) { GeomApp.fail('Pont ue.geom absent : voir LogMaths_T1'); return; }
    setTimeout(() => waitForBridge(tries - 1), 100);
  }
  if (fromUnreal) { document.body.classList.add('ue'); waitForBridge(50); }        // 5 s
  else if (hasBridge()) GeomApp.boot(window.ue.geom, true);
  else setTimeout(() => (hasBridge() ? GeomApp.boot(window.ue.geom, true) : GeomApp.boot(createSim(), false)), 250);

  function createSim() {
    const cv = document.getElementById('scene'), ctx = cv.getContext('2d');
    const S = {
      points: [], nextId: 1, algo: 'jarvis', tool: 'add', kernel: false,
      layers: { hull: true, edges: true, labels: true, grid: true }, overlays: {},
      result: null, hover: null, drag: null, pan: null, seq: 0,
      cam: { x: 0, y: 0 }, width: 4000, // largeur visible en cm, comme OrthoWidth
      construction: { on: false, siteId: -1, step: 0, steps: null, skipped: 0 }, vertices: [], cursorScreen: null, vertexTip: null,
    };
    const stub = location.hash === '#vide';
    const later = (fn) => setTimeout(fn, 0); // comme ExecuteJavascript : asynchrone

    /* ---- « C++ » simulé ---- */
    function compute() {
      const P = Geo.unique(S.points), n = S.points.length, t0 = performance.now();
      const res = { algo: S.algo, call: '', ms: 0, n, seq: ++S.seq, firstId: S.points.length ? S.points[0].id : 0, kind: S.algo === 'jarvis' || S.algo === 'graham' ? 'hull' : S.algo === 'voronoi' ? 'voronoi' : 'tri', kernel: S.kernel, hull: [], tris: [], edges: [], counts: { vertices: 0, edges: 0, faces: 0, invalidFaces: 0, invalidEdges: 0 } };
      res.call = { jarvis: 'ComputeJarvisMarch', graham: 'ComputeGrahamScan', incr: 'IncrementalTriangulation2D', delaunay: 'IncrementalTriangulation2D + FlipEdgesToDelaunay', voronoi: 'ComputeVoronoiHalfPlanes' }[S.algo];
      if (P.length >= 3 && !stub) {
        if (res.kind === 'hull') res.hull = Geo.hull(P).map((p) => [p.x, p.y]);
        else if (res.kind === 'voronoi') {
          // Même format que AGeomVisualizer::ResultToJson
          const B = Geo.voronoiBounds(P), cells = Geo.voronoiCells(P, B), edges = Geo.voronoiEdgesOf(cells);
          const idOf = new Map(S.points.map((p) => [Geo.pk(p), p.id])), r2 = (v) => Math.round(v * 100) / 100;
          res.cells = cells.map((c) => [idOf.get(Geo.pk(c.site)), c.nb.includes(-1) ? 1 : 0, c.site.x, c.site.y, ...c.poly.flatMap((v) => [r2(v.x), r2(v.y)])]);
          res.vedges = edges.map((e) => [r2(e[0].x), r2(e[0].y), r2(e[1].x), r2(e[1].y), e[2].x, e[2].y, e[3].x, e[3].y]);
          res.bounds = [r2(B.minX), r2(B.minY), r2(B.maxX), r2(B.maxY)];
          const vs = [];
          for (const c of cells) for (const v of c.poly) {
            const onBox = Math.abs(v.x - B.minX) <= 0.01 || Math.abs(v.x - B.maxX) <= 0.01 || Math.abs(v.y - B.minY) <= 0.01 || Math.abs(v.y - B.maxY) <= 0.01;
            if (!onBox && !vs.some((w) => Math.hypot(v.x - w.x, v.y - w.y) <= 0.01)) vs.push(v);
          }
          res.counts = { vertices: vs.length, edges: edges.length, faces: cells.length, invalidFaces: 0, invalidEdges: 0 };
          S.vertices = vs;
        } else {
          const tris = S.algo === 'delaunay' ? Geo.delaunay(P).tris : Geo.triangulate(P);
          res.tris = tris.map((t) => [t[0].x, t[0].y, t[1].x, t[1].y, t[2].x, t[2].y]);
          const b = Geo.hullWithBoundary(P).length;
          res.counts = { vertices: P.length, edges: tris.length ? (3 * tris.length + b) / 2 : 0, faces: tris.length, invalidFaces: 0, invalidEdges: 0 };
        }
      }
      res.ms = n >= 3 ? performance.now() - t0 : 0;
      if (res.kind !== 'voronoi') S.vertices = [];
      S.result = res;
      buildSteps();
      later(() => window.onGeomResult(JSON.parse(JSON.stringify(res))));
      draw();
    }
    const sendPoints = () => later(() => window.onGeomPoints({ points: S.points.map((p) => ({ ...p })) }));
    const changed = () => { sendPoints(); compute(); };

    const api = {
      ready() { later(() => window.onGeomState({ algo: S.algo, tool: S.tool, kernel: S.kernel })); changed(); },
      setalgorithm(id) { S.algo = id; compute(); },
      setpoints(packed) {
        S.points = []; S.nextId = 1;
        for (const item of String(packed).split(';')) {
          const v = item.split(','); if (v.length !== 3) continue;
          const id = +v[0] || S.nextId; S.points.push({ id, x: +v[1], y: +v[2] }); S.nextId = Math.max(S.nextId, id + 1);
        }
        changed();
      },
      setlayer(name, on) { S.layers[name] = on; draw(); },
      settool(t) { S.tool = t; cv.style.cursor = t === 'del' ? 'not-allowed' : t === 'move' ? 'grab' : 'crosshair'; },
      setkernelmode(on) { S.kernel = on; },
      setconstruction(on, siteId, step) {
        const C = S.construction, changedSite = +siteId > 0 && +siteId !== C.siteId;
        C.on = !!on; if (+siteId > 0) C.siteId = +siteId;
        C.step = changedSite ? 0 : +step;
        buildSteps(); draw();
      },
      setoverlay(name, packed, color) {
        if (!packed) { delete S.overlays[name]; draw(); return; }
        S.overlays[name] = { color, segs: packed.split(';').map((s) => s.split(',').map(Number)) };
        draw();
      },
      frameall() { setTimeout(frame, 0); },
      recompute() { compute(); },
      togglepanel() {
        const panel = document.getElementById('panel'), reopen = document.getElementById('reopen');
        panel.hidden = !panel.hidden; reopen.hidden = !panel.hidden; resize();
      },
      log(msg) { console.log('[geom]', msg); },
    };

    /* ---- Caméra : X à droite, Y en haut, comme la caméra de AGeomTestPawn ---- */
    let W = 0, H = 0, dpr = 1;
    const panelPx = () => { const p = document.getElementById('panel'); return W > 720 && !p.hidden ? p.offsetWidth + 24 : 0; };
    const px = () => S.width / W; // cm par pixel
    const toS = (p) => ({ x: W / 2 + (p.x - S.cam.x) / px(), y: H / 2 - (p.y - S.cam.y) / px() });
    const toW = (sx, sy) => ({ x: (sx - W / 2) * px() + S.cam.x, y: -(sy - H / 2) * px() + S.cam.y });
    function resize() { dpr = Math.min(window.devicePixelRatio || 1, 2); W = cv.clientWidth; H = cv.clientHeight; cv.width = W * dpr; cv.height = H * dpr; draw(); }
    function frame() {
      let minX = -2000, maxX = 2000, minY = -1500, maxY = 1500;
      if (S.points.length > 1) { const xs = S.points.map((p) => p.x), ys = S.points.map((p) => p.y); minX = Math.min(...xs); maxX = Math.max(...xs); minY = Math.min(...ys); maxY = Math.max(...ys); }
      const sx = Math.max(400, maxX - minX) * 1.25, sy = Math.max(400, maxY - minY) * 1.25, free = Math.max(200, W - panelPx());
      S.width = Math.max(sx * W / free, sy * W / H);
      S.cam = { x: (minX + maxX) / 2 - panelPx() * 0.5 * px(), y: (minY + maxY) / 2 };
      draw();
    }

    /* ---- Dessin : mêmes formes, couleurs et tailles que AGeomVisualizer::Paint (C++) ---- */
    const hsv = (h, s, v) => { const f = (n) => { const k = (n + h / 60) % 6; return Math.round((v - v * s * Math.max(0, Math.min(k, 4 - k, 1))) * 255); }; return [f(5), f(3), f(1)]; };
    const cellRgb = (i) => hsv(150 + ((i * 137.508) % 180), 0.5, 0.95); // du cyan au rose, sans jaune ni olive (ternes sur fond sombre)
    const rgba = (c, a) => `rgba(${c[0]},${c[1]},${c[2]},${a})`;
    const HEX = { teal: [127, 227, 218], tealDeep: [79, 209, 197], white: [238, 243, 249], hull: [255, 181, 71], tri: [95, 168, 255], violet: [197, 139, 255], red: [255, 122, 89], pink: [255, 93, 122], ink: [133, 146, 165] };
    function seg(a, b, w, color, dash) { ctx.save(); ctx.lineCap = 'butt'; ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.strokeStyle = color; ctx.lineWidth = w; if (dash) ctx.setLineDash(dash); ctx.stroke(); ctx.restore(); }
    function fadeSeg(a, b, w, rgb, alpha) { // plein jusqu'à 55 %, puis s'efface vers b
      const m = { x: a.x + (b.x - a.x) * 0.55, y: a.y + (b.y - a.y) * 0.55 }, g = ctx.createLinearGradient(m.x, m.y, b.x, b.y);
      g.addColorStop(0, rgba(rgb, alpha)); g.addColorStop(1, rgba(rgb, 0));
      seg(a, m, w, rgba(rgb, alpha)); seg(m, b, w, g);
    }
    function poly(pts, fill, stroke, w, dash) { if (pts.length < 3) return; ctx.save(); ctx.beginPath(); pts.forEach((p, i) => (i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y))); ctx.closePath(); if (fill) { ctx.fillStyle = fill; ctx.fill(); } if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = w; if (dash) ctx.setLineDash(dash); ctx.stroke(); } ctx.restore(); }
    function radial(pts, c, rgb, aCenter, aRim) { // éventail depuis le site : plus dense au centre
      if (pts.length < 3) return;
      const r = Math.max(...pts.map((p) => Math.hypot(p.x - c.x, p.y - c.y))), g = ctx.createRadialGradient(c.x, c.y, 0, c.x, c.y, Math.max(r, 1));
      g.addColorStop(0, rgba(rgb, aCenter)); g.addColorStop(1, rgba(rgb, aRim)); poly(pts, g);
    }
    const disc = (c, r, color) => { ctx.beginPath(); ctx.arc(c.x, c.y, r, 0, 2 * Math.PI); ctx.fillStyle = color; ctx.fill(); };
    const ring = (c, r, w, color, dash) => { ctx.save(); ctx.beginPath(); ctx.arc(c.x, c.y, r, 0, 2 * Math.PI); ctx.strokeStyle = color; ctx.lineWidth = w; if (dash) ctx.setLineDash(dash); ctx.stroke(); ctx.restore(); };
    function infinite(p, dir, w, color, dash) { const L = Math.hypot(dir.x, dir.y) || 1, R = 2 * Math.hypot(W, H); seg({ x: p.x - dir.x / L * R, y: p.y - dir.y / L * R }, { x: p.x + dir.x / L * R, y: p.y + dir.y / L * R }, w, color, dash); }

    function draw() {
      if (!W) return;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const bg = ctx.createLinearGradient(0, 0, 0, H); bg.addColorStop(0, '#0e141d'); bg.addColorStop(1, '#080b10');
      ctx.fillStyle = bg; ctx.fillRect(0, 0, W, H);

      if (S.layers.grid) {
        const a = toW(0, 0), b = toW(W, H);
        for (const [step, alpha] of [[100, 0.035], [500, 0.06], [1000, 0.09]]) {
          if (step / px() < 8) continue;
          for (let x = Math.floor(a.x / step) * step; x <= b.x; x += step) { const sx = toS({ x, y: 0 }).x; seg({ x: sx, y: 0 }, { x: sx, y: H }, 1, `rgba(255,255,255,${alpha})`); }
          for (let y = Math.floor(b.y / step) * step; y <= a.y; y += step) { const sy = toS({ x: 0, y }).y; seg({ x: 0, y: sy }, { x: W, y: sy }, 1, `rgba(255,255,255,${alpha})`); }
        }
        const o = toS({ x: 0, y: 0 });
        seg(o, { x: W, y: o.y }, 1.2, 'rgba(255,93,93,0.45)'); seg(o, { x: o.x, y: 0 }, 1.2, 'rgba(111,220,149,0.45)');
      }

      const r = S.result, C = S.construction, labelsOn = S.layers.labels && S.points.length <= 80;
      const isVor = r && r.kind === 'voronoi' && r.cells && r.cells.length;
      const siteKey = (p) => p.x + ',' + p.y, cellOf = new Map(), hoverP = S.drag || S.hover;
      let hoverCell = -1; const neighbors = new Set();
      let dim = 1;

      if (isVor) {
        r.cells.forEach((c, i) => cellOf.set(c[2] + ',' + c[3], i));
        const cells = r.cells.map((c) => ({ site: { x: c[2], y: c[3] }, poly: Array.from({ length: (c.length - 4) / 2 }, (_, k) => ({ x: c[4 + 2 * k], y: c[5 + 2 * k] })) }));
        const B = { minX: r.bounds[0], minY: r.bounds[1], maxX: r.bounds[2], maxY: r.bounds[3] }, tolB = 1e-6 * Math.max(B.maxX - B.minX, B.maxY - B.minY) + 0.02;
        const onFrame = (p) => Math.abs(p.x - B.minX) <= tolB || Math.abs(p.x - B.maxX) <= tolB || Math.abs(p.y - B.minY) <= tolB || Math.abs(p.y - B.maxY) <= tolB;
        dim = C.on && C.steps ? 0.35 : 1;
        if (hoverP && !C.on) {
          hoverCell = cellOf.has(siteKey(hoverP)) ? cellOf.get(siteKey(hoverP)) : -1;
          if (hoverCell >= 0) for (const e of r.vedges) { const ka = e[4] + ',' + e[5], kb = e[6] + ',' + e[7], h = siteKey(cells[hoverCell].site); if (ka === h) neighbors.add(cellOf.get(kb)); else if (kb === h) neighbors.add(cellOf.get(ka)); }
        }
        // Régions : dégradé depuis le site
        cells.forEach((c, i) => {
          const hot = i === hoverCell, near = neighbors.has(i);
          radial(c.poly.map(toS), toS(c.site), cellRgb(i), (hot ? 0.46 : near ? 0.34 : 0.26) * dim, (hot ? 0.18 : near ? 0.11 : 0.07) * dim);
        });
        // Arêtes : halo puis trait ; celles qui touchent le cadre s'effacent (régions infinies)
        if (S.layers.edges) {
          for (const e of r.vedges) {
            let a = { x: e[0], y: e[1] }, b = { x: e[2], y: e[3] };
            if (onFrame(a) && !onFrame(b)) [a, b] = [b, a];
            const A = toS(a), Bs = toS(b);
            if (onFrame(b)) { fadeSeg(A, Bs, 7, HEX.tealDeep, 0.10 * dim); fadeSeg(A, Bs, 1.6, HEX.teal, 0.95 * dim); }
            else { seg(A, Bs, 7, rgba(HEX.tealDeep, 0.10 * dim)); seg(A, Bs, 1.6, rgba(HEX.teal, 0.95 * dim)); }
          }
          const fr = [{ x: B.minX, y: B.minY }, { x: B.maxX, y: B.minY }, { x: B.maxX, y: B.maxY }, { x: B.minX, y: B.maxY }].map(toS);
          for (let i = 0; i < 4; i++) seg(fr[i], fr[(i + 1) % 4], 1, 'rgba(255,255,255,0.16)', [6, 6]);
          for (const v of S.vertices) disc(toS(v), 2, rgba([191, 243, 238], 0.7 * dim));
        }
        // Survol d'un site : sa région, ses voisins (arêtes de Delaunay)
        if (hoverCell >= 0) {
          const c = cells[hoverCell];
          poly(c.poly.map(toS), null, rgba([217, 251, 247], 0.95), 2.4);
          for (const j of neighbors) seg(toS(c.site), toS(cells[j].site), 1.3, rgba(HEX.tri, 0.85), [5, 4]);
        }
        // Survol d'un sommet : son cercle vide passe par 3 sites (ou plus)
        if (!hoverP && !C.on && S.cursorScreen) {
          let best = null, bd = 12;
          for (const v of S.vertices) { const s = toS(v), d = Math.hypot(s.x - S.cursorScreen.x, s.y - S.cursorScreen.y); if (d < bd) { bd = d; best = v; } }
          if (best) {
            const P = Geo.unique(S.points), rad = Math.min(...P.map((p) => Math.hypot(p.x - best.x, p.y - best.y))), on = P.filter((p) => Math.hypot(p.x - best.x, p.y - best.y) <= rad * (1 + 1e-6) + 0.5);
            const cs = toS(best);
            ring(cs, rad / px(), 1.6, rgba(HEX.violet, 0.95));
            for (const p of on) seg(cs, toS(p), 1.1, rgba(HEX.violet, 0.7), [4, 4]);
            disc(cs, 4.5, rgba(HEX.violet, 1));
            S.vertexTip = { at: cs, text: `sommet · cercle vide · r = ${Math.round(rad)} cm · ${on.length} sites` };
          } else S.vertexTip = null;
        } else S.vertexTip = null;
        // Construction pas à pas
        if (C.on && C.steps && C.steps[C.step]) {
          const cur = C.steps[C.step], pi = C.site;
          if (cur.removed.length) poly(cur.removed.map(toS), rgba(HEX.pink, 0.18), rgba(HEX.pink, 0.6), 1.2, [5, 4]);
          poly(cur.poly.map(toS), rgba(HEX.tealDeep, 0.2), rgba(HEX.teal, 1), 2.2);
          if (cur.nb >= 0) {
            const pj = C.sites[cur.nb], m = { x: (pi.x + pj.x) / 2, y: (pi.y + pj.y) / 2 }, ms = toS(m);
            const d = { x: toS(pj).x - toS(pi).x, y: toS(pj).y - toS(pi).y };
            infinite(ms, { x: -d.y, y: d.x }, 1.6, rgba(HEX.red, cur.cut ? 0.95 : 0.45), [9, 6]);
            seg(toS(pi), toS(pj), 1, rgba(HEX.red, 0.5), [3, 4]);
            ring(toS(pj), 10, 2, rgba(HEX.red, 1));
            disc(ms, 3, rgba(HEX.red, 1));
          }
          ring(toS(pi), 12, 2, rgba(HEX.teal, 1));
        }
      }

      if (r && r.kind === 'tri' && S.layers.edges) for (const t of r.tris) {
        const p = [{ x: t[0], y: t[1] }, { x: t[2], y: t[3] }, { x: t[4], y: t[5] }].map(toS);
        poly(p, rgba(HEX.tri, 0.05));
        for (let i = 0; i < 3; i++) seg(p[i], p[(i + 1) % 3], 1.3, rgba(HEX.tri, 0.9));
      }
      for (const o of Object.values(S.overlays)) for (const s of o.segs) seg(toS({ x: s[0], y: s[1] }), toS({ x: s[2], y: s[3] }), 1.2, o.color);
      if (r && S.layers.hull && r.hull.length > 1) {
        const h = r.hull.map((p) => toS({ x: p[0], y: p[1] }));
        if (h.length > 2) poly(h, rgba(HEX.hull, 0.05));
        for (let i = 0; i < h.length; i++) { seg(h[i], h[(i + 1) % h.length], 8, rgba(HEX.hull, 0.12)); seg(h[i], h[(i + 1) % h.length], 2.4, rgba(HEX.hull, 1)); }
      }

      // Points : halo, contour sombre, corps
      const onHull = new Set(r ? r.hull.map((p) => p[0] + ',' + p[1]) : []);
      for (const p of S.points) {
        const s = toS(p), h = onHull.has(p.x + ',' + p.y), ci = cellOf.get(siteKey(p));
        const focus = !C.on || !C.steps || (C.site && p.x === C.site.x && p.y === C.site.y) || (C.steps[C.step] && C.steps[C.step].nb >= 0 && C.sites[C.steps[C.step].nb] && C.sites[C.steps[C.step].nb].x === p.x && C.sites[C.steps[C.step].nb].y === p.y);
        const a = focus ? 1 : 0.45;
        disc(s, 8, ci != null ? rgba(cellRgb(ci), 0.35 * a) : `rgba(255,255,255,${0.06 * a})`);
        disc(s, 5.8, `rgba(10,14,20,${a})`);
        disc(s, 4.2, h ? rgba(HEX.hull, a) : rgba(HEX.white, a));
      }
      if (S.drag) ring(toS(S.drag), 11, 1.5, rgba(HEX.violet, 1)); else if (S.hover) ring(toS(S.hover), 11, 1.5, 'rgba(255,255,255,0.9)');

      // Étiquettes : jamais l'une sur l'autre (le point survolé passe en premier)
      ctx.font = '11px Consolas, monospace'; ctx.textBaseline = 'top';
      if (labelsOn) {
        const placed = [], order = S.points.slice().sort((p, q) => (q === hoverP) - (p === hoverP));
        for (const p of order) {
          const s = toS(p), text = 'P' + p.id, w = ctx.measureText(text).width, box = { x: s.x + 8, y: s.y - 20, w, h: 13 };
          if (box.x > W || box.y > H || box.x + w < 0 || box.y + 13 < 0) continue;
          if (placed.some((q) => box.x < q.x + q.w + 3 && q.x < box.x + box.w + 3 && box.y < q.y + q.h && q.y < box.y + box.h)) continue;
          placed.push(box);
          ctx.fillStyle = 'rgba(0,0,0,0.85)'; ctx.fillText(text, box.x + 1, box.y + 1);
          ctx.fillStyle = p === hoverP ? '#ffffff' : 'rgba(133,146,165,0.95)'; ctx.fillText(text, box.x, box.y);
        }
      }
      if (S.vertexTip) { const t = S.vertexTip; ctx.fillStyle = 'rgba(0,0,0,0.85)'; ctx.fillText(t.text, t.at.x + 13, t.at.y + 9); ctx.fillStyle = '#e9d8ff'; ctx.fillText(t.text, t.at.x + 12, t.at.y + 8); }
      if (C.on && C.steps && C.steps[C.step] && C.steps[C.step].nb >= 0) {
        const pj = C.sites[C.steps[C.step].nb], pi = C.site, ms = toS({ x: (pi.x + pj.x) / 2, y: (pi.y + pj.y) / 2 });
        const text = `médiatrice de [P${C.siteId} P${C.ids[C.steps[C.step].nb]}]${C.steps[C.step].cut ? '' : ' · ne coupe rien'}`;
        ctx.fillStyle = 'rgba(0,0,0,0.85)'; ctx.fillText(text, ms.x + 9, ms.y + 7); ctx.fillStyle = '#ffd2c4'; ctx.fillText(text, ms.x + 8, ms.y + 6);
      }
    }

    /* ---- Construction pas à pas (comme AGeomVisualizer) ---- */
    function buildSteps() {
      const C = S.construction;
      C.steps = null;
      if (!C.on || S.algo !== 'voronoi') { sendConstruction(); return; }
      const P = Geo.unique(S.points);
      if (P.length < 3) { sendConstruction(); return; }
      let site = S.points.find((p) => p.id === C.siteId);
      if (!site) { site = S.points[0]; C.siteId = site.id; }
      const i = P.findIndex((p) => p.x === site.x && p.y === site.y), B = Geo.voronoiBounds(P), res = Geo.voronoiCellSteps(P, i, B);
      C.site = P[i]; C.sites = P; C.ids = P.map((p) => (S.points.find((q) => q.x === p.x && q.y === p.y) || {}).id);
      C.steps = res.steps; C.skipped = res.skipped;
      C.step = Math.max(0, Math.min(C.step < 0 ? res.steps.length - 1 : C.step, res.steps.length - 1));
      sendConstruction();
    }
    function sendConstruction() {
      const C = S.construction, st = C.steps && C.steps[C.step];
      const info = { on: C.on && !!C.steps, site: C.siteId, step: C.step, total: C.steps ? C.steps.length : 0, neighbor: st && st.nb >= 0 ? C.ids[st.nb] : -1, cut: !!(st && st.cut), skipped: C.skipped || 0, cutSteps: C.steps ? C.steps.map((x, k) => (x.cut ? k : -1)).filter((k) => k >= 0) : [] };
      later(() => window.onGeomConstruction && window.onGeomConstruction(info));
    }

    /* ---- Souris (même comportement que AGeomTestPlayerController) ---- */
    const nearest = (sx, sy) => { let best = null, bd = 14; for (const p of S.points) { const s = toS(p), d = Math.hypot(s.x - sx, s.y - sy); if (d < bd) { bd = d; best = p; } } return best; };
    const remove = (p) => { S.points = S.points.filter((q) => q !== p); S.hover = null; later(() => window.onGeomHover(-1)); changed(); };
    cv.addEventListener('mousedown', (e) => {
      if (e.button === 1) { e.preventDefault(); S.pan = { x: e.clientX, y: e.clientY, cam: { ...S.cam } }; return; }
      if (e.button !== 0) return;
      const hit = nearest(e.offsetX, e.offsetY);
      if (S.tool === 'del') { if (hit) remove(hit); return; }
      if (hit) { S.drag = hit; if (S.construction.on && hit.id !== S.construction.siteId) { S.construction.siteId = hit.id; S.construction.step = 0; buildSteps(); } return; }
      if (S.tool === 'add') { const w = toW(e.offsetX, e.offsetY); S.points.push({ id: S.nextId++, x: Math.round(w.x), y: Math.round(w.y) }); changed(); }
    });
    window.addEventListener('mousemove', (e) => {
      if (S.pan) { S.cam = { x: S.pan.cam.x - (e.clientX - S.pan.x) * px(), y: S.pan.cam.y + (e.clientY - S.pan.y) * px() }; draw(); return; }
      if (e.target !== cv && !S.drag) return;
      const rect = cv.getBoundingClientRect(), sx = e.clientX - rect.left, sy = e.clientY - rect.top, w = toW(sx, sy);
      window.onGeomCursor(w.x, w.y);
      if (S.drag) { S.drag.x = Math.round(w.x); S.drag.y = Math.round(w.y); S.cursorScreen = null; changed(); return; }
      S.cursorScreen = { x: sx, y: sy };
      const h = nearest(sx, sy); if (h !== S.hover) { S.hover = h; later(() => window.onGeomHover(h ? h.id : -1)); }
      draw();
    });
    window.addEventListener('mouseup', () => { S.drag = null; S.pan = null; draw(); });
    cv.addEventListener('mouseleave', () => { S.cursorScreen = null; draw(); });
    cv.addEventListener('contextmenu', (e) => { e.preventDefault(); const p = nearest(e.offsetX, e.offsetY); if (p) remove(p); });
    cv.addEventListener('wheel', (e) => {
      e.preventDefault();
      const c = toW(e.offsetX, e.offsetY), old = S.width;
      S.width = Math.min(200000, Math.max(200, S.width * (e.deltaY < 0 ? 1 / 1.15 : 1.15)));
      const k = S.width / old; S.cam = { x: c.x - (c.x - S.cam.x) * k, y: c.y - (c.y - S.cam.y) * k }; draw();
    }, { passive: false });
    document.getElementById('reopen').onclick = () => api.togglepanel();
    window.addEventListener('resize', resize);
    resize(); frame();
    return api;
  }
})();
