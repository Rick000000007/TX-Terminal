package com.tx.terminal.jni

import android.util.Log

/**
 * JNI bridge to native terminal implementation
 * All methods are thread-safe and can be called from any thread
 */
object NativeTerminal {
    private const val TAG = "NativeTerminal"
    
    init {
        try {
            System.loadLibrary("tx_jni")
            Log.i(TAG, "Native library loaded")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library", e)
            throw e
        }
    }
    
    // Native methods
    
    /**
     * Create a new terminal instance
     * @param columns Number of columns
     * @param rows Number of rows
     * @param shellPath Path to shell executable
     * @param initialCommand Optional initial command to execute
     * @param environment Array of environment variables (KEY=VALUE)
     * @return Native handle (0 on failure)
     */
    @JvmStatic
    external fun create(
        columns: Int,
        rows: Int,
        shellPath: String,
        initialCommand: String?,
        environment: Array<String>? = null
    ): Long
    
    /**
     * Destroy a terminal instance
     * @param handle Native handle
     */
    @JvmStatic
    external fun destroy(handle: Long)
    
    /**
     * Set the rendering surface
     * @param handle Native handle
     * @param surface Android Surface
     */
    @JvmStatic
    external fun setSurface(handle: Long, surface: android.view.Surface)
    
    /**
     * Destroy the rendering surface
     * @param handle Native handle
     */
    @JvmStatic
    external fun destroySurface(handle: Long)
    
    /**
     * Render one frame
     * @param handle Native handle
     */
    @JvmStatic
    external fun render(handle: Long)
    
    /**
     * Resize the terminal
     * @param handle Native handle
     * @param columns New column count
     * @param rows New row count
     */
    @JvmStatic
    external fun resize(handle: Long, columns: Int, rows: Int)
    
    /**
     * Send a key event
     * @param handle Native handle
     * @param keyCode Key code
     * @param modifiers Modifier flags (shift=1, ctrl=2, alt=4, meta=8)
     * @param pressed True for key down, false for key up
     */
    @JvmStatic
    external fun sendKey(handle: Long, keyCode: Int, modifiers: Int, pressed: Boolean)
    
    /**
     * Send a character
     * @param handle Native handle
     * @param codepoint Unicode codepoint
     */
    @JvmStatic
    external fun sendChar(handle: Long, codepoint: Int)
    
    /**
     * Send text
     * @param handle Native handle
     * @param text Text to send
     */
    @JvmStatic
    external fun sendText(handle: Long, text: String)
    
    /**
     * Get screen content as string (for debugging)
     * @param handle Native handle
     * @return Screen content
     */
    @JvmStatic
    external fun getScreenContent(handle: Long): String
    
    /**
     * Get number of columns
     * @param handle Native handle
     * @return Column count
     */
    @JvmStatic
    external fun getColumns(handle: Long): Int
    
    /**
     * Get number of rows
     * @param handle Native handle
     * @return Row count
     */
    @JvmStatic
    external fun getRows(handle: Long): Int
    
    /**
     * Check if terminal is still running
     * @param handle Native handle
     * @return true if running
     */
    @JvmStatic
    external fun isRunning(handle: Long): Boolean
    
    /**
     * Get exit code
     * @param handle Native handle
     * @return Exit code, or -1 if still running
     */
    @JvmStatic
    external fun getExitCode(handle: Long): Int
    
    /**
     * Copy selection to clipboard
     * @param handle Native handle
     * @return Selected text
     */
    @JvmStatic
    external fun copySelection(handle: Long): String
    
    /**
     * Set selection
     * @param handle Native handle
     * @param startCol Start column
     * @param startRow Start row
     * @param endCol End column
     * @param endRow End row
     */
    @JvmStatic
    external fun setSelection(
        handle: Long,
        startCol: Int,
        startRow: Int,
        endCol: Int,
        endRow: Int
    )
    
    /**
     * Clear selection
     * @param handle Native handle
     */
    @JvmStatic
    external fun clearSelection(handle: Long)
    
    /**
     * Get version string
     * @return Version string
     */
    @JvmStatic
    external fun getVersion(): String
}
