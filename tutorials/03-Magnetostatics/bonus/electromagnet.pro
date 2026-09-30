// This tutorial revisits the nonlinear magnetostatic model of tutorial 3 and
// hands the nonlinear iteration over to GetDP. In tutorial 3 the linearized
// equations were written out term by term in the Formulation, and the
// iterations were driven by an explicit "While[]" loop that re-assembled the
// system, called "GetResidual[]" and tested the residual by hand. Here the
// tangent contribution is declared with a single "JacNL[]" term, the system is
// assembled and solved incrementally with "GenerateJac[]" and "SolveJac[]", and
// the iterations are driven by the built-in "IterativeLoop[]" resolution
// operation, which applies the increment and monitors convergence on its own.
//
// The physical model, the weak formulation, the function space, the material
// law and the boundary conditions are exactly those of tutorial 3, and are not
// repeated here. Two things differ: the core is always nonlinear (the linear
// option would make the iterative loop pointless), and the geometry is the
// quarter model, taking advantage of both symmetries.

Include "electromagnet_common.pro";

Group {
  // Physical regions:
  Core = Region[ 1 ];
  Air = Region[ 2 ];
  AirInf = Region[ 3 ];
  CoilRight = Region[ 4 ];
  Bottom = Region[ 10 ];
  Left = Region[ 11 ];
  Inf = Region[ 12 ];

  // Abstract regions (see tutorial 3). The core always carries the nonlinear
  // constitutive law, so "Vol_NL_Mag" is never empty:
  Vol_Mag = Region[ {Air, AirInf, Core, CoilRight} ];
  Vol_S_Mag = Region[ CoilRight ];
  Vol_Inf_Mag = Region[ AirInf ];
  Sur_Neu_Mag = Region[ {} ]; // empty
  Vol_NL_Mag = Region[ Core ];
  Vol_L_Mag = Region[ {Vol_Mag, -Vol_NL_Mag} ];
}

Function {
  DefineConstant[
    NewtonRaphson = {1, Choices {0="Picard", 1="Newton-Raphson"},
      Name "Parameters/Linearization method"}
    Current = {1, Min 0.01, Max 100, Step 0.1,
      Name "Parameters/Current [A]"}
  ];

  mu0 = 4.e-7 * Pi;
  nu0 = 1 / mu0;
  nu [ Region[{Air, AirInf, CoilRight}] ] = nu0;

  // Same b-h curve samples and same Akima interpolation of "nu(|b|^2)" as in
  // tutorial 3:
  data_h() = {
    0.0000e+00, 5.5023e+00, 1.1018e+01, 1.6562e+01, 2.2149e+01, 2.7798e+01,
    3.3528e+01, 3.9363e+01, 4.5335e+01, 5.1479e+01, 5.7842e+01, 6.4481e+01,
    7.1470e+01, 7.8906e+01, 8.6910e+01, 9.5644e+01, 1.0532e+02, 1.1620e+02,
    1.2868e+02, 1.4322e+02, 1.6050e+02, 1.8139e+02, 2.0711e+02, 2.3932e+02,
    2.8028e+02, 3.3314e+02, 4.0231e+02, 4.9395e+02, 6.1678e+02, 7.8320e+02,
    1.0110e+03, 1.3257e+03, 1.7645e+03, 2.3819e+03, 3.2578e+03, 4.5110e+03,
    6.3187e+03, 8.9478e+03, 1.2802e+04, 1.8500e+04, 2.6989e+04, 3.9739e+04,
    5.9047e+04, 8.8520e+04, 1.3388e+05, 2.0425e+05, 3.1434e+05, 4.8796e+05,
    7.6403e+05
  };
  data_b() = {
    0.0000e+00, 5.0000e-02, 1.0000e-01, 1.5000e-01, 2.0000e-01, 2.5000e-01,
    3.0000e-01, 3.5000e-01, 4.0000e-01, 4.5000e-01, 5.0000e-01, 5.5000e-01,
    6.0000e-01, 6.5000e-01, 7.0000e-01, 7.5000e-01, 8.0000e-01, 8.5000e-01,
    9.0000e-01, 9.5000e-01, 1.0000e+00, 1.0500e+00, 1.1000e+00, 1.1500e+00,
    1.2000e+00, 1.2500e+00, 1.3000e+00, 1.3500e+00, 1.4000e+00, 1.4500e+00,
    1.5000e+00, 1.5500e+00, 1.6000e+00, 1.6500e+00, 1.7000e+00, 1.7500e+00,
    1.8000e+00, 1.8500e+00, 1.9000e+00, 1.9500e+00, 2.0000e+00, 2.0500e+00,
    2.1000e+00, 2.1500e+00, 2.2000e+00, 2.2500e+00, 2.3000e+00, 2.3500e+00,
    2.4000e+00
  };

  data_nu = data_h() / data_b();
  data_nu(0) = data_nu(1);
  data_b2_nu = ListAlt[data_b()^2, data_nu()];
  nu[ Core ] = InterpolationAkima[ SquNorm[$1] ]{ data_b2_nu() };
  dnudb2[ Core ] = dInterpolationAkima[ SquNorm[$1] ]{ data_b2_nu() };

  // Tutorial 3 assembled the full Jacobian of the constitutive law,
  //
  //   dh/db = I nu + 2 nu'(|b|^2) b b^T,
  //
  // and used it in two Formulation terms that cancelled its "I nu" part. Here
  // we only need the second, rank-one term, for the reason explained before the
  // Formulation below:
  dhdb_NL[ Core ] = 2 * dnudb2[$1#1] * SquDyadicProduct[#1];

  // Number of turns in the coil:
  NbTurns = 1000;

  // Current density in the inductor, along the z-axis:
  js[ CoilRight ] = Vector[0, 0, -NbTurns * Current / (dxCoil * dyCoil)];

  // Parameters of the iterative loop below. Note that, unlike the hand-written
  // loop of tutorial 3, "IterativeLoop[]" takes a single (relative) tolerance;
  // an absolute tolerance can be specified with "IterativeLoopAdvanced[]",
  // mentioned at the end of this file. The relaxation factor multiplies the
  // correction applied at each iteration: 1 means a full step, and values below
  // 1 under-relax the iteration, which can rescue a diverging Newton-Raphson
  // sequence on a steeply saturating b-h curve at the price of more iterations:
  NLTolRel = 1e-6;
  NLIterMax = 20;
  NLRelax = 1.0;
}

Constraint {
  { Name a_Mag_2D;
    Case {
      { Region Left; Value 0.; }
      { Region Inf; Value 0.; }
    }
  }
}

Group {
  Dom_Hcurl_a_Mag_2D = Region[ {Vol_Mag, Sur_Neu_Mag} ];
}

FunctionSpace {
  { Name Hcurl_a_Mag_2D; Type Form1P;
    BasisFunction {
      { Name se; NameOfCoef ae; Function BF_PerpendicularEdge;
        Support Dom_Hcurl_a_Mag_2D; Entity NodesOf[ All ]; }
    }
    Constraint {
      { NameOfCoef ae; EntityType NodesOf; NameOfConstraint a_Mag_2D; }
    }
  }
}

Jacobian {
  { Name Vol;
    Case {
      { Region Vol_Inf_Mag; Jacobian VolSphShell {rInt, rExt}; }
      { Region All; Jacobian Vol; }
    }
  }
}

Integration {
  { Name Int;
    Case {
      { Type Gauss;
        Case {
          { GeoElement Triangle; NumberOfPoints 1; }
        }
      }
    }
  }
}

// The ingredients above are those of tutorial 3. The difference starts here,
// with the way the Newton-Raphson linearization is written.
//
// Tutorial 3 built, at each iteration "k", a system whose unknown was the
// solution itself, "a_k". Substituting "dh/db = I nu + 2 nu'(|b|^2) b b^T" in
// the Newton-Raphson approximation
//
//   h(b_k) \approx h(b_k-1) + (dh/db)_k-1 (b_k - b_k-1)
//
// leads to
//
//   h(b_k) \approx nu_k-1 b_k-1 + (I nu + 2 nu'(|b|^2) b b^T)_k-1 (b_k - b_k-1)
//                = nu_k-1 b_k + 2 (nu'(|b|^2) b b^T)_k-1 (b_k - b_k-1)
//
// The first term, "nu_k-1 b_k", is an ordinary term in the unknown "a_k", i.e.
// the Picard term. Everything genuinely incremental is concentrated in the
// second one, the rank-one correction "dhdb_NL[]" defined above, which acts on
// "b_k - b_k-1" only. This is the split that the built-in iterative loop in
// GetDP expects:
//
//  - terms written normally contribute to the system matrix "A" and its
//    right-hand side "b", so that the residual of the nonlinear problem is
//    "r(x) = b - A(x) x";
//  - terms marked "JacNL[]" contribute to the tangent (Jacobian) matrix only:
//    "J = A + JacNL".
//
// "GenerateJac[]" assembles "J" and "r" at the current iterate, and
// "SolveJac[]" solves "J dx = r" and updates the solution as "x <- x + relax *
// dx". There is no need to write the increment anywhere: it is what the linear
// solve returns.
//
// Note that "dhdb_NL[]" is evaluated at the current iterate through its
// argument "{d a}" (with no "Dof"), whereas the unknown of the tangent system
// appears as "Dof{d a}". The register optimization "$1#1" in its definition is
// the one introduced in tutorial 3.
//
// The Picard scheme is then simply the same formulation without the "JacNL[]"
// term. In that case "J = A(x_k)", and the update (with a unit relaxation
// factor)
//
//   x_k+1 = x_k + dx = x_k + A(x_k)^-1 (b - A(x_k) x_k) = A(x_k)^-1 b
//
// is exactly the fixed-point iteration of tutorial 3. In other words,
// "GenerateJac[]" and "SolveJac[]" cover both linearization schemes: which one
// is performed depends only on whether a "JacNL[]" term is present.

Formulation {
  { Name Magnetostatics_a_2D; Type FemEquation;
    Quantity {
      { Name a; Type Local; NameOfSpace Hcurl_a_Mag_2D; }
    }
    Equation {
      Integral { [ nu[] * Dof{d a} , {d a} ];
        In Vol_L_Mag; Jacobian Vol; Integration Int; }

      // The Picard term, with "nu" evaluated at the current iterate and the
      // unknown under "Dof":
      Integral { [ nu[{d a}] * Dof{d a} , {d a} ];
        In Vol_NL_Mag; Jacobian Vol; Integration Int; }

      If(NewtonRaphson)
        // A single additional term turns it into a Newton-Raphson scheme:
        Integral { JacNL [ dhdb_NL[{d a}] * Dof{d a} , {d a} ];
          In Vol_NL_Mag; Jacobian Vol; Integration Int; }
      EndIf

      Integral { [ - js[] , {a} ];
        In Vol_S_Mag; Jacobian Vol; Integration Int; }
    }
  }
}

// The whole iteration of tutorial 3 -- initial solve, re-generation, residual
// computation, convergence test, iteration counter and printout -- now reduces
// to a single "IterativeLoop[]" operation. Its three arguments are the maximum
// number of iterations, the relative error to reach, and the relaxation factor
// multiplying the correction "dx". GetDP stops when either the requested
// accuracy is met or the iteration budget is exhausted, and prints the
// iteration number and the current error as it goes.
//
// Convergence is assessed on the relative change of the solution from one
// iteration to the next, rather than on the residual as in tutorial 3. The two
// criteria are not equivalent -- a stalled iteration with a large residual
// would satisfy the first but not the second -- so on a difficult problem it is
// worth checking that the two agree. The current iteration number and residual
// are available in the resolution as the runtime variables "$Iteration" and
// "$Residual", which can be printed or reused in the resolution.
//
// Beyond brevity, the built-in loop brings two things the hand-written one did
// not have. First, relaxation: "SolveJac[]" applies "relax * dx" instead of
// "dx", which is what makes strongly saturating laws tractable. Second,
// composability: the loop is an ordinary resolution operation, so it can be
// nested inside a "TimeLoopTheta[]" to solve a nonlinear transient problem, and
// its body may drive several systems in turn, which is how staggered coupled
// problems are iterated to convergence (compare with tutorial 7).
//
// When more control is needed, "IterativeLoopAdvanced[]" takes the maximum
// number of iterations and the relaxation factor, followed by a list of systems
// (or post-operations) to assess, each with its own relative and absolute
// tolerances, the quantity to test ("Solution", "Residual" or "RecalcResidual")
// and the norm to use ("L1Norm", "MeanL1Norm", "L2Norm", "MeanL2Norm" or
// "LinfNorm"). The equivalent of the loop below, with an absolute tolerance
// added, would read:
//
//   IterativeLoopAdvanced[NLIterMax, NLRelax,
//     System { {Sys_Mag, NLTolRel, 1e-10, Solution MeanL2Norm} } ] {
//     GenerateJac[Sys_Mag]; SolveJac[Sys_Mag];
//   }
//
// Testing "Residual" only makes sense for Newton-Raphson, and costs one extra
// iteration, since the residual assessed is the one left over from the previous
// iteration; "RecalcResidual" re-assembles it instead, which is more accurate
// but more expensive.

Resolution {
  { Name Mag;
    System {
      { Name Sys_Mag; NameOfFormulation Magnetostatics_a_2D; }
    }
    Operation {
      // The first linearization needs a solution to be evaluated at, so we
      // start the iteration from "a = 0":
      InitSolution[Sys_Mag];

      IterativeLoop[NLIterMax, NLTolRel, NLRelax] {
        GenerateJac[Sys_Mag]; SolveJac[Sys_Mag];
      }

      SaveSolution[Sys_Mag];
    }
  }
}

PostProcessing {
  { Name Mag; NameOfFormulation Magnetostatics_a_2D;
    Quantity {
      { Name a;
        Value {
          Term { [ {a} ]; In Vol_Mag; Jacobian Vol; }
        }
      }
      { Name az; // z-component of the vector potential
        Value {
          Term { [ CompZ[{a}] ]; In Vol_Mag; Jacobian Vol; }
        }
      }
      { Name b;
        Value {
          Term { [ {d a} ]; In Vol_Mag; Jacobian Vol; }
        }
      }
      // Since the core is always nonlinear here, "nu[]" must be given its
      // argument in "Vol_NL_Mag", and the magnetic field is defined piecewise:
      { Name h;
        Value {
          Term { [ nu[] * {d a} ]; In Vol_L_Mag; Jacobian Vol; }
          Term { [ nu[{d a}] * {d a} ]; In Vol_NL_Mag; Jacobian Vol; }
        }
      }
      { Name js;
        Value {
          Term { [ js[] ]; In Vol_S_Mag; Jacobian Vol; }
        }
      }
    }
  }
}

PostOperation {
  { Name Map; NameOfPostProcessing Mag;
    Operation {
      Print[ a, OnElementsOf Vol_Mag, File "a.pos" ];
      Print[ js, OnElementsOf Vol_S_Mag, File "js.pos" ];
      Print[ az, OnElementsOf Vol_Mag, File "az.pos" ];
      // Don't save "b" in the ring region "AirInf"
      Print[ b, OnElementsOf Region[ {Vol_Mag, -AirInf} ], File "b.pos" ];
      Print[ h, OnElementsOf Vol_Mag, File "h.pos" ];
      Print[ b, OnLine{{mm, mm, 0}{rInt, mm, 0}}{50}, File "cutb.pos" ];
    }
  }
}
