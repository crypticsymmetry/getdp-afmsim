"""Write a copy of a mesh in MSH 2.2, the newest format GetDP reads without Gmsh."""
import sys
from pathlib import Path

import gmsh

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
gmsh.initialize()
gmsh.option.setNumber("General.Terminal", 0)
gmsh.open(str(src))
gmsh.option.setNumber("Mesh.MshFileVersion", 2.2)
gmsh.option.setNumber("Mesh.Binary", 0)
gmsh.write(str(dst))
n_nodes = len(gmsh.model.mesh.getNodes()[0])
groups = gmsh.model.getPhysicalGroups()
gmsh.finalize()
print(f"{dst}: {n_nodes} nodes, {len(groups)} physical groups")
