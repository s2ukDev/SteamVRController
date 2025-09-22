package org.s2uk.vrcontroller

data class InboundDecompressResults(@JvmField var status: Boolean,
                                    @JvmField var leftController: Boolean,
                                    @JvmField val amplitude: Float,
                                    @JvmField val frequency: Float,
                                    @JvmField var durationSeconds: Float) {
    fun status() = status
    fun leftController() = leftController
    fun amplitude() = amplitude
    fun frequency() = frequency
    fun durationSeconds() = durationSeconds
}
