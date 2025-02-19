; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -S -passes=simplifycfg -simplifycfg-require-and-preserve-domtree=1 < %s | FileCheck -check-prefixes=CHECK,NOSINK %s
; RUN: opt -S -passes=simplifycfg < %s | FileCheck -check-prefixes=CHECK,NOSINK %s
; RUN: opt -S -passes='simplifycfg<hoist-common-insts;sink-common-insts>' < %s | FileCheck -check-prefixes=CHECK,SINK %s

declare void @foo() convergent
declare void @bar1()
declare void @bar2()
declare void @bar3()
declare i32 @tid()
declare i32 @mbcnt(i32 %a, i32 %b) convergent
declare i32 @bpermute(i32 %a, i32 %b) convergent

define i32 @test_01(i32 %a) {
; CHECK-LABEL: @test_01(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[COND:%.*]] = icmp eq i32 [[A:%.*]], 0
; CHECK-NEXT:    br i1 [[COND]], label [[MERGE:%.*]], label [[IF_FALSE:%.*]]
; CHECK:       if.false:
; CHECK-NEXT:    call void @foo()
; CHECK-NEXT:    br label [[MERGE]]
; CHECK:       merge:
; CHECK-NEXT:    call void @foo()
; CHECK-NEXT:    br i1 [[COND]], label [[EXIT:%.*]], label [[IF_FALSE_2:%.*]]
; CHECK:       if.false.2:
; CHECK-NEXT:    call void @foo()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    ret i32 [[A]]
;
entry:
  %cond = icmp eq i32 %a, 0
  br i1 %cond, label %merge, label %if.false

if.false:
  call void @foo()
  br label %merge

merge:
  call void @foo()
  br i1 %cond, label %exit, label %if.false.2

if.false.2:
  call void @foo()
  br label %exit

exit:
  ret i32 %a
}

define i32 @test_01a(i32 %a) {
; CHECK-LABEL: @test_01a(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[COND:%.*]] = icmp eq i32 [[A:%.*]], 0
; CHECK-NEXT:    br i1 [[COND]], label [[EXIT_CRITEDGE:%.*]], label [[IF_FALSE:%.*]]
; CHECK:       if.false:
; CHECK-NEXT:    call void @bar1()
; CHECK-NEXT:    call void @bar2()
; CHECK-NEXT:    call void @bar3()
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       exit.critedge:
; CHECK-NEXT:    call void @bar2()
; CHECK-NEXT:    br label [[EXIT]]
; CHECK:       exit:
; CHECK-NEXT:    ret i32 [[A]]
;
entry:
  %cond = icmp eq i32 %a, 0
  br i1 %cond, label %merge, label %if.false

if.false:
  call void @bar1()
  br label %merge

merge:
  call void @bar2()
  br i1 %cond, label %exit, label %if.false.2

if.false.2:
  call void @bar3()
  br label %exit

exit:
  ret i32 %a
}

define void @test_02(ptr %y.coerce) convergent {
; NOSINK-LABEL: @test_02(
; NOSINK-NEXT:  entry:
; NOSINK-NEXT:    [[TMP0:%.*]] = tail call i32 @tid()
; NOSINK-NEXT:    [[REM:%.*]] = and i32 [[TMP0]], 1
; NOSINK-NEXT:    [[CMP_NOT:%.*]] = icmp eq i32 [[REM]], 0
; NOSINK-NEXT:    br i1 [[CMP_NOT]], label [[IF_ELSE:%.*]], label [[IF_THEN:%.*]]
; NOSINK:       if.then:
; NOSINK-NEXT:    [[TMP1:%.*]] = tail call i32 @mbcnt(i32 -1, i32 0)
; NOSINK-NEXT:    [[TMP2:%.*]] = tail call i32 @mbcnt(i32 -1, i32 [[TMP1]])
; NOSINK-NEXT:    [[AND_I:%.*]] = shl i32 [[TMP2]], 2
; NOSINK-NEXT:    [[ADD_I:%.*]] = and i32 [[AND_I]], -256
; NOSINK-NEXT:    [[SHL_I:%.*]] = or i32 [[ADD_I]], 16
; NOSINK-NEXT:    [[TMP3:%.*]] = tail call i32 @bpermute(i32 [[SHL_I]], i32 [[TMP0]])
; NOSINK-NEXT:    [[IDXPROM:%.*]] = zext i32 [[TMP0]] to i64
; NOSINK-NEXT:    [[ARRAYIDX:%.*]] = getelementptr inbounds i32, ptr [[Y_COERCE:%.*]], i64 [[IDXPROM]]
; NOSINK-NEXT:    store i32 [[TMP3]], ptr [[ARRAYIDX]], align 4
; NOSINK-NEXT:    br label [[IF_END:%.*]]
; NOSINK:       if.else:
; NOSINK-NEXT:    [[TMP4:%.*]] = tail call i32 @mbcnt(i32 -1, i32 0)
; NOSINK-NEXT:    [[TMP5:%.*]] = tail call i32 @mbcnt(i32 -1, i32 [[TMP4]])
; NOSINK-NEXT:    [[AND_I11:%.*]] = shl i32 [[TMP5]], 2
; NOSINK-NEXT:    [[ADD_I12:%.*]] = and i32 [[AND_I11]], -256
; NOSINK-NEXT:    [[SHL_I13:%.*]] = or i32 [[ADD_I12]], 8
; NOSINK-NEXT:    [[TMP6:%.*]] = tail call i32 @bpermute(i32 [[SHL_I13]], i32 [[TMP0]])
; NOSINK-NEXT:    [[IDXPROM4:%.*]] = zext i32 [[TMP0]] to i64
; NOSINK-NEXT:    [[ARRAYIDX5:%.*]] = getelementptr inbounds i32, ptr [[Y_COERCE]], i64 [[IDXPROM4]]
; NOSINK-NEXT:    store i32 [[TMP6]], ptr [[ARRAYIDX5]], align 4
; NOSINK-NEXT:    br label [[IF_END]]
; NOSINK:       if.end:
; NOSINK-NEXT:    ret void
;
; SINK-LABEL: @test_02(
; SINK-NEXT:  entry:
; SINK-NEXT:    [[TMP0:%.*]] = tail call i32 @tid()
; SINK-NEXT:    [[REM:%.*]] = and i32 [[TMP0]], 1
; SINK-NEXT:    [[CMP_NOT:%.*]] = icmp eq i32 [[REM]], 0
; SINK-NEXT:    [[IDXPROM4:%.*]] = zext i32 [[TMP0]] to i64
; SINK-NEXT:    [[ARRAYIDX5:%.*]] = getelementptr inbounds i32, ptr [[Y_COERCE:%.*]], i64 [[IDXPROM4]]
; SINK-NEXT:    br i1 [[CMP_NOT]], label [[IF_ELSE:%.*]], label [[IF_THEN:%.*]]
; SINK:       if.then:
; SINK-NEXT:    [[TMP1:%.*]] = tail call i32 @mbcnt(i32 -1, i32 0)
; SINK-NEXT:    [[TMP2:%.*]] = tail call i32 @mbcnt(i32 -1, i32 [[TMP1]])
; SINK-NEXT:    [[AND_I:%.*]] = shl i32 [[TMP2]], 2
; SINK-NEXT:    [[ADD_I:%.*]] = and i32 [[AND_I]], -256
; SINK-NEXT:    [[SHL_I:%.*]] = or i32 [[ADD_I]], 16
; SINK-NEXT:    [[TMP3:%.*]] = tail call i32 @bpermute(i32 [[SHL_I]], i32 [[TMP0]])
; SINK-NEXT:    br label [[IF_END:%.*]]
; SINK:       if.else:
; SINK-NEXT:    [[TMP4:%.*]] = tail call i32 @mbcnt(i32 -1, i32 0)
; SINK-NEXT:    [[TMP5:%.*]] = tail call i32 @mbcnt(i32 -1, i32 [[TMP4]])
; SINK-NEXT:    [[AND_I11:%.*]] = shl i32 [[TMP5]], 2
; SINK-NEXT:    [[ADD_I12:%.*]] = and i32 [[AND_I11]], -256
; SINK-NEXT:    [[SHL_I13:%.*]] = or i32 [[ADD_I12]], 8
; SINK-NEXT:    [[TMP6:%.*]] = tail call i32 @bpermute(i32 [[SHL_I13]], i32 [[TMP0]])
; SINK-NEXT:    br label [[IF_END]]
; SINK:       if.end:
; SINK-NEXT:    [[DOTSINK:%.*]] = phi i32 [ [[TMP6]], [[IF_ELSE]] ], [ [[TMP3]], [[IF_THEN]] ]
; SINK-NEXT:    store i32 [[DOTSINK]], ptr [[ARRAYIDX5]], align 4
; SINK-NEXT:    ret void
;
entry:
  %0 = tail call i32 @tid()
  %rem = and i32 %0, 1
  %cmp.not = icmp eq i32 %rem, 0
  br i1 %cmp.not, label %if.else, label %if.then

if.then:
  %1 = tail call i32 @mbcnt(i32 -1, i32 0)
  %2 = tail call i32 @mbcnt(i32 -1, i32 %1)
  %and.i = shl i32 %2, 2
  %add.i = and i32 %and.i, -256
  %shl.i = or i32 %add.i, 16
  %3 = tail call i32 @bpermute(i32 %shl.i, i32 %0)
  %idxprom = zext i32 %0 to i64
  %arrayidx = getelementptr inbounds i32, ptr %y.coerce, i64 %idxprom
  store i32 %3, ptr %arrayidx
  br label %if.end

if.else:
  %4 = tail call i32 @mbcnt(i32 -1, i32 0)
  %5 = tail call i32 @mbcnt(i32 -1, i32 %4)
  %and.i11 = shl i32 %5, 2
  %add.i12 = and i32 %and.i11, -256
  %shl.i13 = or i32 %add.i12, 8
  %6 = tail call i32 @bpermute(i32 %shl.i13, i32 %0)
  %idxprom4 = zext i32 %0 to i64
  %arrayidx5 = getelementptr inbounds i32, ptr %y.coerce, i64 %idxprom4
  store i32 %6, ptr %arrayidx5
  br label %if.end

if.end:
  ret void
}
