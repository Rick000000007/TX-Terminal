package com.tx.terminal.data

import android.os.Handler
import android.os.Looper
import android.os.Message
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Surface
import com.tx.terminal.TXApplication
import com.tx.terminal.jni.NativeTerminal
import com.tx.terminal.util.ByteQueue
import java.io.FileDescriptor
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Terminal Session - A single terminal session with PTY and shell process.
 *
 * Based on Termux's TerminalSession architecture:
 * - Uses ByteQueue for thread-safe I/O between PTY and terminal
 * - Handler-based main thread communication
 * - Proper session lifecycle management
 * - Survives Activity restarts when bound to Service
 */
class TerminalSession(
    val id: String,
    val name: String,
    private val shellPath: String = "/system/bin/sh",
    private val initialCommand: String? = null,
    private val columns: Int = 80,
    private val rows: Int = 24
) {
    companion object {
        private const val TAG = "TerminalSession"

        // Message types for handler
        private const val MSG_NEW_INPUT = 1
        private const val MSG_PROCESS_EXITED = 2
        private const val MSG_SCREEN_UPDATED = 3
    }

    // Title flow for UI
    private val _title = MutableStateFlow(name)
    val title: StateFlow<String> = _title.asStateFlow()

    // Running state flow for UI
    private val _isRunningFlow = MutableStateFlow(false)
    val isRunning: StateFlow<Boolean> = _isRunningFlow.asStateFlow()

    // Native handle from JNI
    private var nativeHandle: Long = 0

    // Session state
    private val _isRunning = AtomicBoolean(false)
    val isRunningValue: Boolean get() = _isRunning.get()

    // Exit code
    private var exitCode: Int = -1

    // PTY file descriptor (when using direct PTY mode)
    private var ptyFd: Int = -1
    private var shellPid: Int = -1
    private var ptyFileDescriptor: ParcelFileDescriptor? = null

    // I/O queues for thread-safe communication
    private val processToTerminalQueue = ByteQueue(64 * 1024)
    private val terminalToProcessQueue = ByteQueue(4096)

    // UTF-8 input buffer for codepoint conversion
    private val utf8InputBuffer = ByteArray(5)

    // Surface for rendering
    private var currentSurface: Surface? = null

    // Handler for main thread callbacks
    private val mainHandler = Handler(Looper.getMainLooper()) { msg ->
        when (msg.what) {
            MSG_NEW_INPUT -> processInput()
            MSG_PROCESS_EXITED -> handleProcessExit(msg.obj as Int)
            MSG_SCREEN_UPDATED -> notifyScreenUpdate()
            else -> false
        }
        true
    }

    // Callbacks
    var onScreenUpdate: (() -> Unit)? = null
    var onTitleChange: ((String) -> Unit)? = null
    var onSessionFinished: ((Int) -> Unit)? = null
    var onProcessExit: ((Int) -> Unit)? = null

    // Reader/writer threads
    private var inputReaderThread: Thread? = null
    private var outputWriterThread: Thread? = null
    private var processWaiterThread: Thread? = null

    init {
        Log.d(TAG, "Creating session: $id")
    }

    /**
     * Initialize the terminal session.
     * Creates PTY, spawns shell, and starts I/O threads.
     */
    fun initialize(): Boolean {
        if (nativeHandle != 0L || _isRunning.get()) {
            Log.w(TAG, "Session already initialized")
            return true
        }

        // Get runtime environment
        val env = arrayOf(
        "TERM=xterm-256color",
        "PATH=/system/bin:/system/xbin",
        "HOME=/data/data/com.tx.terminal/files/home",
        "PWD=/data/data/com.tx.terminal/files/home"
         )
        Log.d(TAG, "Environment: ${env.joinToString()}")

        // Create native terminal
        nativeHandle = NativeTerminal.create(columns, rows, shellPath, initialCommand, env)
        if (nativeHandle == 0L) {
            Log.e(TAG, "Failed to create native terminal")
            return false
        }

        _isRunning.set(true)
        _isRunningFlow.value = true

        // Start I/O threads
        startIOThreads()
        sendText("cd /data/data/com.tx.terminal/files/home\n")

        Log.d(TAG, "Session initialized: $id (handle=$nativeHandle)")
        return true
    }

    /**
     * Initialize with direct PTY file descriptor (alternative to JNI mode).
     * This is used for more direct control over the PTY.
     */
    fun initializeWithPty(ptyFd: Int, shellPid: Int): Boolean {
        if (_isRunning.get()) {
            Log.w(TAG, "Session already initialized")
            return true
        }

        this.ptyFd = ptyFd
        this.shellPid = shellPid
        this.nativeHandle = -1 // Mark as direct PTY mode

        // Create ParcelFileDescriptor from raw fd
        this.ptyFileDescriptor = ParcelFileDescriptor.adoptFd(ptyFd)

        _isRunning.set(true)
        _isRunningFlow.value = true

        // Start I/O threads for direct PTY
        startDirectPtyThreads()

        Log.d(TAG, "Session initialized with PTY: $id (fd=$ptyFd, pid=$shellPid)")
        return true
    }

    /**
     * Start I/O threads for native JNI mode.
     */
    private fun startIOThreads() {
        // Input reader thread - reads from native terminal
        inputReaderThread = Thread({
            val buffer = ByteArray(4096)
            while (_isRunning.get()) {
                try {
                    // Get screen content changes
                    val content = NativeTerminal.getScreenContent(nativeHandle)
                    if (content.isNotEmpty()) {
                        val bytes = content.toByteArray(Charsets.UTF_8)
                        if (processToTerminalQueue.write(bytes, 0, bytes.size)) {
                            mainHandler.sendEmptyMessage(MSG_NEW_INPUT)
                        }
                    }

                    // Check if process is still running
                    if (!NativeTerminal.isRunning(nativeHandle)) {
                        val exitCode = NativeTerminal.getExitCode(nativeHandle)
                        mainHandler.sendMessage(
                            mainHandler.obtainMessage(MSG_PROCESS_EXITED, exitCode)
                        )
                        break
                    }

                    Thread.sleep(16) // ~60 FPS
                } catch (e: Exception) {
                    if (_isRunning.get()) {
                        Log.e(TAG, "Error in input reader", e)
                    }
                    break
                }
            }
        }, "TX-InputReader-$id").apply { start() }

        // Output writer thread - writes to native terminal
        outputWriterThread = Thread({
            val buffer = ByteArray(4096)
            while (_isRunning.get()) {
                try {
                    val bytesRead = terminalToProcessQueue.read(buffer, true)
                    if (bytesRead > 0) {
                        val text = String(buffer, 0, bytesRead, Charsets.UTF_8)
                        NativeTerminal.sendText(nativeHandle, text)
                    } else if (bytesRead < 0) {
                        break // Queue closed
                    }
                } catch (e: Exception) {
                    if (_isRunning.get()) {
                        Log.e(TAG, "Error in output writer", e)
                    }
                    break
                }
            }
        }, "TX-OutputWriter-$id").apply { start() }
    }

    /**
     * Start I/O threads for direct PTY mode.
     */
    private fun startDirectPtyThreads() {
        val fd = ptyFileDescriptor?.fileDescriptor ?: return

        // Input reader thread
        inputReaderThread = Thread({
            try {
                FileInputStream(fd).use { input ->
                    val buffer = ByteArray(4096)
                    while (_isRunning.get()) {
                        val read = input.read(buffer)
                        if (read == -1) break

                        if (read > 0 && processToTerminalQueue.write(buffer, 0, read)) {
                            mainHandler.sendEmptyMessage(MSG_NEW_INPUT)
                        }
                    }
                }
            } catch (e: Exception) {
                Log.d(TAG, "Input reader ended: ${e.message}")
            }
        }, "TX-PtyInputReader-$id").apply { start() }

        // Output writer thread
        outputWriterThread = Thread({
            try {
                FileOutputStream(fd).use { output ->
                    val buffer = ByteArray(4096)
                    while (_isRunning.get()) {
                        val bytesRead = terminalToProcessQueue.read(buffer, true)
                        if (bytesRead > 0) {
                            output.write(buffer, 0, bytesRead)
                        } else if (bytesRead < 0) {
                            break
                        }
                    }
                }
            } catch (e: Exception) {
                Log.d(TAG, "Output writer ended: ${e.message}")
            }
        }, "TX-PtyOutputWriter-$id").apply { start() }

        // Process waiter thread
        processWaiterThread = Thread({
            try {
                val exitCode = waitForProcess(shellPid)
                mainHandler.sendMessage(
                    mainHandler.obtainMessage(MSG_PROCESS_EXITED, exitCode)
                )
            } catch (e: Exception) {
                Log.e(TAG, "Process waiter error", e)
            }
        }, "TX-ProcessWaiter-$id").apply { start() }
    }

    /**
     * Process input from the PTY queue.
     */
    private fun processInput() {
        if (!_isRunning.get()) return

        // Notify UI to update
        mainHandler.sendEmptyMessage(MSG_SCREEN_UPDATED)
    }

    /**
     * Handle process exit.
     */
    private fun handleProcessExit(exitCode: Int) {
        Log.d(TAG, "Process exited with code: $exitCode")
        this.exitCode = exitCode
        _isRunning.set(false)
        _isRunningFlow.value = false

        onProcessExit?.invoke(exitCode)
        onSessionFinished?.invoke(exitCode)
    }

    /**
     * Notify that screen was updated.
     */
    private fun notifyScreenUpdate() {
        onScreenUpdate?.invoke()
    }

    /**
     * Write bytes to the terminal process.
     */
    fun write(bytes: ByteArray, offset: Int, count: Int) {
        if (!_isRunning.get()) return
        terminalToProcessQueue.write(bytes, offset, count)
    }

    /**
     * Write a Unicode code point to the terminal.
     */
    fun writeCodePoint(prependEscape: Boolean, codePoint: Int) {
        if (codePoint > 0x10FFFF || (codePoint in 0xD800..0xDFFF)) {
            throw IllegalArgumentException("Invalid code point: $codePoint")
        }

        var pos = 0
        if (prependEscape) utf8InputBuffer[pos++] = 27

        when {
            codePoint <= 0x7F -> {
                utf8InputBuffer[pos++] = codePoint.toByte()
            }
            codePoint <= 0x7FF -> {
                utf8InputBuffer[pos++] = (0xC0 or (codePoint shr 6)).toByte()
                utf8InputBuffer[pos++] = (0x80 or (codePoint and 0x3F)).toByte()
            }
            codePoint <= 0xFFFF -> {
                utf8InputBuffer[pos++] = (0xE0 or (codePoint shr 12)).toByte()
                utf8InputBuffer[pos++] = (0x80 or ((codePoint shr 6) and 0x3F)).toByte()
                utf8InputBuffer[pos++] = (0x80 or (codePoint and 0x3F)).toByte()
            }
            else -> {
                utf8InputBuffer[pos++] = (0xF0 or (codePoint shr 18)).toByte()
                utf8InputBuffer[pos++] = (0x80 or ((codePoint shr 12) and 0x3F)).toByte()
                utf8InputBuffer[pos++] = (0x80 or ((codePoint shr 6) and 0x3F)).toByte()
                utf8InputBuffer[pos++] = (0x80 or (codePoint and 0x3F)).toByte()
            }
        }

        write(utf8InputBuffer, 0, pos)
    }

    /**
     * Send text to the terminal.
     */
    fun sendText(text: String) {
        val bytes = text.toByteArray(Charsets.UTF_8)
        write(bytes, 0, bytes.size)
    }

    /**
     * Send a key event.
     */
    fun sendKey(keyCode: Int, modifiers: Int, pressed: Boolean) {
        if (nativeHandle > 0) {
            NativeTerminal.sendKey(nativeHandle, keyCode, modifiers, pressed)
        }
    }

    /**
     * Send a character.
     */
    fun sendChar(codepoint: Int) {
        if (nativeHandle > 0) {
            NativeTerminal.sendChar(nativeHandle, codepoint)
        }
    }

    /**
     * Resize the terminal.
     */
    fun resize(columns: Int, rows: Int) {
        if (nativeHandle > 0) {
            NativeTerminal.resize(nativeHandle, columns, rows)
        }
    }

    /**
     * Get screen content.
     */
    fun getScreenContent(): String {
        return if (nativeHandle > 0) {
            NativeTerminal.getScreenContent(nativeHandle)
        } else {
            ""
        }
    }

    /**
     * Get screen dimensions.
     */
    fun getScreenDimensions(): Pair<Int, Int> {
        return if (nativeHandle > 0) {
            Pair(
                NativeTerminal.getColumns(nativeHandle),
                NativeTerminal.getRows(nativeHandle)
            )
        } else {
            Pair(columns, rows)
        }
    }

    /**
     * Copy selection to clipboard.
     */
    fun copySelection(): String {
        return if (nativeHandle > 0) {
            NativeTerminal.copySelection(nativeHandle)
        } else {
            ""
        }
    }

    /**
     * Paste text to terminal.
     */
    fun paste(text: String) {
        sendText(text)
    }

    /**
     * Send Ctrl+C (interrupt).
     */
    fun sendInterrupt() {
        write(byteArrayOf(0x03), 0, 1) // ETX
    }

    /**
     * Send Ctrl+D (EOF).
     */
    fun sendEOF() {
        write(byteArrayOf(0x04), 0, 1) // EOT
    }

    /**
     * Clear screen.
     */
    fun clearScreen() {
        sendText("\u001bc") // ESC c (reset)
    }

    /**
     * Attach a surface for rendering.
     */
    fun attachSurface(surface: Surface) {
        currentSurface = surface
        if (nativeHandle > 0) {
            NativeTerminal.setSurface(nativeHandle, surface)
        }
    }

    /**
     * Detach the current surface.
     */
    fun detachSurface() {
        if (nativeHandle > 0) {
            NativeTerminal.destroySurface(nativeHandle)
        }
        currentSurface = null
    }

    /**
     * Render the terminal to the attached surface.
     */
    fun render() {
        if (nativeHandle > 0) {
            NativeTerminal.render(nativeHandle)
        }
    }

    /**
     * Update the session title.
     */
    fun setTitle(newTitle: String) {
        _title.value = newTitle
        onTitleChange?.invoke(newTitle)
    }

    /**
     * Finish the session and cleanup resources.
     */
    fun finish() {
        Log.d(TAG, "Finishing session: $id")
        _isRunning.set(false)
        _isRunningFlow.value = false

        // Close queues to unblock threads
        terminalToProcessQueue.close()
        processToTerminalQueue.close()

        // Wait for threads to finish
        inputReaderThread?.join(500)
        outputWriterThread?.join(500)
        processWaiterThread?.join(500)

        // Cleanup native resources
        if (nativeHandle > 0) {
            NativeTerminal.destroy(nativeHandle)
            nativeHandle = 0
        }

        // Close PTY if using direct mode
        ptyFileDescriptor?.let {
            try {
                it.close()
            } catch (e: Exception) {
                // Ignore
            }
            ptyFileDescriptor = null
        }
        ptyFd = -1
    }
    
    /**
 * Wait for a process to exit.
 * Uses fallback polling for maximum compatibility.
 */
private fun waitForProcess(pid: Int): Int {
    return waitForProcessFallback(pid)
}

/**
 * Parse exit status (kept for future use).
 */
private fun parseExitStatus(status: Int): Int {
    return when {
        (status and 0x7F) == 0 -> (status shr 8) and 0xFF
        else -> -(status and 0x7F)
    }
}

/**
 * Fallback method to wait for process exit by polling.
 */
private fun waitForProcessFallback(pid: Int): Int {
    var attempts = 0
    while (attempts < 300) {
        try {
            android.os.Process.sendSignal(pid, 0)
            Thread.sleep(100)
            attempts++
        } catch (_: Exception) {
            return 0
        }
    }

    try {
        android.os.Process.killProcess(pid)
    } catch (_: Exception) {}

    return -1
}
}








