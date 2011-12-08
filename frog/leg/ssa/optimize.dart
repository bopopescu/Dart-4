// Copyright (c) 2011, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

class SsaOptimizerTask extends CompilerTask {
  SsaOptimizerTask(Compiler compiler) : super(compiler);
  String get name() => 'SSA optimizer';

  void optimize(HGraph graph) {
    measure(() {
      new SsaTypePropagator().visitGraph(graph);
      new SsaConstantFolder().visitGraph(graph);
      new SsaRedundantPhiEliminator().visitGraph(graph);
      new SsaDeadPhiEliminator().visitGraph(graph);
      new SsaGlobalValueNumberer(compiler).visitGraph(graph);
      new SsaCodeMotion().visitGraph(graph);
      new SsaDeadCodeEliminator().visitGraph(graph);
    });
  }
}


/**
 * If both inputs to known operations are available execute the operation at
 * compile-time.
 */
class SsaConstantFolder extends HBaseVisitor {
  visitGraph(HGraph graph) {
    visitDominatorTree(graph);
  }

  visitBasicBlock(HBasicBlock block) {
    HInstruction instruction = block.first;
    while (instruction !== null) {
      HInstruction next = instruction.next;
      HInstruction replacement = instruction.accept(this);
      if (replacement !== instruction) {
        if (!replacement.isInBasicBlock()) {
          // The constant folding can return an instruction that is already
          // part of the graph (like an input), so we only add the replacement
          // if necessary.
          block.addAfter(instruction, replacement);
        }
        block.rewrite(instruction, replacement);
        block.remove(instruction);
      }
      instruction = next;
    }
  }

  HInstruction visitInstruction(HInstruction node) {
    return node;
  }

  HInstruction visitBoolify(HBoolify node) {
    List<HInstruction> inputs = node.inputs;
    assert(inputs.length == 1);
    HInstruction input = inputs[0];
    if (input.isBoolean()) return input;
    // All values !== true are boolified to false.
    if (!input.isUnknown()) return new HLiteral(false);
    return node;
  }

  HInstruction visitNot(HNot node) {
    List<HInstruction> inputs = node.inputs;
    assert(inputs.length == 1);
    HInstruction input = inputs[0];
    if (input is HLiteral) {
      HLiteral literal = input;
      return new HLiteral(literal.value !== true);
    }
    return node;
  }

  HInstruction visitInvokeBinary(HInvokeBinary node) => node.fold();
  HInstruction visitInvokeUnary(HInvokeUnary node) => node.fold();

  HInstruction visitAdd(HAdd node) {
    // String + is defined for all literals. We don't need to know which
    // literal type the right-hand side is.
    // TODO(floitsch): is String + literal a compile-time expression? If not
    // we must pay attention not to canonicalize the concatenated string with
    // an already existing string.
    if (node.left.isLiteralString() && node.right is HLiteral) {
      HLiteral op1 = node.left;
      HLiteral op2 = node.right;
      return new HLiteral(new SourceString("${op1.value} + ${op2.value}"));
    }
    return visitInvokeBinary(node);
  }

  HInstruction visitEquals(HEquals node) {
    if (node.left is HLiteral && node.right is HLiteral) {
      HLiteral op1 = node.left;
      HLiteral op2 = node.right;
      return new HLiteral(op1.value == op2.value);
    }
    return node;
  }

  HInstruction visitTypeGuard(HTypeGuard node) {
    HInstruction value = node.inputs[0];
    return (value.type == node.type) ? value : node;
  }
}

class SsaTypePropagator extends HGraphVisitor {

  final Map<int, HInstruction> workmap;
  final List<int> worklist;

  SsaTypePropagator()
      : workmap = new Map<int, HInstruction>(),
        worklist = new List<int>();

  void visitGraph(HGraph graph) {
    visitDominatorTree(graph);
    processWorklist();
    new TypeGuardInserter().visitGraph(graph);
  }

  visitBasicBlock(HBasicBlock block) {
    if (block.isLoopHeader()) {
      block.forEachPhi((HPhi phi) {
        if (phi.updateTypeForLoopPhi()) addToWorklist(phi);
      });
    } else {
      block.forEachPhi((HPhi phi) {
        if (phi.updateType()) addUsersAndInputsToWorklist(phi);
      });
    }

    HInstruction instruction = block.first;
    while (instruction !== null) {
      if (instruction.updateType()) addUsersAndInputsToWorklist(instruction);
      instruction = instruction.next;
    }
  }

  void processWorklist() {
    while (!worklist.isEmpty()) {
      int id = worklist.removeLast();
      HInstruction instruction = workmap[id];
      assert(instruction !== null);
      workmap.remove(id);
      if (instruction.updateType()) addUsersAndInputsToWorklist(instruction);
    }
  }

  void addUsersAndInputsToWorklist(HInstruction instruction) {
    for (int i = 0, length = instruction.usedBy.length; i < length; i++) {
      addToWorklist(instruction.usedBy[i]);
    }
    for (int i = 0, length = instruction.inputs.length; i < length; i++) {
      addToWorklist(instruction.inputs[i]);
    }
  }

  void addToWorklist(HInstruction instruction) {
    final int id = instruction.id;
    if (!workmap.containsKey(id)) {
      worklist.add(id);
      workmap[id] = instruction;
    }
  }
}

class TypeGuardInserter extends HGraphVisitor {

  void visitGraph(HGraph graph) {
    visitDominatorTree(graph);
  }

  visitBasicBlock(HBasicBlock block) {
    HInstruction instruction = block.phis.first;
    while (instruction !== null) {
      instruction = tryInsertGuard(instruction, block.first);
    }
    instruction = block.first;
    while (instruction !== null) {
      instruction = tryInsertGuard(instruction, instruction);
    }
  }

  HInstruction tryInsertGuard(HInstruction instruction,
                              HInstruction insertionPoint) {
    // If we found a type for the instruction, but the instruction
    // does not know if it produces that type, add a guard.
    if (!instruction.isUnknown() && !instruction.hasExpectedType()) {
      HTypeGuard guard = new HTypeGuard(instruction.type, instruction);
      // Remove the instruction's type, the guard is now holding that
      // type.
      instruction.type = HInstruction.TYPE_UNKNOWN;
      instruction.block.rewrite(instruction, guard);
      insertionPoint.block.addAfter(insertionPoint, guard);
      return guard.next;
    }
    return instruction.next;
  }
}

class SsaDeadCodeEliminator extends HGraphVisitor {
  static bool isDeadCode(HInstruction instruction) {
    return !instruction.hasSideEffects() && instruction.usedBy.isEmpty();
  }

  void visitGraph(HGraph graph) {
    visitPostDominatorTree(graph);
  }

  void visitBasicBlock(HBasicBlock block) {
    HInstruction instruction = block.last;
    while (instruction !== null) {
      var previous = instruction.previous;
      if (isDeadCode(instruction)) block.remove(instruction);
      instruction = previous;
    }
  }
}

class SsaDeadPhiEliminator {
  void visitGraph(HGraph graph) {
    final List<HPhi> worklist = <HPhi>[];
    // A set to keep track of the live phis that we found.
    final Set<HPhi> livePhis = new Set<HPhi>();

    // Add to the worklist all live phis: phis referenced by non-phi
    // instructions.
    for (final block in graph.blocks) {
      block.forEachPhi((HPhi phi) {
        for (final user in phi.usedBy) {
          if (user is !HPhi) {
            worklist.add(phi);
            livePhis.add(phi);
            break;
          }
        }
      });
    }

    // Process the worklist by propagating liveness to phi inputs.
    while (!worklist.isEmpty()) {
      HPhi phi = worklist.removeLast();
      for (final input in phi.inputs) {
        if (input is HPhi && !livePhis.contains(input)) {
          worklist.add(input);
          livePhis.add(input);
        }
      }
    }

    // Remove phis that are not live.
    for (final block in graph.blocks) {
      HPhi current = block.phis.first;
      HPhi next = null;
      while (current != null) {
        next = current.next;
        if (!livePhis.contains(current)) block.removePhi(current);
        current = next;
      }
    }
  }
}

class SsaRedundantPhiEliminator {
  void visitGraph(HGraph graph) {
    final List<HPhi> worklist = <HPhi>[];

    // Add all phis in the worklist.
    for (final block in graph.blocks) {
      block.forEachPhi((HPhi phi) => worklist.add(phi));
    }

    while (!worklist.isEmpty()) {
      HPhi phi = worklist.removeLast();

      // If the phi has already been processed, continue.
      if (!phi.isInBasicBlock()) continue;

      // Find if the inputs of the phi are the same instruction.
      // The builder ensures that phi.inputs[0] cannot be the phi
      // itself.
      assert(phi.inputs[0] !== phi);
      HInstruction candidate = phi.inputs[0];
      for (int i = 1; i < phi.inputs.length; i++) {
        HInstruction input = phi.inputs[i];
        // If the input is the phi, the phi is still candidate for
        // elimination.
        if (input !== candidate && input !== phi) {
          candidate = null;
          break;
        }
      }

      // If the inputs are not the same, continue.
      if (candidate == null) continue;

      // Because we're updating the users of this phi, we may have new
      // phis candidate for elimination. Add phis that used this phi
      // to the worklist.
      for (final user in phi.usedBy) {
        if (user is HPhi) worklist.add(user);
      }
      phi.block.rewrite(phi, candidate);
      phi.block.removePhi(phi);
    }
  }
}

class SsaGlobalValueNumberer {
  final Compiler compiler;
  final Set<int> visited;

  List<int> blockChangesFlags;
  List<int> loopChangesFlags;

  SsaGlobalValueNumberer(this.compiler) : visited = new Set<int>();

  void visitGraph(HGraph graph) {
    computeChangesFlags(graph);
    moveLoopInvariantCode(graph);
    visitBasicBlock(graph.entry, new ValueSet());
  }

  void moveLoopInvariantCode(HGraph graph) {
    for (int i = graph.blocks.length - 1; i >= 0; i--) {
      HBasicBlock block = graph.blocks[i];
      if (block.isLoopHeader()) {
        int changesFlags = loopChangesFlags[block.id];
        HBasicBlock last = block.loopInformation.getLastBackEdge();
        for (int j = block.id; j <= last.id; j++) {
          moveLoopInvariantCodeFromBlock(graph.blocks[j], block, changesFlags);
        }
      }
    }
  }

  void moveLoopInvariantCodeFromBlock(HBasicBlock block,
                                      HBasicBlock loopHeader,
                                      int changesFlags) {
    HBasicBlock preheader = loopHeader.predecessors[0];
    int dependsFlags = HInstruction.computeDependsOnFlags(changesFlags);
    HInstruction instruction = block.first;
    while (instruction != null) {
      HInstruction next = instruction.next;
      if (instruction.useGvn() && (instruction.flags & dependsFlags) == 0) {
        bool loopInvariantInputs = true;
        List<HInstruction> inputs = instruction.inputs;
        for (int i = 0, length = inputs.length; i < length; i++) {
          if (isInputDefinedAfterDominator(inputs[i], preheader)) {
            loopInvariantInputs = false;
            break;
          }
        }

        // If the inputs are loop invariant, we can move the
        // instruction from the current block to the pre-header block.
        if (loopInvariantInputs) {
          block.detach(instruction);
          preheader.moveAtExit(instruction);
        }
      }
      instruction = next;
    }
  }

  bool isInputDefinedAfterDominator(HInstruction input,
                                    HBasicBlock dominator) {
    return input.block.id > dominator.id;
  }

  void visitBasicBlock(HBasicBlock block, ValueSet values) {
    HInstruction instruction = block.first;
    while (instruction !== null) {
      HInstruction next = instruction.next;
      int flags = instruction.getChangesFlags();
      if (flags != 0) {
        assert(!instruction.useGvn());
        values.kill(flags);
      } else if (instruction.useGvn()) {
        HInstruction other = values.lookup(instruction);
        if (other !== null) {
          assert(other.equals(instruction) && instruction.equals(other));
          block.rewrite(instruction, other);
          block.remove(instruction);
        } else {
          values.add(instruction);
        }
      }
      instruction = next;
    }

    List<HBasicBlock> dominatedBlocks = block.dominatedBlocks;
    for (int i = 0, length = dominatedBlocks.length; i < length; i++) {
      HBasicBlock dominated = dominatedBlocks[i];
      // No need to copy the value set for the last child.
      ValueSet successorValues = (i == length - 1) ? values : values.copy();
      // If we have no values in our set, we do not have to kill
      // anything. Also, if the range of block ids from the current
      // block to the dominated block is empty, there is no blocks on
      // any path from the current block to the dominated block so we
      // don't have to do anything either.
      assert(block.id < dominated.id);
      if (!successorValues.isEmpty() && block.id + 1 < dominated.id) {
        visited.clear();
        int changesFlags = getChangesFlagsForDominatedBlock(block, dominated);
        successorValues.kill(changesFlags);
      }
      visitBasicBlock(dominated, successorValues);
    }
  }

  void computeChangesFlags(HGraph graph) {
    // Create the changes flags lists. Make sure to initialize the
    // loop changes flags list to zero so we can use bitwise or when
    // propagating loop changes upwards.
    final int length = graph.blocks.length;
    blockChangesFlags = new List<int>(length);
    loopChangesFlags = new List<int>(length);
    for (int i = 0; i < length; i++) loopChangesFlags[i] = 0;

    // Run through all the basic blocks in the graph and fill in the
    // changes flags lists.
    for (int i = length - 1; i >= 0; i--) {
      final HBasicBlock block = graph.blocks[i];
      final int id = block.id;

      // Compute block changes flags for the block.
      int changesFlags = 0;
      HInstruction instruction = block.first;
      while (instruction !== null) {
        instruction.prepareGvn();
        changesFlags |= instruction.getChangesFlags();
        instruction = instruction.next;
      }
      assert(blockChangesFlags[id] === null);
      blockChangesFlags[id] = changesFlags;

      // Loop headers are part of their loop, so update the loop
      // changes flags accordingly.
      if (block.isLoopHeader()) {
        loopChangesFlags[id] |= changesFlags;
      }

      // Propagate loop changes flags upwards.
      HBasicBlock parentLoopHeader = block.parentLoopHeader;
      if (parentLoopHeader !== null) {
        loopChangesFlags[parentLoopHeader.id] |= (block.isLoopHeader())
            ? loopChangesFlags[id]
            : changesFlags;
      }
    }
  }

  int getChangesFlagsForDominatedBlock(HBasicBlock dominator,
                                       HBasicBlock dominated) {
    int changesFlags = 0;
    List<HBasicBlock> predecessors = dominated.predecessors;
    for (int i = 0, length = predecessors.length; i < length; i++) {
      HBasicBlock block = predecessors[i];
      int id = block.id;
      // If the current predecessor block is on the path from the
      // dominator to the dominated, it must have an id that is in the
      // range from the dominator to the dominated.
      if (dominator.id < id && id < dominated.id && !visited.contains(id)) {
        visited.add(id);
        changesFlags |= blockChangesFlags[id];
        changesFlags |= getChangesFlagsForDominatedBlock(dominator, block);
      }
    }
    return changesFlags;
  }
}
