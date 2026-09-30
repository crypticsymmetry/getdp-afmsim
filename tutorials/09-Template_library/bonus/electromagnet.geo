// Gmsh script describing the geometry of the electromagnet (same geometry as
// in tutorial 4).

Include "electromagnet_common.pro";

SetFactory("OpenCASCADE");
Rectangle(1) = {-dxCore, -dyCore, 0, 2 * dxCore, 2 * dyCore};
Rectangle(2) = {xCoil, -dyCoil / 2, 0, dxCoil, dyCoil};
Rectangle(3) = {-xCoil - dxCoil, -dyCoil / 2, 0, dxCoil, dyCoil};
Disk(4) = {0, 0, 0, rInt};
Disk(5) = {0, 0, 0, rExt};
d = 1.1 * rExt;
Rectangle(6) = {0, 0, 0, d, d};
BooleanIntersection{ Surface{6}; Delete; }{ Surface{1:5}; Delete; }

core() = Closest{0, 0, 0}{ Surface{:}; };
air() = Closest{dxCore + mm, 0, 0}{ Surface{:}; };
airinf() = Closest{rInt + mm, 0, 0}{ Surface{:}; };
indr() = Closest{xCoil + mm, 0, 0}{ Surface{:}; };
Physical Surface("Core", 1) = core(0);
Physical Surface("Air", 2) = air(0);
Physical Surface("AirInf", 3) = airinf(0);
Physical Surface("CoilRight", 4) = indr(0);

bot() = Curve In BoundingBox{-mm, -mm, -mm, d, mm, mm};
left() = Curve In BoundingBox{-mm, -mm, -mm, mm, d, mm};
inf() = CombinedBoundary{ Surface{:}; };
inf() -= {bot(), left()};
Physical Curve("Bottom", 10) = bot();
Physical Curve("Left", 11) = left();
Physical Curve("Inf", 12) = inf();

DefineConstant[
  s = {1, Name "Parameters/}Global mesh size factor"}
];

MeshSize{:} = 12.5 * mm * s;
MeshSize{ PointsOf{ Surface{indr(0)}; } } = 5 * mm * s;
MeshSize{ PointsOf{ Surface{core(0)}; } } = 4 * mm * s;

skin() = Curve In BoundingBox{dxCore - mm, -mm, -mm, dxCore + mm, d, mm};
MeshSize{ PointsOf{ Curve{skin(0)}; } } = 0.5 * mm * s;
