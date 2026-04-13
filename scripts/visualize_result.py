"""
Visualisation des sorties kirigami_batch en mode 2D.
Usage: python visualize_result.py <lifted.obj> <ground.obj> [target.obj]
"""
import sys
import math
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.collections import LineCollection
import numpy as np

def read_obj(path):
    verts, faces = [], []
    with open(path) as f:
        for line in f:
            parts = line.strip().split()
            if not parts: continue
            if parts[0] == 'v':
                verts.append([float(parts[1]), float(parts[2])])
            elif parts[0] == 'f':
                # OBJ indices are 1-based
                face = [int(p.split('/')[0]) - 1 for p in parts[1:]]
                faces.append(face)
    return np.array(verts), faces

def mesh_to_segments(verts, faces):
    """Extract all edges as line segments for LineCollection."""
    segs = []
    seen = set()
    for face in faces:
        n = len(face)
        for i in range(n):
            a, b = face[i], face[(i+1) % n]
            key = (min(a,b), max(a,b))
            if key not in seen:
                seen.add(key)
                segs.append([verts[a], verts[b]])
    return segs

def draw_circle(ax, radius=1.0, n=200, color='red', lw=2.5, label='Target'):
    theta = np.linspace(0, 2*math.pi, n)
    ax.plot(radius * np.cos(theta), radius * np.sin(theta),
            color=color, lw=lw, linestyle='--', label=label, zorder=5)

# ── Argument parsing ──────────────────────────────────────────────
args = sys.argv[1:]
if len(args) < 2:
    print("Usage: python visualize_result.py lifted.obj ground.obj [circle_radius]")
    sys.exit(1)

lifted_path = args[0]
ground_path = args[1]
circle_radius = float(args[2]) if len(args) > 2 else None

# ── Load meshes ───────────────────────────────────────────────────
V_lifted, F_lifted = read_obj(lifted_path)
V_ground, F_ground = read_obj(ground_path)

segs_lifted = mesh_to_segments(V_lifted, F_lifted)
segs_ground = mesh_to_segments(V_ground, F_ground)

# ── Figure ────────────────────────────────────────────────────────
fig, axes = plt.subplots(1, 2, figsize=(14, 6))
fig.suptitle('Kirigami 2D — Résultats d\'optimisation', fontsize=14, fontweight='bold')

titles = ['Déployé (lifted)', 'Replié / Patron (ground)']
segs_list = [segs_lifted, segs_ground]
verts_list = [V_lifted, V_ground]
colors = ['#2196F3', '#4CAF50']

for ax, title, segs, V, color in zip(axes, titles, segs_list, verts_list, colors):
    # Draw mesh edges
    lc = LineCollection(segs, colors=color, linewidths=0.8, alpha=0.8)
    ax.add_collection(lc)
    
    # Mark boundary vertices
    boundary_pts = []
    # Simple heuristic: vertices on the convex hull extremes
    ax.scatter(V[:, 0], V[:, 1], s=3, c='gray', zorder=3)
    
    # Draw target circle (estimate radius from data extent if not given)
    if circle_radius is not None:
        draw_circle(ax, circle_radius, label='Cible (cercle)')
    else:
        # Auto-estimate: use max distance from centroid
        centroid = V.mean(axis=0)
        dists = np.linalg.norm(V - centroid, axis=1)
        r_est = dists.max()
        draw_circle(ax, r_est, label=f'Bord estimé (r={r_est:.3f})')
    
    ax.set_aspect('equal')
    ax.autoscale()
    ax.set_title(title, fontsize=12)
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)
    ax.set_xlabel('X')
    ax.set_ylabel('Y')

plt.tight_layout()
out_path = 'kirigami_result.png'
plt.savefig(out_path, dpi=150, bbox_inches='tight')
print(f"Sauvegardé : {out_path}")
plt.show()
