package saturn.chipyard

import org.chipsalliance.cde.config.{Config, Parameters}
import freechips.rocketchip.tile._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.rocket._
import saturn.common._

class MINV64D64RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(64, 64, VectorParams.minParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class MINV128D64RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(128, 64, VectorParams.minParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class MINV256D64RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 64, VectorParams.minParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV128D128RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(128, 128, VectorParams.refParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D64RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 64, VectorParams.refParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D128RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 128, VectorParams.refParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D128M64RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 128, VectorParams.refParams, mLen = Some(64)) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D128RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(512, 128, VectorParams.refParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D256RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(512, 256, VectorParams.refParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class DMAV256D256RocketConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(256, 256, VectorParams.dmaParams) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class MINV128D64ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(128, 64, VectorParams.minParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV128D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(128, 128, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(256, 128, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D256ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(256, 256, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 128, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D256ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D512ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 512, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class GENV512D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 128, VectorParams.genParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class GENV512D256ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.genParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class GENV1024D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(1024, 128, VectorParams.genParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D512M128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 512, VectorParams.refParams, mLen = Some(128)) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D128M64ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(256, 128, VectorParams.refParams, mLen = Some(64)) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV512D256M128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 256, VectorParams.refParams, mLen = Some(128)) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV1024D256ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(1024, 256, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV1024D512ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(1024, 512, VectorParams.refParams) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)

class REFV256D256M128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(256, 256, VectorParams.refParams, mLen = Some(128)) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)


class DSPV512D128ShuttleConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 128, VectorParams.multiMACParams) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new shuttle.common.WithSGTCM(address=0x78000000, size=(8L << 10), banks=16) ++
  new shuttle.common.WithTCM ++
  new shuttle.common.WithShuttleTileBeatBytes(16) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)


class DSPV512D128ShuttleLargeCacheConfig extends Config(
  new saturn.shuttle.WithShuttleVectorUnit(512, 128, VectorParams.multiMACParams) ++
  // Increase L1 ICache: 128 sets, 8 ways (size = 64 * 16 * blockBytes)
  new shuttle.common.WithL1ICacheSets(64) ++
  new shuttle.common.WithL1ICacheWays(16) ++
  // Increase L1 DCache similarly
  new shuttle.common.WithL1DCacheSets(64) ++
  new shuttle.common.WithL1DCacheWays(16) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new shuttle.common.WithSGTCM(address=0x78000000, size=(8L << 10), banks=16) ++
  new shuttle.common.WithTCM ++
  new shuttle.common.WithShuttleTileBeatBytes(16) ++
  new shuttle.common.WithNShuttleCores(1) ++
  new chipyard.config.AbstractConfig)



class DSPV512D128RocketLargeCacheConfig extends Config(
  new saturn.rocket.WithRocketVectorUnit(512, 256, VectorParams.multiMACParams) ++
  new freechips.rocketchip.rocket.WithL1ICacheSets(64) ++
  new freechips.rocketchip.rocket.WithL1ICacheWays(16) ++
  new freechips.rocketchip.rocket.WithL1DCacheSets(64) ++
  new freechips.rocketchip.rocket.WithL1DCacheWays(16) ++
  //new freechips.rocketchip.subsystem.WithCacheBlockBytes(128) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new saturn.rocket.WithRocketTCM(base=0x70000000L, size=0x10000L, banks=1) ++
  new saturn.rocket.WithRocketSGTCM(base=0x78000000L, size=0x10000L, banks=32) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)


class DSPV512D128RocketLargeCacheConfigNOTCM extends Config(
  new saturn.rocket.WithRocketVectorUnit(512, 256, VectorParams.multiMACParams) ++
  new freechips.rocketchip.rocket.WithL1ICacheSets(64) ++
  new freechips.rocketchip.rocket.WithL1ICacheWays(16) ++
  new freechips.rocketchip.rocket.WithL1DCacheSets(64) ++
  new freechips.rocketchip.rocket.WithL1DCacheWays(16) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)