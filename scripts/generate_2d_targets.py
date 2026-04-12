import numpy as np
from scipy.spatial import Delaunay
import os
import argparse

def generate_ellipse_mesh(radius_x, radius_y, num_interior_pts=800, num_boundary_pts=150, filename="ellipse.obj"):
    """
    Génère un maillage 2D triangulé pour une ellipse de rayons 'radius_x' et 'radius_y'.
    Exporte le résultat en format Wavefront .obj.
    """
    print(f"Génération de {filename} (rx={radius_x}, ry={radius_y})...")
    
    # 1. Échantillonnage de la frontière (bord de l'ellipse)
    theta_b = np.linspace(0, 2*np.pi, num_boundary_pts, endpoint=False)
    xb = radius_x * np.cos(theta_b)
    yb = radius_y * np.sin(theta_b)
    
    # 2. Échantillonnage de l'intérieur (répartition semi-uniforme)
    # On utilise des coordonnées polaires aléatoires
    r = np.sqrt(np.random.rand(num_interior_pts)) # Racine carrée pour une distribution uniforme de l'aire
    t = np.random.rand(num_interior_pts) * 2 * np.pi
    xi = radius_x * r * np.cos(t)
    yi = radius_y * r * np.sin(t)
    
    # Fusion des points (frontière + intérieur)
    points_2d = np.vstack((np.c_[xb, yb], np.c_[xi, yi]))
    
    # 3. Triangulation de Delaunay
    tri = Delaunay(points_2d)
    
    # L'algorithme de Delaunay sur un nuage de points aléatoires génère le "Convex Hull".
    # Puisque nos points frontières forment l'ellipse convexe, aucun triangle ne débordera de l'ellipse.
    
    # 4. Écriture du fichier algorithme OBJ
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, 'w') as f:
        f.write(f"# 2D Ellipse Mesh - rx:{radius_x} ry:{radius_y}\n")
        # Vertices (V) - OBJ est 1-indexé, donc z=0.0
        for p in points_2d:
            f.write(f"v {p[0]:.6f} {p[1]:.6f} 0.0\n")
        
        # Faces (F)
        for face in tri.simplices:
            f.write(f"f {face[0]+1} {face[1]+1} {face[2]+1}\n")
            
    print(f"-> Succès: {len(points_2d)} sommets, {len(tri.simplices)} faces.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Générer des primitives planes en .obj")
    parser.add_argument("--outdir", default="../data/targets", help="Dossier de sortie")
    args = parser.parse_args()

    outdir = args.outdir
    
    # Génération d'une famille de configurations :
    # 1. Disque parfait
    generate_ellipse_mesh(1.0, 1.0, filename=os.path.join(outdir, "circle_r1.obj"))
    
    # 2. Ellipses d'allongement progressif
    generate_ellipse_mesh(1.5, 1.0, filename=os.path.join(outdir, "ellipse_rx1.5_ry1.obj"))
    generate_ellipse_mesh(2.0, 1.0, filename=os.path.join(outdir, "ellipse_rx2_ry1.obj"))
    generate_ellipse_mesh(3.0, 0.5, filename=os.path.join(outdir, "ellipse_rx3_ry0.5_thin.obj"))

    print("\nToutes les cibles sont générées dans le dossier:", outdir)
