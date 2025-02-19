//===-- I8085FrameLowering.cpp - I8085 Frame Information ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the I8085 implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "I8085FrameLowering.h"

#include "I8085.h"
#include "I8085InstrInfo.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"

#include <vector>
#include <iostream>

namespace llvm {

I8085FrameLowering::I8085FrameLowering()
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(1), -2) {}

bool I8085FrameLowering::canSimplifyCallFramePseudos(
    const MachineFunction &MF) const {
  // Always simplify call frame pseudo instructions, even when
  // hasReservedCallFrame is false.
  return true;
}

bool I8085FrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // Reserve call frame memory in function prologue under the following
  // conditions:
  // - Y pointer is reserved to be the frame pointer.
  // - The function does not contain variable sized objects.

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return hasFP(MF) && !MFI.hasVarSizedObjects();
}

void I8085FrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = (MBBI != MBB.end()) ? MBBI->getDebugLoc() : DebugLoc();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();
  bool HasFP = hasFP(MF);

  // Early exit if the frame pointer is not needed in this function.
  if (!HasFP) {
    return;
  }

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();

  // Skip the callee-saved push instructions.
  while (
      (MBBI != MBB.end()) && MBBI->getFlag(MachineInstr::FrameSetup)) {
    ++MBBI;
  }
  
  /*Prologue sequence for 8085 processor */ 
  


  if(FrameSize) {
      BuildMI(MBB, MBBI, DL, TII.get(I8085::GROW_STACK_BY))
          .addImm(FrameSize);
      
  }
}

static void restoreStatusRegister(MachineFunction &MF, MachineBasicBlock &MBB) {
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();

  DebugLoc DL = MBBI->getDebugLoc();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

}

void I8085FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const I8085MachineFunctionInfo *AFI = MF.getInfo<I8085MachineFunctionInfo>();

  // Early exit if the frame pointer is not needed in this function except for
  // signal/interrupt handlers where special code generation is required.
  if (!hasFP(MF) && !AFI->isInterruptOrSignalHandler()) {
    return;
  }

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  assert(MBBI->getDesc().isReturn() &&
         "Can only insert epilog into returning blocks");

  DebugLoc DL = MBBI->getDebugLoc();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned FrameSize = MFI.getStackSize() - AFI->getCalleeSavedFrameSize();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

  // Early exit if there is no need to restore the frame pointer.
  if (!FrameSize && !MF.getFrameInfo().hasVarSizedObjects()) {
    restoreStatusRegister(MF, MBB);
    return;
  }

  // Skip the callee-saved pop instructions.
  while (MBBI != MBB.begin()) {
    MachineBasicBlock::iterator PI = std::prev(MBBI);
    int Opc = PI->getOpcode();

    if (!PI->isTerminator()) {
      break;
    }

    --MBBI;
  }


if(FrameSize) {

      BuildMI(MBB, MBBI, DL, TII.get(I8085::SHRINK_STACK_BY))
          .addImm(FrameSize);
}


  // Write back R29R28 to SP and temporarily disable interrupts.
  // BuildMI(MBB, MBBI, DL, TII.get(I8085::SPWRITE), I8085::SP)
  //     .addReg(I8085::D, RegState::Kill);

  restoreStatusRegister(MF, MBB);
}

// Return true if the specified function should have a dedicated frame
// pointer register. This is true if the function meets any of the following
// conditions:
//  - a register has been spilled
//  - has allocas
//  - input arguments are passed using the stack
//
// Notice that strictly this is not a frame pointer because it contains SP after
// frame allocation instead of having the original SP in function entry.
bool I8085FrameLowering::hasFP(const MachineFunction &MF) const {
  const I8085MachineFunctionInfo *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();

  return (FuncInfo->getHasSpills() || FuncInfo->getHasAllocas() ||
          FuncInfo->getHasStackArgs() ||
          MF.getFrameInfo().hasVarSizedObjects());
}

bool I8085FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty()) {
    return false;
  }

  unsigned CalleeFrameSize = 0;
  DebugLoc DL = MBB.findDebugLoc(MI);
  MachineFunction &MF = *MBB.getParent();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  I8085MachineFunctionInfo *I8085FI = MF.getInfo<I8085MachineFunctionInfo>();

  for (const CalleeSavedInfo &I : llvm::reverse(CSI)) {
    Register Reg = I.getReg();
    bool IsNotLiveIn = !MBB.isLiveIn(Reg);

    // assert(TRI->getRegSizeInBits(*TRI->getMinimalPhysRegClass(Reg)) == 8 &&
    //        "Invalid register size");

    // Add the callee-saved register as live-in only if it is not already a
    // live-in register, this usually happens with arguments that are passed
    // through callee-saved registers.
    if (IsNotLiveIn) {
      MBB.addLiveIn(Reg);
    }

    // Do not kill the register when it is an input argument.
    // BuildMI(MBB, MI, DL, TII.get(I8085::PUSHRr))
    //     .addReg(Reg, getKillRegState(IsNotLiveIn))
    //     .setMIFlag(MachineInstr::FrameSetup);
    ++CalleeFrameSize;
  }

  I8085FI->setCalleeSavedFrameSize(CalleeFrameSize);

  return true;
}

bool I8085FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty()) {
    return false;
  }

  DebugLoc DL = MBB.findDebugLoc(MI);
  const MachineFunction &MF = *MBB.getParent();
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();

  for (const CalleeSavedInfo &CCSI : CSI) {
    Register Reg = CCSI.getReg();

    // assert(TRI->getRegSizeInBits(*TRI->getMinimalPhysRegClass(Reg)) == 8 &&
    //        "Invalid register size");

    // BuildMI(MBB, MI, DL, TII.get(I8085::POPRd), Reg);
  }

  return true;
}

MachineBasicBlock::iterator I8085FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const I8085Subtarget &STI = MF.getSubtarget<I8085Subtarget>();
  const I8085InstrInfo &TII = *STI.getInstrInfo();

  // There is nothing to insert when the call frame memory is allocated during
  // function entry. Delete the call frame pseudo
  if (hasReservedCallFrame(MF)) {
    return MBB.erase(MI);
  }

  DebugLoc DL = MI->getDebugLoc();
  unsigned int Opcode = MI->getOpcode();
  int Amount = TII.getFrameSize(*MI);

  // ADJCALLSTACKUP and ADJCALLSTACKDOWN are converted to adiw/subi
  // instructions to read and write the stack pointer in I/O space.
  if (Amount != 0) {
    assert(getStackAlign() == Align(1) && "Unsupported stack alignment");

    if (Opcode == TII.getCallFrameSetupOpcode()) {

        BuildMI(MBB, MI, DL, TII.get(I8085::GROW_STACK_BY))
            .addImm(Amount);

    } else {
      assert(Opcode == TII.getCallFrameDestroyOpcode());

        BuildMI(MBB, MI, DL, TII.get(I8085::SHRINK_STACK_BY))
            .addImm(Amount);

    }
  }

  return MBB.erase(MI);
}

void I8085FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                            BitVector &SavedRegs,
                                            RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  // If we have a frame pointer, the Y register needs to be saved as well.
  // if (hasFP(MF)) {
  //   SavedRegs.set(I8085::H);
  //   SavedRegs.set(I8085::L);
  // }
}
/// The frame analyzer pass.
///
/// Scans the function for allocas and used arguments
/// that are passed through the stack.
struct I8085FrameAnalyzer : public MachineFunctionPass {
  static char ID;
  I8085FrameAnalyzer() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    I8085MachineFunctionInfo *FuncInfo = MF.getInfo<I8085MachineFunctionInfo>();

    // If there are no fixed frame indexes during this stage it means there
    // are allocas present in the function.
    if (MFI.getNumObjects() != MFI.getNumFixedObjects()) {
      // Check for the type of allocas present in the function. We only care
      // about fixed size allocas so do not give false positives if only
      // variable sized allocas are present.
      for (unsigned i = 0, e = MFI.getObjectIndexEnd(); i != e; ++i) {
        // Variable sized objects have size 0.
        if (MFI.getObjectSize(i)) {
          FuncInfo->setHasAllocas(true);
          break;
        }
      }
    }

    // If there are fixed frame indexes present, scan the function to see if
    // they are really being used.
    if (MFI.getNumFixedObjects() == 0) {
      return false;
    }

    // Ok fixed frame indexes present, now scan the function to see if they
    // are really being used, otherwise we can ignore them.
    for (const MachineBasicBlock &BB : MF) {
      for (const MachineInstr &MI : BB) {
        int Opcode = MI.getOpcode();

        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isFI()) {
            continue;
          }

          if (MFI.isFixedObjectIndex(MO.getIndex())) {
            FuncInfo->setHasStackArgs(true);
            return false;
          }
        }
      }
    }

    return false;
  }

  StringRef getPassName() const override { return "I8085 Frame Analyzer"; }
};

char I8085FrameAnalyzer::ID = 0;

/// Creates instance of the frame analyzer pass.
FunctionPass *createI8085FrameAnalyzerPass() { return new I8085FrameAnalyzer(); }

} // end of namespace llvm
