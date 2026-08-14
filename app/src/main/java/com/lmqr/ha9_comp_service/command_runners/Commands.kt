package com.lmqr.ha9_comp_service.command_runners

object Commands {
    fun BLACK_THRESHOLD(v: Int) = "stb${v}"
    fun WHITE_THRESHOLD(v: Int) = "stw${v}"
    fun CONTRAST(v: Int) = "sco${v}"

    // Arms the daemon's double tap watcher. Only useful while the screen is off, and it keeps
    // the daemon reading the touch device, so disarm it again on screen on.
    fun DOUBLE_TAP(enabled: Boolean) = "dt${if (enabled) 1 else 0}"

    const val FORCE_CLEAR = "r"
    const val COMMIT_BITMAP = "cm"
    const val SPEED_CLEAR = "c"
    const val SPEED_BALANCED = "b"
    const val SPEED_SMOOTH = "s"
    const val SPEED_FAST = "p"
}