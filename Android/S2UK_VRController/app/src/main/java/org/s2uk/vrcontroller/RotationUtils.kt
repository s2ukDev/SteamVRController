package org.s2uk.vrcontroller

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin

object RotationUtils {

    private fun degToRad(d: Float) = d * (PI.toFloat() / 180f)
    private fun radToDeg(r: Float) = r * (180f / PI.toFloat())

    fun eulerToQuaternion(euler: SVec3): Quaternion {
        val cy = cos(degToRad(euler.x) * 0.5f)
        val sy = sin(degToRad(euler.x) * 0.5f)
        val cp = cos(degToRad(euler.y) * 0.5f)
        val sp = sin(degToRad(euler.y) * 0.5f)
        val cr = cos(degToRad(euler.z) * 0.5f)
        val sr = sin(degToRad(euler.z) * 0.5f)

        return Quaternion(
            cr * cp * cy + sr * sp * sy,
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy
        )
    }

    private fun copySign(magnitude: Float, sign: Float): Float {
        return if (sign >= 0f) abs(magnitude) else -abs(magnitude)
    }

    fun quaternionToEuler(q: Quaternion): SVec3 {
        // yaw (Z)
        val sinyCosp = 2f * (q.w * q.z + q.x * q.y)
        val cosyCosp = 1f - 2f * (q.y * q.y + q.z * q.z)
        val yaw = atan2(sinyCosp, cosyCosp)

        // pitch (Y)
        val sinp = 2f * (q.w * q.y - q.z * q.x)
        val pitch = if (abs(sinp) >= 1)
            copySign(PI.toFloat() / 2f, sinp)
        else
            asin(sinp)

        // roll (X)
        val sinrCosp = 2f * (q.w * q.x + q.y * q.z)
        val cosrCosp = 1f - 2f * (q.x * q.x + q.y * q.y)
        val roll = atan2(sinrCosp, cosrCosp)

        return SVec3(radToDeg(yaw), radToDeg(pitch), radToDeg(roll))
    }

    fun rotateEuler(a: SVec3, b: SVec3): SVec3 {
        val qa = eulerToQuaternion(a)
        val qb = eulerToQuaternion(b)
        val qr = qa * qb
        return quaternionToEuler(qr)
    }
}