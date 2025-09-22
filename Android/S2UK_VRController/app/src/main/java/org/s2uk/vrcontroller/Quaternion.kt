package org.s2uk.vrcontroller

data class Quaternion(val w: Float, val x: Float, val y: Float, val z: Float) {
    operator fun times(q: Quaternion): Quaternion {
        return Quaternion(
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        )
    }
}