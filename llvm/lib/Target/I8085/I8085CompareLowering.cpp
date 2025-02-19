//===-- I8085ISelLowering.cpp - I8085 DAG Lowering Implementation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that I8085 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "I8085ISelLowering.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

#include <iostream>

#include "I8085.h"
#include "I8085MachineFunctionInfo.h"
#include "I8085Subtarget.h"
#include "I8085TargetMachine.h"
#include "MCTargetDesc/I8085MCTargetDesc.h"

namespace llvm {


MachineBasicBlock *I8085TargetLowering::insertCond8Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  trueMBB->splice(trueMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  trueMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));



  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::MOV))
        .addReg(I8085::A, RegState::Define)
        .addReg(operandOne);

  BuildMI(MBB, dl, TII.get(I8085::SUB))
        .addReg(operandTwo);

  BuildMI(MBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegOne, RegState::Define)
        .addImm(1);      

  if(Opc == I8085::SET_EQ_8){
    BuildMI(MBB, dl, TII.get(I8085::JZ)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_NE_8){
    BuildMI(MBB, dl, TII.get(I8085::JNZ)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_UGT_8 ){
    BuildMI(MBB, dl, TII.get(I8085::JZ)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JNC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_ULT_8){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_UGE_8 ){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);  
  }

  else if(Opc == I8085::SET_ULE_8 ){
    BuildMI(MBB, dl, TII.get(I8085::JZ)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB); 
  }

  
  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);

  // Unconditionally flow back to the true block
  BuildMI(falseMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegTwo, RegState::Define)
        .addImm(0);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP))
            .addMBB(trueMBB);

  falseMBB->addSuccessor(trueMBB);
  
  BuildMI(*trueMBB, trueMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(MBB)
      .addReg(tempRegTwo)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return trueMBB;
}


MachineBasicBlock *I8085TargetLowering::insertSigned8Cond(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *samesignMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *diffsignMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, samesignMBB);
  MF->insert(I, diffsignMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));



  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::MOV))
        .addReg(I8085::A, RegState::Define)
        .addReg(operandOne);

  BuildMI(MBB, dl, TII.get(I8085::XRA))
        .addReg(operandTwo);
  
  BuildMI(MBB, dl, TII.get(I8085::ANI))
        .addImm(128);      
  
  BuildMI(MBB, dl, TII.get(I8085::JZ))
        .addMBB(samesignMBB);

  BuildMI(MBB, dl, TII.get(I8085::JNZ))
        .addMBB(diffsignMBB);
    
  
  if(Opc == I8085::SET_GT_8 ){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGT_8)) .addReg(tempRegOne, RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GT_8)).addReg(tempRegTwo, RegState::Define).addReg(operandOne).addReg(operandTwo); 
  }

  else if(Opc == I8085::SET_LT_8){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULT_8)) .addReg(tempRegOne, RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LT_8)).addReg(tempRegTwo, RegState::Define).addReg(operandOne).addReg(operandTwo);   
  }

  else if(Opc == I8085::SET_GE_8 ){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGE_8)) .addReg(tempRegOne, RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GE_8)).addReg(tempRegTwo, RegState::Define).addReg(operandOne).addReg(operandTwo);  
  }

  else if(Opc == I8085::SET_LE_8 ){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULE_8)) .addReg(tempRegOne, RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LE_8)).addReg(tempRegTwo, RegState::Define).addReg(operandOne).addReg(operandTwo); 
  }

  BuildMI(samesignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(diffsignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 

  
  MBB->addSuccessor(samesignMBB);
  MBB->addSuccessor(diffsignMBB);

  samesignMBB->addSuccessor(continMBB);
  diffsignMBB->addSuccessor(continMBB);
  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(samesignMBB)
      .addReg(tempRegTwo)
      .addMBB(diffsignMBB);

  MI.eraseFromParent();
  return continMBB;
}

MachineBasicBlock *I8085TargetLowering::insertDifferentSigned8Cond(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandPosMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandNegMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, firstOperandPosMBB);
  MF->insert(I, firstOperandNegMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));


  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::MOV))
        .addReg(I8085::A, RegState::Define)
        .addReg(operandOne);
  
  BuildMI(MBB, dl, TII.get(I8085::ANI))
        .addImm(128);   
  
  BuildMI(MBB, dl, TII.get(I8085::JZ))
        .addMBB(firstOperandPosMBB);

  BuildMI(MBB, dl, TII.get(I8085::JNZ))
        .addMBB(firstOperandNegMBB);        
  
  if(Opc == I8085::SET_DIFF_SIGN_GT_8 || Opc == I8085::SET_DIFF_SIGN_GE_8){
    BuildMI(firstOperandPosMBB, dl, TII.get(I8085::MVI)).addReg(tempRegOne, RegState::Define).addImm(1);
    BuildMI(firstOperandNegMBB, dl, TII.get(I8085::MVI)).addReg(tempRegTwo, RegState::Define).addImm(0);
  }

  else if(Opc == I8085::SET_DIFF_SIGN_LT_8 || I8085::SET_DIFF_SIGN_LE_8){
    BuildMI(firstOperandPosMBB, dl, TII.get(I8085::MVI)).addReg(tempRegOne, RegState::Define).addImm(0);
    BuildMI(firstOperandNegMBB, dl, TII.get(I8085::MVI)).addReg(tempRegTwo, RegState::Define).addImm(1);
  }

  BuildMI(firstOperandPosMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(firstOperandNegMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 

  
  MBB->addSuccessor(firstOperandPosMBB);
  MBB->addSuccessor(firstOperandNegMBB);

  firstOperandPosMBB->addSuccessor(continMBB);
  firstOperandNegMBB->addSuccessor(continMBB);
  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(firstOperandPosMBB)
      .addReg(tempRegTwo)
      .addMBB(firstOperandNegMBB);

  MI.eraseFromParent();
  return continMBB;
}


MachineBasicBlock *I8085TargetLowering::insertCond16Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;

  MF->insert(I, continMBB);  
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);
  

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned tempRegThree = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i16));



  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::SUB_16))
        .addReg(tempRegThree, RegState::Define)
        .addReg(operandOne)
        .addReg(operandTwo);  

  if(Opc == I8085::SET_UGT_16){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_NOT_EQUAL)).addReg(operandOne).addReg(operandTwo).addMBB(trueMBB); 
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_ULT_16){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_UGE_16){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);  
  }

  else if(Opc == I8085::SET_ULE_16){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_NOT_EQUAL)).addReg(operandOne).addReg(operandTwo).addMBB(falseMBB); 
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB); 
  }

  
  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);
  
  BuildMI(trueMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegOne, RegState::Define)
        .addImm(1);

  BuildMI(trueMBB, dl, TII.get(I8085::JMP))
            .addMBB(continMBB);


  // Unconditionally flow back to the true block
  BuildMI(falseMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegTwo, RegState::Define)
        .addImm(0);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP))
            .addMBB(continMBB);

  falseMBB->addSuccessor(continMBB);
  trueMBB->addSuccessor(continMBB);
  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(trueMBB)
      .addReg(tempRegTwo)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return continMBB;
}


MachineBasicBlock *I8085TargetLowering::insertEqualityCond16Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  trueMBB->splice(trueMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  trueMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));


  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegOne, RegState::Define)
        .addImm(1);      

  if(Opc == I8085::SET_EQ_16){
      BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_NOT_EQUAL))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(falseMBB); 
      BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);   
  }

  else if(Opc == I8085::SET_NE_16){
      BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_NOT_EQUAL))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(trueMBB);
      BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB); 
  }

  
  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);

  // Unconditionally flow back to the true block
  BuildMI(falseMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegTwo, RegState::Define)
        .addImm(0);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP))
            .addMBB(trueMBB);

  falseMBB->addSuccessor(trueMBB);
  
  BuildMI(*trueMBB, trueMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(MBB)
      .addReg(tempRegTwo)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return trueMBB;
}


MachineBasicBlock *I8085TargetLowering::insertSignedCond16Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *samesignMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *diffsignMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, samesignMBB);
  MF->insert(I, diffsignMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_SAME_SIGN))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(samesignMBB);

  BuildMI(MBB, dl, TII.get(I8085::JMP))
        .addMBB(diffsignMBB);      


  if(Opc == I8085::SET_GT_16){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGT_16)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GT_16)).addReg(tempRegTwo,RegState::Define).addReg(operandOne).addReg(operandTwo); 
  }

  else if(Opc == I8085::SET_LT_16){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULT_16)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LT_16)).addReg(tempRegTwo,RegState::Define).addReg(operandOne).addReg(operandTwo);  
  }

  else if(Opc == I8085::SET_GE_16){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGE_16)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LT_16)).addReg(tempRegTwo,RegState::Define).addReg(operandTwo).addReg(operandOne);  
  }

  else if(Opc == I8085::SET_LE_16){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULE_16)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo); 
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GT_16)).addReg(tempRegTwo,RegState::Define).addReg(operandTwo).addReg(operandOne);
  }
  
  BuildMI(samesignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(diffsignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 
  
  MBB->addSuccessor(samesignMBB);
  MBB->addSuccessor(diffsignMBB);

  samesignMBB->addSuccessor(continMBB);
  diffsignMBB->addSuccessor(continMBB);

  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(samesignMBB)
      .addReg(tempRegTwo)
      .addMBB(diffsignMBB);

  MI.eraseFromParent();
  return continMBB;
}



MachineBasicBlock *I8085TargetLowering::insertDifferentSignedCond16Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandPos = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandNeg = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, firstOperandPos);
  MF->insert(I, firstOperandNeg);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::JMP_16_IF_POSITIVE))
        .addReg(operandOne)
        .addMBB(firstOperandPos);

  BuildMI(MBB, dl, TII.get(I8085::JMP))
        .addMBB(firstOperandNeg);      


  if(Opc == I8085::SET_DIFF_SIGN_LT_16){
    BuildMI(firstOperandPos, dl, TII.get(I8085::MVI)).addReg(tempRegOne,RegState::Define).addImm(0);
    BuildMI(firstOperandNeg, dl, TII.get(I8085::MVI)).addReg(tempRegTwo,RegState::Define).addImm(1);
  }

  else if(Opc == I8085::SET_DIFF_SIGN_GT_16){
    BuildMI(firstOperandPos, dl, TII.get(I8085::MVI)).addReg(tempRegOne,RegState::Define).addImm(1);
    BuildMI(firstOperandNeg, dl, TII.get(I8085::MVI)).addReg(tempRegTwo,RegState::Define).addImm(0); 
  }
  
  BuildMI(firstOperandPos, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(firstOperandNeg, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 
  
  MBB->addSuccessor(firstOperandPos);
  MBB->addSuccessor(firstOperandNeg);

  firstOperandPos->addSuccessor(continMBB);
  firstOperandNeg->addSuccessor(continMBB);

  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(firstOperandPos)
      .addReg(tempRegTwo)
      .addMBB(firstOperandNeg);

  MI.eraseFromParent();
  return continMBB;
}

MachineBasicBlock *I8085TargetLowering::insertEqualityCond32Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  trueMBB->splice(trueMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  trueMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));


  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegOne, RegState::Define)
        .addImm(1);      

  if(Opc == I8085::SET_EQ_32){
      BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_NOT_EQUAL))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(falseMBB); 
      BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);   
  }

  else if(Opc == I8085::SET_NE_32){
      BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_NOT_EQUAL))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(trueMBB);
      BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB); 
  }

  
  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);

  // Unconditionally flow back to the true block
  BuildMI(falseMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegTwo, RegState::Define)
        .addImm(0);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP))
            .addMBB(trueMBB);

  falseMBB->addSuccessor(trueMBB);
  
  BuildMI(*trueMBB, trueMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(MBB)
      .addReg(tempRegTwo)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return trueMBB;
}

MachineBasicBlock *I8085TargetLowering::insertCond32Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *trueMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *falseMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, trueMBB);
  MF->insert(I, falseMBB);
  MF->insert(I, continMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned tempRegThree = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));



  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::SUB_32))
        .addReg(tempRegThree, RegState::Define)
        .addReg(operandOne)
        .addReg(operandTwo);    


  if(Opc == I8085::SET_UGT_32){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_NOT_EQUAL)).addReg(operandOne).addReg(operandTwo).addMBB(trueMBB); 
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_ULT_32){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(falseMBB);  
  }

  else if(Opc == I8085::SET_UGE_32){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(falseMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB);  
  }

  else if(Opc == I8085::SET_ULE_32){
    BuildMI(MBB, dl, TII.get(I8085::JC)).addMBB(trueMBB);
    BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_NOT_EQUAL)).addReg(operandOne).addReg(operandTwo).addMBB(falseMBB); 
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(trueMBB); 
  }

  
  MBB->addSuccessor(falseMBB);
  MBB->addSuccessor(trueMBB);

  // Unconditionally flow back to the true block
  BuildMI(falseMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegTwo, RegState::Define)
        .addImm(0);

  BuildMI(falseMBB, dl, TII.get(I8085::JMP))
            .addMBB(continMBB);


  BuildMI(trueMBB, dl, TII.get(I8085::MVI))
        .addReg(tempRegOne, RegState::Define)
        .addImm(1);

  BuildMI(trueMBB, dl, TII.get(I8085::JMP))
            .addMBB(continMBB);          

  falseMBB->addSuccessor(continMBB);
  trueMBB->addSuccessor(continMBB);
  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(trueMBB)
      .addReg(tempRegTwo)
      .addMBB(falseMBB);

  MI.eraseFromParent();
  return continMBB;
}

MachineBasicBlock *I8085TargetLowering::insertSignedCond32Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *samesignMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *diffsignMBB = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, samesignMBB);
  MF->insert(I, diffsignMBB);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_SAME_SIGN))
        .addReg(operandOne)
        .addReg(operandTwo)
        .addMBB(samesignMBB);

  BuildMI(MBB, dl, TII.get(I8085::JMP))
        .addMBB(diffsignMBB);      


  if(Opc == I8085::SET_GT_32){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGT_32)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GT_32)).addReg(tempRegTwo,RegState::Define).addReg(operandOne).addReg(operandTwo); 
  }

  else if(Opc == I8085::SET_LT_32){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULT_32)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LT_32)).addReg(tempRegTwo,RegState::Define).addReg(operandOne).addReg(operandTwo);  
  }

  else if(Opc == I8085::SET_GE_32){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_UGE_32)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo);
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_LT_32)).addReg(tempRegTwo,RegState::Define).addReg(operandTwo).addReg(operandOne);  
  }

  else if(Opc == I8085::SET_LE_32){
    BuildMI(samesignMBB, dl, TII.get(I8085::SET_ULE_32)).addReg(tempRegOne,RegState::Define).addReg(operandOne).addReg(operandTwo); 
    BuildMI(diffsignMBB, dl, TII.get(I8085::SET_DIFF_SIGN_GT_32)).addReg(tempRegTwo,RegState::Define).addReg(operandTwo).addReg(operandOne);
  }
  
  BuildMI(samesignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(diffsignMBB, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 
  
  MBB->addSuccessor(samesignMBB);
  MBB->addSuccessor(diffsignMBB);

  samesignMBB->addSuccessor(continMBB);
  diffsignMBB->addSuccessor(continMBB);

  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(samesignMBB)
      .addReg(tempRegTwo)
      .addMBB(diffsignMBB);

  MI.eraseFromParent();
  return continMBB;
}


MachineBasicBlock *I8085TargetLowering::insertDifferentSignedCond32Set(MachineInstr &MI,
                                                  MachineBasicBlock *MBB) const {

  int Opc = MI.getOpcode();
  const I8085InstrInfo &TII = (const I8085InstrInfo &)*MI.getParent()
                                ->getParent()
                                ->getSubtarget()
                                .getInstrInfo();

  DebugLoc dl = MI.getDebugLoc();

  // To "insert" a SELECT instruction, we insert the diamond
  // control-flow pattern. The incoming instruction knows the
  // destination vreg to set, the condition code register to branch
  // on, the true/false values to select between, and a branch opcode
  // to use.

  MachineFunction *MF = MBB->getParent();
  
  const BasicBlock *LLVM_BB = MBB->getBasicBlock();
  MachineBasicBlock *FallThrough = MBB->getFallThrough();

  // If the current basic block falls through to another basic block,
  // we must insert an unconditional branch to the fallthrough destination
  // if we are to insert basic blocks at the prior fallthrough point.
  if (FallThrough != nullptr) {
    BuildMI(MBB, dl, TII.get(I8085::JMP)).addMBB(FallThrough);
  }

  MachineBasicBlock *continMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandPos = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *firstOperandNeg = MF->CreateMachineBasicBlock(LLVM_BB);

  MachineFunction::iterator I;
  for (I = MF->begin(); I != MF->end() && &(*I) != MBB; ++I)
    ;
  if (I != MF->end())
    ++I;
  MF->insert(I, continMBB);
  MF->insert(I, firstOperandPos);
  MF->insert(I, firstOperandNeg);

  // Transfer remaining instructions and all successors of the current
  // block to the block which will contain the Phi node for the
  // select.
  continMBB->splice(continMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());

  continMBB->transferSuccessorsAndUpdatePHIs(MBB);

  unsigned operandOne = MI.getOperand(1).getReg(); 
  unsigned operandTwo = MI.getOperand(2).getReg();
  
  unsigned tempRegOne = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));
  unsigned tempRegTwo = MF->getRegInfo().createVirtualRegister(getRegClassFor(MVT::i8));

  unsigned destReg = MI.getOperand(0).getReg();

  BuildMI(MBB, dl, TII.get(I8085::JMP_32_IF_POSITIVE))
        .addReg(operandOne)
        .addMBB(firstOperandPos);

  BuildMI(MBB, dl, TII.get(I8085::JMP))
        .addMBB(firstOperandNeg);      


  if(Opc == I8085::SET_DIFF_SIGN_LT_32){
    BuildMI(firstOperandPos, dl, TII.get(I8085::MVI)).addReg(tempRegOne,RegState::Define).addImm(0);
    BuildMI(firstOperandNeg, dl, TII.get(I8085::MVI)).addReg(tempRegTwo,RegState::Define).addImm(1);
  }

  else if(Opc == I8085::SET_DIFF_SIGN_GT_32){
    BuildMI(firstOperandPos, dl, TII.get(I8085::MVI)).addReg(tempRegOne,RegState::Define).addImm(1);
    BuildMI(firstOperandNeg, dl, TII.get(I8085::MVI)).addReg(tempRegTwo,RegState::Define).addImm(0); 
  }
  
  BuildMI(firstOperandPos, dl, TII.get(I8085::JMP)) .addMBB(continMBB);
  BuildMI(firstOperandNeg, dl, TII.get(I8085::JMP)) .addMBB(continMBB); 
  
  MBB->addSuccessor(firstOperandPos);
  MBB->addSuccessor(firstOperandNeg);

  firstOperandPos->addSuccessor(continMBB);
  firstOperandNeg->addSuccessor(continMBB);

  
  BuildMI(*continMBB, continMBB->begin(), dl, TII.get(I8085::PHI),destReg)
      .addReg(tempRegOne)
      .addMBB(firstOperandPos)
      .addReg(tempRegTwo)
      .addMBB(firstOperandNeg);

  MI.eraseFromParent();
  return continMBB;
}

} 