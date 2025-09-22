package org.s2uk.vrcontroller

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.Handler
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration

import kotlin.math.pow
import kotlin.math.min
import kotlin.math.sqrt
import kotlin.math.atan2
import kotlin.math.roundToInt
import androidx.core.graphics.scale
import androidx.core.graphics.toColorInt


// NOTE: Expects attributes to be defined in 'attrs.xml'.

class JoystickView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr), Runnable {

    companion object {
        // Looper
        private const val REFRESH_INTERVAL_MS = 10L
        private const val MULTI_PRESS_MOVE_THRESHOLD = 5

        // Colors
        private const val DEFAULT_BUTTON_COLOR = Color.WHITE
        private val DEFAULT_BORDER_COLOR = "#424242".toColorInt()
        private const val DEFAULT_BORDER_ALPHA = 255
        private const val DEFAULT_BACKGROUND_COLOR = Color.TRANSPARENT

        // Styling
        private const val DEFAULT_VIEW_SIZE_DP = 150
        private const val DEFAULT_BORDER_WIDTH_PX = 3
         // Layout
        private const val DEFAULT_FIXED_CENTER = true
        private const val DEFAULT_AUTO_RECENTER = true
        private const val DEFAULT_STICK_TO_BORDER = false

        // Extra params
        const val BUTTON_DIRECTION_BOTH = 0
    }

    // angle = degrees 0-359, strength = 0-100
    interface OnMoveListener {
        fun onMove(angle: Int, strength: Int)
    }

    interface OnMultipleLongPressListener {
        fun onMultipleLongPress()
    }

    private var paintCircleButton: Paint
    private var paintCircleBorder: Paint
    private var paintBackground: Paint
    private var paintBitmapButton: Paint? = null
    private var buttonBitmap: Bitmap? = null

    // Sub-elements default sizing
    private var buttonSizeRatio = 0.23f
    private var backgroundSizeRatio = 0.75f


    // pos - current position of the joystick in view coordinates relative to center
    // centerPos - current center/origin
    // fixedCenterPos - fixed center used for drawing background/border
    private var pos = SVec2i(0, 0);
    private var centerPos = SVec2i(0, 0);
    private var fixedCenterPos = SVec2i(0, 0);

    // Behavior flags
    private var fixedCenter = DEFAULT_FIXED_CENTER
    private var autoRecenterButton = DEFAULT_AUTO_RECENTER
    private var buttonStickToBorder = DEFAULT_STICK_TO_BORDER

    private var buttonDirection = BUTTON_DIRECTION_BOTH

    // Size
    private var buttonRad = 0
    private var borderRad = 0
    private var borderAlpha = DEFAULT_BORDER_ALPHA
    private var backgroundRad = 0f

    // listeners, threads
    private var moveListener: OnMoveListener? = null
    private var loopInterval = REFRESH_INTERVAL_MS
    private var thread: Thread? = null

    // multi-touch support
    private var multiTouchListener: OnMultipleLongPressListener? = null
    private val handlerMultipleLongPress = Handler()
    private val multipleLongPressRunnable = Runnable {
        multiTouchListener?.onMultipleLongPress()
    }
    private var joySensitivity: Int = 0

    init {
        // Read attributes
        val styledAttributes = context.theme.obtainStyledAttributes(
            attrs,
            R.styleable.JoystickView,
            0, 0
        )
        val buttonColor: Int
        val borderColor: Int
        val backgroundColor: Int
        val borderWidth: Int
        try {
            // Get colors, sizes, toggles
            buttonColor = styledAttributes.getColor(R.styleable.JoystickView_JOY_buttonColor, DEFAULT_BUTTON_COLOR)
            backgroundColor = styledAttributes.getColor(R.styleable.JoystickView_JOY_backgroundColor, DEFAULT_BACKGROUND_COLOR)
            borderColor = styledAttributes.getColor(R.styleable.JoystickView_JOY_borderColor, DEFAULT_BORDER_COLOR)
            borderAlpha = styledAttributes.getInt(R.styleable.JoystickView_JOY_borderAlpha, DEFAULT_BORDER_ALPHA)
            borderWidth = styledAttributes.getDimensionPixelSize(R.styleable.JoystickView_JOY_borderWidth, DEFAULT_BORDER_WIDTH_PX)
            fixedCenter = styledAttributes.getBoolean(R.styleable.JoystickView_JOY_fixedCenter, DEFAULT_FIXED_CENTER)
            autoRecenterButton = styledAttributes.getBoolean(R.styleable.JoystickView_JOY_autoReCenterButton, DEFAULT_AUTO_RECENTER)
            buttonStickToBorder = styledAttributes.getBoolean(R.styleable.JoystickView_JOY_buttonStickToBorder, DEFAULT_STICK_TO_BORDER)

            val buttonDrawable = styledAttributes.getDrawable(R.styleable.JoystickView_JOY_buttonImage)
            isEnabled = styledAttributes.getBoolean(R.styleable.JoystickView_JOY_enabled, true)
            buttonSizeRatio = styledAttributes.getFraction(R.styleable.JoystickView_JOY_buttonSizeRatio, 1, 1, buttonSizeRatio)
            backgroundSizeRatio = styledAttributes.getFraction(R.styleable.JoystickView_JOY_backgroundSizeRatio, 1, 1, backgroundSizeRatio)
            buttonDirection = styledAttributes.getInt(R.styleable.JoystickView_JOY_buttonDirection, BUTTON_DIRECTION_BOTH)
            if (buttonDrawable is BitmapDrawable) {
                buttonBitmap = buttonDrawable.bitmap
                paintBitmapButton = Paint()
            }
        } finally {
            styledAttributes.recycle()
        }

        paintCircleButton = Paint().apply {
            isAntiAlias = true
            color = buttonColor
            style = Paint.Style.FILL
        }

        paintCircleBorder = Paint().apply {
            isAntiAlias = true
            color = borderColor
            style = Paint.Style.STROKE
            strokeWidth = borderWidth.toFloat()
            if (borderColor != Color.TRANSPARENT) {
                alpha = borderAlpha
            }
        }

        paintBackground = Paint().apply {
            isAntiAlias = true
            color = backgroundColor
            style = Paint.Style.FILL
        }
    }

    /**
     * Initialize positions when view size is known.
     * Places center and positions to the middle of the view.
     */
    private fun initPosition() {
        val half = width / 2
        fixedCenterPos.x = half
        centerPos.x = half
        pos.x = half
        fixedCenterPos.y = half
        centerPos.y = half
        pos.y = half
    }

    /**
     * Draw background circle, border and button (bitmap or circle).
     */
    override fun onDraw(canvas: Canvas) {
        // background fill
        canvas.drawCircle(fixedCenterPos.x.toFloat(), fixedCenterPos.y.toFloat(), backgroundRad, paintBackground)
        // border
        canvas.drawCircle(fixedCenterPos.x.toFloat(), fixedCenterPos.y.toFloat(), borderRad.toFloat(), paintCircleBorder)

        // draw button
        if (buttonBitmap != null) {
            canvas.drawBitmap(
                buttonBitmap!!,
                (pos.x + fixedCenterPos.x - centerPos.x - buttonRad).toFloat(),
                (pos.y + fixedCenterPos.y - centerPos.y - buttonRad).toFloat(),
                paintBitmapButton
            )
        } else {
            // Draw a filled circle at the button position
            canvas.drawCircle(
                (pos.x + fixedCenterPos.x - centerPos.x).toFloat(),
                (pos.y + fixedCenterPos.y - centerPos.y).toFloat(),
                buttonRad.toFloat(),
                paintCircleButton
            )
        }
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        initPosition()
        val d = min(w, h)

        buttonRad = (d / 2 * buttonSizeRatio).toInt()
        borderRad = (d / 2 * backgroundSizeRatio).toInt()

        backgroundRad = borderRad - paintCircleBorder.strokeWidth / 2

        if (buttonBitmap != null) {
            buttonBitmap = buttonBitmap!!.scale(buttonRad * 2, buttonRad * 2)
        }
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val d = min(measureDimension(widthMeasureSpec), measureDimension(heightMeasureSpec))
        setMeasuredDimension(d, d)
    }

    // Helper
    private fun measureDimension(measureSpec: Int): Int {
        val mode = MeasureSpec.getMode(measureSpec)
        val size = MeasureSpec.getSize(measureSpec)

        // Convert default DP size to pixels
        val defaultPx = (DEFAULT_VIEW_SIZE_DP * resources.displayMetrics.density).toInt()

        return when (mode) {
            MeasureSpec.UNSPECIFIED -> {
                // No constraint from parent
                defaultPx
            }
            MeasureSpec.AT_MOST -> {
                // Parent has a maximum size
                min(defaultPx, size)
            }
            MeasureSpec.EXACTLY -> {
                // Parent has an exact size (match_parent or fixed dp)
                size
            }
            else -> defaultPx
        }
    }

    /**
     * Main touch handling logic.
     *
     * - Updates pos.X/pos.Y based on touch events
     * - Starts/stops a background thread that periodically calls the move listener
     * - Handles auto-recenter on ACTION_UP
     * - Handles fixedCenter flag
     * - Handles multi-touch detection for a custom listener
     */
    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEnabled) return true

        pos.y = if (buttonDirection < 0) centerPos.y else event.y.toInt()
        pos.x = if (buttonDirection > 0) centerPos.x else event.x.toInt()

        // ACTION_UP
        if (event.action == MotionEvent.ACTION_UP) {
            thread?.interrupt()
            if (autoRecenterButton) {
                resetButtonPosition()
                moveListener?.onMove(getAngle(), getStrength())
            }
        }

        // ACTION_DOWN
        if (event.action == MotionEvent.ACTION_DOWN) {
            thread?.interrupt()
            thread = Thread(this)
            thread!!.start()
            moveListener?.onMove(getAngle(), getStrength())
        }

        // Handle different touch actions
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (!fixedCenter) {
                    centerPos.x = pos.x
                    centerPos.y = pos.y
                }
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 2) {
                    handlerMultipleLongPress.postDelayed(multipleLongPressRunnable, ViewConfiguration.getLongPressTimeout() * 2L)
                    joySensitivity = MULTI_PRESS_MOVE_THRESHOLD
                }
            }
            MotionEvent.ACTION_MOVE -> {
                joySensitivity--
                if (joySensitivity == 0) {
                    handlerMultipleLongPress.removeCallbacks(multipleLongPressRunnable)
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (event.pointerCount == 2) {
                    handlerMultipleLongPress.removeCallbacks(multipleLongPressRunnable)
                }
            }
        }

        // Constrain the button to the border radius
        val dx = (pos.x - centerPos.x).toDouble()
        val dy = (pos.y - centerPos.y).toDouble()
        val abs = sqrt(dx * dx + dy * dy)
        if (abs > borderRad || (buttonStickToBorder && abs != 0.0)) {
            pos.x = ((pos.x - centerPos.x) * borderRad / abs + centerPos.x).toInt()
            pos.y = ((pos.y - centerPos.y) * borderRad / abs + centerPos.y).toInt()
        }

        if (!autoRecenterButton) {
            moveListener?.onMove(getAngle(), getStrength())
        }

        // Redraw with updated positions
        invalidate()
        return true
    }

    private fun getAngle(): Int {
        var angle = Math.toDegrees(atan2((centerPos.y - pos.y).toDouble(), (pos.x - centerPos.x).toDouble())).toInt()
        if (angle < 0) angle += 360
        return angle
    }

    private fun getStrength(): Int {
        val distance = sqrt(((pos.x - centerPos.x).toDouble().pow(2)) + ((pos.y - centerPos.y).toDouble().pow(2)))
        return (100 * distance / borderRad).toInt()
    }

    fun resetButtonPosition() {
        pos.x = centerPos.x
        pos.y = centerPos.y
    }

    // Getters
    fun getButtonDirection(): Int = buttonDirection
    fun getButtonSizeRatio(): Float = buttonSizeRatio
    fun getBackgroundSizeRatio(): Float = backgroundSizeRatio
    fun isAutoRecenterButton(): Boolean = autoRecenterButton
    fun isButtonStickToBorder(): Boolean = buttonStickToBorder

    fun getNormalizedX(): Int {
        if (width == 0) return 50
        return ((pos.x - buttonRad) * 100f / (width - buttonRad * 2)).roundToInt()
    }

    fun getNormalizedY(): Int {
        if (height == 0) return 50
        return ((pos.y - buttonRad) * 100f / (height - buttonRad * 2)).roundToInt()
    }

    fun getBorderAlpha(): Int = borderAlpha

    fun setButtonDrawable(d: Drawable?) {
        if (d is BitmapDrawable) {
            buttonBitmap = d.bitmap
            if (buttonRad != 0) {
                // scale to button size if available
                buttonBitmap = buttonBitmap!!.scale(buttonRad * 2, buttonRad * 2)
            }
            if (paintBitmapButton == null) {
                paintBitmapButton = Paint()
            }
        }
    }

    // Button appearance
    fun setButtonColor(color: Int) {
        paintCircleButton.color = color
        invalidate()
    }

    fun setBorderColor(color: Int) {
        paintCircleBorder.color = color
        if (color != Color.TRANSPARENT) {
            paintCircleBorder.alpha = borderAlpha
        }
        invalidate()
    }

    fun setBorderAlpha(alpha: Int) {
        borderAlpha = alpha
        paintCircleBorder.alpha = alpha
        invalidate()
    }

    override fun setBackgroundColor(color: Int) {
        paintBackground.color = color
        invalidate()
    }

    fun setBorderWidth(width: Int) {
        paintCircleBorder.strokeWidth = width.toFloat()
        backgroundRad = borderRad - width / 2f
        invalidate()
    }

    /**
     * Set the move listener and the refresh rate for callbacks
     * while user keeps touching the joystick.
     *
     * @param l listener or null to clear
     * @param refreshRate refresh interval in milliseconds
     */
    @JvmOverloads
    fun setOnMoveListener(l: OnMoveListener?, refreshRate: Int = REFRESH_INTERVAL_MS.toInt()) {
        moveListener = l
        loopInterval = refreshRate.toLong()
    }

    fun setOnMultiLongPressListener(l: OnMultipleLongPressListener) {
        multiTouchListener = l
    }

    fun setFixedCenter(fixed: Boolean) {
        if (fixed) initPosition()
        fixedCenter = fixed
        invalidate()
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
    }

    fun setButtonSizeRatio(newRatio: Float) {
        if (newRatio > 0.0f && newRatio <= 1.0f) {
            buttonSizeRatio = newRatio
        }
    }

    fun setBackgroundSizeRatio(newRatio: Float) {
        if (newRatio > 0.0f && newRatio <= 1.0f) {
            backgroundSizeRatio = newRatio
        }
    }

    fun setAutoRecenterButton(b: Boolean) {
        autoRecenterButton = b
    }

    fun setButtonStickToBorder(b: Boolean) {
        buttonStickToBorder = b
    }

    fun setButtonDirection(direction: Int) {
        buttonDirection = direction
    }

    /**
     * Executed in a background thread while the touch is active.
     * Posts to the UI thread to call the moveListener with
     * current angle & strength.
     */
    override fun run() {
        while (!Thread.currentThread().isInterrupted) {
            post {
                moveListener?.onMove(getAngle(), getStrength())
            }
            try {
                Thread.sleep(loopInterval)
            } catch (e: InterruptedException) {
                break
            }
        }
    }
}
