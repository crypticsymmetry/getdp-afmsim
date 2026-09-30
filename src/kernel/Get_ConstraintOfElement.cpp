// GetDP - Copyright (C) 1997-2026 P. Dular and C. Geuzaine, University of Liege
//
// See the LICENSE.txt file for license information. Please report all
// issues on https://gitlab.onelab.info/getdp/getdp/issues.
//
// Contributor(s):
//   Francois Henrotte
//   Guillaume Demesy

#include <math.h>
#include "ProData.h"
#include "GeoData.h"
#include "Get_DofOfElement.h"
#include "Get_ConstraintOfElement.h"
#include "ExtendedGroup.h"
#include "Cal_Quantity.h"
#include "Cal_Value.h"
#include "MallocUtils.h"
#include "Message.h"
#include "rtree.h"

extern struct Problem Problem_S;
extern struct CurrentData Current;

extern List_T *PreResolutionIndex_L;

extern int Nbr_ElementaryBF;

/* ------------------------------------------------------------------------ */
/*  T r e a t m e n t _ C o n s t r a i n t F o r E l e m e n t             */
/* ------------------------------------------------------------------------ */

void Treatment_ConstraintForElement(struct FunctionSpace *FunctionSpace_P,
                                    struct QuantityStorage *QuantityStorage_P,
                                    int Num_Entity[], int i_Entity,
                                    int i_BFunction, int TypeConstraint)
{
  int Nbr_Constraint, i_Constraint, k, Index_GeoElement;
  List_T *Constraint_L;
  struct ConstraintInFS *Constraint_P;
  struct ConstraintPerRegion *ConstraintPerRegion_P;
  struct GlobalQuantity *GlobalQuantity_P;
  struct Group *GroupEntity_Pr;

  QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint = NONE;
  QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint_Index = -1;

  Constraint_L = FunctionSpace_P->Constraint;
  Nbr_Constraint = List_Nbr(Constraint_L);

  for(i_Constraint = 0;
      i_Constraint < Nbr_Constraint &&
      !QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint;
      i_Constraint++) {
    Constraint_P =
      (struct ConstraintInFS *)List_Pointer(Constraint_L, i_Constraint);
    ConstraintPerRegion_P = Constraint_P->ConstraintPerRegion;

    switch(ConstraintPerRegion_P->Type) {
    case ASSIGN:
    case INIT:
    case ASSIGNFROMRESOLUTION:
    case INITFROMRESOLUTION:
    case CST_LINK:
    case CST_LINKCPLX:

      switch(Constraint_P->QuantityType) {
      case LOCALQUANTITY:

        if(Constraint_P->ReferenceIndex == i_BFunction) {
          GroupEntity_Pr = (struct Group *)List_Pointer(
            Problem_S.Group, Constraint_P->EntityIndex);
          if(Check_IsEntityInExtendedGroup(GroupEntity_Pr,
                                           abs(Num_Entity[i_Entity]), 1)) {
            QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint =
              ConstraintPerRegion_P->Type;
            QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
              .Constraint_Index = i_Constraint;

            if(ConstraintPerRegion_P->Type == ASSIGN ||
               ConstraintPerRegion_P->Type == INIT) {
              bool KeepCurrentElement = false;
              switch(TypeConstraint) {
              case NODESOF:
              case GROUPSOFEDGESONNODESOF:
                Current.NumEntity = abs(Num_Entity[i_Entity]);
                // Note: Current.Element will be set to 0 in
                // Get_ValueForConstraint so that Current.{x,y,z} will be used
                // in CoordXYZ[] functions
                Geo_GetNodesCoordinates(1, &Current.NumEntity, &Current.x,
                                        &Current.y, &Current.z);
                break;
              case VOLUMESOF:
                Current.NumEntity = abs(Num_Entity[i_Entity]);
                KeepCurrentElement = true;
                break;
              case EDGESOF:
                Current.NumEntity = abs(Num_Entity[i_Entity]);
                Current.NumEntityInElement = i_Entity;
                break;
              }
              Get_ValueForConstraint(
                Constraint_P,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value2,
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .TimeFunctionIndex,
                KeepCurrentElement);
            }
            else if(ConstraintPerRegion_P->Type == CST_LINK ||
                    ConstraintPerRegion_P->Type == CST_LINKCPLX) {
              Get_LinkForConstraint(
                Constraint_P, abs(Num_Entity[i_Entity]),
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .CodeEntity_Link,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                  .BasisFunction->Orient,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value);
              if(abs(Num_Entity[i_Entity]) ==
                 QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .CodeEntity_Link)
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint =
                  NONE; // Code linked with itself not allowed
            }
            else {
              Get_PreResolutionForConstraint(
                Constraint_P,
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .TimeFunctionIndex);
            }
          }
        }
        break; /* LOCALQUANTITY */

      case GLOBALQUANTITY:

        GlobalQuantity_P = (struct GlobalQuantity *)List_Pointer(
          FunctionSpace_P->GlobalQuantity, Constraint_P->ReferenceIndex);
        if((GlobalQuantity_P->Type == ALIASOF) &&
           (GlobalQuantity_P->ReferenceIndex == i_BFunction)) {
          GroupEntity_Pr = (struct Group *)List_Pointer(
            Problem_S.Group, Constraint_P->EntityIndex);

          if(List_Search(GroupEntity_Pr->InitialList, &Num_Entity[i_Entity],
                         fcmp_int)) {
            QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint =
              ConstraintPerRegion_P->Type;

            if(ConstraintPerRegion_P->Type == ASSIGN ||
               ConstraintPerRegion_P->Type == INIT) {
              Get_ValueForConstraint(
                Constraint_P,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value2,
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .TimeFunctionIndex);
            }
            else if(ConstraintPerRegion_P->Type == CST_LINK ||
                    ConstraintPerRegion_P->Type == CST_LINKCPLX) {
              // Message::Error("CST_LINK for GlobalQuantity not done yet") ;
              // FIXME: to be validated
              Get_LinkForConstraint(
                Constraint_P, abs(Num_Entity[i_Entity]),
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .CodeEntity_Link,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                  .BasisFunction->Orient,
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value);
              if(abs(Num_Entity[i_Entity]) ==
                 QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .CodeEntity_Link)
                QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint =
                  NONE;
            }
            else {
              Get_PreResolutionForConstraint(
                Constraint_P,
                &QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
                   .TimeFunctionIndex);
            }
          }
        }
        break; /* GLOBALQUANTITY */

      default:
        Message::Error("Unknown type of Quantity in Constraint of type Fixed");
        break;
      }

      break; /* ASSIGN || INIT || ASSIGNFROMRESOLUTION || INITFROMRESOLUTION */

    default: Message::Error("Unknown type of Constraint"); break;
    }

  } /* for i_Constraint ... */

  /* Constraints due to P-refinement */
  if(Current.GeoData->P) {
    Index_GeoElement = Geo_GetGeoElementIndex(Current.Element->GeoElement);
    if(Current.GeoData->P[Index_GeoElement + 1] >= 0 &&
       Current.GeoData->P[Index_GeoElement + 1] <
         QuantityStorage_P->BasisFunction[Nbr_ElementaryBF]
           .BasisFunction->Order) {
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Constraint = ASSIGN;
      for(k = 0; k < Current.NbrHar; k++)
        QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Value[k] = 0.;
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].TimeFunctionIndex = -1;
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ V a l u e F o r C o n s t r a i n t                             */
/* ------------------------------------------------------------------------ */

void Get_ValueForConstraint(struct ConstraintInFS *Constraint_P, double Value[],
                            double Value2[], int *Index_TimeFunction,
                            bool KeepCurrentElement)
{
  int k;
  struct Value Val_Modulus, Val_Modulus2, Val_TimeFunction;

  // Note: Current.{u,v,w} is not defined, so we cannot interpolate expressions
  // in the reference element. We thus set Current.Element=0 and rely on
  // Current.{x,y,z}.
  struct Element *old = Current.Element;

  if(!KeepCurrentElement) Current.Element = 0;

  Get_ValueOfExpression(
    (struct Expression *)List_Pointer(
      Problem_S.Expression,
      Constraint_P->ConstraintPerRegion->Case.Fixed.ExpressionIndex),
    NULL, 0., 0., 0., &Val_Modulus);

  int idx2 = Constraint_P->ConstraintPerRegion->Case.Fixed.ExpressionIndex2;
  if(idx2 >= 0) {
    Get_ValueOfExpression(
      (struct Expression *)List_Pointer(Problem_S.Expression, idx2), NULL, 0.,
      0., 0., &Val_Modulus2);
  }
  else {
    Cal_ZeroValue(&Val_Modulus2);
  }

  *Index_TimeFunction = Constraint_P->ConstraintPerRegion->TimeFunctionIndex;

  if(Current.NbrHar > 1) {
    if(*Index_TimeFunction >= 0) {
      Get_ValueOfExpression(
        (struct Expression *)List_Pointer(
          Problem_S.Expression,
          Constraint_P->ConstraintPerRegion->TimeFunctionIndex),
        NULL, 0., 0., 0., &Val_TimeFunction);
      Cal_ProductValue(&Val_Modulus, &Val_TimeFunction, &Val_Modulus);
    }
    for(k = 0; k < Current.NbrHar; k++) {
      Value[k] = Val_Modulus.Val[MAX_DIM * k];
      Value2[k] = Val_Modulus2.Val[MAX_DIM * k];
    }
  }
  else {
    Value[0] = Val_Modulus.Val[0];
    Value2[0] = Val_Modulus2.Val[0];
    // Set this to zero to avoid having an uninitialized imaginary part if you
    // use a complex arithmetic solver (on a real matrix) --
    // cf. LinAlg_SetScalar() calls in DofData.cpp
    Value[1] = 0.;
    Value2[1] = 0.;
  }

  Current.Element = old;
}

/* ------------------------------------------------------------------------ */
/*  T r e a t m e n t _ C o n s t r a i n t F o r R e g i o n               */
/* ------------------------------------------------------------------------ */

void Treatment_ConstraintForRegion(struct GlobalQuantity *GlobalQuantity_P,
                                   struct FunctionSpace *FunctionSpace_P,
                                   struct QuantityStorage *QuantityStorage_P)
{
  int Nbr_Constraint, i_Constraint;
  List_T *Constraint_L;
  struct ConstraintInFS *Constraint_P;
  struct ConstraintPerRegion *ConstraintPerRegion_P;
  struct Group *GroupEntity_Pr;
  struct GlobalQuantity *GlobalQuantity_Pr;

  QuantityStorage_P->BasisFunction[0].Constraint = NONE;

  Constraint_L = FunctionSpace_P->Constraint;
  Nbr_Constraint = List_Nbr(Constraint_L);

  for(i_Constraint = 0; i_Constraint < Nbr_Constraint &&
                        !QuantityStorage_P->BasisFunction[0].Constraint;
      i_Constraint++) {
    Constraint_P =
      (struct ConstraintInFS *)List_Pointer(Constraint_L, i_Constraint);
    ConstraintPerRegion_P = Constraint_P->ConstraintPerRegion;

    if(Constraint_P->QuantityType == GLOBALQUANTITY) {
      switch(ConstraintPerRegion_P->Type) {
      case ASSIGN:
      case INIT:
      case ASSIGNFROMRESOLUTION:
      case INITFROMRESOLUTION:
      case CST_LINK:
      case CST_LINKCPLX:

        GlobalQuantity_Pr = (struct GlobalQuantity *)List_Pointer(
          FunctionSpace_P->GlobalQuantity, Constraint_P->ReferenceIndex);

        if(GlobalQuantity_Pr == GlobalQuantity_P) {
          GroupEntity_Pr = (struct Group *)List_Pointer(
            Problem_S.Group, Constraint_P->EntityIndex);

          if(/*(GroupEntity_Pr->FunctionType ==
           ((struct Group *)List_Pointer(Problem_S.Group,
           BasisFunction_P->EntityIndex))
           ->FunctionType)  && */
             List_Search(GroupEntity_Pr->InitialList,
                         &QuantityStorage_P->BasisFunction[0].CodeEntity,
                         fcmp_int)) {
            QuantityStorage_P->BasisFunction[0].Constraint =
              ConstraintPerRegion_P->Type;

            if(ConstraintPerRegion_P->Type == ASSIGN ||
               ConstraintPerRegion_P->Type == INIT) {
              Get_ValueForConstraint(
                Constraint_P, QuantityStorage_P->BasisFunction[0].Value,
                QuantityStorage_P->BasisFunction[0].Value2,
                &QuantityStorage_P->BasisFunction[0].TimeFunctionIndex);
            }
            else if(ConstraintPerRegion_P->Type == CST_LINK ||
                    ConstraintPerRegion_P->Type == CST_LINKCPLX) {
              Get_LinkForConstraint(
                Constraint_P, QuantityStorage_P->BasisFunction[0].CodeEntity,
                &QuantityStorage_P->BasisFunction[0].CodeEntity_Link,
                QuantityStorage_P->BasisFunction[0].BasisFunction->Orient,
                QuantityStorage_P->BasisFunction[0].Value);
              if(QuantityStorage_P->BasisFunction[0].CodeEntity ==
                 QuantityStorage_P->BasisFunction[0].CodeEntity_Link)
                QuantityStorage_P->BasisFunction[0].Constraint = NONE;
              /* Code linked with itself not allowed */
            }
            else
              Get_PreResolutionForConstraint(
                Constraint_P,
                &QuantityStorage_P->BasisFunction[0].TimeFunctionIndex);
          }
        }
        break; /* ASSIGN || INIT || ASSIGNFROMRESOLUTION || INITFROMRESOLUTION
                */

      default: Message::Error("Unknown type of Constraint"); break;
      }
    } /* if (GLOBALQUANTITY) ... */

  } /* for i_Constraint ... */
}

/* ------------------------------------------------------------------------ */
/*  G e t _ P r e R e s o l u t i o n F o r C o n s t r a i n t             */
/* ------------------------------------------------------------------------ */

void Get_PreResolutionForConstraint(struct ConstraintInFS *Constraint_P,
                                    int *Index_TimeFunction)
{
  struct PreResolutionInfo PreResolutionInfo_S;

  int fcmp_Resolution_Name(const void *a, const void *b);

  *Index_TimeFunction = Constraint_P->ConstraintPerRegion->TimeFunctionIndex;

  if(Constraint_P->Active.ResolutionIndex < 0)
    if((Constraint_P->Active.ResolutionIndex = List_ISearchSeq(
          Problem_S.Resolution,
          Constraint_P->ConstraintPerRegion->Case.Solve.ResolutionName,
          fcmp_Resolution_Name)) < 0) {
      Message::Error(
        "Unknown ResolutionName '%s' in Constraint",
        Constraint_P->ConstraintPerRegion->Case.Solve.ResolutionName);
    }
  if(List_ISearchSeq(PreResolutionIndex_L,
                     &Constraint_P->Active.ResolutionIndex, fcmp_int) < 0) {
    PreResolutionInfo_S.Index = Constraint_P->Active.ResolutionIndex;
    PreResolutionInfo_S.Type = PR_CONSTRAINT;
    List_Add(PreResolutionIndex_L, &PreResolutionInfo_S);
    Message::Info("  Adding Resolution '%s' for Pre-Resolution (Constraint)",
                  Constraint_P->ConstraintPerRegion->Case.Solve.ResolutionName);
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ L i n k F o r C o n s t r a i n t   &   C o                     */
/* ------------------------------------------------------------------------ */

struct TwoIntOneDouble {
  int Int1, Int2;
  double Double, Double2;
};

void Get_LinkForConstraint(struct ConstraintInFS *Constraint_P, int Num_Entity,
                           int *CodeEntity_Link, int Orient, double Value[])
{
  // Note: Current.{u,v,w} is not defined, so we cannot interpolate expressions
  // in the reference element. We thus set Current.Element=0 and rely on
  // Current.{x,y,z}.
  struct Element *old = Current.Element;
  Current.Element = 0;

  struct TwoIntOneDouble *TwoIntOneDouble_P;
  List_T *Couples_L;

  void Generate_Link(struct ConstraintInFS * Constraint_P, int Flag_New);

  if(!Constraint_P->Active.Active)
    Generate_Link(Constraint_P, 1);
  else if(Constraint_P->Active.Active->TimeStep != (int)Current.TimeStep)
    Generate_Link(Constraint_P, 0);
  else if(Constraint_P->Active.Active->SubTimeStep != Current.SubTimeStep)
    Generate_Link(Constraint_P, 0); /* +++ */

  TwoIntOneDouble_P =
    (struct TwoIntOneDouble
       *)((Couples_L = Constraint_P->Active.Active->Case.Link.Couples) ?
            List_PQuery(Couples_L, &Num_Entity, fcmp_absint) :
            NULL);

  if(TwoIntOneDouble_P) {
    *CodeEntity_Link = abs(TwoIntOneDouble_P->Int2);
    Value[0] = TwoIntOneDouble_P->Double;
    if(Orient && TwoIntOneDouble_P->Int1 < 0) Value[0] *= -1.;
    Value[1] = TwoIntOneDouble_P->Double2; /* LinkCplx */
    if(Orient && TwoIntOneDouble_P->Int1 < 0) Value[1] *= -1.;
  }
  else {
    Message::Warning("Did not find link for entity %d", Num_Entity);
  }
  Current.Element = old;
}

/* Utility structs and classes for node, edge and face connections */

struct NodeXYZ {
  int NumNode;
  double x, y, z;
};

struct EdgeXYZ {
  int NumEdge;
  int Node1, Node2;
  double x, y, z;
};

struct FacetXYZ {
  int NumFacet;
  int Node1, Node2, Node3, Node4;
  double x, y, z;
};

void Generate_ElementaryEntities_EdgeXYZ(List_T *InitialList,
                                         List_T **ExtendedList)
{
  Tree_T *Entity_Tr;
  struct Geo_Element *GeoElement;
  int Nbr_Element, i_Element;
  int Nbr_Entity = 0, i_Entity, *Num_Entities = NULL;

  struct EdgeXYZ EdgeXYZ;
  int *Num_Nodes;

  if(InitialList != NULL) {
    Entity_Tr = Tree_Create(sizeof(struct EdgeXYZ), fcmp_int);

    Nbr_Element = Geo_GetNbrGeoElements();
    for(i_Element = 0; i_Element < Nbr_Element; i_Element++) {
      GeoElement = Geo_GetGeoElement(i_Element);

      if(List_Search(InitialList, &GeoElement->Region, fcmp_int)) {
        if(GeoElement->NbrEdges == 0) Geo_CreateEdgesOfElement(GeoElement);
        Nbr_Entity = GeoElement->NbrEdges;
        Num_Entities = GeoElement->NumEdges;
        for(i_Entity = 0; i_Entity < Nbr_Entity; i_Entity++) {
          EdgeXYZ.NumEdge = abs(Num_Entities[i_Entity]);
          Num_Nodes = Geo_GetNodesOfEdgeInElement(GeoElement, i_Entity);
          EdgeXYZ.Node1 = GeoElement->NumNodes[abs(Num_Nodes[0]) - 1];
          EdgeXYZ.Node2 = GeoElement->NumNodes[abs(Num_Nodes[1]) - 1];
          double x1, y1, z1, x2, y2, z2;
          Geo_GetNodesCoordinates(1, &EdgeXYZ.Node1, &x1, &y1, &z1);
          Geo_GetNodesCoordinates(1, &EdgeXYZ.Node2, &x2, &y2, &z2);
          EdgeXYZ.x = 0.5 * (x1 + x2);
          EdgeXYZ.y = 0.5 * (y1 + y2);
          EdgeXYZ.z = 0.5 * (z1 + z2);
          if(!Tree_Search(Entity_Tr, &EdgeXYZ)) Tree_Add(Entity_Tr, &EdgeXYZ);
        }
      }
    }

    *ExtendedList = Tree2List(Entity_Tr);
    Tree_Delete(Entity_Tr);
  }
  else
    *ExtendedList = NULL;
}

void Generate_ElementaryEntities_FacetXYZ(List_T *InitialList,
                                          List_T **ExtendedList)
{
  Tree_T *Entity_Tr;
  struct Geo_Element *GeoElement;
  int Nbr_Element, i_Element;
  int Nbr_Entity = 0, i_Entity, *Num_Entities = NULL;

  struct FacetXYZ FacetXYZ;
  int *Num_Nodes;

  if(InitialList != NULL) {
    Entity_Tr = Tree_Create(sizeof(struct FacetXYZ), fcmp_int);

    Nbr_Element = Geo_GetNbrGeoElements();
    for(i_Element = 0; i_Element < Nbr_Element; i_Element++) {
      GeoElement = Geo_GetGeoElement(i_Element);

      if(List_Search(InitialList, &GeoElement->Region, fcmp_int)) {
        if(GeoElement->NbrEdges == 0) Geo_CreateEdgesOfElement(GeoElement);
        if(GeoElement->NbrFacets == 0) Geo_CreateFacetsOfElement(GeoElement);
        Nbr_Entity = GeoElement->NbrFacets;
        Num_Entities = GeoElement->NumFacets;
        for(i_Entity = 0; i_Entity < Nbr_Entity; i_Entity++) {
          FacetXYZ.NumFacet = abs(Num_Entities[i_Entity]);
          Num_Nodes = Geo_GetNodesOfFacetInElement(GeoElement, i_Entity);
          FacetXYZ.Node1 = GeoElement->NumNodes[abs(Num_Nodes[0]) - 1];
          FacetXYZ.Node2 = GeoElement->NumNodes[abs(Num_Nodes[1]) - 1];
          FacetXYZ.Node3 = GeoElement->NumNodes[abs(Num_Nodes[2]) - 1];
          FacetXYZ.Node4 =
            Num_Nodes[3] ? GeoElement->NumNodes[abs(Num_Nodes[3]) - 1] : -1;
          double x1, y1, z1, x2, y2, z2, x3, y3, z3;
          Geo_GetNodesCoordinates(1, &FacetXYZ.Node1, &x1, &y1, &z1);
          Geo_GetNodesCoordinates(1, &FacetXYZ.Node2, &x2, &y2, &z2);
          Geo_GetNodesCoordinates(1, &FacetXYZ.Node3, &x3, &y3, &z3);
          if(Num_Nodes[3]) {
            double x4, y4, z4;
            Geo_GetNodesCoordinates(1, &FacetXYZ.Node4, &x4, &y4, &z4);
            FacetXYZ.x = 0.25 * (x1 + x2 + x3 + x4);
            FacetXYZ.y = 0.25 * (y1 + y2 + y3 + y4);
            FacetXYZ.z = 0.25 * (z1 + z2 + z3 + z4);
          }
          else {
            FacetXYZ.x = (x1 + x2 + x3) / 3.;
            FacetXYZ.y = (y1 + y2 + y3) / 3.;
            FacetXYZ.z = (z1 + z2 + z3) / 3.;
          }
          if(!Tree_Search(Entity_Tr, &FacetXYZ)) Tree_Add(Entity_Tr, &FacetXYZ);
        }
      }
    }

    *ExtendedList = Tree2List(Entity_Tr);
    Tree_Delete(Entity_Tr);
  }
  else
    *ExtendedList = NULL;
}

bool SameOrientation_EdgeXYZ(EdgeXYZ &e, EdgeXYZ &eref, double TOL,
                             int Function_Index, int FunctionRef_Index)
{
  // Instead of mapping both end vertices (which can be wrapped by the
  // periodicity for sliding surfaces), just map an intermediate point... which
  // is not in the middle :-)
  double x1, y1, z1, x2, y2, z2;
  Geo_GetNodesCoordinates(1, &e.Node1, &x1, &y1, &z1);
  Geo_GetNodesCoordinates(1, &e.Node2, &x2, &y2, &z2);
  double x3 = Current.x = 0.25 * x1 + 0.75 * x2;
  double y3 = Current.y = 0.25 * y1 + 0.75 * y2;
  double z3 = Current.z = 0.25 * z1 + 0.75 * z2;
  if(Function_Index >= 0) {
    struct Value Value;
    Get_ValueOfExpressionByIndex(Function_Index, NULL, 0., 0., 0., &Value);
    x3 = Value.Val[0];
    y3 = Value.Val[1];
    z3 = Value.Val[2];
  }

  Geo_GetNodesCoordinates(1, &eref.Node1, &x1, &y1, &z1);
  Geo_GetNodesCoordinates(1, &eref.Node2, &x2, &y2, &z2);
  double x3ref = Current.x = 0.25 * x1 + 0.75 * x2;
  double y3ref = Current.y = 0.25 * y1 + 0.75 * y2;
  double z3ref = Current.z = 0.25 * z1 + 0.75 * z2;
  if(FunctionRef_Index >= 0) {
    struct Value Value;
    Get_ValueOfExpressionByIndex(FunctionRef_Index, NULL, 0., 0., 0., &Value);
    x3ref = Value.Val[0];
    y3ref = Value.Val[1];
    z3ref = Value.Val[2];
  }

  if((fabs(x3 - x3ref) > TOL) || (fabs(y3 - y3ref) > TOL) ||
     (fabs(z3 - z3ref) > TOL))
    return false;
  return true;
}

class NodeXYZRTree {
private:
  RTree<struct NodeXYZ *, double, 3, double> *_rtree;
  double _tol;
  static bool rtree_callback(struct NodeXYZ *v, void *ctx)
  {
    struct NodeXYZ **out = static_cast<NodeXYZ **>(ctx);
    *out = v;
    return false; // we're done searching
  }

public:
  NodeXYZRTree(double tolerance = 1.e-8)
  {
    _rtree = new RTree<struct NodeXYZ *, double, 3, double>();
    _tol = tolerance;
  }
  ~NodeXYZRTree()
  {
    _rtree->RemoveAll();
    delete _rtree;
  }
  void insert(struct NodeXYZ *v)
  {
    struct NodeXYZ *out;
    double _min[3] = {v->x - _tol, v->y - _tol, v->z - _tol};
    double _max[3] = {v->x + _tol, v->y + _tol, v->z + _tol};
    if(!_rtree->Search(_min, _max, rtree_callback, &out)) {
      _rtree->Insert(_min, _max, v);
    }
    else {
      Message::Warning("Node %d (%.16g, %.16g, %.16g) already exists "
                       "with tolerance %g: node %d (%.16g, %.16g, %.16g)",
                       v->NumNode, v->x, v->y, v->z, _tol, out->NumNode, out->x,
                       out->y, out->z);
    }
  }
  struct NodeXYZ *find(struct NodeXYZ *n)
  {
    struct NodeXYZ *out;
    double _min[3] = {n->x - _tol, n->y - _tol, n->z - _tol};
    double _max[3] = {n->x + _tol, n->y + _tol, n->z + _tol};
    if(_rtree->Search(_min, _max, rtree_callback, &out)) { return out; }
    else {
      Message::Warning("Could not find node corresponding to reference node "
                       "%d (%g, %g, %g)",
                       n->NumNode, n->x, n->y, n->z);
      return 0;
    }
  }
};

class EdgeXYZRTree {
private:
  RTree<struct EdgeXYZ *, double, 3, double> *_rtree;
  double _tol;
  static bool rtree_callback(struct EdgeXYZ *v, void *ctx)
  {
    struct EdgeXYZ **out = static_cast<EdgeXYZ **>(ctx);
    *out = v;
    return false; // we're done searching
  }

public:
  EdgeXYZRTree(double tolerance = 1.e-8)
  {
    _rtree = new RTree<struct EdgeXYZ *, double, 3, double>();
    _tol = tolerance;
  }
  ~EdgeXYZRTree()
  {
    _rtree->RemoveAll();
    delete _rtree;
  }
  void insert(struct EdgeXYZ *v)
  {
    struct EdgeXYZ *out;
    double _min[3] = {v->x - _tol, v->y - _tol, v->z - _tol};
    double _max[3] = {v->x + _tol, v->y + _tol, v->z + _tol};
    if(!_rtree->Search(_min, _max, rtree_callback, &out)) {
      _rtree->Insert(_min, _max, v);
    }
    else {
      Message::Warning("Edge %d (nodes %d %d, barycenter %g %g %g) already "
                       "exists with tolerance %g: edge %d (nodes %d %d)",
                       v->NumEdge, v->Node1, v->Node2, v->x, v->y, v->z, _tol,
                       out->NumEdge, out->Node1, out->Node2);
    }
  }
  struct EdgeXYZ *find(struct EdgeXYZ *n)
  {
    struct EdgeXYZ *out;
    double _min[3] = {n->x - _tol, n->y - _tol, n->z - _tol};
    double _max[3] = {n->x + _tol, n->y + _tol, n->z + _tol};
    if(_rtree->Search(_min, _max, rtree_callback, &out)) { return out; }
    else {
      Message::Warning("Could not find edge corresponding to reference edge "
                       "%d (nodes %d %d, barycenter %g %g %g)",
                       n->NumEdge, n->Node1, n->Node2, n->x, n->y, n->z);
      return 0;
    }
  }
};

class FacetXYZRTree {
private:
  RTree<struct FacetXYZ *, double, 3, double> *_rtree;
  double _tol;
  static bool rtree_callback(struct FacetXYZ *v, void *ctx)
  {
    struct FacetXYZ **out = static_cast<FacetXYZ **>(ctx);
    *out = v;
    return false; // we're done searching
  }

public:
  FacetXYZRTree(double tolerance = 1.e-8)
  {
    _rtree = new RTree<struct FacetXYZ *, double, 3, double>();
    _tol = tolerance;
  }
  ~FacetXYZRTree()
  {
    _rtree->RemoveAll();
    delete _rtree;
  }
  void insert(struct FacetXYZ *v)
  {
    struct FacetXYZ *out;
    double _min[3] = {v->x - _tol, v->y - _tol, v->z - _tol};
    double _max[3] = {v->x + _tol, v->y + _tol, v->z + _tol};
    if(!_rtree->Search(_min, _max, rtree_callback, &out)) {
      _rtree->Insert(_min, _max, v);
    }
    else {
      Message::Warning("Facet %d (nodes %d %d %d, barycenter %g %g %g) already "
                       "exists with tolerance %g: facet %d (nodes %d %d %d)",
                       v->NumFacet, v->Node1, v->Node2, v->Node3, v->x, v->y,
                       v->z, _tol, out->NumFacet, out->Node1, out->Node2,
                       out->Node3);
    }
  }
  struct FacetXYZ *find(struct FacetXYZ *n)
  {
    struct FacetXYZ *out;
    double _min[3] = {n->x - _tol, n->y - _tol, n->z - _tol};
    double _max[3] = {n->x + _tol, n->y + _tol, n->z + _tol};
    if(_rtree->Search(_min, _max, rtree_callback, &out)) { return out; }
    else {
      Message::Warning("Could not find facet corresponding to reference facet "
                       "%d (nodes %d %d %d, barycenter %g %g %g)",
                       n->NumFacet, n->Node1, n->Node2, n->Node3, n->x, n->y,
                       n->z);
      return 0;
    }
  }
};

/*  G e n e r a t e _ L i n k N o d e s  */

void Generate_LinkNodes(List_T *ExtendedList_L, List_T *ExtendedSuppList_L,
                        struct Group *RegionRef_P, struct Group *SubRegionRef_P,
                        int Index_Filter, int Index_Function,
                        int Index_FunctionRef, int Index_Coef,
                        double ToleranceFactor, List_T *Couples_L)
{
  int Nbr_Entity, i, Nbr_EntityRef, Flag_Filter;
  struct TwoIntOneDouble TwoIntOneDouble;
  struct NodeXYZ NodeXYZ, NodeXYZRef;
  List_T *NodeXYZ_L, *NodeXYZRef_L;
  List_T *ExtendedListRef_L, *ExtendedSuppListRef_L;
  struct Value Value;

  // by default, ToleranceFactor is 1.e-8 (it can be changed with
  // "ToleranceFactor value;" in the Link constraint).
  double TOL = Current.GeoData->CharacteristicLength * ToleranceFactor;

  // We could also use the periodic node information right from the $Periodic
  // section in Gmsh meshes, when available

  // Establish the list of slave nodes, i.e., nodes in "Region" = ExtendedList_L
  // excluding those in "SubRegion" = ExtendedSuppList_L and accounting for a
  // possible user-defined Filter. "Function" is applied to the coordinates of
  // the node before storing it in the list.
  Nbr_Entity = List_Nbr(ExtendedList_L);
  NodeXYZ_L = List_Create(Nbr_Entity, 1, sizeof(struct NodeXYZ));
  for(i = 0; i < Nbr_Entity; i++) {
    List_Read(ExtendedList_L, i, &NodeXYZ.NumNode);
    if(!(ExtendedSuppList_L &&
         List_Search(ExtendedSuppList_L, &NodeXYZ.NumNode, fcmp_int))) {
      Geo_GetNodesCoordinates(1, &NodeXYZ.NumNode, &Current.x, &Current.y,
                              &Current.z);
      Get_ValueOfExpressionByIndex(Index_Function, NULL, 0., 0., 0., &Value);
      Current.x = Value.Val[0];
      Current.y = Value.Val[1];
      Current.z = Value.Val[2];
      if(Index_Filter < 0)
        Flag_Filter = 1;
      else {
        Get_ValueOfExpressionByIndex(Index_Filter, NULL, 0., 0., 0., &Value);
        Flag_Filter = (int)Value.Val[0];
      }

      if(Flag_Filter) {
        NodeXYZ.x = Current.x;
        NodeXYZ.y = Current.y;
        NodeXYZ.z = Current.z;
        List_Add(NodeXYZ_L, &NodeXYZ);
      }
    }
  }
  Nbr_Entity = List_Nbr(NodeXYZ_L);

  // Establish the list of reference (master) nodes, i.e., nodes in "RegionRef"
  // = RegionRef_P->InitialList excluding those in "SubRegionRef" =
  // SubRegionRef_P->InitialList and accounting for a possible user-defined
  // Filter. "FunctionRef" is applied to the coordinates of the node before
  // storing it in the list.
  Generate_ElementaryEntities(RegionRef_P->InitialList, &ExtendedListRef_L,
                              NODESOF);
  if(SubRegionRef_P)
    Generate_ElementaryEntities(SubRegionRef_P->InitialList,
                                &ExtendedSuppListRef_L, NODESOF);
  else
    ExtendedSuppListRef_L = NULL;

  Nbr_EntityRef = List_Nbr(ExtendedListRef_L);
  NodeXYZRef_L = List_Create(Nbr_EntityRef, 1, sizeof(struct NodeXYZ));
  for(i = 0; i < Nbr_EntityRef; i++) {
    List_Read(ExtendedListRef_L, i, &NodeXYZRef.NumNode);
    if(!(ExtendedSuppListRef_L &&
         List_Search(ExtendedSuppListRef_L, &NodeXYZRef.NumNode, fcmp_int))) {
      Geo_GetNodesCoordinates(1, &NodeXYZRef.NumNode, &Current.x, &Current.y,
                              &Current.z);
      if(Index_FunctionRef >= 0) {
        Get_ValueOfExpressionByIndex(Index_FunctionRef, NULL, 0., 0., 0.,
                                     &Value);
        Current.x = Value.Val[0];
        Current.y = Value.Val[1];
        Current.z = Value.Val[2];
      }
      if(Index_Filter < 0)
        Flag_Filter = 1;
      else {
        Get_ValueOfExpressionByIndex(Index_Filter, NULL, 0., 0., 0., &Value);
        Flag_Filter = (int)Value.Val[0];
      }

      if(Flag_Filter) {
        NodeXYZRef.x = Current.x;
        NodeXYZRef.y = Current.y;
        NodeXYZRef.z = Current.z;
        List_Add(NodeXYZRef_L, &NodeXYZRef);
      }
    }
  }
  Nbr_EntityRef = List_Nbr(NodeXYZRef_L);

  // Master (ref) and slave nodes to be paired are now in NodeXYZRef_L and
  // NodeXYZ_L resp. Master nodes are listed with their actual position.  Slave
  // nodes are listed with their position after applying "Function", which maps
  // the coordinates of the slave nodes onto their periodic correspondant in the
  // master surface.  Due to the periodicity, 2 slave nodes can be mapped onto
  // the same location (depending on whether or not the "SubRegions" were given)
  // Fill the rtree with the master (ref) nodes. Filling with the slaves could
  // mean losing some when two are at the same location (and the search for DoFs
  // is performed on slaves)
  NodeXYZRTree rt(TOL);
  List_T *tmp = List_Create(List_Nbr(NodeXYZRef_L), 1, sizeof(struct NodeXYZ));
  List_Copy(NodeXYZRef_L, tmp);
  List_Reset(NodeXYZRef_L);
  for(int i = 0; i < List_Nbr(tmp); i++) {
    rt.insert((struct NodeXYZ *)List_Pointer(tmp, i));
  }
  for(int i = 0; i < List_Nbr(NodeXYZ_L); i++) {
    struct NodeXYZ *n = (struct NodeXYZ *)List_Pointer(NodeXYZ_L, i);
    struct NodeXYZ *ref = rt.find(n); // find matching master node
    if(ref) List_Add(NodeXYZRef_L, ref);
  }
  Nbr_EntityRef = List_Nbr(NodeXYZRef_L);
  List_Delete(tmp);

  if(Nbr_EntityRef != Nbr_Entity) {
    Message::Error(
      "Constraint Link: bad correspondance of number of Nodes (%d, %d)",
      Nbr_Entity, Nbr_EntityRef);
    return;
  }
  // The master node list NodeXYZRef_L has been reordered so that the ith node
  // in NodeXYZ_L is at the same location as the ith node in NodeXYZRef_L.  Two
  // slave nodes can be paired with the same master node.

  Message::Debug("==> List of link for nodes");

  for(i = 0; i < Nbr_Entity; i++) {
    List_Read(NodeXYZ_L, i, &NodeXYZ);
    List_Read(NodeXYZRef_L, i, &NodeXYZRef);

    if((fabs(NodeXYZ.x - NodeXYZRef.x) > TOL) ||
       (fabs(NodeXYZ.y - NodeXYZRef.y) > TOL) ||
       (fabs(NodeXYZ.z - NodeXYZRef.z) > TOL)) {
      Message::Error(
        "Constraint Link: bad correspondance of Nodes (%d, %d)"
        " (%e %e %e), TOL=%g",
        NodeXYZ.NumNode, NodeXYZRef.NumNode, fabs(NodeXYZ.x - NodeXYZRef.x),
        fabs(NodeXYZ.y - NodeXYZRef.y), fabs(NodeXYZ.z - NodeXYZRef.z), TOL);
      return;
    }

    TwoIntOneDouble.Int1 = NodeXYZ.NumNode;
    TwoIntOneDouble.Int2 = NodeXYZRef.NumNode;

    // Computes the coefficient of the link based on the user-defined "Coef"
    // function which determines whether the preimage of the coordinates of the
    // matched stator-slave pair by "Function" is in a positive or a negative
    // periodic copy of the master surface.
    Geo_GetNodesCoordinates(1, &NodeXYZRef.NumNode, &Current.x, &Current.y,
                            &Current.z);
    Get_ValueOfExpressionByIndex(Index_Coef, NULL, 0., 0., 0., &Value);
    TwoIntOneDouble.Double = Value.Val[0];
    if(Current.NbrHar == 1)
      TwoIntOneDouble.Double2 = 0.;
    else
      TwoIntOneDouble.Double2 = Value.Val[MAX_DIM]; // LinkCplx

    List_Add(Couples_L, &TwoIntOneDouble);

    Message::Debug("%d %d : coef %e %e", NodeXYZ.NumNode, NodeXYZRef.NumNode,
                   TwoIntOneDouble.Double, TwoIntOneDouble.Double2);
  }

  List_Delete(NodeXYZ_L);
  List_Delete(NodeXYZRef_L);
}

/*  G e n e r a t e _ L i n k E d g e s  */

void Generate_LinkEdges(struct ConstraintInFS *Constraint_P,
                        struct Group *Group_P, struct Group *RegionRef_P,
                        struct Group *SubRegionRef_P, List_T *Couples_L)
{
  Message::Info("====> Begin Link Edge");

  // Fill ExtendedList_L with the barycenters of the edges of all elements in
  // Group_P->InitialList. Fill ExtendedSuppList_L with the barycenters of the
  // edges of all elements in Group_P->InitialSuppList.

  List_T *ExtendedList_L, *ExtendedSuppList_L;
  Generate_ElementaryEntities_EdgeXYZ(Group_P->InitialList, &ExtendedList_L);
  if(Group_P->InitialSuppList)
    Generate_ElementaryEntities_EdgeXYZ(Group_P->InitialSuppList,
                                        &ExtendedSuppList_L);
  else
    ExtendedSuppList_L = NULL;

  // Add slave edges in the list EdgeXYZ_L from ExtendedList_L excluding
  // possibly those in ExtendedSuppList_L.  No Filter for edges. The function
  // "Function" is applied to the edge barycenter coordinates before storing it
  // in the list.

  int Nbr_Entity = List_Nbr(ExtendedList_L);
  List_T *EdgeXYZ_L = List_Create(Nbr_Entity, 1, sizeof(struct EdgeXYZ));
  for(int i = 0; i < Nbr_Entity; i++) {
    struct EdgeXYZ EdgeXYZ;
    List_Read(ExtendedList_L, i, &EdgeXYZ);
    if(!(ExtendedSuppList_L &&
         List_Search(ExtendedSuppList_L, &EdgeXYZ.NumEdge, fcmp_int))) {
      Current.x = EdgeXYZ.x;
      Current.y = EdgeXYZ.y;
      Current.z = EdgeXYZ.z;
      struct Value Value;
      Get_ValueOfExpressionByIndex(
        Constraint_P->ConstraintPerRegion->Case.Link.FunctionIndex, NULL, 0.,
        0., 0., &Value);
      EdgeXYZ.x = Value.Val[0];
      EdgeXYZ.y = Value.Val[1];
      EdgeXYZ.z = Value.Val[2];
      List_Add(EdgeXYZ_L, &EdgeXYZ);
    }
  }
  Nbr_Entity = List_Nbr(EdgeXYZ_L);

  // Fill ExtendedListRef_L with the barycenters of the edges of all elements in
  // RegionRef_P->InitialList.  Fill ExtendedSuppListRef_L with the barycenters
  // of the edges of all elements in SubRegionRef_P->InitialList.

  List_T *ExtendedListRef_L, *ExtendedSuppListRef_L;
  Generate_ElementaryEntities_EdgeXYZ(RegionRef_P->InitialList,
                                      &ExtendedListRef_L);
  if(SubRegionRef_P)
    Generate_ElementaryEntities_EdgeXYZ(SubRegionRef_P->InitialList,
                                        &ExtendedSuppListRef_L);
  else
    ExtendedSuppListRef_L = NULL;

  // Add master (ref) edges in the list EdgeXYZRef_L from ExtendedListRef_L
  // excluding possibly those in ExtendedSuppListRef_L. The function
  // "FunctionRef" is applied to the edge barycenter coordinates before storing
  // it in the list.

  int Nbr_EntityRef = List_Nbr(ExtendedListRef_L);
  List_T *EdgeXYZRef_L = List_Create(Nbr_EntityRef, 1, sizeof(struct EdgeXYZ));
  for(int i = 0; i < Nbr_EntityRef; i++) {
    struct EdgeXYZ EdgeXYZRef;
    List_Read(ExtendedListRef_L, i, &EdgeXYZRef);
    if(!(ExtendedSuppListRef_L &&
         List_Search(ExtendedSuppListRef_L, &EdgeXYZRef.NumEdge, fcmp_int))) {
      if(Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex >= 0) {
        Current.x = EdgeXYZRef.x;
        Current.y = EdgeXYZRef.y;
        Current.z = EdgeXYZRef.z;
        struct Value Value;
        Get_ValueOfExpressionByIndex(
          Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex, NULL,
          0., 0., 0., &Value);
        EdgeXYZRef.x = Value.Val[0];
        EdgeXYZRef.y = Value.Val[1];
        EdgeXYZRef.z = Value.Val[2];
      }
      List_Add(EdgeXYZRef_L, &EdgeXYZRef);
    }
  }
  Nbr_EntityRef = List_Nbr(EdgeXYZRef_L);

  // fill the rtree with the master (ref) edges
  double TOL = Current.GeoData->CharacteristicLength *
               Constraint_P->ConstraintPerRegion->Case.Link.ToleranceFactor;
  EdgeXYZRTree rt(TOL);
  List_T *tmp = List_Create(List_Nbr(EdgeXYZRef_L), 1, sizeof(struct EdgeXYZ));
  List_Copy(EdgeXYZRef_L, tmp);
  List_Reset(EdgeXYZRef_L);
  for(int i = 0; i < List_Nbr(tmp); i++) {
    rt.insert((struct EdgeXYZ *)List_Pointer(tmp, i));
  }
  for(int i = 0; i < List_Nbr(EdgeXYZ_L); i++) {
    struct EdgeXYZ *n = (struct EdgeXYZ *)List_Pointer(EdgeXYZ_L, i);
    struct EdgeXYZ *ref = rt.find(n);
    if(ref) List_Add(EdgeXYZRef_L, ref);
  }
  Nbr_EntityRef = List_Nbr(EdgeXYZRef_L);
  List_Delete(tmp);

  if(Nbr_EntityRef != Nbr_Entity) {
    Message::Error(
      "Constraint Link: bad correspondance of number of edges (%d, %d)",
      Nbr_Entity, Nbr_EntityRef);
    return;
  }

  for(int i = 0; i < Nbr_Entity; i++) {
    struct EdgeXYZ EdgeXYZ, EdgeXYZRef;
    List_Read(EdgeXYZ_L, i, &EdgeXYZ);
    List_Read(EdgeXYZRef_L, i, &EdgeXYZRef);

    Message::Debug("Final : %d: edge %d (nodes %d %d) / edge %d (nodes %d %d)",
                   i, EdgeXYZ.NumEdge, EdgeXYZ.Node1, EdgeXYZ.Node2,
                   EdgeXYZRef.NumEdge, EdgeXYZRef.Node1, EdgeXYZRef.Node2);

    TwoIntOneDouble TwoIntOneDouble;
    TwoIntOneDouble.Int1 = EdgeXYZ.NumEdge;
    TwoIntOneDouble.Int2 = EdgeXYZRef.NumEdge;

    // Compute coefficient based on the barycenter of the master (ref) edge
    Current.x = EdgeXYZRef.x;
    Current.y = EdgeXYZRef.y;
    Current.z = EdgeXYZRef.z;
    struct Value Value;
    Get_ValueOfExpressionByIndex(
      Constraint_P->ConstraintPerRegion->Case.Link.CoefIndex, NULL, 0., 0., 0.,
      &Value);
    TwoIntOneDouble.Double = Value.Val[0];
    if(Current.NbrHar == 1)
      TwoIntOneDouble.Double2 = 0.;
    else
      TwoIntOneDouble.Double2 = Value.Val[MAX_DIM]; // LinkCplx

    // Set positive orientation for both edges
    if(EdgeXYZ.Node1 > EdgeXYZ.Node2) {
      int tmp = EdgeXYZ.Node1;
      EdgeXYZ.Node1 = EdgeXYZ.Node2;
      EdgeXYZ.Node2 = tmp;
    }
    if(EdgeXYZRef.Node1 > EdgeXYZRef.Node2) {
      int tmp = EdgeXYZRef.Node1;
      EdgeXYZRef.Node1 = EdgeXYZRef.Node2;
      EdgeXYZRef.Node2 = tmp;
    }
    if(!SameOrientation_EdgeXYZ(
         EdgeXYZ, EdgeXYZRef, TOL,
         Constraint_P->ConstraintPerRegion->Case.Link.FunctionIndex,
         Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex))
      TwoIntOneDouble.Int1 *= -1;

    List_Add(Couples_L, &TwoIntOneDouble);
  }

  List_Delete(EdgeXYZ_L);
  List_Delete(EdgeXYZRef_L);

  Message::Info("====> End Link Edge");
}

/*  G e n e r a t e _ L i n k F a c e t s  */

void Generate_LinkFacets(struct ConstraintInFS *Constraint_P,
                         struct Group *Group_P, struct Group *RegionRef_P,
                         struct Group *SubRegionRef_P, List_T *Couples_L)
{
  Message::Info("====> Begin Link Facet");

  // Fill ExtendedList_L with the barycenters of the faces of all elements in
  // Group_P->InitialList. Fill ExtendedSuppList_L with the barycenters of the
  // facets of all elements in Group_P->InitialSuppList.

  List_T *ExtendedList_L, *ExtendedSuppList_L;
  Generate_ElementaryEntities_FacetXYZ(Group_P->InitialList, &ExtendedList_L);
  if(Group_P->InitialSuppList)
    Generate_ElementaryEntities_FacetXYZ(Group_P->InitialSuppList,
                                         &ExtendedSuppList_L);
  else
    ExtendedSuppList_L = NULL;

  // Add slave facets in the list FacetXYZ_L from ExtendedList_L excluding
  // possibly those in ExtendedSuppList_L. No Filter for facets. The function
  // "Function" is applied to the facet barycenter coordinates before storing it
  // in the list.

  int Nbr_Entity = List_Nbr(ExtendedList_L);
  List_T *FacetXYZ_L = List_Create(Nbr_Entity, 1, sizeof(struct FacetXYZ));
  for(int i = 0; i < Nbr_Entity; i++) {
    struct FacetXYZ FacetXYZ;
    List_Read(ExtendedList_L, i, &FacetXYZ);
    if(!(ExtendedSuppList_L &&
         List_Search(ExtendedSuppList_L, &FacetXYZ.NumFacet, fcmp_int))) {
      Current.x = FacetXYZ.x;
      Current.y = FacetXYZ.y;
      Current.z = FacetXYZ.z;
      struct Value Value;
      Get_ValueOfExpressionByIndex(
        Constraint_P->ConstraintPerRegion->Case.Link.FunctionIndex, NULL, 0.,
        0., 0., &Value);
      FacetXYZ.x = Value.Val[0];
      FacetXYZ.y = Value.Val[1];
      FacetXYZ.z = Value.Val[2];
      List_Add(FacetXYZ_L, &FacetXYZ);
    }
  }
  Nbr_Entity = List_Nbr(FacetXYZ_L);

  // Fill ExtendedListRef_L with the barycenters of the facets of all elements
  // in RegionRef_P->InitialList.  Fill ExtendedSuppListRef_L with the
  // barycenters of the facets of all elements in SubRegionRef_P->InitialList.

  List_T *ExtendedListRef_L, *ExtendedSuppListRef_L;
  Generate_ElementaryEntities_FacetXYZ(RegionRef_P->InitialList,
                                       &ExtendedListRef_L);
  if(SubRegionRef_P)
    Generate_ElementaryEntities_FacetXYZ(SubRegionRef_P->InitialList,
                                         &ExtendedSuppListRef_L);
  else
    ExtendedSuppListRef_L = NULL;

  // Add master (ref) facets in the list FacetXYZRef_L from ExtendedListRef_L
  // excluding possibly those in ExtendedSuppListRef_L. The function
  // "FunctionRef" is applied to the facet barycenter coordinates before storing
  // it in the list.

  int Nbr_EntityRef = List_Nbr(ExtendedListRef_L);
  List_T *FacetXYZRef_L =
    List_Create(Nbr_EntityRef, 1, sizeof(struct FacetXYZ));
  for(int i = 0; i < Nbr_EntityRef; i++) {
    struct FacetXYZ FacetXYZRef;
    List_Read(ExtendedListRef_L, i, &FacetXYZRef);
    if(!(ExtendedSuppListRef_L &&
         List_Search(ExtendedSuppListRef_L, &FacetXYZRef.NumFacet, fcmp_int))) {
      if(Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex >= 0) {
        Current.x = FacetXYZRef.x;
        Current.y = FacetXYZRef.y;
        Current.z = FacetXYZRef.z;
        struct Value Value;
        Get_ValueOfExpressionByIndex(
          Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex, NULL,
          0., 0., 0., &Value);
        FacetXYZRef.x = Value.Val[0];
        FacetXYZRef.y = Value.Val[1];
        FacetXYZRef.z = Value.Val[2];
      }
      List_Add(FacetXYZRef_L, &FacetXYZRef);
    }
  }
  Nbr_EntityRef = List_Nbr(FacetXYZRef_L);

  // fill the rtree with the master (ref) facets
  double TOL = Current.GeoData->CharacteristicLength *
               Constraint_P->ConstraintPerRegion->Case.Link.ToleranceFactor;
  FacetXYZRTree rt(TOL);
  List_T *tmp =
    List_Create(List_Nbr(FacetXYZRef_L), 1, sizeof(struct FacetXYZ));
  List_Copy(FacetXYZRef_L, tmp);
  List_Reset(FacetXYZRef_L);
  for(int i = 0; i < List_Nbr(tmp); i++) {
    rt.insert((struct FacetXYZ *)List_Pointer(tmp, i));
  }
  for(int i = 0; i < List_Nbr(FacetXYZ_L); i++) {
    struct FacetXYZ *n = (struct FacetXYZ *)List_Pointer(FacetXYZ_L, i);
    struct FacetXYZ *ref = rt.find(n);
    if(ref) List_Add(FacetXYZRef_L, ref);
  }
  Nbr_EntityRef = List_Nbr(FacetXYZRef_L);
  List_Delete(tmp);

  if(Nbr_EntityRef != Nbr_Entity) {
    Message::Error(
      "Constraint Link: bad correspondance of number of facets (%d, %d)",
      Nbr_Entity, Nbr_EntityRef);
    return;
  }

  for(int i = 0; i < Nbr_Entity; i++) {
    struct FacetXYZ FacetXYZ, FacetXYZRef;
    List_Read(FacetXYZ_L, i, &FacetXYZ);
    List_Read(FacetXYZRef_L, i, &FacetXYZRef);

    Message::Debug(
      "Final : %d: facet %d (nodes %d %d %d %d) / facet %d (nodes %d %d %d %d)",
      i, FacetXYZ.NumFacet, FacetXYZ.Node1, FacetXYZ.Node2, FacetXYZ.Node3,
      FacetXYZ.Node4, FacetXYZRef.NumFacet, FacetXYZRef.Node1,
      FacetXYZRef.Node2, FacetXYZRef.Node3, FacetXYZRef.Node4);

    TwoIntOneDouble TwoIntOneDouble;
    TwoIntOneDouble.Int1 = FacetXYZ.NumFacet;
    TwoIntOneDouble.Int2 = FacetXYZRef.NumFacet;

    // Compute coefficient based on the barycenter of the master (ref) facet
    Current.x = FacetXYZRef.x;
    Current.y = FacetXYZRef.y;
    Current.z = FacetXYZRef.z;
    struct Value Value;
    Get_ValueOfExpressionByIndex(
      Constraint_P->ConstraintPerRegion->Case.Link.CoefIndex, NULL, 0., 0., 0.,
      &Value);
    TwoIntOneDouble.Double = Value.Val[0];
    if(Current.NbrHar == 1)
      TwoIntOneDouble.Double2 = 0.;
    else
      TwoIntOneDouble.Double2 = Value.Val[MAX_DIM]; // LinkCplx

    // FIXME: deal with orientation

    List_Add(Couples_L, &TwoIntOneDouble);
  }

  List_Delete(FacetXYZ_L);
  List_Delete(FacetXYZRef_L);

  Message::Info("====> End Link Facet");
}

/*  G e n e r a t e _ L i n k R e g i o n s  */

void Generate_LinkRegions(struct ConstraintInFS *Constraint_P, List_T *Region_L,
                          List_T *RegionRef_L, int Index_Coef,
                          List_T *Couples_L)
{
  struct TwoIntOneDouble TwoIntOneDouble;
  struct Value Value;

  if(List_Nbr(Region_L) > 1 || List_Nbr(RegionRef_L) > 1) {
    Message::Error("More than one region for link type constraint");
    return;
  }

  List_Read(Region_L, 0, &TwoIntOneDouble.Int1);
  List_Read(RegionRef_L, 0, &TwoIntOneDouble.Int2);

  Get_ValueOfExpressionByIndex(Index_Coef, NULL, 0., 0., 0., &Value);
  TwoIntOneDouble.Double = Value.Val[0];
  if(Current.NbrHar == 1)
    TwoIntOneDouble.Double2 = 0.;
  else
    TwoIntOneDouble.Double2 = Value.Val[MAX_DIM]; /* LinkCplx */

  List_Add(Couples_L, &TwoIntOneDouble);
}

/*  G e n e r a t e _ L i n k */

void Generate_Link(struct ConstraintInFS *Constraint_P, int Flag_New)
{
  struct ConstraintActive *Active;
  struct Group *Group_P, *RegionRef_P, *SubRegionRef_P;
  int Nbr_Entity;

  Message::Debug("C o n s t r a i n t   ( L i n k )");

  if(Flag_New)
    Constraint_P->Active.Active =
      (struct ConstraintActive *)Malloc(sizeof(struct ConstraintActive));

  Active = Constraint_P->Active.Active;
  Active->TimeStep = (int)Current.TimeStep;
  Active->SubTimeStep = Current.SubTimeStep;

  Group_P =
    (struct Group *)List_Pointer(Problem_S.Group, Constraint_P->EntityIndex);
  RegionRef_P = (struct Group *)List_Pointer(
    Problem_S.Group,
    Constraint_P->ConstraintPerRegion->Case.Link.RegionRefIndex);
  SubRegionRef_P =
    (Constraint_P->ConstraintPerRegion->Case.Link.SubRegionRefIndex >= 0) ?
      (struct Group *)List_Pointer(
        Problem_S.Group,
        Constraint_P->ConstraintPerRegion->Case.Link.SubRegionRefIndex) :
      NULL;

  if(Group_P->FunctionType == REGION) {
    Nbr_Entity = List_Nbr(Group_P->InitialList);
  }
  else {
    Nbr_Entity = List_Nbr(Group_P->ExtendedList);
  }

  if(Nbr_Entity) {
    if(Flag_New)
      Active->Case.Link.Couples =
        List_Create(Nbr_Entity, 1, sizeof(struct TwoIntOneDouble));
    else
      List_Reset(Active->Case.Link.Couples);
  }
  else {
    Active->Case.Link.Couples = NULL;
    return;
  }

  switch(Group_P->FunctionType) {
  case NODESOF:
    Generate_LinkNodes(
      Group_P->ExtendedList, Group_P->ExtendedSuppList, RegionRef_P,
      SubRegionRef_P, Constraint_P->ConstraintPerRegion->Case.Link.FilterIndex,
      Constraint_P->ConstraintPerRegion->Case.Link.FunctionIndex,
      Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex,
      Constraint_P->ConstraintPerRegion->Case.Link.CoefIndex,
      Constraint_P->ConstraintPerRegion->Case.Link.ToleranceFactor,
      Active->Case.Link.Couples);
    break;
  case EDGESOF:
    Generate_LinkEdges(Constraint_P, Group_P, RegionRef_P, SubRegionRef_P,
                       Active->Case.Link.Couples);
    break;
  case FACETSOF:
    Generate_LinkFacets(Constraint_P, Group_P, RegionRef_P, SubRegionRef_P,
                        Active->Case.Link.Couples);
    {
      // keep track of slave->master nodes on the groups where we have facet
      // links, so that we can retrieve the periodic node information when
      // generating high-order facet functions (BF_Edge_3F), which depend on an
      // ordering based on global node tags
      List_T *ExtendedListNodes_L;
      Generate_ElementaryEntities(Group_P->InitialList, &ExtendedListNodes_L,
                                  NODESOF);
      List_T *CouplesOfNodes_L = List_Create(List_Nbr(ExtendedListNodes_L), 1,
                                             sizeof(struct TwoIntOneDouble));
      Generate_LinkNodes(
        ExtendedListNodes_L, NULL, RegionRef_P, NULL,
        Constraint_P->ConstraintPerRegion->Case.Link.FilterIndex,
        Constraint_P->ConstraintPerRegion->Case.Link.FunctionIndex,
        Constraint_P->ConstraintPerRegion->Case.Link.FunctionRefIndex,
        Constraint_P->ConstraintPerRegion->Case.Link.CoefIndex,
        Constraint_P->ConstraintPerRegion->Case.Link.ToleranceFactor,
        CouplesOfNodes_L);
      if(!Current.GeoData->PeriodicNodes)
        Current.GeoData->PeriodicNodes = new std::map<int, int>;
      for(int i = 0; i < List_Nbr(CouplesOfNodes_L); i++) {
        struct TwoIntOneDouble t;
        List_Read(CouplesOfNodes_L, i, &t);
        (*Current.GeoData->PeriodicNodes)[t.Int1] = t.Int2;
        Message::Debug("Keeping track of periodic node slave %d -> master %d",
                       t.Int1, t.Int2);
      }
      List_Delete(CouplesOfNodes_L);
      List_Delete(ExtendedListNodes_L);
    }
    break;
  case REGION:
    Generate_LinkRegions(Constraint_P, Group_P->InitialList,
                         RegionRef_P->InitialList,
                         Constraint_P->ConstraintPerRegion->Case.Link.CoefIndex,
                         Active->Case.Link.Couples);
    break;
  default:
    Message::Error("Bad function type for Constraint Link: %d",
                   Group_P->FunctionType);
    break;
  }
}
