// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                               DecomposeLongs                              XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

//
// This file contains code to decompose 64-bit LONG operations on 32-bit platforms
// into multiple single-register operations so individual register usage and requirements
// are explicit for LSRA. The rationale behind this is to avoid adding code complexity
// downstream caused by the introduction of handling longs as special cases,
// especially in LSRA.
//
// Long decomposition happens on a statement immediately prior to more general
// purpose lowering.
//

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifndef TARGET_64BIT // DecomposeLongs is only used on 32-bit platforms

#include "decomposelongs.h"

//------------------------------------------------------------------------
// DecomposeLongs::PrepareForDecomposition:
//    Do one-time preparation required for LONG decomposition. Namely,
//    promote long variables to multi-register structs.
//
// Arguments:
//    None
//
// Return Value:
//    None.
//
void DecomposeLongs::PrepareForDecomposition()
{
    PromoteLongVars();
}

//------------------------------------------------------------------------
// DecomposeLongs::DecomposeBlock:
//    Do LONG decomposition on all the nodes in the given block. This must
//    be done before lowering the block, as decomposition can insert
//    additional nodes.
//
// Arguments:
//    block - the block to process
//
// Return Value:
//    None.
//
void DecomposeLongs::DecomposeBlock(BasicBlock* block)
{
    assert(block == m_compiler->compCurBB); // compCurBB must already be set.
    assert(block->isEmpty() || block->IsLIR());
    m_range = &LIR::AsRange(block);
    DecomposeRangeHelper();
}

//------------------------------------------------------------------------
// DecomposeLongs::DecomposeRange:
//    Do LONG decomposition on all the nodes in the given range. This must
//    be done before inserting a range of un-decomposed IR into a block
//    that has already been decomposed.
//
// Arguments:
//    compiler    - The compiler context.
//    range       - The range to decompose.
//
// Return Value:
//    None.
//
void DecomposeLongs::DecomposeRange(Compiler* compiler, Lowering* lowering, LIR::Range& range)
{
    assert(compiler != nullptr);

    DecomposeLongs decomposer(compiler, lowering);
    decomposer.m_range = &range;

    decomposer.DecomposeRangeHelper();
}

//------------------------------------------------------------------------
// DecomposeLongs::DecomposeRangeHelper:
//    Decompose each node in the current range.
//
//    Decomposition is done as an execution-order walk. Decomposition of
//    a particular node can create new nodes that need to be further
//    decomposed at higher levels. That is, decomposition "bubbles up"
//    through dataflow.
//
void DecomposeLongs::DecomposeRangeHelper()
{
    assert(m_range != nullptr);

    GenTree* node = Range().FirstNode();
    while (node != nullptr)
    {
        node = DecomposeNode(node);
    }

    assert(Range().CheckLIR(m_compiler, true));
}

//------------------------------------------------------------------------
// DecomposeNode: Decompose long-type trees into lower and upper halves.
//
// Arguments:
//    tree - the tree that will, if needed, be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeNode(GenTree* tree)
{
    // Handle the case where we are implicitly using the lower half of a long lclVar.
    if (tree->TypeIs(TYP_INT) && tree->OperIsLocal())
    {
        LclVarDsc* varDsc = m_compiler->lvaGetDesc(tree->AsLclVarCommon());
        if (varTypeIsLong(varDsc) && varDsc->lvPromoted)
        {
            JITDUMP("Changing implicit reference to lo half of long lclVar to an explicit reference of its promoted "
                    "half:\n");
            DISPTREERANGE(Range(), tree);

            unsigned loVarNum = varDsc->lvFieldLclStart;
            tree->AsLclVarCommon()->SetLclNum(loVarNum);
            return tree->gtNext;
        }
    }

#if defined(FEATURE_HW_INTRINSICS) && defined(TARGET_X86)
    if (!tree->TypeIs(TYP_LONG) &&
        !(tree->OperIs(GT_CAST) && varTypeIsLong(tree->AsCast()->CastOp()) && varTypeIsFloating(tree)))
#else
    if (!tree->TypeIs(TYP_LONG))
#endif // FEATURE_HW_INTRINSICS && TARGET_X86
    {
        return tree->gtNext;
    }

    LIR::Use use;
    if (!Range().TryGetUse(tree, &use))
    {
        LIR::Use::MakeDummyUse(Range(), tree, &use);
    }

#if defined(FEATURE_HW_INTRINSICS) && defined(TARGET_X86)
    if (!use.IsDummyUse())
    {
        // HWIntrinsics can consume/produce a long directly, provided its source/target is memory.
        // Here we do a conservative check for specific cases where it is certain the load/store
        // can be contained. In those cases, we can skip decomposition.

        GenTree* user = use.User();

        if (tree->TypeIs(TYP_LONG) && (user->OperIsHWIntrinsic() || (user->OperIs(GT_CAST) && varTypeIsFloating(user))))
        {
            if (tree->OperIs(GT_CNS_LNG) ||
                (tree->OperIs(GT_IND, GT_LCL_FLD) && m_lowering->IsSafeToContainMem(user, tree)))
            {
                if (user->OperIsHWIntrinsic())
                {
                    NamedIntrinsic intrinsicId = user->AsHWIntrinsic()->GetHWIntrinsicId();
                    assert(HWIntrinsicInfo::IsVectorCreate(intrinsicId) ||
                           HWIntrinsicInfo::IsVectorCreateScalar(intrinsicId) ||
                           HWIntrinsicInfo::IsVectorCreateScalarUnsafe(intrinsicId));
                }

                return tree->gtNext;
            }
        }
        else if (user->OperIs(GT_STOREIND) && tree->OperIsHWIntrinsic() && m_compiler->opts.Tier0OptimizationEnabled())
        {
            NamedIntrinsic intrinsicId = tree->AsHWIntrinsic()->GetHWIntrinsicId();
            if (HWIntrinsicInfo::IsVectorToScalar(intrinsicId) && m_lowering->IsSafeToContainMem(user, tree))
            {
                return tree->gtNext;
            }
        }
    }

    if (tree->OperIs(GT_STOREIND) && tree->AsStoreInd()->Data()->OperIsHWIntrinsic())
    {
        // We should only get here if we matched the second pattern above.
        assert(HWIntrinsicInfo::IsVectorToScalar(tree->AsStoreInd()->Data()->AsHWIntrinsic()->GetHWIntrinsicId()));

        return tree->gtNext;
    }
#endif // FEATURE_HW_INTRINSICS && TARGET_X86

    JITDUMP("Decomposing TYP_LONG tree.  BEFORE:\n");
    DISPTREERANGE(Range(), tree);

    GenTree* nextNode = nullptr;
    switch (tree->OperGet())
    {
        case GT_LCL_VAR:
            nextNode = DecomposeLclVar(use);
            break;

        case GT_LCL_FLD:
            nextNode = DecomposeLclFld(use);
            break;

        case GT_STORE_LCL_VAR:
            nextNode = DecomposeStoreLclVar(use);
            break;

        case GT_CAST:
            nextNode = DecomposeCast(use);
            break;

        case GT_CNS_LNG:
            nextNode = DecomposeCnsLng(use);
            break;

        case GT_CALL:
            nextNode = DecomposeCall(use);
            break;

        case GT_RETURN:
        case GT_SWIFT_ERROR_RET:
            assert(tree->AsOp()->GetReturnValue()->OperIs(GT_LONG));
            break;

        case GT_STOREIND:
            nextNode = DecomposeStoreInd(use);
            break;

        case GT_STORE_LCL_FLD:
            nextNode = DecomposeStoreLclFld(use);
            break;

        case GT_IND:
            nextNode = DecomposeInd(use);
            break;

        case GT_NOT:
            nextNode = DecomposeNot(use);
            break;

        case GT_NEG:
            nextNode = DecomposeNeg(use);
            break;

        // Binary operators. Those that require different computation for upper and lower half are
        // handled by the use of GetHiOper().
        case GT_ADD:
        case GT_SUB:
        case GT_OR:
        case GT_XOR:
        case GT_AND:
            nextNode = DecomposeArith(use);
            break;

        case GT_MUL:
            nextNode = DecomposeMul(use);
            break;

        case GT_UMOD:
            nextNode = DecomposeUMod(use);
            break;

        case GT_LSH:
        case GT_RSH:
        case GT_RSZ:
            nextNode = DecomposeShift(use);
            break;

        case GT_ROL:
        case GT_ROR:
            nextNode = DecomposeRotate(use);
            break;

#ifdef FEATURE_HW_INTRINSICS
        case GT_HWINTRINSIC:
            nextNode = DecomposeHWIntrinsic(use);
            break;
#endif // FEATURE_HW_INTRINSICS

        case GT_SELECT:
            nextNode = DecomposeSelect(use);
            break;

        case GT_LOCKADD:
        case GT_XORR:
        case GT_XAND:
        case GT_XADD:
        case GT_XCHG:
        case GT_CMPXCHG:
            NYI("Interlocked operations on TYP_LONG");
            break;

        default:
        {
            JITDUMP("Illegal TYP_LONG node %s in Decomposition.", GenTree::OpName(tree->OperGet()));
            assert(!"Illegal TYP_LONG node in Decomposition.");
            break;
        }
    }

    // If we replaced the argument to a GT_FIELD_LIST element with a GT_LONG node, split that field list
    // element into two elements: one for each half of the GT_LONG.
    if (use.Def()->OperIs(GT_LONG) && !use.IsDummyUse() && use.User()->OperIs(GT_FIELD_LIST))
    {
        DecomposeFieldList(use.User()->AsFieldList(), use.Def()->AsOp());
    }

    // NOTE: st_lcl_var doesn't dump properly afterwards.
    JITDUMP("Decomposing TYP_LONG tree.  AFTER:\n");
    DISPTREERANGE(Range(), use.Def());

    // When casting from a decomposed long to a smaller integer we can discard the high part.
    if (m_compiler->opts.OptimizationEnabled() && !use.IsDummyUse() && use.User()->OperIs(GT_CAST) &&
        use.User()->TypeIs(TYP_INT) && use.Def()->OperIs(GT_LONG))
    {
        nextNode = OptimizeCastFromDecomposedLong(use.User()->AsCast(), nextNode);
    }

    return nextNode;
}

//------------------------------------------------------------------------
// FinalizeDecomposition: A helper function to finalize LONG decomposition by
// taking the resulting two halves of the decomposition, and tie them together
// with a new GT_LONG node that will replace the original node.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//    loResult - the decomposed low part
//    hiResult - the decomposed high part
//    insertResultAfter - the node that the GT_LONG should be inserted after
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::FinalizeDecomposition(LIR::Use& use,
                                               GenTree*  loResult,
                                               GenTree*  hiResult,
                                               GenTree*  insertResultAfter)
{
    assert(use.IsInitialized());
    assert(loResult != nullptr);
    assert(hiResult != nullptr);
    assert(Range().Contains(loResult));
    assert(Range().Contains(hiResult));

    GenTree* gtLong = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, loResult, hiResult);
    if (use.IsDummyUse())
    {
        gtLong->SetUnusedValue();
    }

    loResult->ClearUnusedValue();
    hiResult->ClearUnusedValue();

    Range().InsertAfter(insertResultAfter, gtLong);

    use.ReplaceWith(gtLong);

    return gtLong->gtNext;
}

//------------------------------------------------------------------------
// DecomposeLclVar: Decompose GT_LCL_VAR.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeLclVar(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_LCL_VAR);

    GenTree*   tree     = use.Def();
    unsigned   varNum   = tree->AsLclVarCommon()->GetLclNum();
    LclVarDsc* varDsc   = m_compiler->lvaGetDesc(varNum);
    GenTree*   loResult = tree;
    loResult->gtType    = TYP_INT;

    GenTree* hiResult = m_compiler->gtNewLclLNode(varNum, TYP_INT);
    Range().InsertAfter(loResult, hiResult);

    if (varDsc->lvPromoted)
    {
        assert(varDsc->lvFieldCnt == 2);
        unsigned loVarNum = varDsc->lvFieldLclStart;
        unsigned hiVarNum = loVarNum + 1;
        loResult->AsLclVarCommon()->SetLclNum(loVarNum);
        hiResult->AsLclVarCommon()->SetLclNum(hiVarNum);
    }
    else
    {
        m_compiler->lvaSetVarDoNotEnregister(varNum DEBUGARG(DoNotEnregisterReason::LocalField));
        loResult->SetOper(GT_LCL_FLD);
        loResult->AsLclFld()->SetLclOffs(0);

        hiResult->SetOper(GT_LCL_FLD);
        hiResult->AsLclFld()->SetLclOffs(4);
    }

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeLclFld: Decompose GT_LCL_FLD.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeLclFld(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_LCL_FLD);

    GenTree*       tree     = use.Def();
    GenTreeLclFld* loResult = tree->AsLclFld();
    loResult->gtType        = TYP_INT;

    GenTree* hiResult = m_compiler->gtNewLclFldNode(loResult->GetLclNum(), TYP_INT, loResult->GetLclOffs() + 4);
    Range().InsertAfter(loResult, hiResult);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeStoreLclVar: Decompose GT_STORE_LCL_VAR.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeStoreLclVar(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_STORE_LCL_VAR);

    GenTree* tree = use.Def();
    GenTree* rhs  = tree->gtGetOp1();
    if (rhs->OperIs(GT_CALL, GT_MUL_LONG))
    {
        // GT_CALLs are not decomposed, so will not be converted to GT_LONG
        // GT_STORE_LCL_VAR = GT_CALL are handled in genMultiRegCallStoreToLocal
        // GT_MULs are not decomposed, so will not be converted to GT_LONG
        return tree->gtNext;
    }

    noway_assert(rhs->OperIs(GT_LONG));

    const LclVarDsc* varDsc = m_compiler->lvaGetDesc(tree->AsLclVarCommon());
    if (!varDsc->lvPromoted)
    {
        // We cannot decompose a st.lclVar that is not promoted because doing so
        // changes its liveness semantics. For example, consider the following
        // decomposition of a st.lclVar into two st.lclFlds:
        //
        // Before:
        //
        //          /--* t0      int
        //          +--* t1      int
        //     t2 = *  gt_long   long
        //
        //          /--* t2      long
        //          *  st.lclVar long    V0
        //
        // After:
        //          /--* t0      int
        //          *  st.lclFld int     V0    [+0]
        //
        //          /--* t1      int
        //          *  st.lclFld int     V0    [+4]
        //
        // Before decomposition, the `st.lclVar` is a simple def of `V0`. After
        // decomposition, each `st.lclFld` is a partial def of `V0`. This partial
        // def is treated as both a use and a def of the appropriate lclVar. This
        // difference will affect any situation in which the liveness of a variable
        // at a def matters (e.g. dead store elimination, live-in sets, etc.). As
        // a result, we leave these stores as-is and generate the decomposed store
        // in the code generator.
        //
        // NOTE: this does extend the lifetime of the low half of the `GT_LONG`
        // node as compared to the decomposed form. If we start doing more code
        // motion in the backend, this may cause some CQ issues and some sort of
        // decomposition could be beneficial.
        return tree->gtNext;
    }

    assert(varDsc->lvFieldCnt == 2);
    GenTreeOp* value = rhs->AsOp();
    Range().Remove(value);

    const unsigned loVarNum = varDsc->lvFieldLclStart;
    GenTree*       loStore  = tree;
    loStore->AsLclVarCommon()->SetLclNum(loVarNum);
    loStore->AsOp()->gtOp1 = value->gtOp1;
    loStore->gtType        = TYP_INT;

    const unsigned hiVarNum = loVarNum + 1;
    GenTree*       hiStore  = m_compiler->gtNewLclLNode(hiVarNum, TYP_INT);
    hiStore->SetOper(GT_STORE_LCL_VAR);
    hiStore->AsOp()->gtOp1 = value->gtOp2;
    hiStore->gtFlags |= GTF_VAR_DEF;

    Range().InsertAfter(tree, hiStore);

    return hiStore->gtNext;
}

//------------------------------------------------------------------------
// DecomposeStoreLclFld: Decompose GT_STORE_LCL_FLD.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeStoreLclFld(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_STORE_LCL_FLD);

    GenTreeLclFld* store = use.Def()->AsLclFld();

    GenTreeOp* value = store->gtOp1->AsOp();
    assert(value->OperIs(GT_LONG));
    Range().Remove(value);

    // The original store node will be repurposed to store the low half of the GT_LONG.
    GenTreeLclFld* loStore = store;
    loStore->gtOp1         = value->gtOp1;
    loStore->gtType        = TYP_INT;
    loStore->gtFlags |= GTF_VAR_USEASG;

    // Create the store for the upper half of the GT_LONG and insert it after the low store.
    GenTreeLclFld* hiStore =
        m_compiler->gtNewStoreLclFldNode(loStore->GetLclNum(), TYP_INT, loStore->GetLclOffs() + 4, value->gtOp2);

    Range().InsertAfter(loStore, hiStore);

    return hiStore->gtNext;
}

//------------------------------------------------------------------------
// DecomposeCast: Decompose GT_CAST.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeCast(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperIs(GT_CAST));

    GenTreeCast* cast    = use.Def()->AsCast();
    var_types    srcType = cast->CastFromType();
    var_types    dstType = cast->CastToType();

    if (cast->IsUnsigned())
    {
        srcType = varTypeToUnsigned(srcType);
    }

#if defined(FEATURE_HW_INTRINSICS) && defined(TARGET_X86)
    if (varTypeIsFloating(dstType))
    {
        // We will reach this path only if morph did not convert the cast to a helper call,
        // meaning we can perform the cast using SIMD instructions.
        // The sequence this creates is simply:
        //    AVX512DQ.VL.ConvertToVector128Single(Vector128.CreateScalarUnsafe(LONG)).ToScalar()

        NamedIntrinsic intrinsicId      = NI_Illegal;
        GenTree*       srcOp            = cast->CastOp();
        var_types      dstType          = cast->CastToType();
        CorInfoType    baseFloatingType = (dstType == TYP_FLOAT) ? CORINFO_TYPE_FLOAT : CORINFO_TYPE_DOUBLE;
        CorInfoType    baseIntegralType = cast->IsUnsigned() ? CORINFO_TYPE_ULONG : CORINFO_TYPE_LONG;

        assert(!cast->gtOverflow());
        assert(m_compiler->compIsaSupportedDebugOnly(InstructionSet_AVX512));

        intrinsicId = (dstType == TYP_FLOAT) ? NI_AVX512_ConvertToVector128Single : NI_AVX512_ConvertToVector128Double;

        GenTree* createScalar = m_compiler->gtNewSimdCreateScalarUnsafeNode(TYP_SIMD16, srcOp, baseIntegralType, 16);
        GenTree* convert =
            m_compiler->gtNewSimdHWIntrinsicNode(TYP_SIMD16, createScalar, intrinsicId, baseIntegralType, 16);
        GenTree* toScalar = m_compiler->gtNewSimdToScalarNode(dstType, convert, baseFloatingType, 16);

        Range().InsertAfter(cast, createScalar, convert, toScalar);
        Range().Remove(cast);

        if (createScalar->IsCnsVec())
        {
            Range().Remove(srcOp);
        }

        if (use.IsDummyUse())
        {
            toScalar->SetUnusedValue();
        }
        use.ReplaceWith(toScalar);

        return toScalar->gtNext;
    }
#endif // FEATURE_HW_INTRINSICS && TARGET_X86

    bool     skipDecomposition = false;
    GenTree* loResult          = nullptr;
    GenTree* hiResult          = nullptr;

    if (varTypeIsLong(srcType))
    {
        if (cast->gtOverflow() && (varTypeIsUnsigned(srcType) != varTypeIsUnsigned(dstType)))
        {
            GenTree* srcOp = cast->CastOp();
            noway_assert(srcOp->OperIs(GT_LONG));
            GenTree* loSrcOp = srcOp->gtGetOp1();
            GenTree* hiSrcOp = srcOp->gtGetOp2();

            //
            // When casting between long types an overflow check is needed only if the types
            // have different signedness. In both cases (long->ulong and ulong->long) we only
            // need to check if the high part is negative or not. Use the existing cast node
            // to perform a int->uint cast of the high part to take advantage of the overflow
            // check provided by codegen.
            //

            const bool signExtend = !cast->IsUnsigned();
            loResult              = EnsureIntSized(loSrcOp, signExtend);

            hiResult                       = cast;
            hiResult->gtType               = TYP_INT;
            hiResult->AsCast()->gtCastType = TYP_UINT;
            hiResult->ClearUnsigned();
            hiResult->AsOp()->gtOp1 = hiSrcOp;

            Range().Remove(srcOp);
        }
        else
        {
            NYI("Unimplemented long->long no-op cast decomposition");
        }
    }
    else if (varTypeIsIntegralOrI(srcType))
    {
        if (cast->gtOverflow() && !varTypeIsUnsigned(srcType) && varTypeIsUnsigned(dstType))
        {
            //
            // An overflow check is needed only when casting from a signed type to ulong.
            // Change the cast type to uint to take advantage of the overflow check provided
            // by codegen and then zero extend the resulting uint to ulong.
            //

            loResult                       = cast;
            loResult->AsCast()->gtCastType = TYP_UINT;
            loResult->gtType               = TYP_INT;

            hiResult = m_compiler->gtNewZeroConNode(TYP_INT);

            Range().InsertAfter(loResult, hiResult);
        }
        else
        {
            if (!use.IsDummyUse() && use.User()->OperIs(GT_MUL))
            {
                //
                // This int->long cast is used by a GT_MUL that will be transformed by DecomposeMul into a
                // GT_MUL_LONG and as a result the high operand produced by the cast will become dead.
                // Skip cast decomposition so DecomposeMul doesn't need to bother with dead code removal,
                // especially in the case of sign extending casts that also introduce new lclvars.
                //

                assert(use.User()->Is64RsltMul());

                skipDecomposition = true;
            }
            else if (varTypeIsUnsigned(srcType))
            {
                const bool signExtend = !cast->IsUnsigned();
                loResult              = EnsureIntSized(cast->gtGetOp1(), signExtend);

                hiResult = m_compiler->gtNewZeroConNode(TYP_INT);

                Range().InsertAfter(cast, hiResult);
                Range().Remove(cast);
            }
            else
            {
                LIR::Use src(Range(), &(cast->AsOp()->gtOp1), cast);
                unsigned lclNum = src.ReplaceWithLclVar(m_compiler);

                loResult = src.Def();

                GenTree* loCopy  = m_compiler->gtNewLclvNode(lclNum, TYP_INT);
                GenTree* shiftBy = m_compiler->gtNewIconNode(31, TYP_INT);
                hiResult         = m_compiler->gtNewOperNode(GT_RSH, TYP_INT, loCopy, shiftBy);

                Range().InsertAfter(cast, loCopy, shiftBy, hiResult);
                Range().Remove(cast);
            }
        }
    }
    else
    {
        NYI("Unimplemented cast decomposition");
    }

    if (skipDecomposition)
    {
        return cast->gtNext;
    }

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeCnsLng: Decompose GT_CNS_LNG.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeCnsLng(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_CNS_LNG);

    GenTree* tree  = use.Def();
    INT32    loVal = tree->AsLngCon()->LoVal();
    INT32    hiVal = tree->AsLngCon()->HiVal();

    GenTree* loResult = tree;
    loResult->BashToConst(loVal);

    GenTree* hiResult = m_compiler->gtNewIconNode(hiVal);
    Range().InsertAfter(loResult, hiResult);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeFieldList: Decompose GT_FIELD_LIST.
//
// Arguments:
//    fieldList - the GT_FIELD_LIST node that uses the given GT_LONG node.
//    longNode - the node to decompose
//
// Return Value:
//    The next node to process.
//
// Notes:
//    Split a LONG field list element into two elements: one for each half of the GT_LONG.
//
GenTree* DecomposeLongs::DecomposeFieldList(GenTreeFieldList* fieldList, GenTreeOp* longNode)
{
    assert(longNode->OperIs(GT_LONG));

    GenTreeFieldList::Use* loUse = nullptr;
    for (GenTreeFieldList::Use& use : fieldList->Uses())
    {
        if (use.GetNode() == longNode)
        {
            loUse = &use;
            break;
        }
    }
    assert(loUse != nullptr);

    Range().Remove(longNode);

    loUse->SetNode(longNode->gtGetOp1());
    loUse->SetType(TYP_INT);

    fieldList->InsertFieldLIR(m_compiler, loUse, longNode->gtGetOp2(), loUse->GetOffset() + 4, TYP_INT);

    return fieldList->gtNext;
}

//------------------------------------------------------------------------
// DecomposeCall: Decompose GT_CALL.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeCall(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_CALL);

    // We only need to force var = call() if the call's result is used.
    return StoreNodeToVar(use);
}

//------------------------------------------------------------------------
// DecomposeStoreInd: Decompose GT_STOREIND.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeStoreInd(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_STOREIND);

    GenTree* tree = use.Def();

    assert(tree->AsOp()->gtOp2->OperIs(GT_LONG));

    // Example input (address expression omitted):
    //
    //  t51 = const     int    0x37C05E7D
    // t154 = const     int    0x2A0A3C80
    //      / --*  t51    int
    //      + --*  t154   int
    // t155 = *gt_long   long
    //      / --*  t52    byref
    //      + --*  t155   long
    //      *  storeIndir long

    GenTree* gtLong = tree->AsOp()->gtOp2;

    // Save address to a temp. It is used in storeIndLow and storeIndHigh trees.
    LIR::Use address(Range(), &tree->AsOp()->gtOp1, tree);
    address.ReplaceWithLclVar(m_compiler);
    JITDUMP("[DecomposeStoreInd]: Saving address tree to a temp var:\n");
    DISPTREERANGE(Range(), address.Def());

    if (!gtLong->AsOp()->gtOp1->OperIsLeaf())
    {
        LIR::Use op1(Range(), &gtLong->AsOp()->gtOp1, gtLong);
        op1.ReplaceWithLclVar(m_compiler);
        JITDUMP("[DecomposeStoreInd]: Saving low data tree to a temp var:\n");
        DISPTREERANGE(Range(), op1.Def());
    }

    if (!gtLong->AsOp()->gtOp2->OperIsLeaf())
    {
        LIR::Use op2(Range(), &gtLong->AsOp()->gtOp2, gtLong);
        op2.ReplaceWithLclVar(m_compiler);
        JITDUMP("[DecomposeStoreInd]: Saving high data tree to a temp var:\n");
        DISPTREERANGE(Range(), op2.Def());
    }

    GenTree* addrBase    = tree->AsOp()->gtOp1;
    GenTree* dataHigh    = gtLong->AsOp()->gtOp2;
    GenTree* dataLow     = gtLong->AsOp()->gtOp1;
    GenTree* storeIndLow = tree;

    Range().Remove(gtLong);
    Range().Remove(dataHigh);
    storeIndLow->AsOp()->gtOp2 = dataLow;
    storeIndLow->gtType        = TYP_INT;

    GenTree* addrBaseHigh = new (m_compiler, GT_LCL_VAR)
        GenTreeLclVar(GT_LCL_VAR, addrBase->TypeGet(), addrBase->AsLclVarCommon()->GetLclNum());
    GenTree* addrHigh =
        new (m_compiler, GT_LEA) GenTreeAddrMode(TYP_REF, addrBaseHigh, nullptr, 0, genTypeSize(TYP_INT));
    GenTree* storeIndHigh = new (m_compiler, GT_STOREIND) GenTreeStoreInd(TYP_INT, addrHigh, dataHigh);
    storeIndHigh->gtFlags = (storeIndLow->gtFlags & (GTF_ALL_EFFECT | GTF_LIVENESS_MASK));

    Range().InsertAfter(storeIndLow, dataHigh, addrBaseHigh, addrHigh, storeIndHigh);

    return storeIndHigh;

    // Example final output:
    //
    //      /--*  t52    byref
    //      *  st.lclVar byref  V07 rat0
    // t158 = lclVar    byref  V07 rat0
    //  t51 = const     int    0x37C05E7D
    //      /--*  t158   byref
    //      +--*  t51    int
    //      *  storeIndir int
    // t154 = const     int    0x2A0A3C80
    // t159 = lclVar    byref  V07 rat0
    //        /--*  t159   byref
    // t160 = *  lea(b + 4)  ref
    //      /--*  t154   int
    //      +--*  t160   ref
    //      *  storeIndir int
}

//------------------------------------------------------------------------
// DecomposeInd: Decompose GT_IND.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeInd(LIR::Use& use)
{
    GenTree* indLow = use.Def();

    LIR::Use address(Range(), &indLow->AsOp()->gtOp1, indLow);
    address.ReplaceWithLclVar(m_compiler);
    JITDUMP("[DecomposeInd]: Saving addr tree to a temp var:\n");
    DISPTREERANGE(Range(), address.Def());

    // Change the type of lower ind.
    indLow->gtType = TYP_INT;

    // Create tree of ind(addr+4)
    GenTree* addrBase     = indLow->gtGetOp1();
    GenTree* addrBaseHigh = new (m_compiler, GT_LCL_VAR)
        GenTreeLclVar(GT_LCL_VAR, addrBase->TypeGet(), addrBase->AsLclVarCommon()->GetLclNum());
    GenTree* addrHigh =
        new (m_compiler, GT_LEA) GenTreeAddrMode(TYP_REF, addrBaseHigh, nullptr, 0, genTypeSize(TYP_INT));
    GenTree* indHigh = new (m_compiler, GT_IND) GenTreeIndir(GT_IND, TYP_INT, addrHigh, nullptr);
    indHigh->gtFlags |= (indLow->gtFlags & (GTF_GLOB_REF | GTF_EXCEPT | GTF_IND_FLAGS));

    Range().InsertAfter(indLow, addrBaseHigh, addrHigh, indHigh);

    return FinalizeDecomposition(use, indLow, indHigh, indHigh);
}

//------------------------------------------------------------------------
// DecomposeNot: Decompose GT_NOT.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeNot(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_NOT);

    GenTree* tree   = use.Def();
    GenTree* gtLong = tree->gtGetOp1();
    noway_assert(gtLong->OperIs(GT_LONG));
    GenTree* loOp1 = gtLong->gtGetOp1();
    GenTree* hiOp1 = gtLong->gtGetOp2();

    Range().Remove(gtLong);

    GenTree* loResult       = tree;
    loResult->gtType        = TYP_INT;
    loResult->AsOp()->gtOp1 = loOp1;

    GenTree* hiResult = new (m_compiler, GT_NOT) GenTreeOp(GT_NOT, TYP_INT, hiOp1, nullptr);
    Range().InsertAfter(loResult, hiResult);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeNeg: Decompose GT_NEG.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeNeg(LIR::Use& use)
{
    assert(use.IsInitialized());
    assert(use.Def()->OperGet() == GT_NEG);

    GenTree* tree   = use.Def();
    GenTree* gtLong = tree->gtGetOp1();
    noway_assert(gtLong->OperIs(GT_LONG));

    GenTree* loOp1 = gtLong->gtGetOp1();
    GenTree* hiOp1 = gtLong->gtGetOp2();

    Range().Remove(gtLong);

    GenTree* loResult       = tree;
    loResult->gtType        = TYP_INT;
    loResult->AsOp()->gtOp1 = loOp1;

    GenTree* zero = m_compiler->gtNewZeroConNode(TYP_INT);

#if defined(TARGET_X86)

    GenTree* hiAdjust = m_compiler->gtNewOperNode(GT_ADD_HI, TYP_INT, hiOp1, zero);
    GenTree* hiResult = m_compiler->gtNewOperNode(GT_NEG, TYP_INT, hiAdjust);
    Range().InsertAfter(loResult, zero, hiAdjust, hiResult);

    loResult->gtFlags |= GTF_SET_FLAGS;

#elif defined(TARGET_ARM)

    // We tend to use "movs" to load zero to a register, and that sets the flags, so put the
    // zero before the loResult, which is setting the flags needed by GT_SUB_HI.
    GenTree* hiResult = m_compiler->gtNewOperNode(GT_SUB_HI, TYP_INT, zero, hiOp1);
    Range().InsertBefore(loResult, zero);
    Range().InsertAfter(loResult, hiResult);

    loResult->gtFlags |= GTF_SET_FLAGS;

#endif

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeArith: Decompose GT_ADD, GT_SUB, GT_OR, GT_XOR, GT_AND.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeArith(LIR::Use& use)
{
    assert(use.IsInitialized());

    GenTree*   tree = use.Def();
    genTreeOps oper = tree->OperGet();

    assert((oper == GT_ADD) || (oper == GT_SUB) || (oper == GT_OR) || (oper == GT_XOR) || (oper == GT_AND));

    GenTree* op1 = tree->gtGetOp1();
    GenTree* op2 = tree->gtGetOp2();

    // Both operands must have already been decomposed into GT_LONG operators.
    noway_assert(op1->OperIs(GT_LONG) && op2->OperIs(GT_LONG));

    // Capture the lo and hi halves of op1 and op2.
    GenTree* loOp1 = op1->gtGetOp1();
    GenTree* hiOp1 = op1->gtGetOp2();
    GenTree* loOp2 = op2->gtGetOp1();
    GenTree* hiOp2 = op2->gtGetOp2();

    // Now, remove op1 and op2 from the node list.
    Range().Remove(op1);
    Range().Remove(op2);

    // We will reuse "tree" for the loResult, which will now be of TYP_INT, and its operands
    // will be the lo halves of op1 from above.
    GenTree* loResult = tree;
    loResult->SetOper(GetLoOper(oper));
    loResult->gtType        = TYP_INT;
    loResult->AsOp()->gtOp1 = loOp1;
    loResult->AsOp()->gtOp2 = loOp2;

    GenTree* hiResult = new (m_compiler, oper) GenTreeOp(GetHiOper(oper), TYP_INT, hiOp1, hiOp2);
    Range().InsertAfter(loResult, hiResult);

    if ((oper == GT_ADD) || (oper == GT_SUB))
    {
        loResult->gtFlags |= GTF_SET_FLAGS;

        if ((loResult->gtFlags & GTF_OVERFLOW) != 0)
        {
            hiResult->gtFlags |= GTF_OVERFLOW | GTF_EXCEPT;
            loResult->gtFlags &= ~(GTF_OVERFLOW | GTF_EXCEPT);
        }
        if (loResult->gtFlags & GTF_UNSIGNED)
        {
            hiResult->gtFlags |= GTF_UNSIGNED;
        }
    }

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeShift: Decompose GT_LSH, GT_RSH, GT_RSZ. For shift nodes being shifted
// by a constant int, we can inspect the shift amount and decompose to the appropriate
// node types, generating a shl/shld pattern for GT_LSH, a shrd/shr pattern for GT_RSZ,
// and a shrd/sar pattern for GT_SHR for most shift amounts. Shifting by 0, >= 32 and
// >= 64 are special cased to produce better code patterns.
//
// For all other shift nodes, we need to use the shift helper functions, so we here convert
// the shift into a helper call by pulling its arguments out of linear order and making
// them the args to a call, then replacing the original node with the new call.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeShift(LIR::Use& use)
{
    assert(use.IsInitialized());

    GenTree* shift     = use.Def();
    GenTree* gtLong    = shift->gtGetOp1();
    GenTree* loOp1     = gtLong->gtGetOp1();
    GenTree* hiOp1     = gtLong->gtGetOp2();
    GenTree* shiftByOp = shift->gtGetOp2();

    genTreeOps oper        = shift->OperGet();
    genTreeOps shiftByOper = shiftByOp->OperGet();

    // tLo = ...
    // ...
    // tHi = ...
    // ...
    // tLong = long tLo, tHi
    // ...
    // tShiftAmount = ...
    // ...
    // tShift = shift tLong, tShiftAmount

    assert((oper == GT_LSH) || (oper == GT_RSH) || (oper == GT_RSZ));

    // If we are shifting by a constant int, we do not want to use a helper, instead, we decompose.
    if (shiftByOper == GT_CNS_INT)
    {
        // Reduce count modulo 64 to match behavior found in the shift helpers,
        // Compiler::gtFoldExpr and ValueNumStore::EvalOpIntegral.
        unsigned int count = shiftByOp->AsIntCon()->gtIconVal & 0x3F;
        Range().Remove(shiftByOp);

        if (count == 0)
        {
            GenTree* next = shift->gtNext;
            // Remove shift and don't do anything else.
            if (shift->IsUnusedValue())
            {
                gtLong->SetUnusedValue();
            }
            Range().Remove(shift);
            use.ReplaceWith(gtLong);
            return next;
        }

        GenTree* loResult;
        GenTree* hiResult;

        GenTree* insertAfter;

        switch (oper)
        {
            case GT_LSH:
            {
                if (count < 32)
                {
                    // For shifts of < 32 bits, we transform the code to:
                    //
                    //     tLo = ...
                    //           st.lclVar vLo, tLo
                    //     ...
                    //     tHi = ...
                    //     ...
                    //     tShiftLo = lsh vLo, tShiftAmountLo
                    //     tShitHiLong = long vLo, tHi
                    //     tShiftHi = lsh_hi tShiftHiLong, tShiftAmountHi
                    //
                    // This will produce:
                    //
                    //     reg1 = lo
                    //     shl lo, shift
                    //     shld hi, reg1, shift

                    loOp1                = RepresentOpAsLocalVar(loOp1, gtLong, &gtLong->AsOp()->gtOp1);
                    unsigned loOp1LclNum = loOp1->AsLclVarCommon()->GetLclNum();
                    Range().Remove(loOp1);

                    GenTree* shiftByHi = m_compiler->gtNewIconNode(count, TYP_INT);
                    GenTree* shiftByLo = m_compiler->gtNewIconNode(count, TYP_INT);

                    loResult = m_compiler->gtNewOperNode(GT_LSH, TYP_INT, loOp1, shiftByLo);

                    // Create a GT_LONG that contains loCopy and hiOp1. This will be used in codegen to
                    // generate the shld instruction
                    GenTree* loCopy = m_compiler->gtNewLclvNode(loOp1LclNum, TYP_INT);
                    GenTree* hiOp   = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, loCopy, hiOp1);
                    hiResult        = m_compiler->gtNewOperNode(GT_LSH_HI, TYP_INT, hiOp, shiftByHi);

                    Range().InsertBefore(shift, loOp1, shiftByLo, loResult);
                    Range().InsertBefore(shift, loCopy, hiOp, shiftByHi, hiResult);

                    insertAfter = hiResult;
                }
                else
                {
                    assert(count >= 32 && count < 64);

                    // Since we're left shifting at least 32 bits, we can remove the hi part of the shifted value iff
                    // it has no side effects.
                    //
                    // TODO-CQ: we could go perform this removal transitively (i.e. iteratively remove everything that
                    // feeds the hi operand while there are no side effects)
                    if ((hiOp1->gtFlags & GTF_ALL_EFFECT) == 0)
                    {
                        Range().Remove(hiOp1, true);
                    }
                    else
                    {
                        hiOp1->SetUnusedValue();
                    }

                    if (count == 32)
                    {
                        // Move loOp1 into hiResult (shift of 32 bits is just a mov of lo to hi)
                        // We need to make sure that we save lo to a temp variable so that we don't overwrite lo
                        // before saving it to hi in the case that we are doing an inplace shift. I.e.:
                        // x = x << 32

                        LIR::Use loOp1Use(Range(), &gtLong->AsOp()->gtOp1, gtLong);
                        loOp1Use.ReplaceWithLclVar(m_compiler);

                        hiResult = loOp1Use.Def();
                    }
                    else
                    {
                        assert(count > 32 && count < 64);

                        // Move loOp1 into hiResult, do a GT_LSH with count - 32.
                        // We will compute hiResult before loResult in this case, so we don't need to store lo to a
                        // temp
                        GenTree* shiftBy = m_compiler->gtNewIconNode(count - 32, TYP_INT);
                        hiResult         = m_compiler->gtNewOperNode(oper, TYP_INT, loOp1, shiftBy);
                        Range().InsertBefore(shift, shiftBy, hiResult);
                    }

                    // Zero out loResult (shift of >= 32 bits shifts all lo bits to hiResult)
                    loResult = m_compiler->gtNewZeroConNode(TYP_INT);
                    Range().InsertBefore(shift, loResult);

                    insertAfter = loResult;
                }
            }
            break;
            case GT_RSZ:
            {
                if (count < 32)
                {
                    // Hi is a GT_RSZ, lo is a GT_RSH_LO. Will produce:
                    // reg1 = hi
                    // shrd lo, reg1, shift
                    // shr hi, shift

                    hiOp1                = RepresentOpAsLocalVar(hiOp1, gtLong, &gtLong->AsOp()->gtOp2);
                    unsigned hiOp1LclNum = hiOp1->AsLclVarCommon()->GetLclNum();
                    GenTree* hiCopy      = m_compiler->gtNewLclvNode(hiOp1LclNum, TYP_INT);

                    GenTree* shiftByHi = m_compiler->gtNewIconNode(count, TYP_INT);
                    GenTree* shiftByLo = m_compiler->gtNewIconNode(count, TYP_INT);

                    hiResult = m_compiler->gtNewOperNode(GT_RSZ, TYP_INT, hiOp1, shiftByHi);

                    // Create a GT_LONG that contains loOp1 and hiCopy. This will be used in codegen to
                    // generate the shrd instruction
                    GenTree* loOp = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, loOp1, hiCopy);
                    loResult      = m_compiler->gtNewOperNode(GT_RSH_LO, TYP_INT, loOp, shiftByLo);

                    Range().InsertBefore(shift, hiCopy, loOp);
                    Range().InsertBefore(shift, shiftByLo, loResult);
                    Range().InsertBefore(shift, shiftByHi, hiResult);
                }
                else
                {
                    assert(count >= 32 && count < 64);

                    // Since we're right shifting at least 32 bits, we can remove the lo part of the shifted value iff
                    // it has no side effects.
                    //
                    // TODO-CQ: we could go perform this removal transitively (i.e. iteratively remove everything that
                    // feeds the lo operand while there are no side effects)
                    if ((loOp1->gtFlags & (GTF_ALL_EFFECT | GTF_SET_FLAGS)) == 0)
                    {
                        Range().Remove(loOp1, true);
                    }
                    else
                    {
                        loOp1->SetUnusedValue();
                    }

                    if (count == 32)
                    {
                        // Move hiOp1 into loResult.
                        loResult = hiOp1;
                    }
                    else
                    {
                        assert(count > 32 && count < 64);

                        // Move hiOp1 into loResult, do a GT_RSZ with count - 32.
                        GenTree* shiftBy = m_compiler->gtNewIconNode(count - 32, TYP_INT);
                        loResult         = m_compiler->gtNewOperNode(oper, TYP_INT, hiOp1, shiftBy);
                        Range().InsertBefore(shift, shiftBy, loResult);
                    }

                    // Zero out hi
                    hiResult = m_compiler->gtNewZeroConNode(TYP_INT);
                    Range().InsertBefore(shift, hiResult);
                }

                insertAfter = hiResult;
            }
            break;
            case GT_RSH:
            {
                hiOp1                = RepresentOpAsLocalVar(hiOp1, gtLong, &gtLong->AsOp()->gtOp2);
                unsigned hiOp1LclNum = hiOp1->AsLclVarCommon()->GetLclNum();
                GenTree* hiCopy      = m_compiler->gtNewLclvNode(hiOp1LclNum, TYP_INT);
                Range().Remove(hiOp1);

                if (count < 32)
                {
                    // Hi is a GT_RSH, lo is a GT_RSH_LO. Will produce:
                    // reg1 = hi
                    // shrd lo, reg1, shift
                    // sar hi, shift

                    GenTree* shiftByHi = m_compiler->gtNewIconNode(count, TYP_INT);
                    GenTree* shiftByLo = m_compiler->gtNewIconNode(count, TYP_INT);

                    hiResult = m_compiler->gtNewOperNode(GT_RSH, TYP_INT, hiOp1, shiftByHi);

                    // Create a GT_LONG that contains loOp1 and hiCopy. This will be used in codegen to
                    // generate the shrd instruction
                    GenTree* loOp = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, loOp1, hiCopy);
                    loResult      = m_compiler->gtNewOperNode(GT_RSH_LO, TYP_INT, loOp, shiftByLo);

                    Range().InsertBefore(shift, hiCopy, loOp);
                    Range().InsertBefore(shift, shiftByLo, loResult);
                    Range().InsertBefore(shift, shiftByHi, hiOp1, hiResult);
                }
                else
                {
                    assert(count >= 32 && count < 64);

                    // Since we're right shifting at least 32 bits, we can remove the lo part of the shifted value iff
                    // it has no side effects.
                    //
                    // TODO-CQ: we could go perform this removal transitively (i.e. iteratively remove everything that
                    // feeds the lo operand while there are no side effects)
                    if ((loOp1->gtFlags & (GTF_ALL_EFFECT | GTF_SET_FLAGS)) == 0)
                    {
                        Range().Remove(loOp1, true);
                    }
                    else
                    {
                        loOp1->SetUnusedValue();
                    }

                    if (count == 32)
                    {
                        // Move hiOp1 into loResult.
                        loResult = hiOp1;
                        Range().InsertBefore(shift, loResult);
                    }
                    else
                    {
                        assert(count > 32 && count < 64);

                        // Move hiOp1 into loResult, do a GT_RSH with count - 32.
                        GenTree* shiftBy = m_compiler->gtNewIconNode(count - 32, TYP_INT);
                        loResult         = m_compiler->gtNewOperNode(oper, TYP_INT, hiOp1, shiftBy);
                        Range().InsertBefore(shift, hiOp1, shiftBy, loResult);
                    }

                    // Propagate sign bit in hiResult
                    GenTree* shiftBy = m_compiler->gtNewIconNode(31, TYP_INT);
                    hiResult         = m_compiler->gtNewOperNode(GT_RSH, TYP_INT, hiCopy, shiftBy);
                    Range().InsertBefore(shift, shiftBy, hiCopy, hiResult);
                }

                insertAfter = hiResult;
            }
            break;
            default:
                unreached();
        }

        // Remove shift from Range
        Range().Remove(gtLong);
        Range().Remove(shift);

        return FinalizeDecomposition(use, loResult, hiResult, insertAfter);
    }
    else
    {
        // Because calls must be created as HIR and lowered to LIR, we need to dump
        // any LIR temps into lclVars before using them as arguments.
        shiftByOp = RepresentOpAsLocalVar(shiftByOp, shift, &shift->AsOp()->gtOp2);
        loOp1     = RepresentOpAsLocalVar(loOp1, gtLong, &gtLong->AsOp()->gtOp1);
        hiOp1     = RepresentOpAsLocalVar(hiOp1, gtLong, &gtLong->AsOp()->gtOp2);

        Range().Remove(shiftByOp);
        Range().Remove(gtLong);
        Range().Remove(loOp1);
        Range().Remove(hiOp1);

        unsigned helper;

        switch (oper)
        {
            case GT_LSH:
                helper = CORINFO_HELP_LLSH;
                break;
            case GT_RSH:
                helper = CORINFO_HELP_LRSH;
                break;
            case GT_RSZ:
                helper = CORINFO_HELP_LRSZ;
                break;
            default:
                unreached();
        }

        GenTreeCall* call       = m_compiler->gtNewHelperCallNode(helper, TYP_LONG);
        NewCallArg   loArg      = NewCallArg::Primitive(loOp1).WellKnown(WellKnownArg::ShiftLow);
        NewCallArg   hiArg      = NewCallArg::Primitive(hiOp1).WellKnown(WellKnownArg::ShiftHigh);
        NewCallArg   shiftByArg = NewCallArg::Primitive(shiftByOp);
        call->gtArgs.PushFront(m_compiler, loArg, hiArg, shiftByArg);
        call->gtFlags |= shift->gtFlags & GTF_ALL_EFFECT;

        if (shift->IsUnusedValue())
        {
            call->SetUnusedValue();
        }

        call = m_compiler->fgMorphArgs(call);
        Range().InsertAfter(shift, LIR::SeqTree(m_compiler, call));

        Range().Remove(shift);
        use.ReplaceWith(call);
        return call;
    }
}

//------------------------------------------------------------------------
// DecomposeRotate: Decompose GT_ROL and GT_ROR with constant shift amounts. We can
// inspect the rotate amount and decompose to the appropriate node types, generating
// a shld/shld pattern for GT_ROL, a shrd/shrd pattern for GT_ROR, for most rotate
// amounts.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeRotate(LIR::Use& use)
{
    GenTree* tree       = use.Def();
    GenTree* gtLong     = tree->gtGetOp1();
    GenTree* rotateByOp = tree->gtGetOp2();

    genTreeOps oper = tree->OperGet();

    assert((oper == GT_ROL) || (oper == GT_ROR));
    assert(rotateByOp->IsCnsIntOrI());

    // For longs, we need to change rols into two GT_LSH_HIs and rors into two GT_RSH_LOs
    // so we will get:
    //
    // shld lo, hiCopy, rotateAmount
    // shld hi, loCopy, rotateAmount
    //
    // or:
    //
    // shrd lo, hiCopy, rotateAmount
    // shrd hi, loCopy, rotateAmount

    if (oper == GT_ROL)
    {
        oper = GT_LSH_HI;
    }
    else
    {
        oper = GT_RSH_LO;
    }

    unsigned count = (unsigned)rotateByOp->AsIntCon()->gtIconVal;
    Range().Remove(rotateByOp);

    // Make sure the rotate amount is between 0 and 63.
    assert((count < 64) && (count != 0));

    GenTree* loResult;
    GenTree* hiResult;

    if (count == 32)
    {
        // If the rotate amount is 32, then swap hi and lo
        LIR::Use loOp1Use(Range(), &gtLong->AsOp()->gtOp1, gtLong);
        loOp1Use.ReplaceWithLclVar(m_compiler);

        LIR::Use hiOp1Use(Range(), &gtLong->AsOp()->gtOp2, gtLong);
        hiOp1Use.ReplaceWithLclVar(m_compiler);

        hiResult              = loOp1Use.Def();
        loResult              = hiOp1Use.Def();
        gtLong->AsOp()->gtOp1 = loResult;
        gtLong->AsOp()->gtOp2 = hiResult;

        if (tree->IsUnusedValue())
        {
            gtLong->SetUnusedValue();
        }

        GenTree* next = tree->gtNext;
        // Remove tree and don't do anything else.
        Range().Remove(tree);
        use.ReplaceWith(gtLong);
        return next;
    }
    else
    {
        GenTree* loOp1;
        GenTree* hiOp1;

        if (count > 32)
        {
            // If count > 32, we swap hi and lo, and subtract 32 from count
            hiOp1 = gtLong->gtGetOp1();
            loOp1 = gtLong->gtGetOp2();

            loOp1 = RepresentOpAsLocalVar(loOp1, gtLong, &gtLong->AsOp()->gtOp2);
            hiOp1 = RepresentOpAsLocalVar(hiOp1, gtLong, &gtLong->AsOp()->gtOp1);

            count -= 32;
        }
        else
        {
            loOp1 = gtLong->gtGetOp1();
            hiOp1 = gtLong->gtGetOp2();

            loOp1 = RepresentOpAsLocalVar(loOp1, gtLong, &gtLong->AsOp()->gtOp1);
            hiOp1 = RepresentOpAsLocalVar(hiOp1, gtLong, &gtLong->AsOp()->gtOp2);
        }

        if (oper == GT_RSH_LO)
        {
            // lsra/codegen expects these operands in the opposite order
            std::swap(loOp1, hiOp1);
        }
        Range().Remove(gtLong);

        unsigned loOp1LclNum = loOp1->AsLclVarCommon()->GetLclNum();
        unsigned hiOp1LclNum = hiOp1->AsLclVarCommon()->GetLclNum();

        Range().Remove(loOp1);
        Range().Remove(hiOp1);

        GenTree* rotateByHi = m_compiler->gtNewIconNode(count, TYP_INT);
        GenTree* rotateByLo = m_compiler->gtNewIconNode(count, TYP_INT);

        // Create a GT_LONG that contains loOp1 and hiCopy. This will be used in codegen to
        // generate the shld instruction
        GenTree* hiCopy = m_compiler->gtNewLclvNode(hiOp1LclNum, TYP_INT);
        GenTree* loOp   = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, hiCopy, loOp1);
        loResult        = m_compiler->gtNewOperNode(oper, TYP_INT, loOp, rotateByLo);

        // Create a GT_LONG that contains loCopy and hiOp1. This will be used in codegen to
        // generate the shld instruction
        GenTree* loCopy = m_compiler->gtNewLclvNode(loOp1LclNum, TYP_INT);
        GenTree* hiOp   = new (m_compiler, GT_LONG) GenTreeOp(GT_LONG, TYP_LONG, loCopy, hiOp1);
        hiResult        = m_compiler->gtNewOperNode(oper, TYP_INT, hiOp, rotateByHi);

        Range().InsertBefore(tree, hiCopy, loOp1, loOp);
        Range().InsertBefore(tree, rotateByLo, loResult);
        Range().InsertBefore(tree, loCopy, hiOp1, hiOp);
        Range().InsertBefore(tree, rotateByHi, hiResult);

        Range().Remove(tree);

        return FinalizeDecomposition(use, loResult, hiResult, hiResult);
    }
}

//------------------------------------------------------------------------
// DecomposeSelect: Decompose 64-bit GT_SELECT into a 32-bit GT_SELECT and
// 32-bit GT_SELECT_HI.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeSelect(LIR::Use& use)
{
    GenTreeConditional* select = use.Def()->AsConditional();
    GenTree*            op1    = select->gtOp1;
    GenTree*            op2    = select->gtOp2;

    assert(op1->OperIs(GT_LONG));
    assert(op2->OperIs(GT_LONG));

    GenTree* loOp1 = op1->gtGetOp1();
    GenTree* hiOp1 = op1->gtGetOp2();

    GenTree* loOp2 = op2->gtGetOp1();
    GenTree* hiOp2 = op2->gtGetOp2();

    select->gtType = TYP_INT;
    select->gtOp1  = loOp1;
    select->gtOp2  = loOp2;

    Range().Remove(op1);
    Range().Remove(op2);

    // Normally GT_SELECT is responsible for evaluating the condition into
    // flags, but for the "upper half" we treat the lower GT_SELECT similar to
    // other flag producing nodes and reuse them. GT_SELECTCC is the variant
    // that uses existing flags and has no condition as part of it.
    select->gtFlags |= GTF_SET_FLAGS;
    GenTree* hiSelect = m_compiler->gtNewOperCC(GT_SELECTCC, TYP_INT, GenCondition::NE, hiOp1, hiOp2);

    Range().InsertAfter(select, hiSelect);

    return FinalizeDecomposition(use, select, hiSelect, hiSelect);
}

//------------------------------------------------------------------------
// DecomposeMul: Decompose GT_MUL. The only GT_MULs that make it to decompose are
// those with the GTF_MUL_64RSLT flag set. These muls result in a mul instruction that
// returns its result in two registers like GT_CALLs do. Additionally, these muls are
// guaranteed to be in the form long = (long)int * (long)int. Therefore, to decompose
// these nodes, we convert them into GT_MUL_LONGs, undo the cast from int to long by
// stripping out the lo ops, and force them into the form var = mul, as we do for
// GT_CALLs. In codegen, we then produce a mul instruction that produces the result
// in edx:eax on x86 or in any two chosen by RA registers on arm32, and store those
// registers on the stack in genStoreLongLclVar.
//
// All other GT_MULs have been converted to helper calls in morph.cpp
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeMul(LIR::Use& use)
{
    assert(use.IsInitialized());

    GenTree* tree = use.Def();

    assert(tree->OperIs(GT_MUL));
    assert(tree->Is64RsltMul());

    GenTree* op1 = tree->gtGetOp1();
    GenTree* op2 = tree->gtGetOp2();

    assert(op1->TypeIs(TYP_LONG) && op2->TypeIs(TYP_LONG));

    // We expect the first operand to be an int->long cast.
    // DecomposeCast specifically ignores such casts when they are used by GT_MULs.
    assert(op1->OperIs(GT_CAST));

    // The second operand can be a cast or a constant.
    if (!op2->OperIs(GT_CAST))
    {
        assert(op2->OperIs(GT_LONG));
        assert(op2->gtGetOp1()->IsIntegralConst());
        assert(op2->gtGetOp2()->IsIntegralConst());

        Range().Remove(op2->gtGetOp2());
    }

    Range().Remove(op1);
    Range().Remove(op2);

    tree->AsOp()->gtOp1 = op1->gtGetOp1();
    tree->AsOp()->gtOp2 = op2->gtGetOp1();
    tree->SetOper(GT_MUL_LONG);

    return StoreNodeToVar(use);
}

//------------------------------------------------------------------------
// DecomposeUMod: Decompose GT_UMOD. The only GT_UMODs that make it to decompose
// are guaranteed to be an unsigned long mod with op2 which is a cast to long from
// a constant int whose value is between 2 and 0x3fffffff. All other GT_UMODs are
// morphed into helper calls. These GT_UMODs will actually return an int value in
// RDX. In decompose, we make the lo operation a TYP_INT GT_UMOD, with op2 as the
// original lo half and op1 as a GT_LONG. We make the hi part 0,  so we end up with:
//
// GT_UMOD[TYP_INT] ( GT_LONG [TYP_LONG] (loOp1, hiOp1), loOp2 [TYP_INT] )
//
// With the expectation that we will generate:
//
// EDX = hiOp1
// EAX = loOp1
// reg = loOp2
// idiv reg
// EDX is the remainder, and result of GT_UMOD
// mov hiReg = 0
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeUMod(LIR::Use& use)
{
    assert(use.IsInitialized());

    GenTree*   tree = use.Def();
    genTreeOps oper = tree->OperGet();

    assert(oper == GT_UMOD);

    GenTree* op1 = tree->gtGetOp1();
    GenTree* op2 = tree->gtGetOp2();
    assert(op1->OperIs(GT_LONG));
    assert(op2->OperIs(GT_LONG));

    GenTree* loOp2 = op2->gtGetOp1();
    GenTree* hiOp2 = op2->gtGetOp2();

    assert(loOp2->OperIs(GT_CNS_INT));
    assert(hiOp2->OperIs(GT_CNS_INT));
    assert((loOp2->AsIntCon()->gtIconVal >= 2) && (loOp2->AsIntCon()->gtIconVal <= 0x3fffffff));
    assert(hiOp2->AsIntCon()->gtIconVal == 0);

    // Get rid of op2's hi part. We don't need it.
    Range().Remove(hiOp2);
    Range().Remove(op2);

    // Lo part is the GT_UMOD
    GenTree* loResult       = tree;
    loResult->AsOp()->gtOp2 = loOp2;
    loResult->gtType        = TYP_INT;

    // Set the high part to 0
    GenTree* hiResult = m_compiler->gtNewZeroConNode(TYP_INT);

    Range().InsertAfter(loResult, hiResult);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

#ifdef FEATURE_HW_INTRINSICS

//------------------------------------------------------------------------
// DecomposeHWIntrinsic: Decompose GT_HWINTRINSIC.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeHWIntrinsic(LIR::Use& use)
{
    GenTree* tree = use.Def();
    assert(tree->OperIs(GT_HWINTRINSIC));

    GenTreeHWIntrinsic* hwintrinsicTree = tree->AsHWIntrinsic();

    switch (hwintrinsicTree->GetHWIntrinsicId())
    {
        case NI_Vector128_GetElement:
        case NI_Vector256_GetElement:
        case NI_Vector512_GetElement:
        {
            return DecomposeHWIntrinsicGetElement(use, hwintrinsicTree);
        }

        case NI_Vector128_ToScalar:
        case NI_Vector256_ToScalar:
        case NI_Vector512_ToScalar:
        {
            return DecomposeHWIntrinsicToScalar(use, hwintrinsicTree);
        }

        case NI_AVX512_MoveMask:
        {
            return DecomposeHWIntrinsicMoveMask(use, hwintrinsicTree);
        }

        default:
        {
            noway_assert(!"unexpected GT_HWINTRINSIC node in long decomposition");
            break;
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------
// DecomposeHWIntrinsicGetElement: Decompose GT_HWINTRINSIC -- NI_Vector*_GetElement.
//
// Decompose a get[i] node on Vector*<long>. For:
//
// GT_HWINTRINSIC{GetElement}[long](simd_var, index)
//
// create:
//
// tmp_simd_var = simd_var
// tmp_index_times_two = index * 2
// lo_result = GT_HWINTRINSIC{GetElement}[int](tmp_simd_var, tmp_index_times_two)
// hi_result = GT_HWINTRINSIC{GetElement}[int](tmp_simd_var, tmp_index_times_two + 1)
// return: GT_LONG(lo_result, hi_result)
//
// This isn't optimal codegen, since NI_Vector*_GetElement sometimes requires
// temps that could be shared, for example.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//   node - the hwintrinsic node to decompose
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeHWIntrinsicGetElement(LIR::Use& use, GenTreeHWIntrinsic* node)
{
    assert(node == use.Def());
    assert(varTypeIsLong(node));
    assert(HWIntrinsicInfo::IsVectorGetElement(node->GetHWIntrinsicId()));

    GenTree*  op1          = node->Op(1);
    GenTree*  op2          = node->Op(2);
    var_types simdBaseType = node->GetSimdBaseType();
    unsigned  simdSize     = node->GetSimdSize();

    assert(varTypeIsLong(simdBaseType));
    assert(varTypeIsSIMD(op1->TypeGet()));
    assert(op2->TypeIs(TYP_INT));

    GenTree* simdTmpVar    = RepresentOpAsLocalVar(op1, node, &node->Op(1));
    unsigned simdTmpVarNum = simdTmpVar->AsLclVarCommon()->GetLclNum();
    JITDUMP("[DecomposeHWIntrinsicGetElement]: Saving op1 tree to a temp var:\n");
    DISPTREERANGE(Range(), simdTmpVar);
    Range().Remove(simdTmpVar);

    GenTree* indexTimesTwo;
    bool     indexIsConst = op2->OperIsConst();

    if (indexIsConst)
    {
        // Reuse the existing index constant node.

        indexTimesTwo = op2;
        Range().Remove(op2);

        indexTimesTwo->AsIntCon()->SetIconValue(op2->AsIntCon()->IconValue() * 2);
        Range().InsertBefore(node, simdTmpVar, indexTimesTwo);
    }
    else
    {
        GenTree* one  = m_compiler->gtNewIconNode(1, TYP_INT);
        indexTimesTwo = m_compiler->gtNewOperNode(GT_LSH, TYP_INT, op2, one);
        Range().InsertBefore(node, simdTmpVar, one, indexTimesTwo);
    }

    // Create:
    //      loResult = GT_HWINTRINSIC{GetElement}[int](tmp_simd_var, tmp_index_times_two)

    GenTreeHWIntrinsic* loResult =
        m_compiler->gtNewSimdHWIntrinsicNode(TYP_INT, simdTmpVar, indexTimesTwo, node->GetHWIntrinsicId(),
                                             CORINFO_TYPE_INT, simdSize);
    Range().InsertBefore(node, loResult);

    simdTmpVar = m_compiler->gtNewLclLNode(simdTmpVarNum, simdTmpVar->TypeGet());
    Range().InsertBefore(node, simdTmpVar);

    // Create:
    //      hiResult = GT_HWINTRINSIC{GetElement}[int](tmp_simd_var, tmp_index_times_two + 1)

    GenTree* indexTimesTwoPlusOne;

    if (indexIsConst)
    {
        indexTimesTwoPlusOne = m_compiler->gtNewIconNode(indexTimesTwo->AsIntCon()->IconValue() + 1, TYP_INT);
        Range().InsertBefore(node, indexTimesTwoPlusOne);
    }
    else
    {
        indexTimesTwo                = RepresentOpAsLocalVar(indexTimesTwo, loResult, &loResult->Op(2));
        unsigned indexTimesTwoVarNum = indexTimesTwo->AsLclVarCommon()->GetLclNum();
        JITDUMP("[DecomposeHWIntrinsicWithElement]: Saving indexTimesTwo tree to a temp var:\n");
        DISPTREERANGE(Range(), indexTimesTwo);

        indexTimesTwo        = m_compiler->gtNewLclLNode(indexTimesTwoVarNum, indexTimesTwo->TypeGet());
        GenTree* one         = m_compiler->gtNewIconNode(1, TYP_INT);
        indexTimesTwoPlusOne = m_compiler->gtNewOperNode(GT_ADD, TYP_INT, indexTimesTwo, one);
        Range().InsertBefore(node, indexTimesTwo, one, indexTimesTwoPlusOne);
    }

    GenTreeHWIntrinsic* hiResult =
        m_compiler->gtNewSimdHWIntrinsicNode(TYP_INT, simdTmpVar, indexTimesTwoPlusOne, node->GetHWIntrinsicId(),
                                             CORINFO_TYPE_INT, simdSize);
    Range().InsertBefore(node, hiResult);

    // Done with the original tree; remove it.
    Range().Remove(node);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeHWIntrinsicToScalar: Decompose GT_HWINTRINSIC -- NI_Vector*_ToScalar.
//
// create:
//
// tmp_simd_var = simd_var
// lo_result = GT_HWINTRINSIC{ToScalar}[int](tmp_simd_var)
// hi_result = GT_HWINTRINSIC{GetElement}[int](tmp_simd_var, 1)
//             - or -
//             GT_HWINTRINSIC{ToScalar}[int](GT_RSZ(tmp_simd_var, 32))
// return: GT_LONG(lo_result, hi_result)
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//   node - the hwintrinsic node to decompose
//
// Return Value:
//    The GT_LONG node wrapping the upper and lower halves.
//
GenTree* DecomposeLongs::DecomposeHWIntrinsicToScalar(LIR::Use& use, GenTreeHWIntrinsic* node)
{
    assert(node == use.Def());
    assert(varTypeIsLong(node));
    assert(HWIntrinsicInfo::IsVectorToScalar(node->GetHWIntrinsicId()));

    GenTree*       op1          = node->Op(1);
    NamedIntrinsic intrinsicId  = node->GetHWIntrinsicId();
    var_types      simdBaseType = node->GetSimdBaseType();
    unsigned       simdSize     = node->GetSimdSize();

    assert(varTypeIsLong(simdBaseType));
    assert(varTypeIsSIMD(op1));

    GenTree* simdTmpVar    = RepresentOpAsLocalVar(op1, node, &node->Op(1));
    unsigned simdTmpVarNum = simdTmpVar->AsLclVarCommon()->GetLclNum();
    JITDUMP("[DecomposeHWIntrinsicToScalar]: Saving op1 tree to a temp var:\n");
    DISPTREERANGE(Range(), simdTmpVar);

    GenTree* loResult = m_compiler->gtNewSimdToScalarNode(TYP_INT, simdTmpVar, CORINFO_TYPE_INT, simdSize);
    Range().InsertAfter(simdTmpVar, loResult);

    simdTmpVar = m_compiler->gtNewLclLNode(simdTmpVarNum, simdTmpVar->TypeGet());
    Range().InsertAfter(loResult, simdTmpVar);

    GenTree* hiResult;
    if (m_compiler->compOpportunisticallyDependsOn(InstructionSet_SSE42))
    {
        GenTree* one = m_compiler->gtNewIconNode(1);
        hiResult     = m_compiler->gtNewSimdGetElementNode(TYP_INT, simdTmpVar, one, CORINFO_TYPE_INT, simdSize);

        Range().InsertAfter(simdTmpVar, one, hiResult);
    }
    else
    {
        GenTree* thirtyTwo = m_compiler->gtNewIconNode(32);
        GenTree* shift     = m_compiler->gtNewSimdBinOpNode(GT_RSZ, op1->TypeGet(), simdTmpVar, thirtyTwo,
                                                            node->GetSimdBaseJitType(), simdSize);
        hiResult           = m_compiler->gtNewSimdToScalarNode(TYP_INT, shift, CORINFO_TYPE_INT, simdSize);

        Range().InsertAfter(simdTmpVar, thirtyTwo, shift, hiResult);
    }

    Range().Remove(node);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}

//------------------------------------------------------------------------
// DecomposeHWIntrinsicMoveMask: Decompose GT_HWINTRINSIC -- NI_AVX512_MoveMask
//
// Decompose a MoveMask(x) node on Vector512<*>. For:
//
// GT_HWINTRINSIC{MoveMask}[*](simd_var)
//
// create:
//
// tmp_simd_var = simd_var
// tmp_simd_lo  = GT_HWINTRINSIC{GetLower}(tmp_simd_var)
// lo_result = GT_HWINTRINSIC{MoveMask}(tmp_simd_lo)
// tmp_simd_hi  = GT_HWINTRINSIC{GetUpper}(tmp_simd_var)
// hi_result = GT_HWINTRINSIC{MoveMask}(tmp_simd_hi)
// return: GT_LONG(lo_result, hi_result)
//
// Noting that for all types except byte/sbyte, hi_result will be exclusively
// zero and so we can actually optimize this a bit more directly
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//   node - the hwintrinsic node to decompose
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::DecomposeHWIntrinsicMoveMask(LIR::Use& use, GenTreeHWIntrinsic* node)
{
    assert(node == use.Def());
    assert(varTypeIsLong(node));
    assert(node->GetHWIntrinsicId() == NI_AVX512_MoveMask);

    GenTree*    op1             = node->Op(1);
    CorInfoType simdBaseJitType = node->GetSimdBaseJitType();
    var_types   simdBaseType    = node->GetSimdBaseType();
    unsigned    simdSize        = node->GetSimdSize();

    assert(varTypeIsArithmetic(simdBaseType));
    assert(op1->TypeIs(TYP_MASK));
    assert(simdSize == 64);

    GenTree* loResult = nullptr;
    GenTree* hiResult = nullptr;

    if (varTypeIsByte(simdBaseType))
    {
        // Create:
        //      simdTmpVar = op1

        GenTree* simdTmpVar    = RepresentOpAsLocalVar(op1, node, &node->Op(1));
        unsigned simdTmpVarNum = simdTmpVar->AsLclVarCommon()->GetLclNum();
        JITDUMP("[DecomposeHWIntrinsicMoveMask]: Saving op1 tree to a temp var:\n");
        DISPTREERANGE(Range(), simdTmpVar);
        Range().Remove(simdTmpVar);

        Range().InsertBefore(node, simdTmpVar);

        // Create:
        //      loResult  = GT_HWINTRINSIC{MoveMask}(simdTmpVar)

        loResult = m_compiler->gtNewSimdHWIntrinsicNode(TYP_INT, simdTmpVar, NI_AVX512_MoveMask, simdBaseJitType, 32);
        Range().InsertBefore(node, loResult);

        simdTmpVar = m_compiler->gtNewLclLNode(simdTmpVarNum, simdTmpVar->TypeGet());
        Range().InsertBefore(node, simdTmpVar);

        // Create:
        //      simdTmpVar = GT_HWINTRINSIC{ShiftRightMask}(simdTmpVar, 32)
        //      hiResult  = GT_HWINTRINSIC{MoveMask}(simdTmpVar)

        GenTree* shiftIcon = m_compiler->gtNewIconNode(32, TYP_INT);
        Range().InsertBefore(node, shiftIcon);

        simdTmpVar = m_compiler->gtNewSimdHWIntrinsicNode(TYP_MASK, simdTmpVar, shiftIcon, NI_AVX512_ShiftRightMask,
                                                          simdBaseJitType, 64);
        Range().InsertBefore(node, simdTmpVar);

        hiResult = m_compiler->gtNewSimdHWIntrinsicNode(TYP_INT, simdTmpVar, NI_AVX512_MoveMask, simdBaseJitType, 32);
        Range().InsertBefore(node, hiResult);
    }
    else
    {
        // Create:
        //      loResult  = GT_HWINTRINSIC{MoveMask}(op1)

        loResult = m_compiler->gtNewSimdHWIntrinsicNode(TYP_INT, op1, NI_AVX512_MoveMask, simdBaseJitType, simdSize);
        Range().InsertBefore(node, loResult);

        // Create:
        //      hiResult  = GT_ICON(0)

        hiResult = m_compiler->gtNewZeroConNode(TYP_INT);
        Range().InsertBefore(node, hiResult);
    }

    // Done with the original tree; remove it.
    Range().Remove(node);

    return FinalizeDecomposition(use, loResult, hiResult, hiResult);
}
#endif // FEATURE_HW_INTRINSICS

//------------------------------------------------------------------------
// OptimizeCastFromDecomposedLong: optimizes a cast from GT_LONG by discarding
// the high part of the source and, if the cast is to INT, the cast node itself.
// Accounts for side effects and marks nodes unused as necessary.
//
// Only accepts casts to integer types that are not long.
// Does not optimize checked casts.
//
// Arguments:
//    cast     - the cast tree that has a GT_LONG node as its operand.
//    nextNode - the next candidate for decomposition.
//
// Return Value:
//    The next node to process in DecomposeRange: "nextNode->gtNext" if
//    "cast == nextNode", simply "nextNode" otherwise.
//
// Notes:
//    Because "nextNode" usually is "cast", and this method may remove "cast"
//    from the linear order, it needs to return the updated "nextNode". Instead
//    of receiving it as an argument, it could assume that "nextNode" is always
//    "cast->CastOp()->gtNext", but not making that assumption seems better.
//
GenTree* DecomposeLongs::OptimizeCastFromDecomposedLong(GenTreeCast* cast, GenTree* nextNode)
{
    GenTreeOp* src     = cast->CastOp()->AsOp();
    var_types  dstType = cast->CastToType();

    assert(src->OperIs(GT_LONG));
    assert(genActualType(dstType) == TYP_INT);

    if (cast->gtOverflow())
    {
        return nextNode;
    }

    GenTree* loSrc = src->gtGetOp1();
    GenTree* hiSrc = src->gtGetOp2();

    JITDUMP("Optimizing a truncating cast [%06u] from decomposed LONG [%06u]\n", cast->gtTreeID, src->gtTreeID);
    INDEBUG(GenTree* treeToDisplay = cast);

    // TODO-CQ: we could go perform this removal transitively.
    // See also identical code in shift decomposition.
    if ((hiSrc->gtFlags & (GTF_ALL_EFFECT | GTF_SET_FLAGS)) == 0)
    {
        JITDUMP("Removing the HI part of [%06u] and marking its operands unused:\n", src->gtTreeID);
        DISPNODE(hiSrc);
        Range().Remove(hiSrc, /* markOperandsUnused */ true);
    }
    else
    {
        JITDUMP("The HI part of [%06u] has side effects, marking it unused\n", src->gtTreeID);
        hiSrc->SetUnusedValue();
    }

    JITDUMP("Removing the LONG source:\n");
    DISPNODE(src);
    Range().Remove(src);

    if (varTypeIsSmall(dstType))
    {
        JITDUMP("Cast is to a small type, keeping it, the new source is [%06u]\n", loSrc->gtTreeID);
        cast->CastOp() = loSrc;
    }
    else
    {
        LIR::Use useOfCast;
        if (Range().TryGetUse(cast, &useOfCast))
        {
            useOfCast.ReplaceWith(loSrc);
        }
        else
        {
            loSrc->SetUnusedValue();
        }

        if (nextNode == cast)
        {
            nextNode = nextNode->gtNext;
        }

        INDEBUG(treeToDisplay = loSrc);
        JITDUMP("Removing the cast:\n");
        DISPNODE(cast);

        Range().Remove(cast);
    }

    JITDUMP("Final result:\n")
    DISPTREERANGE(Range(), treeToDisplay);

    return nextNode;
}

//------------------------------------------------------------------------
// StoreNodeToVar: Check if the user is a STORE_LCL_VAR, and if it isn't,
// store the node to a var. Then decompose the new LclVar.
//
// Arguments:
//    use - the LIR::Use object for the def that needs to be decomposed.
//
// Return Value:
//    The next node to process.
//
GenTree* DecomposeLongs::StoreNodeToVar(LIR::Use& use)
{
    if (use.IsDummyUse())
        return use.Def()->gtNext;

    GenTree* tree = use.Def();
    GenTree* user = use.User();

    if (user->OperIs(GT_STORE_LCL_VAR))
    {
        // If parent is already a STORE_LCL_VAR, just mark it lvIsMultiRegRet.
        m_compiler->lvaGetDesc(user->AsLclVar())->SetIsMultiRegDest();
        return tree->gtNext;
    }

    // Otherwise, we need to force var = call()
    unsigned lclNum = use.ReplaceWithLclVar(m_compiler);
    m_compiler->lvaGetDesc(lclNum)->SetIsMultiRegDest();

    if (m_compiler->lvaEnregMultiRegVars)
    {
        TryPromoteLongVar(lclNum);
    }

    // Decompose the new LclVar use
    return DecomposeLclVar(use);
}

//------------------------------------------------------------------------
// Check is op already local var, if not store it to local.
//
// Arguments:
//    op - GenTree* to represent as local variable
//    user - user of op
//    edge - edge from user to op
//
// Return Value:
//    op represented as local var
//
GenTree* DecomposeLongs::RepresentOpAsLocalVar(GenTree* op, GenTree* user, GenTree** edge)
{
    if (op->OperIs(GT_LCL_VAR))
    {
        return op;
    }
    else
    {
        LIR::Use opUse(Range(), edge, user);
        opUse.ReplaceWithLclVar(m_compiler);
        return *edge;
    }
}

//------------------------------------------------------------------------
// DecomposeLongs::EnsureIntSized:
//    Checks to see if the given node produces an int-sized value and
//    performs the appropriate widening if it does not.
//
// Arguments:
//    node       - The node that may need to be widened.
//    signExtend - True if the value should be sign-extended; false if it
//                 should be zero-extended.
//
// Return Value:
//    The node that produces the widened value.
GenTree* DecomposeLongs::EnsureIntSized(GenTree* node, bool signExtend)
{
    assert(node != nullptr);
    if (!varTypeIsSmall(node))
    {
        assert(genTypeSize(node) == genTypeSize(TYP_INT));
        return node;
    }

    if (node->OperIs(GT_LCL_VAR) && !m_compiler->lvaTable[node->AsLclVarCommon()->GetLclNum()].lvNormalizeOnLoad())
    {
        node->gtType = TYP_INT;
        return node;
    }

    GenTree* const cast = m_compiler->gtNewCastNode(TYP_INT, node, !signExtend, node->TypeGet());
    Range().InsertAfter(node, cast);
    return cast;
}

//------------------------------------------------------------------------
// GetHiOper: Convert arithmetic operator to "high half" operator of decomposed node.
//
// Arguments:
//    oper - operator to map
//
// Return Value:
//    mapped operator
//
// static
genTreeOps DecomposeLongs::GetHiOper(genTreeOps oper)
{
    switch (oper)
    {
        case GT_ADD:
            return GT_ADD_HI;
            break;
        case GT_SUB:
            return GT_SUB_HI;
            break;
        case GT_OR:
            return GT_OR;
            break;
        case GT_AND:
            return GT_AND;
            break;
        case GT_XOR:
            return GT_XOR;
            break;
        default:
            assert(!"GetHiOper called for invalid oper");
            return GT_NONE;
    }
}

//------------------------------------------------------------------------
// GetLoOper: Convert arithmetic operator to "low half" operator of decomposed node.
//
// Arguments:
//    oper - operator to map
//
// Return Value:
//    mapped operator
//
// static
genTreeOps DecomposeLongs::GetLoOper(genTreeOps oper)
{
    switch (oper)
    {
        case GT_ADD:
            return GT_ADD_LO;
            break;
        case GT_SUB:
            return GT_SUB_LO;
            break;
        case GT_OR:
            return GT_OR;
            break;
        case GT_AND:
            return GT_AND;
            break;
        case GT_XOR:
            return GT_XOR;
            break;
        default:
            assert(!"GetLoOper called for invalid oper");
            return GT_NONE;
    }
}

//------------------------------------------------------------------------
// PromoteLongVars: "Struct promote" all register candidate longs as if they are structs of two ints.
//
void DecomposeLongs::PromoteLongVars()
{
    if (!m_compiler->compEnregLocals())
    {
        return;
    }

    // The lvaTable might grow as we grab temps. Make a local copy here.
    unsigned startLvaCount = m_compiler->lvaCount;
    for (unsigned lclNum = 0; lclNum < startLvaCount; lclNum++)
    {
        LclVarDsc* varDsc = m_compiler->lvaGetDesc(lclNum);
        if (!varTypeIsLong(varDsc))
        {
            continue;
        }

        TryPromoteLongVar(lclNum);
    }

#ifdef DEBUG
    if (m_compiler->verbose)
    {
        printf("\nlvaTable after PromoteLongVars\n");
        m_compiler->lvaTableDump();
    }
#endif // DEBUG
}

//------------------------------------------------------------------------
// PromoteLongVar: Try to promote a long variable into two INT fields.
//
// Promotion can fail, most commonly because it would not be profitable.
//
// Arguments:
//    lclNum - Number denoting the variable to promote
//
void DecomposeLongs::TryPromoteLongVar(unsigned lclNum)
{
    LclVarDsc* varDsc = m_compiler->lvaGetDesc(lclNum);

    assert(varDsc->TypeIs(TYP_LONG));

    if (varDsc->lvDoNotEnregister)
    {
        return;
    }
    if (varDsc->lvRefCnt() == 0)
    {
        return;
    }
    if (varDsc->lvIsStructField)
    {
        return;
    }
    if (m_compiler->fgNoStructPromotion)
    {
        return;
    }
    if (m_compiler->fgNoStructParamPromotion && varDsc->lvIsParam)
    {
        return;
    }
#if defined(FEATURE_HW_INTRINSICS) && defined(TARGET_X86)
    if (varDsc->lvIsParam)
    {
        // Promotion blocks combined read optimizations for SIMD loads of long params
        return;
    }
#endif // FEATURE_HW_INTRINSICS && TARGET_X86

    varDsc->lvFieldCnt      = 2;
    varDsc->lvFieldLclStart = m_compiler->lvaCount;
    varDsc->lvPromoted      = true;
    varDsc->lvContainsHoles = false;

    bool isParam = varDsc->lvIsParam;

    JITDUMP("\nPromoting long local V%02u:", lclNum);

    for (unsigned index = 0; index < 2; ++index)
    {
        // Grab the temp for the field local.

        // Lifetime of field locals might span multiple BBs, so they are long lifetime temps.
        unsigned fieldLclNum = m_compiler->lvaGrabTemp(
            false DEBUGARG(m_compiler->printfAlloc("%s V%02u.%s (fldOffset=0x%x)", "field", lclNum,
                                                   index == 0 ? "lo" : "hi", index * 4)));
        varDsc = m_compiler->lvaGetDesc(lclNum);

        LclVarDsc* fieldVarDsc       = m_compiler->lvaGetDesc(fieldLclNum);
        fieldVarDsc->lvType          = TYP_INT;
        fieldVarDsc->lvIsStructField = true;
        fieldVarDsc->lvFldOffset     = (unsigned char)(index * genTypeSize(TYP_INT));
        fieldVarDsc->lvFldOrdinal    = (unsigned char)index;
        fieldVarDsc->lvParentLcl     = lclNum;
        // Currently we do not support enregistering incoming promoted aggregates with more than one field.
        if (isParam)
        {
            fieldVarDsc->lvIsParam = true;
            m_compiler->lvaSetVarDoNotEnregister(fieldLclNum DEBUGARG(DoNotEnregisterReason::LongParamField));

            fieldVarDsc->lvIsRegArg = varDsc->lvIsRegArg;
        }
    }
}
#endif // !defined(TARGET_64BIT)
