package org.s2uk.vrcontroller

import kotlin.math.roundToInt

data class SVec2i(@JvmField var x: Int, @JvmField var y: Int) {
    constructor() : this(0, 0)

    constructor(v: SVec2) : this(v.x.roundToInt(), v.y.roundToInt())

    fun getX(): Int = x
    fun getY(): Int = y

    fun set(nx: Int, ny: Int): SVec2i {
        x = nx
        y = ny
        return this
    }

    fun add(other: SVec2i): SVec2i {
        x += other.x
        y += other.y
        return this
    }

    operator fun plus(other: SVec2i): SVec2i = SVec2i(x + other.x, y + other.y)

    operator fun plusAssign(other: SVec2i) {
        x += other.x
        y += other.y
    }

    operator fun minus(other: SVec2i): SVec2i = SVec2i(x - other.x, y - other.y)

    operator fun minusAssign(other: SVec2i) {
        x -= other.x
        y -= other.y
    }

    operator fun times(scalar: Int): SVec2i = SVec2i(x * scalar, y * scalar)

    fun timesAssign(scalar: Int) {
        x *= scalar
        y *= scalar
    }

    fun toFloatVec(): SVec2 = SVec2(x.toFloat(), y.toFloat())

    override fun toString(): String = "SVec2i(x=$x, y=$y)"
}
