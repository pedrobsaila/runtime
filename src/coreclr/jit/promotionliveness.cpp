// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "jitpch.h"
#include "promotion.h"

//
// This file implements a specialized liveness analysis for physically promoted struct fields
// and remainders. Unlike standard JIT liveness analysis, it focuses on accurately tracking
// which fields are live at specific program points to optimize physically promoted struct operations.
//
// Key characteristics:
//
// 1. Separate Bit Vectors:
//    - Maintains its own liveness bit vectors separate from the main compiler's bbLiveIn/bbLiveOut
//    - Uses "dense" indices: bit vectors only contain entries for the remainder and replacement
//      fields of physically promoted structs (allocating 1 + num_fields indices per local)
//    - Does not update BasicBlock::bbLiveIn or other standard liveness storage, as this would
//      require allocating regular tracked indices (lvVarIndex) for all new fields
//
// 2. Liveness Representation:
//    - Writes liveness into IR using normal GTF_VAR_DEATH flags
//    - Important: After liveness is computed but before replacement phase completes,
//      GTF_VAR_DEATH semantics temporarily differ from the rest of the JIT
//      (e.g., "LCL_FLD int V16 [+8] (last use)" indicates that specific field is dying,
//      not the whole variable)
//    - For struct uses that can indicate deaths of multiple fields or remainder parts,
//      maintains side information accessed via GetDeathsForStructLocal()
//
// 3. Analysis Process:
//    - Single-pass dataflow computation (no DCE iterations, unlike other liveness passes)
//    - Handles QMark nodes specially for conditional execution
//    - Accounts for implicit exception flow
//    - Distinguishes between full definitions and partial definitions
//
// The liveness information is critical for:
// - Avoiding creation of dead stores (especially to remainders, which the SSA liveness
//   pass handles very conservatively as partial definitions)
// - Marking replacement fields with proper liveness flags for subsequent compiler phases
// - Optimizing read-back operations by determining when they're unnecessary
//

struct BasicBlockLiveness
{
    // Variables used before a full definition.
    BitVec VarUse;
    // Variables fully defined before a use.
    // Note that this differs from our normal liveness: partial definitions are
    // NOT marked but they are also not considered uses.
    BitVec VarDef;
    // Variables live-in to this basic block.
    BitVec LiveIn;
    // Variables live-out of this basic block.
    BitVec LiveOut;
};

//------------------------------------------------------------------------
// Run:
//   Compute liveness information pertaining the promoted structs.
//
void PromotionLiveness::Run()
{
    m_structLclToTrackedIndex = new (m_compiler, CMK_Promotion) unsigned[m_compiler->lvaCount]{};
    unsigned trackedIndex     = 0;
    for (AggregateInfo* agg : m_aggregates)
    {
        m_structLclToTrackedIndex[agg->LclNum] = trackedIndex;
        // Remainder.
        trackedIndex++;
        // Fields.
        trackedIndex += (unsigned)agg->Replacements.size();

#ifdef DEBUG
        // Mark the struct local (remainder) and fields as tracked for DISPTREE to properly
        // show last use information.
        m_compiler->lvaGetDesc(agg->LclNum)->lvTrackedWithoutIndex = true;
        for (size_t i = 0; i < agg->Replacements.size(); i++)
        {
            m_compiler->lvaGetDesc(agg->Replacements[i].LclNum)->lvTrackedWithoutIndex = true;
        }
#endif
    }

    m_numVars = trackedIndex;

    m_bvTraits = new (m_compiler, CMK_Promotion) BitVecTraits(m_numVars, m_compiler);
    m_bbInfo   = m_compiler->fgAllocateTypeForEachBlk<BasicBlockLiveness>(CMK_Promotion);
    BitVecOps::AssignNoCopy(m_bvTraits, m_liveIn, BitVecOps::MakeEmpty(m_bvTraits));
    BitVecOps::AssignNoCopy(m_bvTraits, m_ehLiveVars, BitVecOps::MakeEmpty(m_bvTraits));

    JITDUMP("Computing liveness for %u remainders/fields\n\n", m_numVars);

    ComputeUseDefSets();

    InterBlockLiveness();

    FillInLiveness();
}

//------------------------------------------------------------------------
// ComputeUseDefSets:
//   Compute the use and def sets for all blocks.
//
void PromotionLiveness::ComputeUseDefSets()
{
    for (BasicBlock* block : m_compiler->Blocks())
    {
        BasicBlockLiveness& bb = m_bbInfo[block->bbNum];
        BitVecOps::AssignNoCopy(m_bvTraits, bb.VarUse, BitVecOps::MakeEmpty(m_bvTraits));
        BitVecOps::AssignNoCopy(m_bvTraits, bb.VarDef, BitVecOps::MakeEmpty(m_bvTraits));
        BitVecOps::AssignNoCopy(m_bvTraits, bb.LiveIn, BitVecOps::MakeEmpty(m_bvTraits));
        BitVecOps::AssignNoCopy(m_bvTraits, bb.LiveOut, BitVecOps::MakeEmpty(m_bvTraits));

        if (m_compiler->compQmarkUsed)
        {
            for (Statement* stmt : block->Statements())
            {
                GenTree* dst;
                GenTree* qmark = m_compiler->fgGetTopLevelQmark(stmt->GetRootNode(), &dst);
                if (qmark == nullptr)
                {
                    for (GenTreeLclVarCommon* lcl : stmt->LocalsTreeList())
                    {
                        MarkUseDef(stmt, lcl, bb.VarUse, bb.VarDef);
                    }
                }
                else
                {
                    for (GenTreeLclVarCommon* lcl : stmt->LocalsTreeList())
                    {
                        // Skip liveness updates/marking for defs; they may be conditionally executed.
                        if ((lcl->gtFlags & GTF_VAR_DEF) == 0)
                        {
                            MarkUseDef(stmt, lcl, bb.VarUse, bb.VarDef);
                        }
                    }
                }
            }
        }
        else
        {
            for (Statement* stmt : block->Statements())
            {
                for (GenTreeLclVarCommon* lcl : stmt->LocalsTreeList())
                {
                    MarkUseDef(stmt, lcl, bb.VarUse, bb.VarDef);
                }
            }
        }

#ifdef DEBUG
        if (m_compiler->verbose)
        {
            BitVec allVars(BitVecOps::Union(m_bvTraits, bb.VarUse, bb.VarDef));
            printf(FMT_BB " USE(%u)=", block->bbNum, BitVecOps::Count(m_bvTraits, bb.VarUse));
            DumpVarSet(bb.VarUse, allVars);
            printf("\n" FMT_BB " DEF(%u)=", block->bbNum, BitVecOps::Count(m_bvTraits, bb.VarDef));
            DumpVarSet(bb.VarDef, allVars);
            printf("\n\n");
        }
#endif
    }
}

//------------------------------------------------------------------------
// MarkUseDef:
//   Mark use/def information for a single appearence of a local.
//
// Parameters:
//   stmt   - Statement containing the local
//   lcl    - The local node
//   useSet - The use set to mark in.
//   defSet - The def set to mark in.
//
void PromotionLiveness::MarkUseDef(Statement* stmt, GenTreeLclVarCommon* lcl, BitVec& useSet, BitVec& defSet)
{
    AggregateInfo* agg = m_aggregates.Lookup(lcl->GetLclNum());
    if (agg == nullptr)
    {
        return;
    }

    jitstd::vector<Replacement>& reps  = agg->Replacements;
    bool                         isDef = (lcl->gtFlags & GTF_VAR_DEF) != 0;
    bool                         isUse = !isDef;

    unsigned  baseIndex  = m_structLclToTrackedIndex[lcl->GetLclNum()];
    var_types accessType = lcl->TypeGet();

    if ((accessType == TYP_STRUCT) || lcl->OperIs(GT_LCL_ADDR))
    {
        if (lcl->OperIsScalarLocal())
        {
            // Mark remainder and all fields.
            for (size_t i = 0; i <= reps.size(); i++)
            {
                MarkIndex(baseIndex + (unsigned)i, isUse, isDef, useSet, defSet);
            }
        }
        else
        {
            unsigned offs  = lcl->GetLclOffs();
            unsigned size  = GetSizeOfStructLocal(stmt, lcl);
            size_t   index = Promotion::BinarySearch<Replacement, &Replacement::Offset>(reps, offs);

            if ((ssize_t)index < 0)
            {
                index = ~index;
                if ((index > 0) && reps[index - 1].Overlaps(offs, size))
                {
                    index--;
                }
            }

            while ((index < reps.size()) && (reps[index].Offset < offs + size))
            {
                Replacement& rep = reps[index];
                bool         isFullFieldDef =
                    isDef && (offs <= rep.Offset) && (offs + size >= rep.Offset + genTypeSize(rep.AccessType));
                MarkIndex(baseIndex + 1 + (unsigned)index, isUse, isFullFieldDef, useSet, defSet);
                index++;
            }

            bool isFullDefOfRemainder = isDef && (agg->UnpromotedMin >= offs) && (agg->UnpromotedMax <= (offs + size));
            bool isUseOfRemainder     = isUse && agg->Unpromoted.Intersects(SegmentList::Segment(offs, offs + size));
            MarkIndex(baseIndex, isUseOfRemainder, isFullDefOfRemainder, useSet, defSet);
        }
    }
    else
    {
        unsigned offs  = lcl->GetLclOffs();
        size_t   index = Promotion::BinarySearch<Replacement, &Replacement::Offset>(reps, offs);
        if ((ssize_t)index < 0)
        {
            unsigned size             = genTypeSize(accessType);
            bool isFullDefOfRemainder = isDef && (agg->UnpromotedMin >= offs) && (agg->UnpromotedMax <= (offs + size));
            MarkIndex(baseIndex, isUse, isFullDefOfRemainder, useSet, defSet);
        }
        else
        {
            // Accessing element.
            MarkIndex(baseIndex + 1 + (unsigned)index, isUse, isDef, useSet, defSet);
        }
    }
}

//------------------------------------------------------------------------
// GetSizeOfStructLocal:
//   Get the size of a struct local (either a TYP_STRUCT typed local, or a
//   GT_LCL_ADDR retbuf definition).
//
// Parameters:
//   stmt   - Statement containing the local
//   lcl    - The local node
//
unsigned PromotionLiveness::GetSizeOfStructLocal(Statement* stmt, GenTreeLclVarCommon* lcl)
{
    if (lcl->OperIs(GT_LCL_ADDR))
    {
        // Retbuf definition. Find the definition size from the
        // containing call.
        Compiler::FindLinkData data = m_compiler->gtFindLink(stmt, lcl);
        assert((data.parent != nullptr) && data.parent->IsCall() &&
               (m_compiler->gtCallGetDefinedRetBufLclAddr(data.parent->AsCall()) == lcl));
        return m_compiler->typGetObjLayout(data.parent->AsCall()->gtRetClsHnd)->GetSize();
    }

    return lcl->GetLayout(m_compiler)->GetSize();
}

//------------------------------------------------------------------------
// MarkIndex:
//   Mark specific bits in use/def bit vectors depending on whether this is a use def.
//
// Parameters:
//   index  - The index of the bit to set.
//   isUse  - Whether this is a use
//   isDef  - Whether this is a def
//   useSet - The set of uses
//   defSet - The set of defs
//
void PromotionLiveness::MarkIndex(unsigned index, bool isUse, bool isDef, BitVec& useSet, BitVec& defSet)
{
    if (isUse && !BitVecOps::IsMember(m_bvTraits, defSet, index))
    {
        BitVecOps::AddElemD(m_bvTraits, useSet, index);
    }

    if (isDef)
    {
        BitVecOps::AddElemD(m_bvTraits, defSet, index);
    }
}

//------------------------------------------------------------------------
// InterBlockLiveness:
//   Compute the fixpoint.
//
void PromotionLiveness::InterBlockLiveness()
{
    FlowGraphDfsTree* dfsTree = m_compiler->m_dfsTree;
    assert(dfsTree != nullptr);

    bool changed;
    do
    {
        changed = false;

        for (unsigned i = 0; i < dfsTree->GetPostOrderCount(); i++)
        {
            changed |= PerBlockLiveness(dfsTree->GetPostOrder(i));
        }
    } while (changed && dfsTree->HasCycle());

#ifdef DEBUG
    if (m_compiler->verbose)
    {
        for (BasicBlock* block : m_compiler->Blocks())
        {
            BasicBlockLiveness& bbInfo = m_bbInfo[block->bbNum];
            BitVec              allVars(BitVecOps::Union(m_bvTraits, bbInfo.LiveIn, bbInfo.LiveOut));
            printf(FMT_BB " IN (%u)=", block->bbNum, BitVecOps::Count(m_bvTraits, bbInfo.LiveIn));
            DumpVarSet(bbInfo.LiveIn, allVars);
            printf("\n" FMT_BB " OUT(%u)=", block->bbNum, BitVecOps::Count(m_bvTraits, bbInfo.LiveOut));
            DumpVarSet(bbInfo.LiveOut, allVars);
            printf("\n\n");
        }
    }
#endif
}

//------------------------------------------------------------------------
// PerBlockLiveness:
//   Compute liveness for a single block during a single iteration of the
//   fixpoint computation.
//
// Parameters:
//   block - The block
//
bool PromotionLiveness::PerBlockLiveness(BasicBlock* block)
{
    // We disable promotion for GT_JMP methods.
    assert(!block->endsWithJmpMethod(m_compiler));

    BasicBlockLiveness& bbInfo = m_bbInfo[block->bbNum];
    BitVecOps::ClearD(m_bvTraits, bbInfo.LiveOut);
    block->VisitRegularSuccs(m_compiler, [=, &bbInfo](BasicBlock* succ) {
        BitVecOps::UnionD(m_bvTraits, bbInfo.LiveOut, m_bbInfo[succ->bbNum].LiveIn);
        return BasicBlockVisit::Continue;
    });

    BitVecOps::LivenessD(m_bvTraits, m_liveIn, bbInfo.VarDef, bbInfo.VarUse, bbInfo.LiveOut);

    if (block->HasPotentialEHSuccs(m_compiler))
    {
        BitVecOps::ClearD(m_bvTraits, m_ehLiveVars);
        AddHandlerLiveVars(block, m_ehLiveVars);
        BitVecOps::UnionD(m_bvTraits, m_liveIn, m_ehLiveVars);
        BitVecOps::UnionD(m_bvTraits, bbInfo.LiveOut, m_ehLiveVars);
    }

    bool liveInChanged = !BitVecOps::Equal(m_bvTraits, bbInfo.LiveIn, m_liveIn);

    if (liveInChanged)
    {
        BitVecOps::Assign(m_bvTraits, bbInfo.LiveIn, m_liveIn);
    }

    return liveInChanged;
}

//------------------------------------------------------------------------
// AddHandlerLiveVars:
//   Find variables that are live-in to handlers reachable by implicit control
//   flow and return them in the specified bit vector.
//
// Parameters:
//   block      - The block
//   ehLiveVars - The bit vector to mark in.
//
// Remarks:
//   Similar to Compiler::fgGetHandlerLiveVars used by regular liveness.
//
void PromotionLiveness::AddHandlerLiveVars(BasicBlock* block, BitVec& ehLiveVars)
{
    assert(block->HasPotentialEHSuccs(m_compiler));

    block->VisitEHSuccs(m_compiler, [=, &ehLiveVars](BasicBlock* succ) {
        BitVecOps::UnionD(m_bvTraits, ehLiveVars, m_bbInfo[succ->bbNum].LiveIn);
        return BasicBlockVisit::Continue;
    });
}

//------------------------------------------------------------------------
// FillInLiveness:
//   Starting with the live-out set for each basic block do a backwards traversal
//   marking liveness into the IR.
//
void PromotionLiveness::FillInLiveness()
{
    BitVec life(BitVecOps::MakeEmpty(m_bvTraits));
    BitVec volatileVars(BitVecOps::MakeEmpty(m_bvTraits));

    for (BasicBlock* block : m_compiler->Blocks())
    {
        if (block->firstStmt() == nullptr)
        {
            continue;
        }

        BasicBlockLiveness& bbInfo = m_bbInfo[block->bbNum];

        BitVecOps::ClearD(m_bvTraits, volatileVars);

        if (block->HasPotentialEHSuccs(m_compiler))
        {
            AddHandlerLiveVars(block, volatileVars);
        }

        BitVecOps::Assign(m_bvTraits, life, bbInfo.LiveOut);

        Statement* stmt = block->lastStmt();

        while (true)
        {
            GenTree* qmark = nullptr;
            if (m_compiler->compQmarkUsed)
            {
                GenTree* dst;
                qmark = m_compiler->fgGetTopLevelQmark(stmt->GetRootNode(), &dst);
            }

            if (qmark == nullptr)
            {
                for (GenTree* cur = stmt->GetTreeListEnd(); cur != nullptr; cur = cur->gtPrev)
                {
                    FillInLiveness(life, volatileVars, stmt, cur->AsLclVarCommon());
                }
            }
            else
            {
                for (GenTree* cur = stmt->GetTreeListEnd(); cur != nullptr; cur = cur->gtPrev)
                {
                    // Skip liveness updates/marking for defs; they may be conditionally executed.
                    if ((cur->gtFlags & GTF_VAR_DEF) == 0)
                    {
                        FillInLiveness(life, volatileVars, stmt, cur->AsLclVarCommon());
                    }
                }
            }

            if (stmt == block->firstStmt())
            {
                break;
            }

            stmt = stmt->GetPrevStmt();
        }
    }
}

//------------------------------------------------------------------------
// FillInLiveness:
//   Fill liveness information into the specified IR node.
//
// Parameters:
//   life         - The current life set. Will be read and updated depending on 'lcl'.
//   volatileVars - Bit vector of variables that are live always.
//   stmt         - Statement containing the local
//   lcl          - The IR node to process liveness for and to mark with liveness information.
//
void PromotionLiveness::FillInLiveness(BitVec& life, BitVec volatileVars, Statement* stmt, GenTreeLclVarCommon* lcl)
{
    AggregateInfo* agg = m_aggregates.Lookup(lcl->GetLclNum());
    if (agg == nullptr)
    {
        return;
    }

    bool isDef = (lcl->gtFlags & GTF_VAR_DEF) != 0;
    bool isUse = !isDef;

    unsigned  baseIndex  = m_structLclToTrackedIndex[lcl->GetLclNum()];
    var_types accessType = lcl->TypeGet();

    if ((accessType == TYP_STRUCT) || lcl->OperIs(GT_LCL_ADDR))
    {
        // We need an external bit set to represent dying fields/remainder on a struct use.
        BitVecTraits aggTraits(1 + (unsigned)agg->Replacements.size(), m_compiler);
        BitVec       aggDeaths(BitVecOps::MakeEmpty(&aggTraits));
        if (lcl->OperIsScalarLocal())
        {
            // Handle remainder and all fields.
            for (size_t i = 0; i <= agg->Replacements.size(); i++)
            {
                unsigned varIndex = baseIndex + (unsigned)i;
                if (BitVecOps::IsMember(m_bvTraits, life, varIndex))
                {
                    if (isDef && !BitVecOps::IsMember(m_bvTraits, volatileVars, varIndex))
                    {
                        BitVecOps::RemoveElemD(m_bvTraits, life, varIndex);
                    }
                }
                else
                {
                    BitVecOps::AddElemD(&aggTraits, aggDeaths, (unsigned)i);

                    if (isUse)
                    {
                        BitVecOps::AddElemD(m_bvTraits, life, varIndex);
                    }
                }
            }
        }
        else
        {
            unsigned offs  = lcl->GetLclOffs();
            unsigned size  = GetSizeOfStructLocal(stmt, lcl);
            size_t   index = Promotion::BinarySearch<Replacement, &Replacement::Offset>(agg->Replacements, offs);

            if ((ssize_t)index < 0)
            {
                index = ~index;
                if ((index > 0) && agg->Replacements[index - 1].Overlaps(offs, size))
                {
                    index--;
                }
            }

            // Handle fields.
            while ((index < agg->Replacements.size()) && (agg->Replacements[index].Offset < offs + size))
            {
                unsigned     varIndex = baseIndex + 1 + (unsigned)index;
                Replacement& rep      = agg->Replacements[index];
                if (BitVecOps::IsMember(m_bvTraits, life, varIndex))
                {
                    bool isFullFieldDef =
                        isDef && (offs <= rep.Offset) && (offs + size >= rep.Offset + genTypeSize(rep.AccessType));
                    if (isFullFieldDef && !BitVecOps::IsMember(m_bvTraits, volatileVars, varIndex))
                    {
                        BitVecOps::RemoveElemD(m_bvTraits, life, varIndex);
                    }
                }
                else
                {
                    BitVecOps::AddElemD(&aggTraits, aggDeaths, 1 + (unsigned)index);

                    if (isUse)
                    {
                        BitVecOps::AddElemD(m_bvTraits, life, varIndex);
                    }
                }

                index++;
            }

            // Handle remainder.
            if (BitVecOps::IsMember(m_bvTraits, life, baseIndex))
            {
                bool isFullDefOfRemainder =
                    isDef && (agg->UnpromotedMin >= offs) && (agg->UnpromotedMax <= (offs + size));
                if (isFullDefOfRemainder && !BitVecOps::IsMember(m_bvTraits, volatileVars, baseIndex))
                {
                    BitVecOps::RemoveElemD(m_bvTraits, life, baseIndex);
                }
            }
            else
            {
                BitVecOps::AddElemD(&aggTraits, aggDeaths, 0);

                if (isUse && agg->Unpromoted.Intersects(SegmentList::Segment(offs, offs + size)))
                {
                    BitVecOps::AddElemD(m_bvTraits, life, baseIndex);
                }
            }
        }

        m_aggDeaths.Set(lcl, aggDeaths);
    }
    else
    {
        unsigned offs  = lcl->GetLclOffs();
        size_t   index = Promotion::BinarySearch<Replacement, &Replacement::Offset>(agg->Replacements, offs);
        if ((ssize_t)index < 0)
        {
            // No replacement found, this is a use of the remainder.
            unsigned size = genTypeSize(accessType);
            if (BitVecOps::IsMember(m_bvTraits, life, baseIndex))
            {
                lcl->gtFlags &= ~GTF_VAR_DEATH;

                bool isFullDefOfRemainder =
                    isDef && (agg->UnpromotedMin >= offs) && (agg->UnpromotedMax <= (offs + size));
                if (isFullDefOfRemainder && !BitVecOps::IsMember(m_bvTraits, volatileVars, baseIndex))
                {
                    BitVecOps::RemoveElemD(m_bvTraits, life, baseIndex);
                }
            }
            else
            {
                lcl->gtFlags |= GTF_VAR_DEATH;

                if (isUse)
                {
                    BitVecOps::AddElemD(m_bvTraits, life, baseIndex);
                }
            }
        }
        else
        {
            // Use of a field.
            unsigned varIndex = baseIndex + 1 + (unsigned)index;

            if (BitVecOps::IsMember(m_bvTraits, life, varIndex))
            {
                lcl->gtFlags &= ~GTF_VAR_DEATH;

                if (isDef && !BitVecOps::IsMember(m_bvTraits, volatileVars, varIndex))
                {
                    BitVecOps::RemoveElemD(m_bvTraits, life, varIndex);
                }
            }
            else
            {
                lcl->gtFlags |= GTF_VAR_DEATH;

                if (isUse)
                {
                    BitVecOps::AddElemD(m_bvTraits, life, varIndex);
                }
            }
        }
    }
}

//------------------------------------------------------------------------
// IsReplacementLiveIn:
//   Check if a replacement field is live at the start of a basic block.
//
// Parameters:
//   structLcl        - The struct (base) local
//   replacementIndex - Index of the replacement
//
// Returns:
//   True if the field is in the live-in set.
//
bool PromotionLiveness::IsReplacementLiveIn(BasicBlock* bb, unsigned structLcl, unsigned replacementIndex)
{
    BitVec   liveIn    = m_bbInfo[bb->bbNum].LiveIn;
    unsigned baseIndex = m_structLclToTrackedIndex[structLcl];
    return BitVecOps::IsMember(m_bvTraits, liveIn, baseIndex + 1 + replacementIndex);
}

//------------------------------------------------------------------------
// IsReplacementLiveOut:
//   Check if a replacement field is live at the end of a basic block.
//
// Parameters:
//   structLcl        - The struct (base) local
//   replacementIndex - Index of the replacement
//
// Returns:
//   True if the field is in the live-out set.
//
bool PromotionLiveness::IsReplacementLiveOut(BasicBlock* bb, unsigned structLcl, unsigned replacementIndex)
{
    BitVec   liveOut   = m_bbInfo[bb->bbNum].LiveOut;
    unsigned baseIndex = m_structLclToTrackedIndex[structLcl];
    return BitVecOps::IsMember(m_bvTraits, liveOut, baseIndex + 1 + replacementIndex);
}

//------------------------------------------------------------------------
// GetDeathsForStructLocal:
//   Get a data structure that can be used to query liveness information
//   for a specified local node at its position.
//
// Parameters:
//   lcl - The node
//
// Returns:
//   Liveness information.
//
StructDeaths PromotionLiveness::GetDeathsForStructLocal(GenTreeLclVarCommon* lcl)
{
    assert((lcl->TypeIs(TYP_STRUCT) || (lcl->OperIs(GT_LCL_ADDR) && ((lcl->gtFlags & GTF_VAR_DEF) != 0))) &&
           (m_aggregates.Lookup(lcl->GetLclNum()) != nullptr));
    BitVec aggDeaths;
    bool   found = m_aggDeaths.Lookup(lcl, &aggDeaths);
    assert(found);

    unsigned       lclNum  = lcl->GetLclNum();
    AggregateInfo* aggInfo = m_aggregates.Lookup(lclNum);
    return StructDeaths(aggDeaths, aggInfo);
}

//------------------------------------------------------------------------
// IsRemainderDying:
//   Check if the remainder is dying.
//
// Returns:
//   True if so.
//
bool StructDeaths::IsRemainderDying() const
{
    if (m_aggregate->UnpromotedMax <= m_aggregate->UnpromotedMin)
    {
        // No remainder.
        return true;
    }

    BitVecTraits traits(1 + (unsigned)m_aggregate->Replacements.size(), nullptr);
    return BitVecOps::IsMember(&traits, m_deaths, 0);
}

//------------------------------------------------------------------------
// IsReplacementDying:
//   Check if a specific replacement is dying.
//
// Returns:
//   True if so.
//
bool StructDeaths::IsReplacementDying(unsigned index) const
{
    assert(index < m_aggregate->Replacements.size());

    BitVecTraits traits(1 + (unsigned)m_aggregate->Replacements.size(), nullptr);
    return BitVecOps::IsMember(&traits, m_deaths, 1 + index);
}

#ifdef DEBUG
//------------------------------------------------------------------------
// DumpVarSet:
//   Dump a var set to jitstdout.
//
// Parameters:
//   set     - The set to dump
//   allVars - Set of all variables to print whitespace for if not in 'set'.
//             Used for alignment.
//
void PromotionLiveness::DumpVarSet(BitVec set, BitVec allVars)
{
    printf("{");

    const char* sep = "";
    for (AggregateInfo* agg : m_aggregates)
    {
        for (size_t j = 0; j <= agg->Replacements.size(); j++)
        {
            unsigned index = (unsigned)(m_structLclToTrackedIndex[agg->LclNum] + j);

            if (BitVecOps::IsMember(m_bvTraits, set, index))
            {
                if (j == 0)
                {
                    printf("%sV%02u(remainder)", sep, agg->LclNum);
                }
                else
                {
                    const Replacement& rep = agg->Replacements[j - 1];
                    printf("%sV%02u.[%03u..%03u)", sep, agg->LclNum, rep.Offset,
                           rep.Offset + genTypeSize(rep.AccessType));
                }
                sep = " ";
            }
            else if (BitVecOps::IsMember(m_bvTraits, allVars, index))
            {
                printf("%s              ", sep);
                sep = " ";
            }
        }
    }

    printf("}");
}
#endif
