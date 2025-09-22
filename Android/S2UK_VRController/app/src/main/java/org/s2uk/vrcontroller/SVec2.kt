package org.s2uk.vrcontroller

data class SVec2(@JvmField var x: Float, @JvmField var y: Float) {
    constructor() : this(0f, 0f)

    constructor(v: SVec2i) : this(v.x.toFloat(), v.y.toFloat())

    fun getX(): Float = x
    fun getY(): Float = y

    fun set(nx: Float, ny: Float): SVec2 {
        x = nx
        y = ny
        return this
    }

    fun add(other: SVec2): SVec2 {
        x += other.x
        y += other.y
        return this
    }

    operator fun plus(other: SVec2): SVec2 = SVec2(x + other.x, y + other.y)

    operator fun plusAssign(other: SVec2) {
        x += other.x
        y += other.y
    }

    operator fun minus(other: SVec2): SVec2 = SVec2(x - other.x, y - other.y)

    operator fun minusAssign(other: SVec2) {
        x -= other.x
        y -= other.y
    }

    operator fun times(scalar: Float): SVec2 = SVec2(x * scalar, y * scalar)

    fun timesAssign(scalar: Float) {
        x *= scalar
        y *= scalar
    }

    fun toIntVec(): SVec2i = SVec2i(this)

    override fun toString(): String = "SVec2(x=$x, y=$y)"
}
