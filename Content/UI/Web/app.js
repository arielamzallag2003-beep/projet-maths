/* =====================================================================
   Logique du panneau.
   Parle à « geom » : window.ue.geom dans Unreal (UGeomBridge), ou la
   simulation de sim.js dans un navigateur. Les deux ont la même interface :
     ready() setalgorithm(id) setpoints(packed) setlayer(name, on) settool(t)
     setkernelmode(on) setoverlay(name, packed, color) frameall() recompute() log(msg)
   et rappellent window.onGeomState / onGeomPoints / onGeomResult /
   onGeomCursor / onGeomShortcut.
   ===================================================================== */
'use strict';

const $ = (id) => document.getElementById(id);
const CALLS = {
  jarvis: 'ComputeJarvisMarch',
  graham: 'ComputeGrahamScan',
  incr: 'IncrementalTriangulation2D',
  delaunay: 'IncrementalTriangulation2D + FlipEdgesToDelaunay',
};
const OVERLAY_COLORS = { reference: '#7a8597', circles: '#7d5aa8', voronoi: '#4fd1c5' };

const state = {
  geom: null, isUE: false,
  algo: 'jarvis', tool: 'add', kernel: false,
  points: [], result: null, checks: [],
  overlays: { reference: false, circles: false, voronoi: false },
  seeded: false,
};

/* ---------- Historique (annuler / rétablir) construit à partir des points reçus ---------- */
const history = { undo: [], redo: [], current: '', applying: false, lastPush: 0 };
const pack = (pts) => pts.map((p) => `${p.id},${Math.round(p.x * 100) / 100},${Math.round(p.y * 100) / 100}`).join(';');
function recordPoints(packed) {
  if (packed === history.current) return;
  if (!history.applying) {
    const now = performance.now();
    // Un glisser-déposer envoie beaucoup d'états : on les regroupe en une seule étape
    if (now - history.lastPush > 600) { history.undo.push(history.current); if (history.undo.length > 100) history.undo.shift(); }
    history.lastPush = now;
    history.redo.length = 0;
  }
  history.applying = false;
  history.current = packed;
  $('undo').disabled = !history.undo.length;
  $('redo').disabled = !history.redo.length;
}
function undo() { if (!history.undo.length) return; history.redo.push(history.current); history.applying = true; state.geom.setpoints(history.undo.pop()); note('Annulé'); }
function redo() { if (!history.redo.length) return; history.undo.push(history.current); history.applying = true; state.geom.setpoints(history.redo.pop()); note('Rétabli'); }

/* ---------- Messages ---------- */
let noteTimer = 0;
function note(text) {
  const el = $('status'); el.textContent = text; el.classList.add('flash');
  clearTimeout(noteTimer); noteTimer = setTimeout(() => el.classList.remove('flash'), 1600);
}
const fmt = (v, d = 2) => Number(v).toFixed(d).replace('.', ',');

/* ---------- Choix de l'algorithme, de l'outil ---------- */
function showAlgo(id) {
  state.algo = id;
  document.querySelectorAll('.seg button').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.algo === id)));
  const panel = $('panel');
  panel.dataset.algo = id;
  panel.dataset.family = id === 'jarvis' || id === 'graham' ? 'hull' : 'tri';
  $('kernel').disabled = id !== 'delaunay';
}
function selectAlgo(id) { showAlgo(id); state.geom.setalgorithm(id); note(`Algorithme : ${CALLS[id]}`); }
function showTool(t) { state.tool = t; document.querySelectorAll('.tools .btn').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.tool === t))); }
function selectTool(t) { showTool(t); state.geom.settool(t); }

/* ---------- Génération de nuages (côté page, envoyés au C++ en un seul appel) ---------- */
function gauss() { let u = 0, v = 0; while (!u) u = Math.random(); while (!v) v = Math.random(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }
const GEN_NAMES = { disc: 'disque', gauss: 'gaussien', clusters: 'amas', circle: 'cercle', grid: 'grille', line: 'alignés' };
function generate(kind, n) {
  const pts = []; let id = 1;
  const add = (x, y) => pts.push({ id: id++, x: Math.round(x), y: Math.round(y) });
  if (kind === 'disc') for (let i = 0; i < n; i++) { const a = Math.random() * 2 * Math.PI, r = Math.sqrt(Math.random()) * 1300; add(Math.cos(a) * r * 1.4, Math.sin(a) * r * 0.9); }
  if (kind === 'gauss') for (let i = 0; i < n; i++) add(gauss() * 620, gauss() * 420);
  if (kind === 'clusters') { const C = [[-900, 400], [800, 500], [0, -600]]; for (let i = 0; i < n; i++) { const [cx, cy] = C[i % 3]; add(cx + gauss() * 230, cy + gauss() * 200); } }
  if (kind === 'circle') for (let i = 0; i < n; i++) { const a = (i / n) * 2 * Math.PI; add(Math.cos(a) * 1200, Math.sin(a) * 1200); }
  if (kind === 'grid') { const c = Math.max(2, Math.round(Math.sqrt(n))); for (let i = 0; i < c; i++) for (let j = 0; j < c; j++) add((i - (c - 1) / 2) * 2600 / c, (j - (c - 1) / 2) * 2000 / c); }
  if (kind === 'line') { const m = Math.max(2, n - 1); for (let i = 0; i < m; i++) add(-1500 + (i * 3000) / (m - 1), -300 + (i * 600) / (m - 1)); add(200, 700); }
  state.geom.setpoints(pack(pts));
  state.geom.frameall();
  note(`Nuage ${GEN_NAMES[kind]} : ${pts.length} points`);
}

/* ---------- Calques calculés par la page ---------- */
const packSegs = (segs) => segs.map(([a, b]) => `${a.x.toFixed(1)},${a.y.toFixed(1)},${b.x.toFixed(1)},${b.y.toFixed(1)}`).join(';');
function uniqueEdges(tris) {
  const seen = new Set(), out = [];
  for (const t of tris) for (let e = 0; e < 3; e++) { const a = t[e], b = t[(e + 1) % 3], k = Geo.key(a, b); if (!seen.has(k)) { seen.add(k); out.push([a, b]); } }
  return out;
}
function pushOverlays() {
  const r = state.result, P = state.points;
  for (const name of Object.keys(state.overlays)) {
    let segs = [];
    if (state.overlays[name] && P.length >= 3) {
      if (name === 'reference') {
        if (state.algo === 'jarvis' || state.algo === 'graham') { const H = Geo.hull(P); segs = H.map((p, i) => [p, H[(i + 1) % H.length]]); }
        else segs = uniqueEdges(state.algo === 'delaunay' ? Geo.delaunay(P).tris : Geo.triangulate(P));
      }
      if (name === 'circles' && r && r.tris.length) segs = Geo.circleSegments(r.tris);
      if (name === 'voronoi' && r && r.tris.length) segs = Geo.voronoiSegments(r.tris);
    }
    state.geom.setoverlay(name, packSegs(segs), OVERLAY_COLORS[name]);
  }
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

  $('callName').textContent = r.call || CALLS[r.algo];
  $('callMs').textContent = `${fmt(r.ms, r.ms < 10 ? 2 : 1)} ms`;
  const c = r.counts || {};
  $('callIo').innerHTML = r.kind === 'hull'
    ? `Entrée <b>${r.n}</b> points → sortie <b>${r.hull.length}</b> sommet${r.hull.length > 1 ? 's' : ''}`
    : `Entrée <b>${r.n}</b> points → <b>${c.faces ?? r.tris.length}</b> faces · <b>${c.edges ?? 0}</b> arêtes · <b>${c.vertices ?? 0}</b> sommets${r.kernel ? ' · mode noyau' : ''}`;

  $('sH').textContent = r.kind === 'hull' ? r.hull.length : Geo.hull(state.points).length;
  $('sMs').textContent = fmt(r.ms, r.ms < 10 ? 2 : 0);
  $('sF').textContent = r.kind === 'tri' ? (c.faces ?? r.tris.length) : '–';
  $('sE').textContent = r.kind === 'tri' ? (c.edges ?? 0) : '–';
  $('sV').textContent = r.kind === 'tri' ? (c.vertices ?? 0) : '–';

  const checks = state.checks;
  const ko = checks.filter((x) => x.ok === false).length, ok = checks.filter((x) => x.ok === true).length;
  const empty = r.kind === 'hull' ? r.hull.length === 0 : r.tris.length === 0 && !(c.edges > 0);
  if (empty && !Geo.allCollinear(state.points)) setVerdict('empty', 'Sortie vide : fonction encore à écrire ?');
  else if (ko) setVerdict('bad', `${ko} test${ko > 1 ? 's' : ''} en échec sur ${ok + ko}`);
  else setVerdict('ok', `Résultat cohérent · ${ok} test${ok > 1 ? 's' : ''} passé${ok > 1 ? 's' : ''}`);

  $('badge').textContent = checks.length ? (ko ? `${ko} ✕` : '✓') : '';
  $('badge').style.color = ko ? 'var(--bad)' : 'var(--good)';
  const esc = (s) => String(s).replace(/[&<>]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[ch]));
  $('checks').innerHTML = checks.length
    ? checks.map((x) => `<li class="${x.ok === null ? 'na' : x.ok ? 'ok' : 'ko'}"><span class="ico">${x.ok === null ? '–' : x.ok ? '✓' : '✕'}</span><span>${esc(x.label)}</span><b>${esc(x.value)}</b>${x.hint && x.ok !== true ? `<span class="hint">${esc(x.hint)}</span>` : ''}</li>`).join('')
    : '<li class="na"><span class="ico">–</span><span>Rien à vérifier</span><b></b></li>';
}
function setVerdict(stateName, text) { $('verdict').dataset.state = stateName; $('verdictText').textContent = text; }

/* ---------- Rappels venant du C++ (ou de la simulation) ---------- */
window.onGeomState = (s) => { if (s.algo) showAlgo(s.algo); if (s.tool) showTool(s.tool); $('kernel').checked = !!s.kernel; state.kernel = !!s.kernel; };

window.onGeomPoints = (d) => {
  state.points = d.points || [];
  recordPoints(pack(state.points));
  if (!state.seeded) {
    state.seeded = true;
    if (!state.points.length) { generate('disc', +$('count').value); history.undo.length = 0; $('undo').disabled = true; }
  }
  render();
};

window.onGeomResult = (r) => {
  const toP = (a) => ({ x: a[0], y: a[1] });
  r.hull = (r.hull || []).map(toP);
  r.tris = (r.tris || []).map((t) => [{ x: t[0], y: t[1] }, { x: t[2], y: t[3] }, { x: t[4], y: t[5] }]);
  r.edges = (r.edges || []).map((e) => [{ x: e[0], y: e[1] }, { x: e[2], y: e[3] }]);
  state.result = r;
  if (r.algo && r.algo !== state.algo) showAlgo(r.algo);
  state.checks = runChecks(r, state.points);
  render();
  pushOverlays();
};

window.onGeomCursor = (x, y) => { $('cursor').textContent = `X ${Math.round(x)} · Y ${Math.round(y)} cm`; };

window.onGeomShortcut = (name) => {
  if (name.startsWith('algo:')) selectAlgo(name.slice(5));
  else if (name.startsWith('tool:')) showTool(name.slice(5));
  else if (name === 'undo') undo();
  else if (name === 'redo') redo();
};

/* ---------- Contrôles ---------- */
document.querySelectorAll('.seg button').forEach((b) => (b.onclick = () => selectAlgo(b.dataset.algo)));
document.querySelectorAll('.tools .btn').forEach((b) => (b.onclick = () => selectTool(b.dataset.tool)));
document.querySelectorAll('[data-gen]').forEach((b) => (b.onclick = () => generate(b.dataset.gen, +$('count').value)));
$('count').oninput = (e) => ($('countOut').textContent = `${e.target.value} points`);
$('undo').onclick = undo;
$('redo').onclick = redo;
$('clear').onclick = () => { if (state.points.length) { state.geom.setpoints(''); note('Tous les points supprimés'); } };
$('frame').onclick = () => state.geom.frameall();
$('rerun').onclick = () => { state.geom.recompute(); note('Appel relancé'); };
$('kernel').onchange = (e) => { state.kernel = e.target.checked; state.geom.setkernelmode(state.kernel); note(state.kernel ? 'Mode noyau activé : le prochain ajout appelle DelaunayKernel_AddPoint' : 'Mode noyau désactivé'); };
document.querySelectorAll('[data-layer]').forEach((c) => (c.onchange = () => state.geom.setlayer(c.dataset.layer, c.checked)));
document.querySelectorAll('[data-overlay]').forEach((c) => (c.onchange = () => { state.overlays[c.dataset.overlay] = c.checked; pushOverlays(); }));
function openTab(id) {
  document.querySelectorAll('[role=tab]').forEach((t) => { const on = t.id === id; t.setAttribute('aria-selected', String(on)); $(t.getAttribute('aria-controls')).hidden = !on; });
}
document.querySelectorAll('[role=tab]').forEach((t) => (t.onclick = () => openTab(t.id)));
$('verdict').onclick = () => openTab('t-check');

window.addEventListener('keydown', (e) => {
  if (e.target.matches('input[type=range]')) return;
  const k = e.key.toLowerCase();
  if ((e.ctrlKey || e.metaKey) && k === 'z') { e.preventDefault(); undo(); return; }
  if ((e.ctrlKey || e.metaKey) && k === 'y') { e.preventDefault(); redo(); return; }
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (/^[1-4]$/.test(e.key)) selectAlgo(['jarvis', 'graham', 'incr', 'delaunay'][+e.key - 1]);
  else if (k === 'a') selectTool('add');
  else if (k === 'm') selectTool('move');
  else if (k === 'x') selectTool('del');
  else if (k === 'f') state.geom.frameall();
});

/* ---------- Démarrage ---------- */
function boot(geom, isUE) {
  state.geom = geom; state.isUE = isUE;
  document.body.classList.toggle('ue', isUE);
  const pill = $('bridge');
  pill.textContent = isUE ? 'ue.geom connecté' : 'simulation';
  pill.classList.toggle('simulated', !isUE);
  pill.title = isUE ? 'Les appels partent vers UGeomBridge (C++)' : 'Hors Unreal : sim.js remplace le C++';
  showAlgo(state.algo); showTool(state.tool); render();
  geom.ready();
  if (isUE) geom.log('Panneau chargé');
}
window.GeomApp = { boot };
