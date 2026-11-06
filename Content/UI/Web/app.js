/* =====================================================================
   Logique du panneau.
   Parle à « geom » : window.ue.geom dans Unreal (UGeomBridge), ou la
   simulation de sim.js dans un navigateur. Interface commune :
     ready() setalgorithm(id) setpoints(packed) setlayer(name, on) settool(t)
     setkernelmode(on) setoverlay(name, packed, color) frameall() recompute()
     togglepanel() log(msg)
   Rappels : window.onGeomState / onGeomPoints / onGeomResult / onGeomCursor /
   onGeomHover / onGeomShortcut.
   ===================================================================== */
'use strict';

const $ = (id) => document.getElementById(id);
const ALGOS = ['jarvis', 'graham', 'incr', 'delaunay', 'voronoi'];
const CALLS = {
  jarvis: 'ComputeJarvisMarch',
  graham: 'ComputeGrahamScan',
  incr: 'IncrementalTriangulation2D',
  delaunay: 'IncrementalTriangulation2D + FlipEdgesToDelaunay',
  voronoi: 'ComputeVoronoiHalfPlanes',
};
const SHORT = { jarvis: 'Jarvis', graham: 'Graham', incr: 'Incr.', delaunay: 'Delaunay', voronoi: 'Voronoï' };
const CURVE_COLORS = { jarvis: '#ffb547', graham: '#ff7a59', incr: '#5fa8ff', delaunay: '#c58bff', voronoi: '#4fd1c5' };
const OVERLAY_COLORS = { reference: '#7a8597', circles: '#7d5aa8', voronoi: '#4fd1c5', dual: '#5fa8ff', diffMissing: '#ff5d7a', diffExtra: '#c58bff' };
const GEN_NAMES = {
  disc: 'disque', gauss: 'gaussien', clusters: 'amas', circle: 'cercle', grid: 'grille', line: 'alignés',
  three: '3 points', square: 'carré + centre', collinear: 'colinéaires', duplicates: 'doublons', vertical: 'verticales', tiny: 'minuscule',
};

const state = {
  geom: null, isUE: false,
  algo: 'jarvis', tool: 'add', kernel: false,
  points: [], result: null, checks: [], hoverId: -1,
  overlays: { diff: false, reference: false, circles: false, voronoi: false, dual: false },
  layers: { hull: true, edges: true, labels: true, grid: true },
  seeded: false, bench: null,
  build: { on: false, info: null, timer: 0 },
};

/* ---------- Appels vers le C++ : jamais d'exception qui casse la page ---------- */
function G(fn, ...args) {
  const g = state.geom;
  if (!g || typeof g[fn] !== 'function') return;
  try { const r = g[fn](...args); if (r && typeof r.catch === 'function') r.catch((e) => report(e, fn)); }
  catch (e) { report(e, fn); }
}
function report(err, where) {
  const msg = `Erreur JS${where ? ` (${where})` : ''} : ${err && err.message ? err.message : err}`;
  note(msg);
  if (state.geom && typeof state.geom.log === 'function' && where !== 'log') { try { state.geom.log(msg); } catch (_) { /* rien */ } }
}
window.addEventListener('error', (e) => report(e.error || e.message));
window.addEventListener('unhandledrejection', (e) => report(e.reason));

/* ---------- Réglages mémorisés (localStorage peut être absent : tout est protégé) ---------- */
const STORE_KEY = 'geomPanel.v1';
function loadPrefs() { try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (_) { return {}; } }
function savePrefs() {
  try {
    localStorage.setItem(STORE_KEY, JSON.stringify({
      algo: state.algo, layers: state.layers, overlays: state.overlays,
      tab: document.querySelector('[role=tab][aria-selected=true]')?.id, seed: +$('seed').value, count: +$('count').value,
    }));
  } catch (_) { /* rien */ }
}

/* ---------- Historique (annuler / rétablir), construit à partir des points reçus ---------- */
const history = { undo: [], redo: [], current: '', applying: false, lastPush: 0 };
const pack = (pts) => pts.map((p) => `${p.id},${+(+p.x).toFixed(2)},${+(+p.y).toFixed(2)}`).join(';');
function recordPoints(packed) {
  if (packed === history.current) { history.applying = false; return; }
  if (!history.applying) {
    const now = performance.now();
    // Un glisser-déposer envoie beaucoup d'états : regroupés en une seule étape
    if (now - history.lastPush > 600) { history.undo.push(history.current); if (history.undo.length > 100) history.undo.shift(); }
    history.lastPush = now;
    history.redo.length = 0;
  }
  history.applying = false;
  history.current = packed;
  syncUndo();
}
function syncUndo() { $('undo').disabled = !history.undo.length || !!state.bench; $('redo').disabled = !history.redo.length || !!state.bench; }
function undo() { if (state.bench || !history.undo.length) return; history.redo.push(history.current); history.applying = true; G('setpoints', history.undo.pop()); note('Annulé'); }
function redo() { if (state.bench || !history.redo.length) return; history.undo.push(history.current); history.applying = true; G('setpoints', history.redo.pop()); note('Rétabli'); }

/* ---------- Messages ---------- */
let noteTimer = 0, lastNote = 'Prêt';
function note(text) {
  lastNote = text;
  const el = $('status'); el.textContent = text; el.classList.add('flash');
  clearTimeout(noteTimer); noteTimer = setTimeout(() => el.classList.remove('flash'), 1600);
}
const fmt = (v, d = 2) => Number(v).toFixed(d).replace('.', ',');
const esc = (s) => String(s).replace(/[&<>"]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));

/* ---------- Référence JS, mise en cache par jeu de points ---------- */
const refCache = { key: '', hull: null, tri: null, del: null, vor: null, vorBounds: '' };
function ref(kind) {
  const key = history.current;
  if (refCache.key !== key) { refCache.key = key; refCache.hull = refCache.tri = refCache.del = refCache.vor = null; }
  const P = Geo.unique(state.points);
  if (kind === 'hull') return refCache.hull || (refCache.hull = Geo.hull(P));
  if (kind === 'tri') return refCache.tri || (refCache.tri = Geo.triangulate(P));
  if (kind === 'vor') {
    // Même cadre que le C++ quand il l'a renvoyé, sinon le cadre automatique
    const B = (state.result && state.result.bounds) || Geo.voronoiBounds(P), bk = JSON.stringify(B);
    if (!refCache.vor || refCache.vorBounds !== bk) { refCache.vorBounds = bk; refCache.vor = Geo.voronoiEdgesOf(Geo.voronoiCells(P, B)); }
    return refCache.vor;
  }
  return refCache.del || (refCache.del = Geo.delaunay(P).tris);
}

/* ---------- Algorithme, outil ---------- */
const familyOf = (id) => (id === 'jarvis' || id === 'graham' ? 'hull' : id === 'voronoi' ? 'voronoi' : 'tri');
function showAlgo(id) {
  if (!ALGOS.includes(id)) return;
  state.algo = id;
  document.querySelectorAll('.seg button').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.algo === id)));
  const panel = $('panel');
  panel.dataset.algo = id;
  panel.dataset.family = familyOf(id);
  $('kernel').disabled = id !== 'delaunay';
  $('kernelHint').textContent = id === 'delaunay'
    ? 'Ajouts et suppressions passent par DelaunayKernel_AddPoint / RemovePoint au lieu de tout recalculer.'
    : 'Disponible avec Delaunay uniquement.';
}
function selectAlgo(id) { if (state.bench) return; if (id !== 'voronoi' && state.build.on) setBuild(false); showAlgo(id); G('setalgorithm', id); note(`Algorithme : ${CALLS[id]}`); savePrefs(); }
function showTool(t) { state.tool = t; document.querySelectorAll('.tools .btn').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.tool === t))); }
function selectTool(t) { showTool(t); G('settool', t); }

/* ---------- Nuages ---------- */
function withIds(pts, base = 1) { return pts.map((p, i) => ({ id: base + i, x: p.x, y: p.y })); }
function generate(kind) {
  if (state.bench) return;
  const n = +$('count').value, seed = +$('seed').value || 0;
  const pts = withIds(Geo.cloud(kind, n, seed));
  G('setpoints', pack(pts));
  G('frameall');
  note(`Nuage ${GEN_NAMES[kind]} · ${pts.length} points · graine ${seed}`);
  savePrefs();
}

/* ---------- Export / import ---------- */
function exportScene() {
  $('sceneText').value = state.points.map((p) => `${p.x} ${p.y}`).join('\n');
  $('sceneText').select();
  note(`${state.points.length} points exportés : copiez le texte (Ctrl C)`);
}
function importScene() {
  if (state.bench) return;
  const text = $('sceneText').value.trim();
  let pts = [];
  try {
    if (text.startsWith('[') || text.startsWith('{')) {
      const j = JSON.parse(text), arr = Array.isArray(j) ? j : j.points || [];
      pts = arr.map((p) => (Array.isArray(p) ? { x: +p[0], y: +p[1] } : { x: +p.x, y: +p.y }));
    } else {
      pts = text.split(/\r?\n/).map((l) => l.trim().split(/[\s,;]+/).map(Number)).filter((v) => v.length >= 2).map((v) => ({ x: v[0], y: v[1] }));
    }
  } catch (e) { note('Import impossible : JSON invalide'); return; }
  pts = pts.filter((p) => Number.isFinite(p.x) && Number.isFinite(p.y) && Math.abs(p.x) < 1e6 && Math.abs(p.y) < 1e6);
  if (!pts.length) { note('Import : aucun point lisible (une ligne « x y » par point)'); return; }
  G('setpoints', pack(withIds(pts)));
  G('frameall');
  note(`${pts.length} points importés`);
}

/* ---------- Calques calculés par la page ---------- */
const packSegs = (segs) => segs.map(([a, b]) => `${a.x.toFixed(1)},${a.y.toFixed(1)},${b.x.toFixed(1)},${b.y.toFixed(1)}`).join(';');
function uniqueEdges(tris) {
  const seen = new Set(), out = [];
  for (const t of tris) for (let e = 0; e < 3; e++) { const a = t[e], b = t[(e + 1) % 3], k = Geo.key(a, b); if (!seen.has(k)) { seen.add(k); out.push([a, b]); } }
  return out;
}
function rings(pts, r) {
  const segs = [];
  for (const c of pts) for (let i = 0; i < 16; i++) {
    const a0 = (i / 16) * 2 * Math.PI, a1 = ((i + 1) / 16) * 2 * Math.PI;
    segs.push([{ x: c.x + Math.cos(a0) * r, y: c.y + Math.sin(a0) * r }, { x: c.x + Math.cos(a1) * r, y: c.y + Math.sin(a1) * r }]);
  }
  return segs;
}
let sentOverlays = {};
function setOverlay(name, segs) {
  const packed = packSegs(segs);
  if (sentOverlays[name] === packed) return; // rien de nouveau : on n'envoie pas
  sentOverlays[name] = packed;
  G('setoverlay', name, packed, OVERLAY_COLORS[name]);
}
function pushOverlays() {
  if (state.bench) return;
  const r = state.result, P = state.points, ok = P.length >= 3;
  const isHull = state.algo === 'jarvis' || state.algo === 'graham', isVor = state.algo === 'voronoi';

  let refSegs = [];
  if (state.overlays.reference && ok) {
    if (isHull) { const H = ref('hull'); refSegs = H.map((p, i) => [p, H[(i + 1) % H.length]]); }
    else if (isVor) refSegs = ref('vor').map((e) => [e[0], e[1]]);
    else refSegs = uniqueEdges(state.algo === 'delaunay' ? ref('del') : ref('tri'));
  }
  setOverlay('reference', refSegs);
  // Pour Voronoï, cercles et dual viennent de la triangulation de Delaunay de référence
  const tris = isVor ? (ok ? ref('del') : []) : r ? r.tris : [];
  setOverlay('circles', state.overlays.circles && tris.length ? Geo.circleSegments(tris) : []);
  setOverlay('voronoi', state.overlays.voronoi && !isVor && tris.length ? Geo.voronoiSegments(tris) : []);
  setOverlay('dual', state.overlays.dual && isVor && tris.length ? uniqueEdges(tris) : []);

  let missing = [], extra = [];
  if (state.overlays.diff && ok && r) {
    const xs = P.map((p) => p.x), ys = P.map((p) => p.y);
    const radius = Math.max(15, Math.hypot(Math.max(...xs) - Math.min(...xs), Math.max(...ys) - Math.min(...ys)) * 0.012);
    if (isHull) {
      const d = Geo.hullDiff(r.hull, ref('hull'));
      missing = rings(d.missing, radius); extra = rings(d.extra, radius * 0.7);
    } else if (state.algo === 'delaunay' && !Geo.hasCocircular(ref('del'))) {
      const d = Geo.trisDiff(r.tris, ref('del'));
      missing = uniqueEdges(d.missing); extra = uniqueEdges(d.extra);
    } else if (isVor) {
      // Arêtes attendues absentes (rouge) ou en trop (violet), repérées par la paire de sites
      const out = r.vedges || [], refE = ref('vor'), ko = (e) => Geo.key(e[2], e[3]);
      const so = new Set(out.map(ko)), sr = new Set(refE.map(ko));
      missing = refE.filter((e) => !so.has(ko(e))).map((e) => [e[0], e[1]]);
      extra = out.filter((e) => !sr.has(ko(e))).map((e) => [e[0], e[1]]);
    } else if (state.algo === 'incr') {
      // Plusieurs triangulations sont correctes : on ne montre que les triangles plats
      extra = uniqueEdges(r.tris.filter((t) => Geo.triArea(t) < 1e-6));
    }
  }
  setOverlay('diffMissing', missing);
  setOverlay('diffExtra', extra);
}

/* ---------- Affichage du résultat ---------- */
function render() {
  const r = state.result, n = state.points.length;
  $('sN').textContent = n;

  if (!r || n < 3) {
    $('callName').textContent = CALLS[state.algo];
    $('callMs').textContent = '';
    $('callIo').textContent = n < 3 ? `Il faut au moins 3 points (${n} actuellement). Cliquez dans le niveau.` : 'En attente du résultat.';
    setVerdict('none', n < 3 ? 'Pas assez de points' : 'Aucun résultat');
    ['sH', 'sMs', 'sF', 'sE', 'sV'].forEach((id) => ($(id).textContent = '–'));
    $('checks').innerHTML = '<li class="na"><span class="ico">–</span><span>Rien à vérifier</span><b></b></li>';
    $('badge').textContent = '';
    return;
  }

  const c = r.counts || {};
  $('callName').textContent = r.call || CALLS[r.algo];
  $('callMs').textContent = `${fmt(r.ms, r.ms < 10 ? 2 : 1)} ms`;
  $('callIo').innerHTML = r.kind === 'hull'
    ? `Entrée <b>${r.n}</b> points → sortie <b>${r.hull.length}</b> sommet${r.hull.length > 1 ? 's' : ''}`
    : r.kind === 'voronoi'
    ? `Entrée <b>${r.n}</b> points → <b>${r.cells.length}</b> régions · <b>${c.edges ?? 0}</b> arêtes · <b>${c.vertices ?? 0}</b> sommets`
    : `Entrée <b>${r.n}</b> points → <b>${c.faces ?? r.tris.length}</b> faces · <b>${c.edges ?? 0}</b> arêtes · <b>${c.vertices ?? 0}</b> sommets${r.kernel ? ' · mode noyau' : ''}`;

  $('sH').textContent = r.kind === 'hull' ? r.hull.length : ref('hull').length;
  $('sMs').textContent = fmt(r.ms, r.ms < 10 ? 2 : 0);
  const graph = r.kind === 'tri' || r.kind === 'voronoi';
  $('sF').textContent = graph ? (c.faces ?? r.tris.length) : '–';
  $('sE').textContent = graph ? (c.edges ?? 0) : '–';
  $('sV').textContent = graph ? (c.vertices ?? 0) : '–';
  $('sFLabel').textContent = r.kind === 'voronoi' ? 'régions' : 'faces';

  const v = verdictOf(r, state.checks, Geo.unique(state.points));
  setVerdict(v.state, v.text);
  const ko = state.checks.filter((x) => x.ok === false).length;
  $('badge').textContent = state.checks.length ? (ko ? `${ko} ✕` : '✓') : '';
  $('badge').style.color = ko ? 'var(--bad)' : 'var(--good)';
  $('checks').innerHTML = state.checks.length
    ? state.checks.map((x) => `<li class="${x.ok === null ? 'na' : x.ok ? 'ok' : 'ko'}"><span class="ico">${x.ok === null ? '–' : x.ok ? '✓' : '✕'}</span><span>${esc(x.label)}</span><b>${esc(x.value)}</b>${x.hint && x.ok !== true ? `<span class="hint">${esc(x.hint)}</span>` : ''}</li>`).join('')
    : '<li class="na"><span class="ico">–</span><span>Rien à vérifier</span><b></b></li>';
}
function verdictOf(r, checks, points) {
  const c = r.counts || {};
  const ko = checks.filter((x) => x.ok === false), ok = checks.filter((x) => x.ok === true).length;
  const empty = r.kind === 'hull' ? r.hull.length === 0 : r.kind === 'voronoi' ? r.cells.length === 0 : r.tris.length === 0 && !(c.edges > 0);
  if (empty && (r.kind === 'voronoi' || !Geo.allCollinear(points))) return { state: 'empty', text: 'Sortie vide : fonction encore à écrire ?', first: 'sortie vide' };
  if (ko.length) return { state: 'bad', text: `${ko.length} test${ko.length > 1 ? 's' : ''} en échec sur ${ok + ko.length}`, first: ko[0].label };
  return { state: 'ok', text: `Résultat cohérent · ${ok} test${ok > 1 ? 's' : ''} passé${ok > 1 ? 's' : ''}`, first: '' };
}
function setVerdict(stateName, text) { $('verdict').dataset.state = stateName; $('verdictText').textContent = text; }

/* ---------- Point survolé dans le niveau ---------- */
function renderHover() {
  const p = state.points.find((q) => q.id === state.hoverId);
  const el = $('status');
  if (!p) { el.textContent = lastNote; return; }
  const k = Geo.pk(p), r = state.result;
  let info = '';
  if (r && r.kind === 'hull') {
    const inOut = r.hull.some((h) => Geo.pk(h) === k), inRef = ref('hull').some((h) => Geo.pk(h) === k);
    info = ` · env. C++ ${inOut ? '✓' : '✕'} · réf. ${inRef ? '✓' : '✕'}`;
  } else if (r && r.kind === 'voronoi') {
    const cell = r.cells.find((c) => Geo.pk(c.site) === k);
    info = cell ? ` · région à ${cell.poly.length} côtés${cell.touches ? ', infinie (coupée par le cadre)' : ', bornée'}` : ' · aucune région C++';
  } else if (r) {
    info = ` · ${r.tris.filter((t) => t.some((q) => Geo.pk(q) === k)).length} triangle(s) C++`;
  }
  el.innerHTML = `<b>P${p.id}</b> (${Math.round(p.x)}, ${Math.round(p.y)})${esc(info)}`;
}

/* ---------- Rappels venant du C++ (ou de la simulation) ---------- */
window.onGeomState = (s) => {
  if (s.algo && !state.bench) showAlgo(s.algo);
  if (s.tool) showTool(s.tool);
  $('kernel').checked = !!s.kernel; state.kernel = !!s.kernel;
};

window.onGeomPoints = (d) => {
  if (state.bench) return; // pendant le banc d'essai, la scène est temporaire
  state.points = Array.isArray(d && d.points) ? d.points : [];
  recordPoints(pack(state.points));
  if (!state.seeded) {
    state.seeded = true;
    if (!state.points.length) { generate('disc'); history.undo.length = 0; syncUndo(); }
  }
  render();
};

window.onGeomResult = (raw) => {
  let r;
  try { r = normalize(raw); } catch (e) { report(e, 'onGeomResult'); return; }
  if (state.bench) { benchReceive(r); return; }
  state.result = r;
  if (r.algo && r.algo !== state.algo) showAlgo(r.algo);
  state.checks = runChecks(r, state.points, () => ref('del'));
  render();
  pushOverlays();
  if (state.hoverId > 0) renderHover();
};
function normalize(r) {
  const toP = (a) => ({ x: +a[0], y: +a[1] });
  return {
    ...r,
    hull: (r.hull || []).map(toP),
    tris: (r.tris || []).filter((t) => t.length === 6).map((t) => [toP([t[0], t[1]]), toP([t[2], t[3]]), toP([t[4], t[5]])]),
    edges: (r.edges || []).filter((e) => e.length === 4).map((e) => [toP([e[0], e[1]]), toP([e[2], e[3]])]),
    counts: r.counts || {},
    cells: (r.cells || []).filter((c) => c.length >= 10 && c.length % 2 === 0).map((c) => ({
      id: c[0], touches: !!c[1], site: toP([c[2], c[3]]), poly: Array.from({ length: (c.length - 4) / 2 }, (_, k) => toP([c[4 + 2 * k], c[5 + 2 * k]])),
    })),
    vedges: (r.vedges || []).filter((e) => e.length === 8).map((e) => [toP([e[0], e[1]]), toP([e[2], e[3]]), toP([e[4], e[5]]), toP([e[6], e[7]])]),
    bounds: Array.isArray(r.bounds) && r.bounds.length === 4 ? { minX: +r.bounds[0], minY: +r.bounds[1], maxX: +r.bounds[2], maxY: +r.bounds[3] } : null,
  };
}

/* ---------- Construction pas à pas (Voronoï) ---------- */
function sendBuild(on, step, site = -1) { G('setconstruction', !!on, site, Math.max(0, step | 0)); }
function stopPlay() { clearInterval(state.build.timer); state.build.timer = 0; $('bPlay').textContent = 'Lecture'; }
function nextStep(c, dir) {
  // Avec « sauter », on passe d'une médiatrice qui coupe à la suivante (et on finit sur la région finale)
  const last = c.total - 1;
  if (!$('bSkip').checked) return Math.max(0, Math.min(last, c.step + dir));
  const cuts = [0, ...(c.cutSteps || []), last].filter((k, i, a) => a.indexOf(k) === i).sort((a, b) => a - b);
  if (dir > 0) return cuts.find((k) => k > c.step) ?? last;
  return [...cuts].reverse().find((k) => k < c.step) ?? 0;
}
function goStep(dir) { const c = state.build.info; if (c && c.on) sendBuild(true, nextStep(c, dir)); }
function renderBuild() {
  const c = state.build.info, on = state.build.on;
  $('buildOn').checked = on;
  $('buildCtl').hidden = !on;
  if (!on) return;
  const el = $('buildInfo');
  if (!c || !c.on) { el.textContent = 'Cliquez un point dans le niveau pour construire sa région.'; return; }
  const last = c.total - 1;
  $('bStep').max = String(Math.max(0, last)); $('bStep').value = String(c.step);
  ['bFirst', 'bPrev'].forEach((id) => ($(id).disabled = c.step <= 0));
  ['bNext', 'bLast'].forEach((id) => ($(id).disabled = c.step >= last));
  let text;
  if (c.step === 0) text = `Étape <b>0 / ${last}</b> · on part du cadre, qui contient tous les sites. La région de <b>P${c.site}</b> va être découpée.`;
  else text = `Étape <b>${c.step} / ${last}</b> · médiatrice de <b>[P${c.site} P${c.neighbor}]</b> : `
    + (c.cut ? '<span class="cut">elle coupe, la partie du côté de P' + c.neighbor + ' est retirée.</span>' : 'elle ne coupe rien, la région est déjà de ce côté.');
  if (c.step === last) text += `<br>Région terminée : ${(c.cutSteps || []).length} médiatrice${(c.cutSteps || []).length > 1 ? 's' : ''} utile${(c.cutSteps || []).length > 1 ? 's' : ''}`
    + (c.skipped ? `, ${c.skipped} site${c.skipped > 1 ? 's' : ''} trop loin, jamais essayé${c.skipped > 1 ? 's' : ''} (arrêt anticipé).` : '.');
  el.innerHTML = text;
}
window.onGeomConstruction = (c) => {
  state.build.info = c;
  if (state.build.timer && c && c.step >= c.total - 1) stopPlay();
  renderBuild();
};
function setBuild(on) {
  state.build.on = on && state.algo === 'voronoi';
  stopPlay();
  sendBuild(state.build.on, 0, state.hoverId > 0 ? state.hoverId : -1);
  renderBuild();
}

window.onGeomCursor = (x, y) => { $('cursor').textContent = `X ${Math.round(x)} · Y ${Math.round(y)} cm`; };
window.onGeomHover = (id) => { state.hoverId = id; renderHover(); };
window.onGeomShortcut = (name) => {
  if (name.startsWith('algo:')) selectAlgo(name.slice(5));
  else if (name.startsWith('tool:')) showTool(name.slice(5));
  else if (name === 'undo') undo();
  else if (name === 'redo') redo();
};

/* =====================================================================
   Banc d'essai : envoie des nuages fixes au C++ et attend chaque réponse.
   Une réponse est reconnue grâce à firstId (identifiant du premier point),
   unique pour chaque requête.
   ===================================================================== */
const BATTERY = [
  ['disc', 10, 11], ['disc', 60, 12], ['disc', 200, 13], ['gauss', 80, 14], ['clusters', 60, 15], ['circle', 24, 16], ['grid', 25, 17],
  ['line', 12, 18], ['three', 3, 19], ['square', 5, 20], ['collinear', 10, 21], ['duplicates', 20, 22], ['vertical', 13, 23], ['tiny', 20, 24],
];
let requestBase = 900000;

function benchReceive(r) {
  const b = state.bench;
  if (b && b.waiter && r.firstId === b.waiter.firstId && r.algo === b.waiter.algo) { const w = b.waiter; b.waiter = null; clearTimeout(w.timer); w.resolve(r); }
}
function request(points, algo) {
  const b = state.bench;
  requestBase += 1000;
  const firstId = requestBase + 1, pts = withIds(points, firstId);
  return new Promise((resolve) => {
    b.waiter = { firstId, algo, resolve, timer: setTimeout(() => { b.waiter = null; resolve(null); }, 5000) };
    if (b.currentAlgo !== algo) { b.currentAlgo = algo; G('setalgorithm', algo); }
    G('setpoints', pack(pts));
  });
}
function caseResult(r, points) {
  if (!r) return { cls: 'ko', label: 'pas de réponse', detail: 'Rien reçu en 5 s', ms: null };
  const checks = runChecks(r, points), v = verdictOf(r, checks, Geo.unique(points));
  return { cls: v.state === 'ok' ? 'ok' : v.state === 'empty' ? 'empty' : 'ko', label: v.state === 'ok' ? 'OK' : v.state === 'empty' ? 'vide' : 'échec', detail: v.first, ms: r.ms };
}
function startBench(kind) {
  if (state.bench || !state.geom) return null;
  state.bench = { kind, stop: false, waiter: null, currentAlgo: state.algo, saved: { algo: state.algo, packed: history.current, build: state.build.on } };
  if (state.build.on) { stopPlay(); sendBuild(false, 0); }
  ['runBattery', 'runAll', 'runCurve'].forEach((id) => ($(id).disabled = true));
  $('stopBench').disabled = false;
  document.querySelectorAll('[data-gen], #clear, #importBtn').forEach((b) => (b.disabled = true));
  syncUndo();
  return state.bench;
}
function endBench(message) {
  const b = state.bench; if (!b) return;
  if (b.waiter) { clearTimeout(b.waiter.timer); b.waiter = null; }
  state.bench = null;
  ['runBattery', 'runAll', 'runCurve'].forEach((id) => ($(id).disabled = false));
  $('stopBench').disabled = true;
  document.querySelectorAll('[data-gen], #clear, #importBtn').forEach((x) => (x.disabled = false));
  // Remise en place de la scène d'origine
  history.applying = true;
  G('setalgorithm', b.saved.algo); showAlgo(b.saved.algo);
  G('setpoints', b.saved.packed);
  if (b.saved.build) { state.build.on = true; sendBuild(true, 0); renderBuild(); }
  syncUndo();
  sentOverlays = {}; // les calques seront renvoyés avec le résultat
  note(message);
}

async function runBattery(algos) {
  const b = startBench('battery'); if (!b) return;
  const all = algos.length > 1, table = $('batteryTable'), body = table.querySelector('tbody');
  table.hidden = false;
  table.querySelector('thead').innerHTML = all
    ? `<tr><th>Cas</th><th>n</th>${algos.map((a) => `<th>${SHORT[a]}</th>`).join('')}</tr>`
    : '<tr><th>Cas</th><th>n</th><th>ms</th><th>Résultat</th></tr>';
  body.innerHTML = '';
  let pass = 0, total = 0;
  for (const [kind, n, seed] of BATTERY) {
    if (b.stop) break;
    const pts = Geo.cloud(kind, n, seed);
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${GEN_NAMES[kind]}</td><td>${pts.length}</td>${all ? algos.map(() => '<td class="wait">…</td>').join('') : '<td>…</td><td class="wait">…</td>'}`;
    tr.title = 'Charger ce cas dans le niveau';
    tr.onclick = () => { if (state.bench) return; G('setpoints', pack(withIds(pts))); G('frameall'); note(`Cas ${GEN_NAMES[kind]} chargé (graine ${seed})`); };
    body.appendChild(tr);
    for (let i = 0; i < algos.length; i++) {
      if (b.stop) break;
      const res = caseResult(await request(pts, algos[i]), pts);
      total++; if (res.cls === 'ok') pass++;
      if (all) { const td = tr.children[2 + i]; td.className = res.cls; td.textContent = res.cls === 'ok' ? '✓' : res.cls === 'empty' ? '∅' : '✕'; td.title = res.detail || 'OK'; }
      else {
        tr.children[2].textContent = res.ms == null ? '–' : fmt(res.ms, res.ms < 10 ? 2 : 1);
        tr.children[3].className = res.cls;
        tr.children[3].innerHTML = `${res.label}${res.detail ? `<small>${esc(res.detail)}</small>` : ''}`;
      }
    }
  }
  $('batterySummary').textContent = `${pass} / ${total} OK`;
  endBench(b.stop ? 'Batterie arrêtée' : `Batterie terminée : ${pass} / ${total} cas OK`);
}

const curves = {}; // algo -> [{n, ms}]
async function runCurve() {
  const b = startBench('curve'); if (!b) return;
  const algo = state.algo, ns = [10, 25, 50, 100, 150, 200], pts = [];
  curves[algo] = pts;
  $('curve').hidden = false;
  for (const n of ns) {
    if (b.stop) break;
    const times = [];
    for (let k = 0; k < 3 && !b.stop; k++) { const r = await request(Geo.cloud('disc', n, 1000 + n), algo); if (r) times.push(r.ms); }
    if (times.length) { times.sort((x, y) => x - y); pts.push({ n, ms: times[Math.floor(times.length / 2)] }); drawCurve(); }
  }
  const last = pts[pts.length - 1];
  $('curveSummary').textContent = last ? `${SHORT[algo]} : ${fmt(last.ms, 2)} ms à n = ${last.n}` : '';
  endBench(b.stop ? 'Courbe arrêtée' : 'Courbe tracée');
}
function drawCurve() {
  const cv = $('curve'), ctx = cv.getContext('2d'), W = cv.width, H = cv.height;
  const L = 56, R = 16, T = 18, B = 40;
  ctx.clearRect(0, 0, W, H);
  const series = Object.entries(curves).filter(([, p]) => p.length);
  const maxMs = Math.max(0.001, ...series.flatMap(([, p]) => p.map((q) => q.ms)));
  const step = niceStep(maxMs / 4), top = Math.ceil(maxMs / step) * step;
  const x = (n) => L + ((n - 0) / 200) * (W - L - R), y = (ms) => H - B - (ms / top) * (H - T - B);
  ctx.font = '20px Consolas, monospace'; ctx.fillStyle = '#5a6679'; ctx.strokeStyle = '#232d3c'; ctx.lineWidth = 1;
  for (let v = 0; v <= top + 1e-9; v += step) { ctx.beginPath(); ctx.moveTo(L, y(v)); ctx.lineTo(W - R, y(v)); ctx.stroke(); ctx.textAlign = 'right'; ctx.textBaseline = 'middle'; ctx.fillText(fmt(v, step < 0.1 ? 2 : step < 1 ? 1 : 0), L - 8, y(v)); }
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  for (const n of [0, 50, 100, 150, 200]) ctx.fillText(String(n), x(n), H - B + 8);
  ctx.fillText('n', W - R - 6, H - B + 8);
  ctx.save(); ctx.translate(14, T + (H - T - B) / 2); ctx.rotate(-Math.PI / 2); ctx.fillText('ms', 0, -6); ctx.restore();
  let ly = T;
  for (const [algo, p] of series) {
    ctx.strokeStyle = CURVE_COLORS[algo]; ctx.fillStyle = CURVE_COLORS[algo]; ctx.lineWidth = 3;
    ctx.beginPath(); p.forEach((q, i) => (i ? ctx.lineTo(x(q.n), y(q.ms)) : ctx.moveTo(x(q.n), y(q.ms)))); ctx.stroke();
    p.forEach((q) => { ctx.beginPath(); ctx.arc(x(q.n), y(q.ms), 5, 0, 2 * Math.PI); ctx.fill(); });
    ctx.textAlign = 'left'; ctx.textBaseline = 'middle'; ctx.fillText(SHORT[algo], L + 12, ly + 10); ly += 26;
  }
}
function niceStep(v) { const p = Math.pow(10, Math.floor(Math.log10(v))), m = v / p; return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p; }

/* ---------- Contrôles ---------- */
document.querySelectorAll('.seg button').forEach((b) => (b.onclick = () => selectAlgo(b.dataset.algo)));
document.querySelectorAll('.tools .btn').forEach((b) => (b.onclick = () => selectTool(b.dataset.tool)));
document.querySelectorAll('[data-gen]').forEach((b) => (b.onclick = () => generate(b.dataset.gen)));
$('count').oninput = (e) => ($('countOut').textContent = `${e.target.value} points`);
$('count').onchange = savePrefs;
$('seed').onchange = () => { const v = Math.max(0, Math.min(999999, Math.floor(+$('seed').value || 0))); $('seed').value = v; savePrefs(); };
$('dice').onclick = () => { $('seed').value = Math.floor(Math.random() * 1e6); savePrefs(); note(`Graine ${$('seed').value} : choisissez un nuage`); };
$('undo').onclick = undo;
$('redo').onclick = redo;
$('clear').onclick = () => { if (!state.bench && state.points.length) { G('setpoints', ''); note('Tous les points supprimés'); } };
$('frame').onclick = () => G('frameall');
$('rerun').onclick = () => { if (!state.bench) { G('recompute'); note('Appel relancé'); } };
$('exportBtn').onclick = exportScene;
$('importBtn').onclick = importScene;
$('kernel').onchange = (e) => {
  state.kernel = e.target.checked; G('setkernelmode', state.kernel);
  note(state.kernel ? 'Mode noyau : le prochain ajout appelle DelaunayKernel_AddPoint' : 'Mode noyau désactivé');
};
document.querySelectorAll('[data-layer]').forEach((c) => (c.onchange = () => { state.layers[c.dataset.layer] = c.checked; G('setlayer', c.dataset.layer, c.checked); savePrefs(); }));
document.querySelectorAll('[data-overlay]').forEach((c) => (c.onchange = () => { state.overlays[c.dataset.overlay] = c.checked; pushOverlays(); savePrefs(); }));
function openTab(id) {
  if (!$(id)) id = 't-scene';
  document.querySelectorAll('[role=tab]').forEach((t) => { const on = t.id === id; t.setAttribute('aria-selected', String(on)); $(t.getAttribute('aria-controls')).hidden = !on; });
  savePrefs();
}
document.querySelectorAll('[role=tab]').forEach((t) => (t.onclick = () => openTab(t.id)));
$('verdict').onclick = () => openTab('t-check');
$('hide').onclick = () => G('togglepanel');
$('runBattery').onclick = () => runBattery([state.algo]);
$('runAll').onclick = () => runBattery(ALGOS);
$('runCurve').onclick = runCurve;
$('buildOn').onchange = (e) => setBuild(e.target.checked);
$('bFirst').onclick = () => { stopPlay(); sendBuild(true, 0); };
$('bLast').onclick = () => { stopPlay(); const c = state.build.info; if (c) sendBuild(true, c.total - 1); };
$('bPrev').onclick = () => { stopPlay(); goStep(-1); };
$('bNext').onclick = () => { stopPlay(); goStep(1); };
$('bStep').oninput = (e) => { stopPlay(); sendBuild(true, +e.target.value); };
$('bPlay').onclick = () => {
  const c = state.build.info;
  if (state.build.timer) { stopPlay(); return; }
  if (!c || !c.on) return;
  if (c.step >= c.total - 1) sendBuild(true, 0); // relancer depuis le début
  $('bPlay').textContent = 'Pause';
  state.build.timer = setInterval(() => goStep(1), 750);
};
$('stopBench').onclick = () => { if (state.bench) { state.bench.stop = true; note('Arrêt demandé…'); } };

window.addEventListener('keydown', (e) => {
  if (e.target.matches('input, textarea')) return; // on tape une valeur : pas de raccourci
  const k = e.key.toLowerCase();
  if ((e.ctrlKey || e.metaKey) && k === 'z') { e.preventDefault(); undo(); return; }
  if ((e.ctrlKey || e.metaKey) && k === 'y') { e.preventDefault(); redo(); return; }
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (/^[1-5]$/.test(e.key)) selectAlgo(ALGOS[+e.key - 1]);
  else if (k === 'a') selectTool('add');
  else if (k === 'm') selectTool('move');
  else if (k === 'x') selectTool('del');
  else if (k === 'f') G('frameall');
  else if (k === 'r') $('rerun').click();
  else if (k === 'h') G('togglepanel');
});

/* ---------- Démarrage ---------- */
function boot(geom, isUE) {
  state.geom = geom; state.isUE = isUE;
  document.body.classList.toggle('ue', isUE);
  const pill = $('bridge');
  pill.textContent = isUE ? 'ue.geom connecté' : 'simulation';
  pill.classList.toggle('simulated', !isUE);
  pill.classList.remove('error');
  pill.title = isUE ? 'Les appels partent vers UGeomBridge (C++)' : 'Hors Unreal : sim.js remplace le C++';

  // Réglages mémorisés
  const prefs = loadPrefs();
  if (prefs.count) { $('count').value = prefs.count; $('countOut').textContent = `${prefs.count} points`; }
  if (prefs.seed != null) $('seed').value = prefs.seed;
  Object.assign(state.layers, prefs.layers || {});
  Object.assign(state.overlays, prefs.overlays || {});
  document.querySelectorAll('[data-layer]').forEach((c) => (c.checked = !!state.layers[c.dataset.layer]));
  document.querySelectorAll('[data-overlay]').forEach((c) => (c.checked = !!state.overlays[c.dataset.overlay]));
  if (prefs.tab) openTab(prefs.tab);

  showAlgo(ALGOS.includes(prefs.algo) ? prefs.algo : state.algo); showTool(state.tool); render();
  // Réglages envoyés AVANT ready() : l'état que le C++ renvoie à ready() est alors déjà le bon
  G('setalgorithm', state.algo);
  Object.entries(state.layers).forEach(([name, on]) => G('setlayer', name, on));
  G('ready');
  if (isUE) G('log', 'Panneau chargé');
}
function fail(message) {
  const pill = $('bridge');
  pill.textContent = 'pont absent'; pill.classList.add('error'); pill.title = message;
  $('callIo').textContent = message;
  setVerdict('bad', 'Le panneau ne reçoit rien du C++');
}
window.GeomApp = { boot, fail };
