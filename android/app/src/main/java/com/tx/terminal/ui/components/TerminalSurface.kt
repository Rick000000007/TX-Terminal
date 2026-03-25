package com.tx.terminal.ui.components

import android.content.Context
import android.graphics.Paint
import android.graphics.Typeface
import android.view.KeyEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import android.view.inputmethod.InputMethodManager
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.viewinterop.AndroidView
import com.tx.terminal.data.TerminalSession
import com.tx.terminal.viewmodel.TerminalViewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Terminal rendering surface using Android SurfaceView
 */
@Composable
fun TerminalSurface(
    viewModel: TerminalViewModel,
    modifier: Modifier = Modifier
) {
    val activeSessionId by viewModel.activeSessionId.collectAsState()
    val sessions by viewModel.sessions.collectAsState()
    val backgroundColor by viewModel.backgroundColor.collectAsState()
    val foregroundColor by viewModel.foregroundColor.collectAsState()
    val fontSize by viewModel.fontSize.collectAsState()

    val activeSession = sessions.find { it.id == activeSessionId }

    Box(
        modifier = modifier
            .background(Color(backgroundColor))
            
    ) {
        AndroidView(
            factory = { ctx ->
                TerminalSurfaceView(ctx).apply {
                    this.viewModel = viewModel
                    updateColors(backgroundColor, foregroundColor)
                    updateFontSize(fontSize)
                    setSession(activeSession)
                }
            },
            update = { view ->
                view.viewModel = viewModel
                view.updateColors(backgroundColor, foregroundColor)
                view.updateFontSize(fontSize)
                view.setSession(activeSession)
            },
            modifier = Modifier.fillMaxSize()
        )
    }
}

class TerminalSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {

    var viewModel: TerminalViewModel? = null
    private var currentSession: TerminalSession? = null

    private var backgroundColorInt: Int = android.graphics.Color.BLACK
    private var foregroundColorInt: Int = android.graphics.Color.WHITE
    private var fontSizeSp: Float = 14f

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.MONOSPACE
        color = foregroundColorInt
        textSize = fontSizeSp
    }

    private var renderScope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    @Volatile
    private var isRendering = false
    @Volatile
    private var renderRequested = true

    init {
        holder.addCallback(this)
        isFocusable = true
        isFocusableInTouchMode = true
        isClickable = true
        isLongClickable = true

        setOnClickListener {
            requestFocus()
            requestFocusFromTouch()
            val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
            imm?.restartInput(this)
            imm?.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    fun setSession(session: TerminalSession?) {
        if (currentSession?.id == session?.id) return

        currentSession?.onScreenUpdate = null
        currentSession?.detachSurface()
        currentSession = session

        currentSession?.onScreenUpdate = {
            requestRender()
        }

        if (holder.surface.isValid) {
            currentSession?.attachSurface(holder.surface)
        }
        requestFocus()
        requestRender()
    }

    fun updateColors(bg: Int, fg: Int) {
        backgroundColorInt = bg
        foregroundColorInt = fg
        paint.color = fg
        setBackgroundColor(bg)
        requestRender()
    }

    fun updateFontSize(size: Float) {
        fontSizeSp = size
        paint.textSize = size
        requestRender()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        currentSession?.attachSurface(holder.surface)
        isRendering = true
        requestRender()
        startRenderLoop()
        post {
            requestFocus()
            requestFocusFromTouch()
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        val charWidth = paint.measureText("M").coerceAtLeast(1f)
        val charHeight = (paint.fontMetrics.descent - paint.fontMetrics.ascent).coerceAtLeast(1f)

        val columns = (width / charWidth).toInt().coerceAtLeast(1)
        val rows = (height / charHeight).toInt().coerceAtLeast(1)

        currentSession?.resize(columns, rows)
        requestRender()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        isRendering = false
        currentSession?.detachSurface()
        renderScope.cancel()
        renderScope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    }

    private fun startRenderLoop() {
        renderScope.launch {
            while (isRendering && isAttachedToWindow) {
                if (renderRequested) {
                    renderRequested = false
                    render()
                }
                delay(16)
            }
        }
    }

    private fun requestRender() {
        renderRequested = true
    }

    private fun render() {
        currentSession?.render()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.action == MotionEvent.ACTION_DOWN) {
            isFocusable = true
            isFocusableInTouchMode = true
            requestFocus()
            requestFocusFromTouch()
            post {
                val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
                imm?.restartInput(this)
                imm?.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
            }
            performClick()
        }
        return true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (hasWindowFocus) {
            requestFocus()
            requestFocusFromTouch()
        }
    }

    override fun isFocused(): Boolean = true

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val session = currentSession ?: return super.onKeyDown(keyCode, event)

        val modifiers = buildModifiers(event)
        session.sendKey(keyCode, modifiers, true)

        val unicodeChar = event.getUnicodeChar(event.metaState)
        if (unicodeChar != 0 && !event.isCtrlPressed && !event.isAltPressed) {
            session.sendChar(unicodeChar)
        }

        requestRender()
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        val session = currentSession ?: return super.onKeyUp(keyCode, event)
        session.sendKey(keyCode, buildModifiers(event), false)
        requestRender()
        return true
    }

    private fun buildModifiers(event: KeyEvent): Int {
        var mods = 0
        if (event.isShiftPressed) mods = mods or 1
        if (event.isCtrlPressed) mods = mods or 2
        if (event.isAltPressed) mods = mods or 4
        if (event.isMetaPressed) mods = mods or 8
        return mods
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_ACTION_NONE
        outAttrs.initialSelStart = 0
        outAttrs.initialSelEnd = 0
        return object : BaseInputConnection(this, false) {
            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                val value = text?.toString().orEmpty()
                if (value.isNotEmpty()) {
                    currentSession?.sendText(value)
                    requestRender()
                }
                return true
            }

            override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                repeat(beforeLength.coerceAtLeast(1)) {
                    currentSession?.sendText("\b")
                }
                requestRender()
                return true
            }

            override fun sendKeyEvent(event: KeyEvent): Boolean {
                return when (event.action) {
                    KeyEvent.ACTION_DOWN -> onKeyDown(event.keyCode, event)
                    KeyEvent.ACTION_UP -> onKeyUp(event.keyCode, event)
                    else -> false
                }
            }
        }
    }

    private fun showKeyboard() {
        val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
        imm?.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
    }
}
