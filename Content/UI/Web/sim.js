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
  function start() {
    if (window.ue && window.ue.geom) { GeomApp.boot(window.ue.geom, true); return; }
    GeomApp.boot(createSim(), false);
  }
  // La liaison permanente d'Unreal est présente avant les scripts ; on laisse une marge par prudence
  if (window.ue && window.ue.geom) start(); else setTimeout(start, 250);

  function createSim() {
    const cv = document.getElementById('scene'), ctx = cv.getContext('2d');
    const S = {
      points: [], nextId: 1, algo: 'jarvis', tool: 'add', kernel: false,
      layers: { hull: true, edges: true, labels: true, grid: true }, overlays: {},
      result: null, hover: null, drag: null, pan: null,
      cam: { x: 0, y: 0 }, width: 4000, // largeur visible en cm, comme OrthoWidth
    };
    const stub = location.hash === '#vide';
    const later = (fn) => setTimeout(fn, 0); // comme ExecuteJavascript : asynchrone

    /* ---- « C++ » simulé ---- */
    function compute() {
      const P = S.points, n = P.length, t0 = performance.now();
      const res = { algo: S.algo, call: '', ms: 0, n, kind: S.algo === 'jarvis' || S.algo === 'graham' ? 'hull' : 'tri', kernel: S.kernel, hull: [], tris: [], edges: [], counts: { vertices: 0, edges: 0, faces: 0, invalidFaces: 0, invalidEdges: 0 } };
      res.call = { jarvis: 'ComputeJarvisMarch', graham: 'ComputeGrahamScan', incr: 'IncrementalTriangulation2D', delaunay: 'IncrementalTriangulation2D + FlipEdgesToDelaunay' }[S.algo];
      if (n >= 3 && !stub) {
        if (res.kind === 'hull') res.hull = Geo.hull(P).map((p) => [p.x, p.y]);
        else {
          const tris = S.algo === 'delaunay' ? Geo.delaunay(P).tris : Geo.triangulate(P);
          res.tris = tris.map((t) => [t[0].x, t[0].y, t[1].x, t[1].y, t[2].x, t[2].y]);
          const b = Geo.hullWithBoundary(P).length;
          res.counts = { vertices: n, edges: tris.length ? (3 * tris.length + b) / 2 : 0, faces: tris.length, invalidFaces: 0, invalidEdges: 0 };
        }
      }
      res.ms = n >= 3 ? performance.now() - t0 : 0;
      S.result = res;
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
      setoverlay(name, packed, color) {
        if (!packed) { delete S.overlays[name]; draw(); return; }
        S.overlays[name] = { color, segs: packed.split(';').map((s) => s.split(',').map(Number)) };
        draw();
      },
      frameall() { setTimeout(frame, 0); },
      recompute() { compute(); },
      log(msg) { console.log('[geom]', msg); },
    };

    /* ---- Caméra : X à droite, Y en haut, comme la caméra de AGeomTestPawn ---- */
    let W = 0, H = 0, dpr = 1;
    const panelPx = () => (W > 720 ? document.getElementById('panel').offsetWidth + 24 : 0);
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

    /* ---- Dessin (mêmes couleurs que AGeomVisualizer) ---- */
    function line(a, b, color, w) { const A = toS(a), B = toS(b); ctx.beginPath(); ctx.moveTo(A.x, A.y); ctx.lineTo(B.x, B.y); ctx.strokeStyle = color; ctx.lineWidth = w; ctx.stroke(); }
    function draw() {
      if (!W) return;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.fillStyle = '#0b0f15'; ctx.fillRect(0, 0, W, H);
      if (S.layers.grid) {
        const a = toW(0, 0), b = toW(W, H);
        for (const [step, color] of [[100, '#1a212c'], [500, '#242e3c'], [1000, '#303c4e']]) {
          if (step / px() < 8) continue;
          ctx.beginPath();
          for (let x = Math.floor(a.x / step) * step; x <= b.x; x += step) { const s = toS({ x, y: 0 }).x; ctx.moveTo(s, 0); ctx.lineTo(s, H); }
          for (let y = Math.floor(b.y / step) * step; y <= a.y; y += step) { const s = toS({ x: 0, y }).y; ctx.moveTo(0, s); ctx.lineTo(W, s); }
          ctx.strokeStyle = color; ctx.lineWidth = 1; ctx.stroke();
        }
        const o = toS({ x: 0, y: 0 });
        ctx.lineWidth = 1.5; ctx.strokeStyle = '#aa3c3c'; ctx.beginPath(); ctx.moveTo(o.x, o.y); ctx.lineTo(W, o.y); ctx.stroke();
        ctx.strokeStyle = '#469650'; ctx.beginPath(); ctx.moveTo(o.x, o.y); ctx.lineTo(o.x, 0); ctx.stroke();
      }
      const r = S.result;
      if (r && S.layers.edges) for (const t of r.tris) { const p = [{ x: t[0], y: t[1] }, { x: t[2], y: t[3] }, { x: t[4], y: t[5] }]; for (let i = 0; i < 3; i++) line(p[i], p[(i + 1) % 3], '#5fa8ff', 1.3); }
      for (const o of Object.values(S.overlays)) for (const s of o.segs) line({ x: s[0], y: s[1] }, { x: s[2], y: s[3] }, o.color, 1);
      if (r && S.layers.hull && r.hull.length > 1) { const h = r.hull.map((p) => ({ x: p[0], y: p[1] })); for (let i = 0; i < h.length; i++) line(h[i], h[(i + 1) % h.length], '#ffb547', 2.5); }
      const onHull = new Set(r ? r.hull.map((p) => p[0] + ',' + p[1]) : []);
      const labels = S.layers.labels && S.points.length <= 80;
      ctx.font = '11px Consolas, monospace'; ctx.textBaseline = 'middle';
      for (const p of S.points) {
        const s = toS(p), h = onHull.has(p.x + ',' + p.y);
        ctx.fillStyle = h ? '#ffb547' : '#eef3f9'; ctx.fillRect(s.x - (h ? 4.5 : 3.5), s.y - (h ? 4.5 : 3.5), h ? 9 : 7, h ? 9 : 7);
        if (labels) { ctx.fillStyle = '#8592a5'; ctx.fillText('P' + p.id, s.x + 8, s.y - 8); }
      }
      const ring = (p, color) => { const s = toS(p); ctx.beginPath(); ctx.arc(s.x, s.y, 12, 0, Math.PI * 2); ctx.strokeStyle = color; ctx.lineWidth = 1.5; ctx.stroke(); };
      if (S.drag) ring(S.drag, '#c58bff'); else if (S.hover) ring(S.hover, '#ffffff');
    }

    /* ---- Souris (même comportement que AGeomTestPlayerController) ---- */
    const nearest = (sx, sy) => { let best = null, bd = 14; for (const p of S.points) { const s = toS(p), d = Math.hypot(s.x - sx, s.y - sy); if (d < bd) { bd = d; best = p; } } return best; };
    const remove = (p) => { S.points = S.points.filter((q) => q !== p); S.hover = null; changed(); };
    cv.addEventListener('mousedown', (e) => {
      if (e.button === 1) { e.preventDefault(); S.pan = { x: e.clientX, y: e.clientY, cam: { ...S.cam } }; return; }
      if (e.button !== 0) return;
      const hit = nearest(e.offsetX, e.offsetY);
      if (S.tool === 'del') { if (hit) remove(hit); return; }
      if (hit) { S.drag = hit; return; }
      if (S.tool === 'add') { const w = toW(e.offsetX, e.offsetY); S.points.push({ id: S.nextId++, x: Math.round(w.x), y: Math.round(w.y) }); changed(); }
    });
    window.addEventListener('mousemove', (e) => {
      if (S.pan) { S.cam = { x: S.pan.cam.x - (e.clientX - S.pan.x) * px(), y: S.pan.cam.y + (e.clientY - S.pan.y) * px() }; draw(); return; }
      if (e.target !== cv && !S.drag) return;
      const rect = cv.getBoundingClientRect(), sx = e.clientX - rect.left, sy = e.clientY - rect.top, w = toW(sx, sy);
      window.onGeomCursor(w.x, w.y);
      if (S.drag) { S.drag.x = Math.round(w.x); S.drag.y = Math.round(w.y); changed(); return; }
      const h = nearest(sx, sy); if (h !== S.hover) { S.hover = h; draw(); }
    });
    window.addEventListener('mouseup', () => { S.drag = null; S.pan = null; draw(); });
    cv.addEventListener('contextmenu', (e) => { e.preventDefault(); const p = nearest(e.offsetX, e.offsetY); if (p) remove(p); });
    cv.addEventListener('wheel', (e) => {
      e.preventDefault();
      const c = toW(e.offsetX, e.offsetY), old = S.width;
      S.width = Math.min(200000, Math.max(200, S.width * (e.deltaY < 0 ? 1 / 1.15 : 1.15)));
      const k = S.width / old; S.cam = { x: c.x - (c.x - S.cam.x) * k, y: c.y - (c.y - S.cam.y) * k }; draw();
    }, { passive: false });
    window.addEventListener('resize', resize);
    resize(); frame();
    return api;
  }
})();
