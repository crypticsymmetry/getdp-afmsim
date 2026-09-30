// GetDP - Copyright (C) 1997-2026 P. Dular and C. Geuzaine, University of Liege
//
// See the LICENSE.txt file for license information. Please report all
// issues on https://gitlab.onelab.info/getdp/getdp/issues.

#include <stdlib.h>
#include "ProData.h"
#include "GeoData.h"
#include "DofData.h"
#include "Get_DofOfElement.h"
#include "Get_ConstraintOfElement.h"
#include "ExtendedGroup.h"
#include "Cal_Quantity.h"
#include "Message.h"

extern struct Problem Problem_S;
extern struct CurrentData Current;

extern int TreatmentStatus;

extern double Flag_ORDER;

extern List_T *PreResolutionIndex_L;

struct BasisFunction *BasisFunction_P;
int Nbr_ElementaryBF, Flag_SubSpace;
struct Group *GroupSupport_P, *GroupEntity_P;

/* ------------------------------------------------------------------------ */
/*  G e t _ I n i t D o f O f E l e m e n t                                 */
/* ------------------------------------------------------------------------ */

void Get_InitDofOfElement(struct Element *Element)
{
  Element->ElementTrace = NULL;
  Element->NumLastElementForNodesCoordinates = -1;
  Element->NumLastElementForGroupsOfEntities = -1;
  Element->NumLastElementForSolidAngle = -1;
  Element->NumLastElementForSortedNodesByFacet = -1;
}

/* ------------------------------------------------------------------------ */
/*  G e t _ D o f O f E l e m e n t                                         */
/* ------------------------------------------------------------------------ */

void Get_DofOfElement(struct Element *Element,
                      struct FunctionSpace *FunctionSpace_P,
                      struct QuantityStorage *QuantityStorage_P,
                      List_T *BasisFunctionIndex_L)
{
  struct BasisFunction *BasisFunction_P0;
  int Nbr_BasisFunction, Nbr_BasisFunctionAll, i_BFunction, StartingIndex, i;
  int *BasisFunctionIndex_P0 = NULL;

  Current.Element = Element;
  Nbr_ElementaryBF = 0;

  /* Get the SubSpace */

  Nbr_BasisFunctionAll = List_Nbr(FunctionSpace_P->BasisFunction);
  BasisFunction_P0 =
    (Nbr_BasisFunctionAll) ?
      (struct BasisFunction *)List_Pointer(FunctionSpace_P->BasisFunction, 0) :
      NULL;

  if(!BasisFunctionIndex_L) {
    Flag_SubSpace = 0;
    Nbr_BasisFunction = Nbr_BasisFunctionAll;
  }
  else {
    Flag_SubSpace = 1;
    Nbr_BasisFunction = List_Nbr(BasisFunctionIndex_L);
    BasisFunctionIndex_P0 =
      (Nbr_BasisFunction) ? (int *)List_Pointer(BasisFunctionIndex_L, 0) : NULL;
  }

  /* Set the DofData if explicitely specified */

  switch(TreatmentStatus) {
  case STATUS_CAL:
  case STATUS_POS:
    if(QuantityStorage_P->DefineQuantity->DofData)
      FunctionSpace_P->DofData = QuantityStorage_P->DefineQuantity->DofData;
    else
      FunctionSpace_P->DofData = FunctionSpace_P->MainDofData;
    break;
  }

  /*  For each subset of Basis Functions */

  for(i = 0; i < Nbr_BasisFunction; i++) {
    i_BFunction = (!Flag_SubSpace) ? i : BasisFunctionIndex_P0[i];

    BasisFunction_P = BasisFunction_P0 + i_BFunction;
    GroupSupport_P = (struct Group *)List_Pointer(
      Problem_S.Group, BasisFunction_P->SupportIndex);

    /*  If the BasisFunction exists for this kind of element
       the interpolation order is lower or equal to the maximum order allowed
       the element is in the support of the BasisFunction */

    if((BasisFunction_P->ElementType & Current.Element->Type) &&
       (Flag_ORDER < 0. || BasisFunction_P->Order <= Flag_ORDER) &&
       ((GroupSupport_P->Type == REGIONLIST &&
         List_Search(GroupSupport_P->InitialList, &Element->Region,
                     fcmp_int)) ||
        (GroupSupport_P->Type == ELEMENTLIST &&
         Check_IsEntityInExtendedGroup(GroupSupport_P, Element->Num, 0)))) {
      GroupEntity_P = (struct Group *)List_Pointer(
        Problem_S.Group, BasisFunction_P->EntityIndex);

      switch(GroupEntity_P->FunctionType) {
      case NODESOF:
        Get_CodesOfElement(
          FunctionSpace_P, QuantityStorage_P, Element->GeoElement->NbrNodes,
          Element->GeoElement->NumNodes, 0, i_BFunction, NODESOF, NULL);
        break;

      case EDGESOF:
      case EDGESOFTREEIN:
        if(Element->GeoElement->NbrEdges == 0)
          Geo_CreateEdgesOfElement(Element->GeoElement);
        Get_CodesOfElement(
          FunctionSpace_P, QuantityStorage_P, Element->GeoElement->NbrEdges,
          Element->GeoElement->NumEdges, 0, i_BFunction, EDGESOF, NULL);
        break;

      case FACETSOF:
      case FACETSOFTREEIN:
        if(Element->GeoElement->NbrEdges == 0)
          Geo_CreateEdgesOfElement(Element->GeoElement);
        if(Element->GeoElement->NbrFacets == 0)
          Geo_CreateFacetsOfElement(Element->GeoElement);
        Get_CodesOfElement(
          FunctionSpace_P, QuantityStorage_P, Element->GeoElement->NbrFacets,
          Element->GeoElement->NumFacets, 0, i_BFunction, FACETSOF, NULL);
        break;

      case VOLUMESOF:
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P, 1,
                           &Element->GeoElement->Num, 0, i_BFunction, VOLUMESOF,
                           NULL);
        break;

      case GROUPSOFNODESOF:
        Get_GroupsOfElementaryEntitiesOfElement(
          Element, &StartingIndex, Element->GeoElement->NbrNodes,
          Element->GeoElement->NumNodes, BasisFunction_P);
        Get_CodesOfElement(
          FunctionSpace_P, QuantityStorage_P, Element->NbrGroupsOfEntities,
          Element->NumGroupsOfEntities, StartingIndex, i_BFunction,
          GROUPSOFNODESOF, Element->NumSubFunction[1]);
        break;

      case GROUPSOFEDGESONNODESOF:
        if(Element->GeoElement->NbrEdges == 0)
          Geo_CreateEdgesOfElement(Element->GeoElement);
        Get_GroupsOfEdgesOnNodesOfElement(Element, &StartingIndex);
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P,
                           Element->NbrGroupsOfEntities,
                           Element->NumGroupsOfEntities, StartingIndex,
                           i_BFunction, GROUPSOFEDGESONNODESOF, NULL);
        break;

      case GROUPSOFEDGESOF:
        if(Element->GeoElement->NbrEdges == 0)
          Geo_CreateEdgesOfElement(Element->GeoElement);
        Get_GroupsOfElementaryEntitiesOfElement(
          Element, &StartingIndex, Element->GeoElement->NbrEdges,
          Element->GeoElement->NumEdges, BasisFunction_P);
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P,
                           Element->NbrGroupsOfEntities,
                           Element->NumGroupsOfEntities, StartingIndex,
                           i_BFunction, GROUPSOFEDGESOF, NULL);
        break;

      case GROUPSOFFACETSOF:
        if(Element->GeoElement->NbrEdges == 0)
          Geo_CreateEdgesOfElement(Element->GeoElement);
        if(Element->GeoElement->NbrFacets == 0)
          Geo_CreateFacetsOfElement(Element->GeoElement);
        Get_GroupsOfElementaryEntitiesOfElement(
          Element, &StartingIndex, Element->GeoElement->NbrFacets,
          Element->GeoElement->NumFacets, BasisFunction_P);
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P,
                           Element->NbrGroupsOfEntities,
                           Element->NumGroupsOfEntities, StartingIndex,
                           i_BFunction, GROUPSOFFACETSOF, NULL);
        break;

      case REGION:
        Get_RegionForElement(Element, &StartingIndex, BasisFunction_P);
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P,
                           Element->NbrGroupsOfEntities,
                           Element->NumGroupsOfEntities, StartingIndex,
                           i_BFunction, REGION, Element->NumSubFunction[1]);
        break;

      case GROUPOFREGIONSOF:
        Get_GroupOfRegionsForElement(Element, &StartingIndex, BasisFunction_P);
        Get_CodesOfElement(
          FunctionSpace_P, QuantityStorage_P, Element->NbrGroupsOfEntities,
          Element->NumGroupsOfEntities, StartingIndex, i_BFunction,
          GROUPOFREGIONSOF, Element->NumSubFunction[1]);
        break;

      case GLOBAL:
        Get_GlobalForElement(Element, &StartingIndex, BasisFunction_P);
        Get_CodesOfElement(FunctionSpace_P, QuantityStorage_P,
                           Element->NbrGroupsOfEntities,
                           Element->NumGroupsOfEntities, StartingIndex,
                           i_BFunction, GLOBAL, NULL);
        break;
      }

    } /* if Region ... */

  } /* for i ... */

  QuantityStorage_P->NbrElementaryBasisFunction = Nbr_ElementaryBF;

  if(Nbr_ElementaryBF > NBR_MAX_BASISFUNCTIONS) {
    Message::Fatal("Maximum number of basis functions exceeded: %d > %d "
                   "(recompile with higher value for NBR_MAX_BASISFUNCTIONS)",
                   Nbr_ElementaryBF, NBR_MAX_BASISFUNCTIONS);
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ G r o u p s O f E l e m e n t a r y E n t i t i e s             */
/*                                                       O f E l e m e n t  */
/* ------------------------------------------------------------------------ */

/* Accumulate one occurrence of the element's local entity 'i' (0-based), with
   orientation 'sign', into group 'j'.

   The group generator can legitimately return the same entity many times: gmsh
   writes a cohomology cut as duplicated Line elements, one copy per unit of the
   cochain coefficient, and Generate_GroupsOfEdges() deliberately keeps every
   copy with its sign (it is what makes BF_GroupOfEdges sum the right weight).
   Storing one array slot per occurrence overruns NumEntitiesInGroups[j][] as
   soon as a coefficient is large -- e.g. a single tetrahedron of the terminal
   cut of a 2x20-turn racetrack coil receives 164 occurrences against a bound of
   NBR_MAX_ENTITIES_IN_ELEMENT (60). Accumulating into a signed coefficient is
   mathematically identical to summing one contribution per occurrence, and is
   bounded by the number of distinct entities of the element. */

static bool Add_EntityInGroup(struct Element *Element, int j, int i, int sign)
{
  int k;

  for(k = 0; k < Element->NbrEntitiesInGroups[j]; k++)
    if(Element->NumEntitiesInGroups[j][k] == i + 1) break;

  if(k == Element->NbrEntitiesInGroups[j]) {
    if(k >= NBR_MAX_ENTITIES_IN_ELEMENT) {
      Message::Error("Reached limit number of entities in a group of entities "
                     "(%d): recompile with a higher value for "
                     "NBR_MAX_ENTITIES_IN_ELEMENT",
                     NBR_MAX_ENTITIES_IN_ELEMENT);
      return false;
    }
    Element->NumEntitiesInGroups[j][k] = i + 1;
    Element->CoefEntitiesInGroups[j][k] = 0;
    Element->NbrEntitiesInGroups[j]++;
  }

  Element->CoefEntitiesInGroups[j][k] += sign;

  return true;
}

void Get_GroupsOfElementaryEntitiesOfElement(
  struct Element *Element, int *StartingIndex, int Nbr_ElementaryEntities,
  int Num_ElementaryEntities[], struct BasisFunction *BasisFunction_P)
{
  /* external input/output :  GroupEntity_P     : In  */

  int i, j, Num_Entity, Nbr_SubFunction, i_SF;
  struct TwoInt *Key_P;

  if(Element->NumLastElementForGroupsOfEntities != Element->Num) {
    Element->NumLastElementForGroupsOfEntities = Element->Num;
    Element->NbrGroupsOfEntities = 0;
  }

  *StartingIndex = Element->NbrGroupsOfEntities;

  if(GroupEntity_P->ExtendedList == NULL) Generate_ExtendedGroup(GroupEntity_P);

  for(i = 0; i < Nbr_ElementaryEntities; i++) {
    Num_Entity = abs(Num_ElementaryEntities[i]);

    auto range = GroupEntity_P->ExtendedListForSearch.equal_range(Num_Entity);
    for(auto it = range.first; it != range.second; ++it) {
      Key_P = &it->second;

      j = *StartingIndex;
      while((j < Element->NbrGroupsOfEntities) &&
            (Element->NumGroupsOfEntities[j] != Key_P->Int2))
        j++;

      if(!BasisFunction_P->SubFunction) {
        if(j == Element->NbrGroupsOfEntities) {
          Element->NumSubFunction[1][j] = 0;
          Element->NumSubFunction[0][j] = -1;
          Element->NumGroupsOfEntities[j] = Key_P->Int2;
          Element->NbrEntitiesInGroups[Element->NbrGroupsOfEntities++] = 0;
          if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
            Message::Error("Reached limit number of groups of entities");
            return;
          }
        }
        if(!Add_EntityInGroup(Element, j, i, (Key_P->Int1 > 0) ? 1 : -1)) return;
      }
      else { /* For SubFunctions (basis functions for a global function) */
        Nbr_SubFunction = List_Nbr(BasisFunction_P->SubFunction);

        if(j == Element->NbrGroupsOfEntities) {
          for(i_SF = 0; i_SF < Nbr_SubFunction; i_SF++) {
            Element->NumSubFunction[1][j + i_SF] = i_SF;
            Element->NumSubFunction[0][j + i_SF] =
              *((int *)List_Pointer(BasisFunction_P->SubFunction, i_SF));
            if(BasisFunction_P->SubdFunction)
              Element->NumSubFunction[2][j + i_SF] =
                *((int *)List_Pointer(BasisFunction_P->SubdFunction, i_SF));
            Element->NumGroupsOfEntities[j + i_SF] = Key_P->Int2;
            Element->NbrEntitiesInGroups[Element->NbrGroupsOfEntities++] = 0;
            if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
              Message::Error("Reached limit number of groups of entities");
              return;
            }
          }
        }
        for(i_SF = 0; i_SF < Nbr_SubFunction; i_SF++)
          if(!Add_EntityInGroup(Element, j + i_SF, i,
                                (Key_P->Int1 > 0) ? 1 : -1))
            return;
      }
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ G r o u p s O f E d g e s O n N o d e s O f E l e m e n t       */
/* ------------------------------------------------------------------------ */

void Get_GroupsOfEdgesOnNodesOfElement(struct Element *Element,
                                       int *StartingIndex)
{
  /* external input/output :  GroupEntity_P     : In  */

  int i, j, Num_Edge, *Num_Nodes, Num_Node;

  if(Element->NumLastElementForGroupsOfEntities != Element->Num) {
    Element->NumLastElementForGroupsOfEntities = Element->Num;
    Element->NbrGroupsOfEntities = 0;
  }

  *StartingIndex = Element->NbrGroupsOfEntities;

  if(GroupEntity_P->ExtendedList == NULL) Generate_ExtendedGroup(GroupEntity_P);

  for(i = 0; i < Element->GeoElement->NbrEdges; i++) {
    Num_Edge = abs(Element->GeoElement->NumEdges[i]);
    if(List_Search(GroupEntity_P->ExtendedList, &Num_Edge, fcmp_int)) {
      Num_Nodes = Geo_GetNodesOfEdgeInElement(Element->GeoElement, i);

      Num_Node = Element->GeoElement->NumNodes[abs(Num_Nodes[0]) - 1];
      j = *StartingIndex;
      while((j < Element->NbrGroupsOfEntities) &&
            (Element->NumGroupsOfEntities[j] != Num_Node))
        j++;
      if(j == Element->NbrGroupsOfEntities) {
        Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] = Num_Node;
        if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
          Message::Error("Reached limit number of groups of entities");
          return;
        }
        Element->NbrEntitiesInGroups[j] = 0;
      }
      if(!Add_EntityInGroup(Element, j, i,
                            (Element->GeoElement->NumEdges[i] > 0) ? -1 : 1))
        return;
      /*-        edge
    node 1 o--->---o node 2   =>   (Phi2 - Phi1) s12 ...
    -> minus sign associated with node 1 for positive edge from node 1 to node 2
      */

      Num_Node = Element->GeoElement->NumNodes[abs(Num_Nodes[1]) - 1];
      j = *StartingIndex;
      while((j < Element->NbrGroupsOfEntities) &&
            (Element->NumGroupsOfEntities[j] != Num_Node))
        j++;
      if(j == Element->NbrGroupsOfEntities) {
        Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] = Num_Node;
        if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
          Message::Error("Reached limit number of groups of entities");
          return;
        }
        Element->NbrEntitiesInGroups[j] = 0;
      }
      if(!Add_EntityInGroup(Element, j, i,
                            (Element->GeoElement->NumEdges[i] > 0) ? 1 : -1))
        return;
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ R e g i o n F o r E l e m e n t                                 */
/* ------------------------------------------------------------------------ */

void Get_RegionForElement(struct Element *Element, int *StartingIndex,
                          struct BasisFunction *BasisFunction_P)
{
  int Nbr_SubFunction, i_SF;

  if(Element->NumLastElementForGroupsOfEntities != Element->Num) {
    Element->NumLastElementForGroupsOfEntities = Element->Num;
    Element->NbrGroupsOfEntities = 0;
  }

  *StartingIndex = Element->NbrGroupsOfEntities;

  if(!BasisFunction_P->SubFunction) {
    Element->NumSubFunction[1][Element->NbrGroupsOfEntities] = 0;
    Element->NumSubFunction[0][Element->NbrGroupsOfEntities] = -1;
    Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
      Element->Region;
    if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
      Message::Error("Reached limit number of groups of entities");
      return;
    }
  }
  else { /* For SubFunctions (basis functions for a global function) */
    Nbr_SubFunction = List_Nbr(BasisFunction_P->SubFunction);

    for(i_SF = 0; i_SF < Nbr_SubFunction; i_SF++) {
      Element->NumSubFunction[1][Element->NbrGroupsOfEntities] =
        i_SF; /* Index SF */
      Element->NumSubFunction[0][Element->NbrGroupsOfEntities] =
        *((int *)List_Pointer(BasisFunction_P->SubFunction,
                              i_SF)); /* Index Expression */
      if(BasisFunction_P->SubdFunction)
        Element->NumSubFunction[2][Element->NbrGroupsOfEntities] =
          *((int *)List_Pointer(BasisFunction_P->SubdFunction,
                                i_SF)); /* Index Expression */
      Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
        Element->Region;
      if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
        Message::Error("Reached limit number of groups of entities");
        return;
      }
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ G r o u p O f R e g i o n s F o r E l e m e n t                 */
/* ------------------------------------------------------------------------ */

void Get_GroupOfRegionsForElement(struct Element *Element, int *StartingIndex,
                                  struct BasisFunction *BasisFunction_P)
{
  int Nbr_SubFunction, i_SF;

  if(Element->NumLastElementForGroupsOfEntities != Element->Num) {
    Element->NumLastElementForGroupsOfEntities = Element->Num;
    Element->NbrGroupsOfEntities = 0;
  }

  *StartingIndex = Element->NbrGroupsOfEntities;

  if(!BasisFunction_P->SubFunction) {
    Element->NumSubFunction[1][Element->NbrGroupsOfEntities] = 0;
    Element->NumSubFunction[0][Element->NbrGroupsOfEntities] = -1;
    Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
      GroupEntity_P->Num;
    if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
      Message::Error("Reached limit number of groups of entities");
      return;
    }
  }
  else { /* For SubFunctions (basis functions for a global function) */
    Nbr_SubFunction = List_Nbr(BasisFunction_P->SubFunction);

    for(i_SF = 0; i_SF < Nbr_SubFunction; i_SF++) {
      Element->NumSubFunction[1][Element->NbrGroupsOfEntities] =
        i_SF; /* Index SF */
      Element->NumSubFunction[0][Element->NbrGroupsOfEntities] =
        *((int *)List_Pointer(BasisFunction_P->SubFunction,
                              i_SF)); /* Index Expression */
      if(BasisFunction_P->SubdFunction)
        Element->NumSubFunction[2][Element->NbrGroupsOfEntities] =
          *((int *)List_Pointer(BasisFunction_P->SubdFunction,
                                i_SF)); /* Index Expression */
      Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
        GroupEntity_P->Num;
      if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
        Message::Error("Reached limit number of groups of entities");
        return;
      }
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ G l o b a l F o r E l e m e n t                                 */
/* ------------------------------------------------------------------------ */

void Get_GlobalForElement(struct Element *Element, int *StartingIndex,
                          struct BasisFunction *BasisFunction_P)
{
  int Nbr_Global, i, *Num_Global;

  if(Element->NumLastElementForGroupsOfEntities != Element->Num) {
    Element->NumLastElementForGroupsOfEntities = Element->Num;
    Element->NbrGroupsOfEntities = 0;
  }

  *StartingIndex = Element->NbrGroupsOfEntities;

  Nbr_Global = List_Nbr(GroupEntity_P->InitialList);
  Num_Global =
    (Nbr_Global) ? (int *)List_Pointer(GroupEntity_P->InitialList, 0) : NULL;

  if(BasisFunction_P->GlobalBasisFunction) {
    for(i = 0; i < Nbr_Global; i++) {
      Element->GlobalBasisFunction[Element->NbrGroupsOfEntities] =
        (struct GlobalBasisFunction *)List_Pointer(
          BasisFunction_P->GlobalBasisFunction, i);
      /* Attention: correspondance i-i si liste triee ! fait dans yacc */
      Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
        Num_Global[i];
      if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
        Message::Error("Reached limit number of groups of entities");
        return;
      }
    }

    if(TreatmentStatus == STATUS_PRE)
      Get_PreResolutionForGlobalBasisFunction(Nbr_Global, *StartingIndex,
                                              Element);
  }
  else {
    for(i = 0; i < Nbr_Global; i++) {
      Element->NumGroupsOfEntities[Element->NbrGroupsOfEntities++] =
        Num_Global[i];
      if(Element->NbrGroupsOfEntities >= NBR_MAX_GROUPS_IN_ELEMENT - 1) {
        Message::Error("Reached limit number of groups of entities");
        return;
      }
    }
  }
}

/* ------------------------------------------------------------------------ */
/*  G e t _ C o d e s O f E l e m e n t                                     */
/* ------------------------------------------------------------------------ */

void Get_CodesOfElement(struct FunctionSpace *FunctionSpace_P,
                        struct QuantityStorage *QuantityStorage_P,
                        int Nbr_Entity, int Num_Entity[], int StartingIndex,
                        int i_BFunction, int TypeConstraint,
                        int *Num_SubFunction)
{
  /* external input/output :
       GroupSupport_P    : In
       GroupEntity_P     : In  */

  int i_Entity, CodeExist = 0, Code_BasisFunction;
  struct Dof *Dof_P = NULL;

  /*  1.  F o r   e a c h   e n t i t y   t o   w h i c h   a   b a s i s
          f u n c t i o n   c o u l d   b e   a s s o c i a t e d :
          (Node, Edge, Facet, Volume, GroupOfNodes, Region, ...)  */

  for(i_Entity = StartingIndex; i_Entity < Nbr_Entity; i_Entity++) {
    Code_BasisFunction =
      BasisFunction_P->Num + (Num_SubFunction ? Num_SubFunction[i_Entity] : 0);

    switch(TreatmentStatus) {
    case STATUS_CAL:
    case STATUS_POS:
    case STATUS_CST:
      if(!FunctionSpace_P->DofData) {
        Message::Error("Empty DofData in FunctionSpace '%s' (no unknowns?)",
                       FunctionSpace_P->Name);
        return;
      }
      CodeExist =
        ((Dof_P = Dof_GetDofStruct(FunctionSpace_P->DofData, Code_BasisFunction,
                                   abs(Num_Entity[i_Entity]), 0)) != NULL);

      if(Flag_SubSpace && CodeExist && TreatmentStatus != STATUS_POS)
        CodeExist = Check_IsEntityInExtendedGroup(GroupEntity_P,
                                                  abs(Num_Entity[i_Entity]), 0);
      /* ... parce que le code peut ne pas exister quand sous-espace ! */
      break;

    case STATUS_PRE:
      CodeExist = Check_IsEntityInExtendedGroup(GroupEntity_P,
                                                abs(Num_Entity[i_Entity]), 0);
      break;

    default:
      Message::Error("Unknown TreatmentStatus (%d)", TreatmentStatus);
      return;
    }

    /*  2.  O n e   a s s o c i a t e s   a   b a s i s   f u n c t i o n :  */

    if(CodeExist) {
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].Dof = Dof_P;
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].NumEntityInElement =
        i_Entity;
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].CodeBasisFunction =
        Code_BasisFunction;
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].CodeEntity =
        abs(Num_Entity[i_Entity]);
      QuantityStorage_P->BasisFunction[Nbr_ElementaryBF].BasisFunction =
        BasisFunction_P;

      if(TreatmentStatus == STATUS_PRE ||
         TreatmentStatus == STATUS_CST) /* Associated Contraints? */
        Treatment_ConstraintForElement(FunctionSpace_P, QuantityStorage_P,
                                       Num_Entity, i_Entity, i_BFunction,
                                       TypeConstraint);

      Nbr_ElementaryBF++;

    } /* if CodeExist ... */

  } /* for i_Entity ... */
}

/* ------------------------------------------------------------------------ */
/*  G e t _ D o f O f R e g i o n                                           */
/* ------------------------------------------------------------------------ */

void Get_DofOfRegion(int Num_Region, struct GlobalQuantity *GlobalQuantity_P,
                     struct FunctionSpace *FunctionSpace_P,
                     struct QuantityStorage *QuantityStorage_P)
{
  int CodeExist = 0, Num_BasisFunction, Num_AssociateBasisFunction;
  int Num_Entity = -1;
  struct Dof *Dof_P = NULL;

  Nbr_ElementaryBF = 0;

  BasisFunction_P = (struct BasisFunction *)List_Pointer(
    FunctionSpace_P->BasisFunction, GlobalQuantity_P->ReferenceIndex);
  GroupEntity_P =
    (struct Group *)List_Pointer(Problem_S.Group, BasisFunction_P->EntityIndex);

  if(GroupEntity_P->Type == REGIONLIST &&
     List_Search(GroupEntity_P->InitialList, &Num_Region, fcmp_int)) {
    if(GlobalQuantity_P->Type == ALIASOF) {
      Num_BasisFunction = BasisFunction_P->Num;
      Num_AssociateBasisFunction = 0;
    }
    else {
      Num_BasisFunction = GlobalQuantity_P->Num;
      Num_AssociateBasisFunction = BasisFunction_P->Num;
    }

    if(GroupEntity_P->FunctionType == GROUPOFREGIONSOF)
      Num_Entity = GroupEntity_P->Num;
    else
      Num_Entity = Num_Region;

    switch(TreatmentStatus) {
    case STATUS_CAL:
    case STATUS_POS:
    case STATUS_CST:
      if(!FunctionSpace_P->DofData) {
        Message::Error("Empty DofData in FunctionSpace '%s' (no unknowns?)",
                       FunctionSpace_P->Name);
        return;
      }

      CodeExist =
        ((Dof_P = Dof_GetDofStruct(FunctionSpace_P->DofData, Num_BasisFunction,
                                   Num_Entity, 0)) != NULL);
      break;
    case STATUS_PRE: CodeExist = 1; break;
    default: break;
    }

    if(CodeExist) {
      QuantityStorage_P->BasisFunction[0].Dof = Dof_P;

      QuantityStorage_P->BasisFunction[0].CodeBasisFunction = Num_BasisFunction;
      QuantityStorage_P->BasisFunction[0].CodeEntity = Num_Entity;

      QuantityStorage_P->BasisFunction[0].CodeAssociateBasisFunction =
        Num_AssociateBasisFunction;

      if(TreatmentStatus == STATUS_PRE ||
         TreatmentStatus == STATUS_CST) /* Contrainte associee ? */
        Treatment_ConstraintForRegion(GlobalQuantity_P, FunctionSpace_P,
                                      QuantityStorage_P);
      Nbr_ElementaryBF = 1;

    } /* if CodeExist ... */

  } /* if REGIONLIST ... */

  QuantityStorage_P->NbrElementaryBasisFunction = Nbr_ElementaryBF;
}

/* ------------------------------------------------------------------------ */
/*  G e t _ P r e R e s o l u t i o n F o r GlobalBasisFunction             */
/* ------------------------------------------------------------------------ */

void Get_PreResolutionForGlobalBasisFunction(int Nbr_Global, int StartingIndex,
                                             struct Element *Element)
{
  int i;
  struct PreResolutionInfo PreResolutionInfo_S;

  for(i = 0; i < Nbr_Global; i++)
    if(List_ISearchSeq(
         PreResolutionIndex_L,
         &(Element->GlobalBasisFunction[StartingIndex + i]->ResolutionIndex),
         fcmp_int) < 0) {
      PreResolutionInfo_S.Index =
        Element->GlobalBasisFunction[StartingIndex + i]->ResolutionIndex;
      PreResolutionInfo_S.Type = PR_GLOBALBASISFUNCTION;
      List_Add(PreResolutionIndex_L, &PreResolutionInfo_S);
      Message::Info("  Adding Resolution '%s' for Pre-Resolution (Global BF)",
                    ((struct Resolution *)List_Pointer(
                       Problem_S.Resolution, PreResolutionInfo_S.Index))
                      ->Name);
    }
}
