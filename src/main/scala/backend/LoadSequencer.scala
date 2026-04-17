package saturn.backend

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config._
import saturn.common._

class LoadSequencerIO(implicit p: Parameters) extends SequencerIO(new LoadRespMicroOp) {
  val rvm  = Decoupled(new VectorReadReq)
}

class LoadSequencer(implicit p: Parameters) extends Sequencer[LoadRespMicroOp]()(p) {
  def accepts(inst: VectorIssueInst) = inst.vmu && !inst.opcode(5)

  val io = IO(new LoadSequencerIO)

  val valid = RegInit(false.B)
  val inst  = Reg(new BackendIssueInst)
  val eidx  = Reg(UInt(log2Ceil(maxVLMax).W))
  val sidx  = Reg(UInt(3.W))
  val wvd_mask = Reg(UInt(egsTotal.W))
  val rvm_mask = Reg(UInt(egsPerVReg.W))
  val head     = Reg(Bool())

  val renvm     = !inst.vm
  val next_eidx = get_next_eidx(inst.vconfig.vl, eidx, inst.mem_elem_size, 0.U, false.B, false.B, mLen)
  val tail      = next_eidx === inst.vconfig.vl && sidx === inst.seg_nf

  io.dis.ready := !valid || (tail && io.iss.fire) && !io.dis_stall

  when (io.dis.fire) {
    val iss_inst = io.dis.bits
    valid := true.B
    inst  := iss_inst
    eidx  := iss_inst.vstart
    sidx  := iss_inst.segstart

    val wvd_arch_mask = Wire(Vec(32, Bool()))
    for (i <- 0 until 32) {
      val group = i.U >> iss_inst.emul
      val rd_group = iss_inst.rd >> iss_inst.emul
      wvd_arch_mask(i) := group >= rd_group && group <= (rd_group + iss_inst.nf)
    }
    wvd_mask := FillInterleaved(egsPerVReg, wvd_arch_mask.asUInt)
    rvm_mask := Mux(!iss_inst.vm, ~(0.U(egsPerVReg.W)), 0.U)
    head := true.B
  } .elsewhen (io.iss.fire) {
    // Debug: print a human-readable mnemonic for the issued load
    val sewVal = MuxCase(32.U, Seq(
      (inst.mem_elem_size === 0.U) -> 8.U,
      (inst.mem_elem_size === 1.U) -> 16.U,
      (inst.mem_elem_size === 2.U) -> 32.U,
      (inst.mem_elem_size === 3.U) -> 64.U
    ))
      when (!inst.store && inst.mop === mopUnit) {
          if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] rid=%d insn=vle%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d eg=%d tail=%d\n", io.cycle, Cat(inst.debug_id, eidx), sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx, io.iss.bits.wvd_eg, tail)
      } .elsewhen (!inst.store && inst.mop === mopStrided) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] insn=vlse%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d eg=%d tail=%d\n", io.cycle, sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx, io.iss.bits.wvd_eg, tail)
      } .elsewhen (!inst.store && inst.mop(0)) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] insn=vluxei%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d eg=%d tail=%d\n", io.cycle, sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx, io.iss.bits.wvd_eg, tail)
      } .otherwise {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] insn=unknown sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d eg=%d tail=%d\n", io.cycle, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx, io.iss.bits.wvd_eg, tail)
      }
    when (tail) {
      // last element issued
      when (!inst.store && inst.mop === mopUnit) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] LAST rid=%d insn=vle%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d\n", io.cycle, Cat(inst.debug_id, eidx), sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx)
      } .elsewhen (!inst.store && inst.mop === mopStrided) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] LAST insn=vlse%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d\n", io.cycle, sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx)
      } .elsewhen (!inst.store && inst.mop(0)) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] LAST insn=vluxei%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d\n", io.cycle, sewVal, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx)
      } .otherwise {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [ISSUE][LD] LAST insn=unknown sew=%d mop=%d store=%d vat=%d dbg=%d eidx=%d\n", io.cycle, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, eidx)
      }
    }
    valid := !tail
    head := false.B
  }

  io.vat := inst.vat
  io.seq_hazard.valid := valid
  io.seq_hazard.bits.rintent := hazardMultiply(rvm_mask)
  io.seq_hazard.bits.wintent := hazardMultiply(wvd_mask)
  io.seq_hazard.bits.vat     := inst.vat

  val vm_read_oh  = Mux(renvm, UIntToOH(io.rvm.bits.eg), 0.U)
  val vd_write_oh = UIntToOH(io.iss.bits.wvd_eg)

  val raw_hazard = (vm_read_oh & io.older_writes) =/= 0.U
  val waw_hazard = (vd_write_oh & io.older_writes) =/= 0.U
  val war_hazard = (vd_write_oh & io.older_reads) =/= 0.U
  val data_hazard = raw_hazard || waw_hazard || war_hazard

  io.rvm.valid := valid && renvm
  io.rvm.bits.eg := getEgId(0.U, eidx, 0.U, true.B)
  io.rvm.bits.oldest := inst.vat === io.vat_head

  io.iss.valid := valid && !data_hazard && (!renvm || io.rvm.ready)
  io.iss.bits.wvd_eg    := getEgId(inst.rd + (sidx << inst.emul), eidx, inst.mem_elem_size, false.B)
  io.iss.bits.tail       := tail
  io.iss.bits.vat        := inst.vat
  io.iss.bits.debug_id   := inst.debug_id
  io.iss.bits.eidx       := eidx

  val head_mask = get_head_mask(~(0.U(dLenB.W)), eidx     , inst.mem_elem_size, dLen)
  val tail_mask = get_tail_mask(~(0.U(dLenB.W)), next_eidx, inst.mem_elem_size, dLen)
  io.iss.bits.eidx_wmask := Mux(sidx > inst.segend && inst.seg_nf =/= 0.U, 0.U, head_mask & tail_mask)
  io.iss.bits.use_rmask := renvm
  io.iss.bits.elem_size := inst.mem_elem_size

  when (io.iss.fire && !tail) {
    if (vParams.enableChaining) {
      when (next_is_new_eg(eidx, next_eidx, inst.mem_elem_size, false.B)) {
        wvd_mask := wvd_mask & ~vd_write_oh
      }
      when (next_is_new_eg(eidx, next_eidx, 0.U, true.B)) {
        rvm_mask := rvm_mask & ~UIntToOH(io.rvm.bits.eg)
      }
    }
    when (sidx === inst.seg_nf) {
      // Commit/advance: print commit info before updating, with mnemonic
      val sewValC = MuxCase(32.U, Seq(
        (inst.mem_elem_size === 0.U) -> 8.U,
        (inst.mem_elem_size === 1.U) -> 16.U,
        (inst.mem_elem_size === 2.U) -> 32.U,
        (inst.mem_elem_size === 3.U) -> 64.U
      ))
      when (!inst.store && inst.mop === mopUnit) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [COMMIT][LD] rid=%d insn=vle%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eg=%d eidx=%d next_eidx=%d\n", io.cycle, Cat(inst.debug_id, eidx), sewValC, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, io.iss.bits.wvd_eg, eidx, next_eidx)
      } .elsewhen (!inst.store && inst.mop === mopStrided) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [COMMIT][LD] insn=vlse%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eg=%d eidx=%d next_eidx=%d\n", io.cycle, sewValC, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, io.iss.bits.wvd_eg, eidx, next_eidx)
      } .elsewhen (!inst.store && inst.mop(0)) {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [COMMIT][LD] insn=vluxei%d.v sew=%d mop=%d store=%d vat=%d dbg=%d eg=%d eidx=%d next_eidx=%d\n", io.cycle, sewValC, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, io.iss.bits.wvd_eg, eidx, next_eidx)
      } .otherwise {
        if (saturn.DebugConfig.enablePrints) printf("time=%d [COMMIT][LD] insn=unknown sew=%d mop=%d store=%d vat=%d dbg=%d eg=%d eidx=%d next_eidx=%d\n", io.cycle, inst.mem_elem_size, inst.mop, inst.store, inst.vat, inst.debug_id, io.iss.bits.wvd_eg, eidx, next_eidx)
      }
      sidx := 0.U
      eidx := next_eidx
    } .otherwise {
      sidx := sidx + 1.U
    }
  }

  io.busy := valid
  io.head := head
}
