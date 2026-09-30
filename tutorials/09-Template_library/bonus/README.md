# GetDP tutorial 9 bonus: Electromagnet model revisited using template library

The electromagnet models from tutorials 3 (magnetostatics) and 4
(magneto-quasistatics) are reimplemented using the template library introduced
in tutorial 9. The resulting model file is compact, yet supports static,
time-domain and frequency-domain analyses, with or without axisymmetry. Using
the library also makes it straightforward to drive the coil with a global
current constraint instead of an imposed current density.

## Features

 - Single compact `.pro` file driving the template library
 - Static, time-domain and frequency-domain analyses from the same model
 - Axisymmetric option

See the comments in `electromagnet.pro` and `electromagnet.geo` for details.

## Running the tutorial

On the command line:
```
> gmsh electromagnet.geo -2
> getdp electromagnet.pro -solve Mag -pos Map
```

Interactively with Gmsh: open `electromagnet.pro` with "File->Open", then press
"Run".
