/*
 * Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/javaClasses.hpp"
#include "gc_implementation/shared/vmGCOperations.hpp"
#include "memory/gcLocker.inline.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/oopFactory.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/instanceRefKlass.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/init.hpp"
#include "runtime/interfaceSupport.hpp"
#include "utilities/dtrace.hpp"
#include "utilities/preserveException.hpp"
#ifndef SERIALGC
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#endif

#ifndef USDT2
HS_DTRACE_PROBE_DECL1(hotspot, gc__begin, bool);
HS_DTRACE_PROBE_DECL(hotspot, gc__end);
#endif /* !USDT2 */

// The same dtrace probe can't be inserted in two different files, so we
// have to call it here, so it's only in one file.  Can't create new probes
// for the other file anymore.   The dtrace probes have to remain stable.
void VM_GC_Operation::notify_gc_begin(bool full) {
#ifndef USDT2
  HS_DTRACE_PROBE1(hotspot, gc__begin, full);
  HS_DTRACE_WORKAROUND_TAIL_CALL_BUG();
#else /* USDT2 */
  HOTSPOT_GC_BEGIN(
                   full);
#endif /* USDT2 */
}

void VM_GC_Operation::notify_gc_end() {
#ifndef USDT2
  HS_DTRACE_PROBE(hotspot, gc__end);
  HS_DTRACE_WORKAROUND_TAIL_CALL_BUG();
#else /* USDT2 */
  HOTSPOT_GC_END(
);
#endif /* USDT2 */
}

void VM_GC_Operation::acquire_pending_list_lock() {
  // we may enter this with pending exception set
  instanceRefKlass::acquire_pending_list_lock(&_pending_list_basic_lock);
}


void VM_GC_Operation::release_and_notify_pending_list_lock() {

  instanceRefKlass::release_and_notify_pending_list_lock(&_pending_list_basic_lock);
}

/**
 * 是否可以放弃本次的GC操作请求
 * 	1.其它线程抢先提交了GC操作请求
 * 	2.内存堆还可以再扩展容量
 */
bool VM_GC_Operation::skip_operation() const {
  bool skip = (_gc_count_before != Universe::heap()->total_collections());
  if (_full && skip) {
    skip = (_full_gc_count_before != Universe::heap()->total_full_collections());
  }

  if (!skip && GC_locker::is_active_and_needs_gc()) {
    skip = Universe::heap()->is_maximal_no_gc();
    assert(!(skip && (_gc_cause == GCCause::_gc_locker)),
           "GC_locker cannot be active when initiating GC");
  }
  return skip;
}

/**
 * GC操作的前置处理(如果返回false，则当前线程放弃本次GC操作请求)
 * 	1. 获取内存堆的全局锁
 * 	2. 是否可放弃本次GC操作请求
 */
bool VM_GC_Operation::doit_prologue() {
  assert(Thread::current()->is_Java_thread(), "just checking");
  assert(((_gc_cause != GCCause::_no_gc) &&
          (_gc_cause != GCCause::_no_cause_specified)), "Illegal GCCause");

  acquire_pending_list_lock();

  // If the GC count has changed someone beat us to the collection
  // Get the Heap_lock after the pending_list_lock.
  Heap_lock->lock();

  //先占据内存堆的全局锁,再判断是否可以放弃本次GC操作
  if (skip_operation()) {
    // skip collection
    Heap_lock->unlock();
    release_and_notify_pending_list_lock();
    _prologue_succeeded = false;
  } else {
    _prologue_succeeded = true;
    SharedHeap* sh = SharedHeap::heap();
    if (sh != NULL) sh->_thread_holds_heap_lock_for_gc = true;
  }

  return _prologue_succeeded;
}

/**
 * Gc操作的后置处理
 */
void VM_GC_Operation::doit_epilogue() {
  assert(Thread::current()->is_Java_thread(), "just checking");

  // Release the Heap_lock first.
  SharedHeap* sh = SharedHeap::heap();
  if (sh != NULL) sh->_thread_holds_heap_lock_for_gc = false;
  Heap_lock->unlock();

  release_and_notify_pending_list_lock();
}

bool VM_GC_HeapInspection::doit_prologue() {
  if (Universe::heap()->supports_heap_inspection()) {
    return VM_GC_Operation::doit_prologue();
  } else {
    return false;
  }
}

bool VM_GC_HeapInspection::skip_operation() const {
  assert(Universe::heap()->supports_heap_inspection(), "huh?");
  return false;
}

void VM_GC_HeapInspection::doit() {
  HandleMark hm;
  CollectedHeap* ch = Universe::heap();
  ch->ensure_parsability(false); // must happen, even if collection does
                                 // not happen (e.g. due to GC_locker)
  if (_full_gc) {
    // The collection attempt below would be skipped anyway if
    // the gc locker is held. The following dump may then be a tad
    // misleading to someone expecting only live objects to show
    // up in the dump (see CR 6944195). Just issue a suitable warning
    // in that case and do not attempt to do a collection.
    // The latter is a subtle point, because even a failed attempt
    // to GC will, in fact, induce one in the future, which we
    // probably want to avoid in this case because the GC that we may
    // be about to attempt holds value for us only
    // if it happens now and not if it happens in the eventual
    // future.
    if (GC_locker::is_active()) {
      warning("GC locker is held; pre-dump GC was skipped");
    } else {
      ch->collect_as_vm_thread(GCCause::_heap_inspection);
    }
  }

  HeapInspection::heap_inspection(_out, _need_prologue /* need_prologue */);
}


void VM_GenCollectForAllocation::doit() {
  SvcGCMarker sgcm(SvcGCMarker::MINOR);

  GenCollectedHeap* gch = GenCollectedHeap::heap();
  GCCauseSetter gccs(gch, _gc_cause);

  //通知内存堆管理器处理一次内存分配失败
  _res = gch->satisfy_failed_allocation(_size, _tlab);
  assert(gch->is_in_reserved_or_null(_res), "result not in heap");

  if (_res == NULL && GC_locker::is_active_and_needs_gc()) {
    set_gc_locked();
  }
}

void VM_GenCollectFull::doit() {
  SvcGCMarker sgcm(SvcGCMarker::FULL);

  GenCollectedHeap* gch = GenCollectedHeap::heap();
  GCCauseSetter gccs(gch, _gc_cause);
  gch->do_full_collection(gch->must_clear_all_soft_refs(), _max_level);
}

void VM_GenCollectForPermanentAllocation::doit() {
  SvcGCMarker sgcm(SvcGCMarker::FULL);

  SharedHeap* heap = (SharedHeap*)Universe::heap();
  GCCauseSetter gccs(heap, _gc_cause);
  switch (heap->kind()) {
    case (CollectedHeap::GenCollectedHeap): {
      GenCollectedHeap* gch = (GenCollectedHeap*)heap;
      gch->do_full_collection(gch->must_clear_all_soft_refs(),
                              gch->n_gens() - 1);
      break;
    }
#ifndef SERIALGC
    case (CollectedHeap::G1CollectedHeap): {
      G1CollectedHeap* g1h = (G1CollectedHeap*)heap;
      g1h->do_full_collection(_gc_cause == GCCause::_last_ditch_collection);
      break;
    }
#endif // SERIALGC
    default:
      ShouldNotReachHere();
  }
  _res = heap->perm_gen()->allocate(_size, false);
  assert(heap->is_in_reserved_or_null(_res), "result not in heap");
  if (_res == NULL && GC_locker::is_active_and_needs_gc()) {
    set_gc_locked();
  }
}
