# GetDP tutorial 3 bonus: Nonlinear magnetostatics with built-in iterative loops

The nonlinear magnetostatic model of tutorial 3 is solved again, letting GetDP
drive the nonlinear iteration. The Newton-Raphson tangent contribution is
declared with a single `JacNL[]` term instead of being written out as separate
linearized terms, and the hand-written `While[]` loop is replaced by the
built-in `IterativeLoop[]` resolution operation, which assembles and solves the
tangent system with `GenerateJac[]` and `SolveJac[]`, applies the relaxed
increment and monitors convergence on its own. Picard iterations then come for
free, as the same loop without the `JacNL[]` term.

## Features

 - Newton-Raphson linearization through a single `JacNL[]` term
 - Incremental resolution with `GenerateJac[]` and `SolveJac[]`
 - Built-in `IterativeLoop[]` nonlinear loop, with relaxation
 - Finer convergence control with `IterativeLoopAdvanced[]`

See the comments in `electromagnet.pro` and `electromagnet.geo` for details.

## Running the tutorial

On the command line:
```
> gmsh electromagnet.geo -2
> getdp electromagnet.pro -solve Mag -pos Map
```

Interactively with Gmsh: open `electromagnet.pro` with "File->Open", then press
"Run".
