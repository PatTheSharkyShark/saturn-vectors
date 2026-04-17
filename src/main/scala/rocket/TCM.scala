package saturn.rocket

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.tile._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.util._
import freechips.rocketchip.rocket._
import shuttle.common._
import shuttle.dmem._

case class RocketTCMParams(
  base: BigInt,
  size: BigInt,
  banks: Int) extends TCMParams

case class RocketSGTCMParams(
  base: BigInt,
  size: BigInt,
  banks: Int) extends TCMParams

case object RocketTCMKey extends Field[Option[RocketTCMParams]](None)
case object RocketSGTCMKey extends Field[Option[RocketSGTCMParams]](None)

trait CanHaveRocketTCM extends HasTileParameters {
  this: BaseTile =>
  val tcmParams = p(RocketTCMKey)
  val sgtcmParams = p(RocketSGTCMKey)
  val tileBeatBytes = p(SystemBusKey).beatBytes

  def tcmAdjusterNode(params: Option[TCMParams]): TLNode = params.map { tcmParams =>
    val replicationSize = (1 << log2Ceil(p(NumTiles))) * tcmParams.size
    val tcm_adjuster = LazyModule(new AddressOffsetter(tcmParams.size-1, replicationSize))
    InModuleBody { tcm_adjuster.module.io.base := tcmParams.base.U + tcmParams.size.U * hartIdSinkNode.bundle }
    tcm_adjuster.node
  } .getOrElse { TLEphemeralNode() }

  def tcmSlaveReplicator(params: Option[TCMParams]): TLNode = params.map { tcmParams =>
    val replicationSize = (1 << log2Ceil(p(NumTiles))) * tcmParams.size
    val tcm_slave_replicator = LazyModule(new RegionReplicator(ReplicatedRegion(
      tcmParams.addressSet,
      tcmParams.addressSet.widen(replicationSize - tcmParams.size)
    )))
    val prefix_slave_source = BundleBridgeSource[UInt](() => UInt(1.W))
    tcm_slave_replicator.prefix := prefix_slave_source
    InModuleBody { prefix_slave_source.bundle := 0.U }
    tcm_slave_replicator.node
  } .getOrElse { TLEphemeralNode() }

  def tcmMasterReplicator(params: Option[TCMParams]): TLNode = params.map { tcmParams =>
    val replicationSize = (1 << log2Ceil(p(NumTiles))) * tcmParams.size
    val tcm_master_replicator = LazyModule(new RegionReplicator(ReplicatedRegion(
      tcmParams.addressSet,
      tcmParams.addressSet.widen((replicationSize << 1) - tcmParams.size)
    )))
    val prefix_master_source = BundleBridgeSource[UInt](() => UInt(1.W))
    tcm_master_replicator.prefix := prefix_master_source
    InModuleBody { prefix_master_source.bundle := 0.U }
    tcm_master_replicator.node := TLFilter(TLFilter.mSubtract(AddressSet(tcmParams.base, replicationSize-1)))
  } .getOrElse { TLEphemeralNode() }
}

class WithRocketTCM(base: BigInt = 0x70000000L, size: BigInt = 0x4000L, banks: Int = 1) extends Config((site, here, up) => {
  case RocketTCMKey => Some(RocketTCMParams(base, size, banks))
})

class WithRocketSGTCM(base: BigInt = 0x78000000L, size: BigInt = 0x4000L, banks: Int = 8) extends Config((site, here, up) => {
  case RocketSGTCMKey => Some(RocketSGTCMParams(base, size, banks))
})
