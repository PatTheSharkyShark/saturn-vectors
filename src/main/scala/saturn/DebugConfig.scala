package saturn

object DebugConfig {
  // Controlled at elaboration time via a JVM system property.
  // Example: `-Dsaturn.enablePrints=true` passed to SBT/runner.
  lazy val enablePrints: Boolean = scala.util.Properties.propOrElse("saturn.enablePrints", "false").toBoolean
}
