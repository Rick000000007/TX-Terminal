package com.tx.terminal;

import android.view.Surface;

/**
 * JNI bridge to native terminal implementation
 */
public class TXNative {
    static {
        System.loadLibrary("tx_jni");
    }
    
    /**
     * Create a new terminal instance
     * @param cols Number of columns
     * @param rows Number of rows
     * @param Shell path to shell executable (null for default)
     * @return Native handle, or 0 on failure
     */
    public static native long createTerminal(int cols, int rows, String shell);
    
    /**
     * Destroy a terminal instance
     * @param handle Native handle from createTerminal
     */
    public static native void destroyTerminal(long handle);
    
    /**
     * Set the rendering surface
     * @param handle Native handle
     * @param surface Android Surface for rendering
     */
    public static native void setSurface(long handle, Surface surface);
    
    /**
     * Destroy the rendering surface
     * @param handle Native handle
     */
    public static native void destroySurface(long handle);
    
    /**
     * Render one frame
     * @param handle Native handle
     */
    public static native void render(long handle);
    
    /**
     * Handle resize
     * @param handle Native handle
     * @param width New width in pixels
     * @param height New height in pixels
     */
    public static native void onResize(long handle, int width, int height);
    
    /**
     * Send a key event
     * @param handle Native handle
     * @param key Key code
     * @param mods Modifier flags (shift=1, ctrl=2, alt=4)
     * @param pressed True for key down, false for key up
     */
    public static native void sendKey(long handle, int key, int mods, boolean pressed);
    
    /**
     * Send a character
     * @param handle Native handle
     * @param codepoint Unicode codepoint
     */
    public static native void sendChar(long handle, int codepoint);
    
    /**
     * Send text
     * @param handle Native handle
     * @param text Text to send
     */
    public static native void sendText(long handle, String text);
    
    /**
     * Get screen content as string (for debugging)
     * @param handle Native handle
     * @return Screen content
     */
    public static native String getScreenContent(long handle);
    
    /**
     * Check if terminal is still running
     * @param handle Native handle
     * @return true if running
     */
    public static native boolean isRunning(long handle);
}
