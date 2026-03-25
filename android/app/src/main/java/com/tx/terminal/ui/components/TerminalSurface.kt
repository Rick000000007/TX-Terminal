package com.tx.terminal.ui.components

import android.content.ClipboardManager
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.viewinterop.AndroidView
import com.tx.terminal.data.TerminalSession
import com.tx.terminal.jni.NativeTerminal
import com.tx.terminal.viewmodel.TerminalViewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlin.math.floor

/**
 * Terminal rendering surface using AndroidView with a custom canvas-based terminal view
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

    val terminalViewHolder = remember { arrayOfNulls<TerminalSurfaceView>(1) }

    Box(
        modifier = modifier.background(Color(backgroundColor))
    ) {
        LaunchedEffect(viewModel, activeSessionId) {
            viewModel.clearSelectionEvent.collectLatest {
                terminalViewHolder[0]?.clearSelectionFromUi()
            }
        }

        key(activeSessionId) {
            AndroidView(
                factory = { ctx ->
                    TerminalSurfaceView(ctx).apply {
                        terminalViewHolder[0] = this
                        this.viewModel = viewModel
                        updateColors(backgroundColor, foregroundColor)
                        updateFontSize(fontSize)
                        setSession(activeSession)
                    }
                },
                update = { view ->
                    terminalViewHolder[0] = view
                    view.viewModel = viewModel
                    view.updateColors(backgroundColor, foregroundColor)
                    view.updateFontSize(fontSize)
                    view.setSession(activeSession)
                },
                modifier = Modifier.fillMaxSize()
            )
        }
    }
}

class TerminalSurfaceView(context: Context) : View(context) {

    fun clearSelectionFromUi() {
        clearPersistentSelection()
        invalidate()
        requestRender()
    }

    var viewModel: TerminalViewModel? = null
    private var currentSession: TerminalSession? = null

    private var backgroundColorInt: Int = android.graphics.Color.BLACK
    private var foregroundColorInt: Int = android.graphics.Color.WHITE
    private var fontSizeSp: Float = 14f

    private val horizontalPadding = 0f
    private val verticalPadding = 0f

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.MONOSPACE
        color = foregroundColorInt
        textSize = fontSizeSp
    }

    private val selectionPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = android.graphics.Color.WHITE
        style = Paint.Style.FILL
    }

    private val selectedTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.MONOSPACE
        color = android.graphics.Color.BLACK
        textSize = fontSizeSp
    }

    private val cursorPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = android.graphics.Color.argb(140, 255, 255, 255)
        style = Paint.Style.FILL
    }

    private val selectionHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = android.graphics.Color.parseColor("#4A90E2")
        style = Paint.Style.FILL
    }

    private var renderScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    @Volatile
    private var isRendering = false

    @Volatile
    private var renderRequested = true

    @Volatile
    private var cursorVisible = true

    private var blinkScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    private var ctrlPressed = false
    private var altPressed = false
    private var shiftPressed = false
    private var metaPressed = false

    private var selectionStartX = 0f
    private var selectionStartY = 0f
    private var isSelecting = false
    private var longPressTriggered = false
    private var hasPersistentSelection = false

    private var scrollOffset = 0
    private var touchScrollAccumY = 0f

    private enum class SelectionHandle {
        NONE,
        START,
        END
    }

    private var selectionStartCol = 0
    private var selectionStartRow = 0
    private var selectionEndCol = 0
    private var selectionEndRow = 0
    private var activeHandle = SelectionHandle.NONE
    private var longPressRunnable: Runnable? = null

    private val vibrator = context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
    private val clipboard =
        context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    private var cellWidth = 1f
    private var cellHeight = 1f
    private var textAscent = 0f
    private var baselineOffset = 0f
    private var terminalColumns = 80
    private var terminalRows = 24

    init {
        isFocusable = true
        isFocusableInTouchMode = true
        isClickable = true
        isLongClickable = true
        isEnabled = true
        setWillNotDraw(false)

        recalculateMetrics()

        setOnClickListener {
            requestFocus()
            requestFocusFromTouch()
            post {
                requestFocus()
                requestFocusFromTouch()
                showKeyboard()
            }
        }
    }

    fun setSession(session: TerminalSession?) {
        if (currentSession?.id == session?.id) return

        currentSession?.onScreenUpdate = null
        currentSession = session
        scrollOffset = 0
        touchScrollAccumY = 0f
        currentSession?.onScreenUpdate = {
            postInvalidateOnAnimation()
        }

        if (width > 0 && height > 0) {
            applyTerminalSize()
        }

        invalidate()
        requestRender()
    }

    fun updateColors(bg: Int, fg: Int) {
        backgroundColorInt = bg
        foregroundColorInt = fg
        paint.color = fg
        selectedTextPaint.color = android.graphics.Color.BLACK
        setBackgroundColor(bg)
        requestRender()
    }

    fun updateFontSize(size: Float) {
        if (fontSizeSp == size) return
        fontSizeSp = size
        paint.textSize = size
        recalculateMetrics()
        applyTerminalSize()
        requestRender()
    }

    private fun recalculateMetrics() {
    paint.typeface = Typeface.MONOSPACE
    paint.isLinearText = true

    val fm = paint.fontMetrics
    textAscent = fm.ascent
    baselineOffset = -fm.ascent

    val sample = "MMMMMMMMMM"
    cellWidth = (paint.measureText(sample) / sample.length).coerceAtLeast(1f)
    cellHeight = (fm.descent - fm.ascent + fm.leading).coerceAtLeast(1f)

    selectedTextPaint.typeface = Typeface.MONOSPACE
    selectedTextPaint.textSize = fontSizeSp

    val selectedFm = selectedTextPaint.fontMetrics
    selectedTextPaint.isLinearText = true
}

    private fun applyTerminalSize() {
    if (width <= 0 || height <= 0) return

    val usableWidth = (width - horizontalPadding * 2).coerceAtLeast(1f)
    val usableHeight = (height - verticalPadding * 2).coerceAtLeast(1f)

    val sample = "MMMMMMMMMM"
    val measuredCellWidth = (paint.measureText(sample) / sample.length).coerceAtLeast(1f)
    val fm = paint.fontMetrics
    val measuredCellHeight = (fm.descent - fm.ascent + fm.leading).coerceAtLeast(1f)

    val newColumns = floor(usableWidth / measuredCellWidth).toInt().coerceAtLeast(2)
    val newRows = floor(usableHeight / measuredCellHeight).toInt().coerceAtLeast(2)

    val sizeChanged = newColumns != terminalColumns || newRows != terminalRows

    terminalColumns = newColumns
    terminalRows = newRows
    viewModel?.updateTerminalSize(newColumns, newRows)
    cellWidth = measuredCellWidth
    cellHeight = measuredCellHeight
    baselineOffset = -fm.ascent

    if (sizeChanged) {
        currentSession?.resize(terminalColumns, terminalRows)
    }
}

    private fun startCursorBlink() {
        blinkScope.launch {
            while (isRendering && isAttachedToWindow) {
                cursorVisible = !cursorVisible
                requestRender()
                delay(530)
            }
        }
    }

    private fun requestRender() {
        renderRequested = true
        postInvalidate()
    }

    private fun render() {
        postInvalidate()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        isRendering = true
        renderScope.cancel()
        blinkScope.cancel()
        renderScope = CoroutineScope(Dispatchers.Default + SupervisorJob())
        blinkScope = CoroutineScope(Dispatchers.Default + SupervisorJob())
        startCursorBlink()
        applyTerminalSize()
    }

    override fun onDetachedFromWindow() {
        isRendering = false
        renderScope.cancel()
        blinkScope.cancel()
        super.onDetachedFromWindow()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        applyTerminalSize()
        requestRender()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        canvas.drawColor(backgroundColorInt)

        val session = currentSession
        val historySize = if (session != null) {
            try {
                NativeTerminal.getHistorySize(session.getNativeHandle())
            } catch (_: Exception) {
                0
            }
        } else {
            0
        }

        val screenRows = if (session != null) {
            try {
                session.getScreenDimensions().second.coerceAtMost(terminalRows)
            } catch (_: Exception) {
                terminalRows
            }
        } else {
            terminalRows
        }

        val totalRows = historySize + screenRows
        val maxScrollOffset = (totalRows - terminalRows).coerceAtLeast(0)
        if (scrollOffset > maxScrollOffset) {
            scrollOffset = maxScrollOffset
        }

        val firstVisibleRow = (totalRows - terminalRows - scrollOffset).coerceAtLeast(0)

        val visibleLines = if (session != null) {
            try {
                NativeTerminal.getVisibleRowsText(
                    session.getNativeHandle(),
                    firstVisibleRow,
                    terminalRows
                )
            } catch (_: Exception) {
                emptyArray()
            }
        } else {
            emptyArray()
        }

        for (row in 0 until terminalRows) {
            val top = verticalPadding + (row * cellHeight)
            val baselineY = top + baselineOffset
            val bottom = top + cellHeight

            if (top > height - verticalPadding) break

            val virtualRow = firstVisibleRow + row
            val line = visibleLines.getOrNull(row).orEmpty()
            val selectedSpan = getSelectionSpanForRow(virtualRow)

            if (selectedSpan != null) {
                val spanStart = selectedSpan.first.coerceIn(0, terminalColumns - 1)
                val spanEnd = selectedSpan.last.coerceIn(0, terminalColumns - 1)
                val selLeft = horizontalPadding + (spanStart * cellWidth)
                val selRight = horizontalPadding + ((spanEnd + 1) * cellWidth)
                val overlap = 1f
                canvas.drawRect(selLeft, top - overlap, selRight, bottom + overlap, selectionPaint)
            }

            for (col in 0 until terminalColumns) {
                val left = horizontalPadding + (col * cellWidth)

                val isSelected = selectedSpan?.let { col in it } == true

                if (col < line.length) {
                    val ch = line[col].toString()
                    val textPaint = if (isSelected) selectedTextPaint else paint
                    canvas.drawText(ch, left + cellWidth * 0.08f, baselineY, textPaint)
                }
            }
        }

        if (session != null && scrollOffset == 0) {
            try {
                val rawCursorCol = NativeTerminal.getCursorCol(session.getNativeHandle())
                val cursorRow = NativeTerminal.getCursorRow(session.getNativeHandle())

                if (cursorRow in 0 until terminalRows) {
                    val cursorCol = rawCursorCol.coerceIn(0, terminalColumns)

                    if (cursorCol in 0..terminalColumns) {
                        val left = horizontalPadding + (cursorCol * cellWidth)
                        val top = verticalPadding + (cursorRow * cellHeight)
                        val right = left + cellWidth
                        val bottom = top + cellHeight
                        canvas.drawRect(left, top, right, bottom, cursorPaint)
                    }
                }
            } catch (_: Exception) {
            }
        }

        if (hasPersistentSelection) {
            val handleRadius = (cellHeight * 0.22f).coerceAtLeast(10f)

            val firstVisibleRow = getFirstVisibleRow()
            val startVisibleRow = selectionStartRow - firstVisibleRow
            val endVisibleRow = selectionEndRow - firstVisibleRow

            val startCx = horizontalPadding + (selectionStartCol * cellWidth)
            val startCy = verticalPadding + (startVisibleRow * cellHeight) + cellHeight
            canvas.drawCircle(startCx, startCy, handleRadius, selectionHandlePaint)

            val endCx = horizontalPadding + ((selectionEndCol + 1) * cellWidth)
            val endCy = verticalPadding + (endVisibleRow * cellHeight) + cellHeight
            canvas.drawCircle(endCx, endCy, handleRadius, selectionHandlePaint)
        }
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType =
            EditorInfo.TYPE_CLASS_TEXT or
            EditorInfo.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
            EditorInfo.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
        outAttrs.imeOptions =
            EditorInfo.IME_FLAG_NO_EXTRACT_UI or
            EditorInfo.IME_ACTION_NONE
        return object : BaseInputConnection(this, true) {
            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                if (!text.isNullOrEmpty()) {
                    currentSession?.sendText(text.toString())
                }
                return true
            }

            override fun sendKeyEvent(event: KeyEvent): Boolean {
                if (event.action == KeyEvent.ACTION_DOWN) {
                    handleKeyEvent(event)
                }
                return true
            }

            override fun deleteSurroundingText(
                beforeLength: Int,
                afterLength: Int
            ): Boolean {
                currentSession?.sendText("\u007f")
                return true
            }
        }
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        return handleKeyEvent(event)
    }

    private fun handleKeyEvent(event: KeyEvent): Boolean {
        when (event.keyCode) {
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> {
                ctrlPressed = true
                return true
            }

            KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT -> {
                altPressed = true
                return true
            }

            KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT -> {
                shiftPressed = true
                return true
            }

            KeyEvent.KEYCODE_META_LEFT, KeyEvent.KEYCODE_META_RIGHT -> {
                metaPressed = true
                return true
            }
        }

        val modifiers =
            (if (shiftPressed) 1 else 0) or
            (if (ctrlPressed) 2 else 0) or
            (if (altPressed) 4 else 0) or
            (if (metaPressed) 8 else 0)

        when (event.keyCode) {
            KeyEvent.KEYCODE_ENTER -> currentSession?.sendText("\n")
            KeyEvent.KEYCODE_DEL -> currentSession?.sendText("\u007f")
            KeyEvent.KEYCODE_TAB -> currentSession?.sendText("\t")
            KeyEvent.KEYCODE_ESCAPE -> currentSession?.sendText("\u001b")

            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT -> currentSession?.sendKey(event.keyCode, modifiers, true)

            else -> {
                val unicode = event.unicodeChar
                if (unicode != 0) {
                    currentSession?.sendChar(unicode)
                } else {
                    currentSession?.sendKey(event.keyCode, modifiers, true)
                }
            }
        }

        requestRender()
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> ctrlPressed = false
            KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT -> altPressed = false
            KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT -> shiftPressed = false
            KeyEvent.KEYCODE_META_LEFT, KeyEvent.KEYCODE_META_RIGHT -> metaPressed = false

            else -> {
                val modifiers =
                    (if (shiftPressed) 1 else 0) or
                    (if (ctrlPressed) 2 else 0) or
                    (if (altPressed) 4 else 0) or
                    (if (metaPressed) 8 else 0)
                currentSession?.sendKey(keyCode, modifiers, false)
            }
        }
        return true
    }

    override fun performClick(): Boolean {
        return super.performClick()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                requestFocus()
                requestFocusFromTouch()

                val tappedHandle = hitTestHandle(event.x, event.y)
                if (tappedHandle != SelectionHandle.NONE) {
                    activeHandle = tappedHandle
                    parent?.requestDisallowInterceptTouchEvent(true)
                    invalidate()
                    return true
                }

                if (hasPersistentSelection) {
                    val (tapCol, tapRow) = screenToCell(event.x, event.y)
                    val insideSelection =
                        tapRow in selectionStartRow..selectionEndRow &&
                        (
                            (selectionStartRow == selectionEndRow &&
                             tapCol in selectionStartCol..selectionEndCol) ||
                            (tapRow == selectionStartRow && tapCol >= selectionStartCol) ||
                            (tapRow == selectionEndRow && tapCol <= selectionEndCol) ||
                            (tapRow > selectionStartRow && tapRow < selectionEndRow)
                        )

                    if (!insideSelection) {
                        clearPersistentSelection()
                        invalidate()
                    }
                }

                selectionStartX = event.x
                selectionStartY = event.y
                isSelecting = false
                longPressTriggered = false
                activeHandle = SelectionHandle.NONE

                longPressRunnable?.let { removeCallbacks(it) }
                longPressRunnable = Runnable {
                    if (!longPressTriggered && !isSelecting && activeHandle == SelectionHandle.NONE) {
                        longPressTriggered = true
                        vibrate()
                        val (col, row) = screenToCell(selectionStartX, selectionStartY)
                        selectionStartCol = col
                        selectionStartRow = row
                        selectionEndCol = col
                        selectionEndRow = row
                        hasPersistentSelection = true
                        updateNativeSelection()
                        isSelecting = true
                        parent?.requestDisallowInterceptTouchEvent(true)
                        invalidate()
                    }
                }
                postDelayed(longPressRunnable, 500)

                return true
            }

            MotionEvent.ACTION_MOVE -> {
                if (activeHandle != SelectionHandle.NONE) {
                    longPressRunnable?.let { removeCallbacks(it) }
                    parent?.requestDisallowInterceptTouchEvent(true)
                    applyHandleDrag(event.x, event.y)
                    return true
                }

                if (!longPressTriggered && activeHandle == SelectionHandle.NONE && !isSelecting) {
                    val dy = event.y - selectionStartY
                    if (kotlin.math.abs(dy) > cellHeight * 0.6f) {
                        val session = currentSession
                        if (session != null) {
                            val historySize = try {
                                NativeTerminal.getHistorySize(session.getNativeHandle())
                            } catch (_: Exception) {
                                0
                            }
                            val screenRows = try {
                                session.getScreenDimensions().second.coerceAtMost(terminalRows)
                            } catch (_: Exception) {
                                terminalRows
                            }
                            val totalRows = historySize + screenRows
                            val maxScrollOffset = (totalRows - terminalRows).coerceAtLeast(0)

                            if (maxScrollOffset > 0) {
                                touchScrollAccumY += (event.y - selectionStartY)
                                val lineDelta = (touchScrollAccumY / cellHeight).toInt()
                                if (lineDelta != 0) {
                                    scrollOffset = (scrollOffset + lineDelta).coerceIn(0, maxScrollOffset)
                                    touchScrollAccumY -= lineDelta * cellHeight
                                    selectionStartY = event.y
                                    invalidate()
                                }
                                longPressRunnable?.let { removeCallbacks(it) }
                                return true
                            }
                        }
                    }
                }

                val dx = kotlin.math.abs(event.x - selectionStartX)
                val dy = kotlin.math.abs(event.y - selectionStartY)
                if (!longPressTriggered && (dx > 16f || dy > 16f)) {
                    longPressRunnable?.let { removeCallbacks(it) }
                }

                if (longPressTriggered || isSelecting) {
                    parent?.requestDisallowInterceptTouchEvent(true)
                    isSelecting = true
                    updateSelection(event.x, event.y)
                    return true
                }

                return true
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                longPressRunnable?.let { removeCallbacks(it) }
                parent?.requestDisallowInterceptTouchEvent(false)
                touchScrollAccumY = 0f

                if (isSelecting || longPressTriggered || activeHandle != SelectionHandle.NONE) {
                    isSelecting = false
                    activeHandle = SelectionHandle.NONE
                    longPressTriggered = false
                    invalidate()
                    return true
                }

                performClick()
                post {
                    showKeyboard()
                }
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun startSelection() {
        isSelecting = true
        hasPersistentSelection = true
        Toast.makeText(context, "Selection mode - drag to select", Toast.LENGTH_SHORT).show()
    }

    private fun getHistorySizeSafe(): Int {
        val session = currentSession ?: return 0
        return try {
            NativeTerminal.getHistorySize(session.getNativeHandle())
        } catch (_: Exception) {
            0
        }
    }

    private fun getScreenRowsSafe(): Int {
        val session = currentSession ?: return terminalRows
        return try {
            session.getScreenDimensions().second.coerceAtMost(terminalRows)
        } catch (_: Exception) {
            terminalRows
        }
    }

    private fun getFirstVisibleRow(): Int {
        val historySize = getHistorySizeSafe()
        val screenRows = getScreenRowsSafe()
        val totalRows = historySize + screenRows
        val maxScrollOffset = (totalRows - terminalRows).coerceAtLeast(0)
        if (scrollOffset > maxScrollOffset) {
            scrollOffset = maxScrollOffset
        }
        return (totalRows - terminalRows - scrollOffset).coerceAtLeast(0)
    }

    private fun screenToVirtualCell(x: Float, y: Float): Pair<Int, Int> {
        val colBias = cellWidth * 0.5f
        val col = (((x - horizontalPadding + colBias) / cellWidth).toInt())
            .coerceIn(0, terminalColumns - 1)
        val visibleRow = (((y - verticalPadding) / cellHeight).toInt())
            .coerceIn(0, terminalRows - 1)
        val virtualRow = getFirstVisibleRow() + visibleRow
        return col to virtualRow
    }

    private fun screenToCell(x: Float, y: Float): Pair<Int, Int> {
        return screenToVirtualCell(x, y)
    }

    private fun getSelectionSpanForRow(row: Int): IntRange? {
        if (!hasPersistentSelection) return null
        if (row < selectionStartRow || row > selectionEndRow) return null

        return when {
            selectionStartRow == selectionEndRow ->
                selectionStartCol..selectionEndCol
            row == selectionStartRow ->
                selectionStartCol..(terminalColumns - 1)
            row == selectionEndRow ->
                0..selectionEndCol
            else ->
                0..(terminalColumns - 1)
        }
    }

    private fun normalizeSelection() {
        if (selectionStartRow > selectionEndRow ||
            (selectionStartRow == selectionEndRow && selectionStartCol > selectionEndCol)
        ) {
            val sc = selectionStartCol
            val sr = selectionStartRow
            selectionStartCol = selectionEndCol
            selectionStartRow = selectionEndRow
            selectionEndCol = sc
            selectionEndRow = sr

            activeHandle = when (activeHandle) {
                SelectionHandle.START -> SelectionHandle.END
                SelectionHandle.END -> SelectionHandle.START
                SelectionHandle.NONE -> SelectionHandle.NONE
            }
        }
    }

    private fun hitTestHandle(x: Float, y: Float): SelectionHandle {
        if (!hasPersistentSelection) return SelectionHandle.NONE

        val handleRadius = (cellHeight * 0.22f).coerceAtLeast(10f)
        fun near(px: Float, py: Float): Boolean {
            val dx = x - px
            val dy = y - py
            return dx * dx + dy * dy <= handleRadius * handleRadius * 4f
        }

        val firstVisibleRow = getFirstVisibleRow()
        val startVisibleRow = selectionStartRow - firstVisibleRow
        val endVisibleRow = selectionEndRow - firstVisibleRow

        val startCx = horizontalPadding + (selectionStartCol * cellWidth)
        val startCy = verticalPadding + (startVisibleRow * cellHeight) + cellHeight
        val endCx = horizontalPadding + ((selectionEndCol + 1) * cellWidth)
        val endCy = verticalPadding + (endVisibleRow * cellHeight) + cellHeight

        return when {
            near(startCx, startCy) -> SelectionHandle.START
            near(endCx, endCy) -> SelectionHandle.END
            else -> SelectionHandle.NONE
        }
    }

    private fun applyHandleDrag(x: Float, y: Float) {
        val historySize = getHistorySizeSafe()
        val screenRows = getScreenRowsSafe()
        val totalRows = historySize + screenRows
        val maxScrollOffset = (totalRows - terminalRows).coerceAtLeast(0)

        val edgeZone = (cellHeight * 0.35f).coerceAtLeast(6f)

        if (maxScrollOffset > 0) {
            when {
                y < verticalPadding + edgeZone -> {
                    scrollOffset = (scrollOffset + 1).coerceAtMost(maxScrollOffset)
                }
                y > height - verticalPadding - edgeZone -> {
                    scrollOffset = (scrollOffset - 1).coerceAtLeast(0)
                }
            }
        }

        val (col, row) = screenToVirtualCell(x, y)

        when (activeHandle) {
            SelectionHandle.START -> {
                selectionStartCol = col
                selectionStartRow = row
            }
            SelectionHandle.END -> {
                selectionEndCol = col
                selectionEndRow = row
            }
            SelectionHandle.NONE -> return
        }
        normalizeSelection()
        updateNativeSelection()
        invalidate()
    }

    private fun updateNativeSelection() {
        currentSession?.let { session ->
            try {
                NativeTerminal.setSelection(
                    session.getNativeHandle(),
                    selectionStartCol,
                    selectionStartRow,
                    selectionEndCol,
                    selectionEndRow
                )
                hasPersistentSelection = true
                requestRender()
            } catch (_: Exception) {
            }
        }
    }

    private fun clearPersistentSelection() {
        currentSession?.let { session ->
            try {
                NativeTerminal.clearSelection(session.getNativeHandle())
            } catch (_: Exception) {
            }
        }
        hasPersistentSelection = false
        isSelecting = false
        longPressTriggered = false
        activeHandle = SelectionHandle.NONE
    }

    private fun updateSelection(x: Float, y: Float) {
        val (startCol, startRow) = screenToCell(selectionStartX, selectionStartY)
        val (endCol, endRow) = screenToCell(x, y)

        selectionStartCol = startCol
        selectionStartRow = startRow
        selectionEndCol = endCol
        selectionEndRow = endRow
        normalizeSelection()
        updateNativeSelection()
    }

    private fun showKeyboard() {
        requestFocus()
        requestFocusFromTouch()
        val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.restartInput(this)
        imm.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
    }

    private fun vibrate() {
        vibrator?.let {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                it.vibrate(VibrationEffect.createOneShot(30, VibrationEffect.DEFAULT_AMPLITUDE))
            } else {
                @Suppress("DEPRECATION")
                it.vibrate(30)
            }
        }
    }
}
