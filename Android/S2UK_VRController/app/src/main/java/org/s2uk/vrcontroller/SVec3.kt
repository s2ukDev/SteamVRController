package org.s2uk.vrcontroller

data class SVec3(@JvmField var x: Float, @JvmField var y: Float, @JvmField var z: Float) {
    constructor() : this(0f, 0f, 0f)

    constructor(v: SVec3) : this(v.x.toFloat(), v.y.toFloat(), v.z.toFloat())

    fun getX(): Float = x
    fun getY(): Float = y
    fun getZ(): Float = z

    fun set(nx: Float, ny: Float, nz: Float): SVec3 {
        x = nx
        y = ny
        z = nz
        return this
    }

    fun add(other: SVec3): SVec3 {
        x += other.x
        y += other.y
        z += other.z
        return this
    }

    operator fun plus(other: SVec3): SVec3 = SVec3(x + other.x, y + other.y, z + other.z)

    operator fun plusAssign(other: SVec3) {
        x += other.x
        y += other.y
        z += other.z
    }

    operator fun minus(other: SVec3): SVec3 = SVec3(x - other.x, y - other.y, z - other.z)

    operator fun minusAssign(other: SVec3) {
        x -= other.x
        y -= other.y
        z -= other.z
    }

    operator fun times(scalar: Float): SVec3 = SVec3(x * scalar, y * scalar, z * scalar)

    fun timesAssign(scalar: Float) {
        x *= scalar
        y *= scalar
        z *= scalar
    }

    override fun toString(): String = "SVec3(x=$x, y=$y, z=$z)"
}