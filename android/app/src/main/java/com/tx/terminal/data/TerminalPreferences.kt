package com.tx.terminal.data

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "terminal_preferences")

class TerminalPreferences(private val context: Context) {
    
    // Keys
    companion object {
        val FONT_SIZE = floatPreferencesKey("font_size")
        val FONT_PATH = stringPreferencesKey("font_path")
        val BACKGROUND_COLOR = intPreferencesKey("background_color")
        val FOREGROUND_COLOR = intPreferencesKey("foreground_color")
        val CURSOR_COLOR = intPreferencesKey("cursor_color")
        val CURSOR_BLINK = booleanPreferencesKey("cursor_blink")
        val CURSOR_STYLE = intPreferencesKey("cursor_style")  // 0=block, 1=underline, 2=bar
        val SHOW_EXTRA_KEYS = booleanPreferencesKey("show_extra_keys")
        val VIBRATE_ON_KEY = booleanPreferencesKey("vibrate_on_key")
        val SHELL_PATH = stringPreferencesKey("shell_path")
        val INITIAL_COMMAND = stringPreferencesKey("initial_command")
        val TERMINAL_COLUMNS = intPreferencesKey("terminal_columns")
        val TERMINAL_ROWS = intPreferencesKey("terminal_rows")
        val SCROLLBACK_LINES = intPreferencesKey("scrollback_lines")
        val LAST_SESSION_ID = stringPreferencesKey("last_session_id")
        val WAKE_LOCK_ENABLED = booleanPreferencesKey("wake_lock_enabled")
    }
    
    // Default values
    object Defaults {
        const val FONT_SIZE = 14f
        const val BACKGROUND_COLOR = 0xFF000000.toInt()
        const val FOREGROUND_COLOR = 0xFFFFFFFF.toInt()
        const val CURSOR_COLOR = 0xFFFFFFFF.toInt()
        const val CURSOR_BLINK = false
        const val SHOW_EXTRA_KEYS = true
        const val VIBRATE_ON_KEY = true
        const val SHELL_PATH = "/system/bin/sh"
        const val TERMINAL_COLUMNS = 80
        const val TERMINAL_ROWS = 24
        const val SCROLLBACK_LINES = 10000
    }
    
    // Flows for reactive UI
    val fontSize: Flow<Float> = context.dataStore.data
        .map { it[FONT_SIZE] ?: Defaults.FONT_SIZE }
    
    val backgroundColor: Flow<Int> = context.dataStore.data
        .map { it[BACKGROUND_COLOR] ?: Defaults.BACKGROUND_COLOR }
    
    val foregroundColor: Flow<Int> = context.dataStore.data
        .map { it[FOREGROUND_COLOR] ?: Defaults.FOREGROUND_COLOR }
    
    val cursorColor: Flow<Int> = context.dataStore.data
        .map { it[CURSOR_COLOR] ?: Defaults.CURSOR_COLOR }
    
    val cursorBlink: Flow<Boolean> = context.dataStore.data
        .map { it[CURSOR_BLINK] ?: Defaults.CURSOR_BLINK }
    
    val showExtraKeys: Flow<Boolean> = context.dataStore.data
        .map { it[SHOW_EXTRA_KEYS] ?: Defaults.SHOW_EXTRA_KEYS }
    
    val vibrateOnKey: Flow<Boolean> = context.dataStore.data
        .map { it[VIBRATE_ON_KEY] ?: Defaults.VIBRATE_ON_KEY }
    
    val shellPath: Flow<String> = context.dataStore.data
        .map { it[SHELL_PATH] ?: Defaults.SHELL_PATH }
    
    val scrollbackLines: Flow<Int> = context.dataStore.data
        .map { it[SCROLLBACK_LINES] ?: Defaults.SCROLLBACK_LINES }
    
    // Synchronous getters (for initial setup)
    fun getFontSizeSync(): Float = runBlocking { fontSize.first() }
    fun getBackgroundColorSync(): Int = runBlocking { backgroundColor.first() }
    fun getForegroundColorSync(): Int = runBlocking { foregroundColor.first() }
    fun getShellPathSync(): String = runBlocking { shellPath.first() }
    
    // Setters
    suspend fun setFontSize(size: Float) {
        context.dataStore.edit { it[FONT_SIZE] = size }
    }
    
    suspend fun setBackgroundColor(color: Int) {
        context.dataStore.edit { it[BACKGROUND_COLOR] = color }
    }
    
    suspend fun setForegroundColor(color: Int) {
        context.dataStore.edit { it[FOREGROUND_COLOR] = color }
    }
    
    suspend fun setCursorBlink(blink: Boolean) {
        context.dataStore.edit { it[CURSOR_BLINK] = blink }
    }
    
    suspend fun setShowExtraKeys(show: Boolean) {
        context.dataStore.edit { it[SHOW_EXTRA_KEYS] = show }
    }
    
    suspend fun setShellPath(path: String) {
        context.dataStore.edit { it[SHELL_PATH] = path }
    }
    
    suspend fun setScrollbackLines(lines: Int) {
        context.dataStore.edit { it[SCROLLBACK_LINES] = lines }
    }

    suspend fun setLastSessionId(sessionId: String) {
        context.dataStore.edit { it[LAST_SESSION_ID] = sessionId }
    }

    suspend fun setWakeLockEnabled(enabled: Boolean) {
        context.dataStore.edit { it[WAKE_LOCK_ENABLED] = enabled }
    }

    suspend fun setCursorStyle(style: Int) {
        context.dataStore.edit { it[CURSOR_STYLE] = style }
    }

    suspend fun setCursorColor(color: Int) {
        context.dataStore.edit { it[CURSOR_COLOR] = color }
    }
}
