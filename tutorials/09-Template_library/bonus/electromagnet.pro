// This tutorial reimplements the electromagnet models from tutorials 3 and 4
// using the "Lib_Magnetoquasistatics_av_2D_Cir.pro" template library. The
// library acts as a black box that hides the complexity of the formulation,
// function spaces, resolution and post-processing: the user only needs to
// define groups, material properties and constraints. The resulting model file
// is compact, yet supports static, time-domain and frequency-domain analyses,
// with or without axisymmetry.

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

  // Abstract regions expected by the library. The inductor is a stranded coil
  // ("Vol_S_Mag"), the core is a massive conductor with eddy currents
  // ("Vol_C_Mag"):
  Vol_Mag     = Region[ {Air, AirInf, Core, CoilRight} ];
  Vol_S_Mag   = Region[ CoilRight ];
  Vol_C_Mag   = Region[ Core ];
  Vol_Inf_Mag = Region[ AirInf ];
}

Function {
  DefineConstant[
    AnalysisType = {0, Choices{0="Static", 1="Time-domain", 2="Frequency-domain"},
      Name "Parameters/Analysis type"}
    Axisymmetric = {0, Choices{0, 1},
      Name "Parameters/Axisymmetric model?"}
    Current = {1, Min 0.01, Max 100, Step 0.1,
      Name "Parameters/Current [A]"}
    f = {5, Min 1, Max 1000, Step 1, Visible AnalysisType > 0,
      Name "Parameters/Frequency [Hz]"}
    mur = {1000, Min 1, Max 5000, Step 1,
      Name "Parameters/Core relative permeability"}
    sigma = {1e6, Min 1, Max 1e6, Step 100,
      Name "Parameters/Core conductivity [S m^-1]"}
  ];

  T = 1 / f;
  dt = T / 20;
  tmax = 2 * T;

  mu0 = 4.e-7 * Pi;
  nu0 = 1 / mu0;
  nu [ Region[{Air, AirInf, CoilRight}] ] = nu0;
  nu [ Core ]  = 1 / (mur * mu0);

  sigma[ Core ] = sigma;
  sigma[ CoilRight ] = sigma;

  If(AnalysisType > 0)
    time_fct[] = F_Sin_wt_p[]{2 * Pi * f, 0};
    SkinDepth = DefineNumber[ 1e3 * Sqrt[2 / (2 * Pi * f * mur * mu0 * sigma)],
      Name "Parameters/Skin depth [mm]", ReadOnly];
  Else
    time_fct[] = 1;
  EndIf

  // Stranded coil parameters: these four definitions are all the library needs
  // to handle the 1000-turn inductor with imposed current:
  Ns[ CoilRight ] = 1000;
  Sc[ CoilRight ] = dxCoil * dyCoil;
  CoefGeo[ CoilRight ] = (Axisymmetric ? 2 * Pi : 1) * (-1);
  CoefGeo[ Core ] = (Axisymmetric ? 2 * Pi : 1);
}

Constraint {
  { Name a_Mag_2D;
    Case {
      { Region Left; Value 0.; }
      { Region Inf; Value 0.; }
    }
  }
  { Name Voltage_Mag_2D;
    Case {
      // Zero voltage on the core
      { Region Core; Value 0; }
    }
  }
  { Name Current_Mag_2D;
    Case {
      // Imposed current in the inductor:
      { Region CoilRight; Value Current; TimeFunction time_fct[]; }
    }
  }
}

// Configure and include the library:
JacRadiusInt = rInt;
JacRadiusExt = rExt;
Freq = f;
TimeFinal = tmax;
DeltaTime = dt;
CoefPower = (AnalysisType == 2) ? 0.5 : 1;
Include "../Lib_Magnetoquasistatics_av_2D_Cir.pro";

PostOperation {
  { Name Map; NameOfPostProcessing Mag;
    Operation {
      Print[ a, OnElementsOf Vol_Mag, File "a.pos" ];
      Print[ az, OnElementsOf Vol_Mag, File "az.pos" ];
      Print[ b, OnElementsOf Vol_Mag, File "b.pos" ];
      Print[ b, OnLine{{mm, mm, 0}{rInt, mm, 0}}{50}, File "cutb.pos" ];
      Print[ j, OnElementsOf Vol_S_Mag, File "js.pos" ];
      Print[ j, OnElementsOf Vol_C_Mag, File "j.pos" ];
      Print[ JouleLosses[Vol_C_Mag], OnGlobal, Format Table, File "losses.txt" ];
    }
  }
}
