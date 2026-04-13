"""
Diagnostic kirigami : faces colorées + vérification des longueurs.
Usage: python diagnose.py <lifted.obj> <ground.obj>
"""
import sys, math, random
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.patches import Polygon as MplPolygon
from matplotlib.collections import PatchCollection, LineCollection
import matplotlib.cm as cm

def read_obj(path):
    verts, faces = [], []
    with open(path) as f:
        for line in f:
            p = line.strip().split()
            if not p: continue
            if p[0] == 'v':
                verts.append([float(p[1]), float(p[2])])
            elif p[0] == 'f':
                face = [int(x.split('/')[0]) - 1 for x in p[1:]]
                faces.append(face)
    return verts, faces

def edge_lengths(verts, faces):
    """Returns list of (length, face_idx, edge_idx) for all edges."""
    result = []
    for fi, face in enumerate(faces):
        n = len(face)
        for j in range(n):
            a, b = face[j], face[(j+1) % n]
            dx = verts[a][0] - verts[b][0]
            dy = verts[a][1] - verts[b][1]
            result.append((math.sqrt(dx*dx + dy*dy), fi, j))
    return result

def draw_mesh_colored(ax, verts, faces, title, circle_r=None):
    colors = cm.tab20.colors
    polys, poly_colors = [], []
    for fi, face in enumerate(faces):
        pts = [[verts[v][0], verts[v][1]] for v in face]
        polys.append(MplPolygon(pts, closed=True))
        poly_colors.append(colors[fi % len(colors)])
    
    pc = PatchCollection(polys, facecolors=poly_colors, edgecolors='black', linewidths=0.8, alpha=0.8)
    ax.add_collection(pc)
    
    # Draw vertices
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    ax.scatter(xs, ys, s=4, c='black', zorder=5)
    
    # Target circle
    if circle_r:
        theta = [2*math.pi*i/200 for i in range(201)]
        ax.plot([circle_r*math.cos(t) for t in theta],
                [circle_r*math.sin(t) for t in theta],
                'r--', lw=2, label=f'Cible (r={circle_r:.3f})')
        ax.legend(fontsize=9)
    
    ax.set_aspect('equal')
    ax.autoscale()
    ax.set_title(title, fontsize=12)
    ax.grid(True, alpha=0.3)

# ── Load ────────────────────────────────────────────────────────────
if len(sys.argv) < 3:
    print("Usage: python diagnose.py lifted.obj ground.obj")
    sys.exit(1)

V_l, F_l = read_obj(sys.argv[1])
V_g, F_g = read_obj(sys.argv[2])

# ── Edge length comparison ──────────────────────────────────────────
print(f"=== Lifted : {len(V_l)} sommets, {len(F_l)} faces ===")
print(f"=== Ground : {len(V_g)} sommets, {len(F_g)} faces ===")
print(f"Même topologie : {len(F_l)==len(F_g) and all(len(F_l[i])==len(F_g[i]) for i in range(len(F_l)))}")

# Compare corresponding edges
if len(F_l) == len(F_g):
    errors, max_err = [], 0
    for fi in range(len(F_l)):
        fl, fg = F_l[fi], F_g[fi]
        n = min(len(fl), len(fg))
        for j in range(n):
            # Lifted edge
            a, b = fl[j], fl[(j+1)%len(fl)]
            ll = math.sqrt((V_l[a][0]-V_l[b][0])**2 + (V_l[a][1]-V_l[b][1])**2)
            # Ground edge
            c, d = fg[j], fg[(j+1)%len(fg)]
            lg = math.sqrt((V_g[c][0]-V_g[d][0])**2 + (V_g[c][1]-V_g[d][1])**2)
            err = abs(ll - lg)
            errors.append(err)
            max_err = max(max_err, err)
    
    avg_err = sum(errors) / len(errors) if errors else 0
    print(f"\n=== Vérification Rigidité (L_lifted = L_ground) ===")
    print(f"  Nombre d'arêtes comparées : {len(errors)}")
    print(f"  Erreur moyenne  : {avg_err:.2e}")
    print(f"  Erreur maximale : {max_err:.2e}")
    if avg_err < 1e-6:
        print("  ✅ Longueurs parfaitement préservées !")
    elif avg_err < 1e-3:
        print("  ⚠️  Légère erreur (acceptable)")
    else:
        print("  ❌ Erreur importante — la rigidité n'est pas bien respectée")

# Check if lifted ≈ ground (no deployment)
if len(V_l) == len(V_g):
    total_diff = sum(math.sqrt((V_l[i][0]-V_g[i][0])**2 + (V_l[i][1]-V_g[i][1])**2)
                     for i in range(len(V_l)))
    print(f"\n=== Déploiement ===")
    print(f"  Diff géométrique lifted vs ground : {total_diff:.4f}")
    if total_diff < 1e-6:
        print("  ❌ Les deux configs sont IDENTIQUES → pas de déploiement !")
    else:
        print("  ✅ Les deux configs sont différentes → déploiement présent")

# ── Visualisation ────────────────────────────────────────────────────
r_max = max(math.sqrt(v[0]**2 + v[1]**2) for v in V_l) if V_l else 0

fig, axes = plt.subplots(1, 2, figsize=(14, 6))
fig.suptitle('Kirigami — Diagnostic (faces colorées)', fontsize=14, fontweight='bold')

draw_mesh_colored(axes[0], V_l, F_l, f'Déployé (lifted)\n{len(V_l)} sommets, {len(F_l)} faces', circle_r=r_max)
draw_mesh_colored(axes[1], V_g, F_g, f'Replié (ground)\n{len(V_g)} sommets, {len(F_g)} faces')

plt.tight_layout()
plt.savefig('kirigami_diagnostic.png', dpi=150, bbox_inches='tight')
print(f"\nImage sauvegardée : kirigami_diagnostic.png")
plt.show()
