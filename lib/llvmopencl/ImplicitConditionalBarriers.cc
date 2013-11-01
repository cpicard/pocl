// LLVM function pass that adds implicit barriers to branches where it sees
// beneficial (and legal).
// 
// Copyright (c) 2013 Pekka Jääskeläinen / TUT
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "config.h"
#include "ImplicitConditionalBarriers.h"
#include "Barrier.h"
#include "BarrierBlock.h"
#include "Workgroup.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#if (defined LLVM_3_1 or defined LLVM_3_2)
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#else
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#endif

#include <iostream>

//#define DEBUG_COND_BARRIERS

using namespace llvm;
using namespace pocl;

namespace {
  static
  RegisterPass<ImplicitConditionalBarriers> X("implicit-cond-barriers",
                                              "Adds implicit barriers to branches.");
}

char ImplicitConditionalBarriers::ID = 0;

void
ImplicitConditionalBarriers::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<PostDominatorTree>();
  AU.addPreserved<PostDominatorTree>();
}

bool
ImplicitConditionalBarriers::runOnFunction (Function &F) {
{
  if (!Workgroup::isKernelToProcess(F))
    return false;
  
  PDT = &getAnalysis<PostDominatorTree>();

  typedef std::vector<BasicBlock*> BarrierBlockIndex;
  BarrierBlockIndex conditionalBarriers;
  for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
    BasicBlock *b = i;
    if (!Barrier::hasBarrier(b)) continue;

    // Unconditional barrier postdominates the entry node.
    if (PDT->dominates(b, &F.getEntryBlock())) continue;

    conditionalBarriers.push_back(b);
  }

  if (conditionalBarriers.size() == 0) return false;

  bool changed = false;

  //F.viewCFG();

  for (BarrierBlockIndex::const_iterator i = conditionalBarriers.begin();
       i != conditionalBarriers.end(); ++i) {
    BasicBlock *b = *i;
    // Trace upwards from the barrier until one encounters another
    // barrier or the split point that makes the barrier conditional. 
    // In case of the latter, add a new barrier to both branches of the split point. 

    // BB before which to inject the barrier.
    BasicBlock *pos = b;
    if (pred_begin(b) == pred_end(b)) {
      b->dump();
      assert (pred_begin(b) == pred_end(b));
    }
    BasicBlock *pred = *pred_begin(b);

    while (!isa<BarrierBlock>(pred) && PDT->dominates(b, pred)) {
      pos = pred;
      // If our BB post dominates the given block, we know it is not the
      // branching block that makes the barrier conditional.
      pred = *pred_begin(pred);

      if (pred == b) break; // Traced across a loop edge, skip this case.

    }

    if (isa<BarrierBlock>(pos)) continue;
    // Inject a barrier at the beginning of the BB and let the CanonicalizeBarrier
    // to clean it up (split to a separate BB).

#ifdef DEBUG_COND_BARRIERS
    std::cerr << "### injecting an implicit barrier to the beginning of BB" << std::endl;
    pos->dump();
#endif
    Barrier::Create(pos->getFirstNonPHI());
    changed = true;
  }

  //F.viewCFG();

  return changed;
}

}

