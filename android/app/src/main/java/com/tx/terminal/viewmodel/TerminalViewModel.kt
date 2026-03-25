package com.tx.terminal.viewmodel

import android.app.Application
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.tx.terminal.TXApplication
import com.tx.terminal.data.TerminalSession
import com.tx.terminal.service.TXService
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*

private const val TAG = "TerminalViewModel"

/**
 * Terminal ViewModel - Manages terminal UI state and coordinates with TXService.
 *
 * Based on Termux's architecture:
 * - Works with TXService for session persistence
 * - Handles activity lifecycle (visible/hidden)
 * - Manages UI state separately from session state
 * - Survives configuration changes
 */
class TerminalViewModel(application: Application) : AndroidViewModel(application) {

    private val preferences = TXApplication.instance.preferences

    // Service reference
    private var txService: TXService? = null

    // Session management
    private val _sessions = MutableStateFlow<List<TerminalSession>>(emptyList())
    val sessions: StateFlow<List<TerminalSession>> = _sessions.asStateFlow()

    private val _activeSessionId = MutableStateFlow<String?>(null)
    val activeSessionId: StateFlow<String?> = _activeSessionId.asStateFlow()

    val activeSession: TerminalSession?
        get() = _sessions.value.find { it.id == _activeSessionId.value }

    // UI state
    private val _showExtraKeys = MutableStateFlow(true)
    val showExtraKeys: StateFlow<Boolean> = _showExtraKeys.asStateFlow()

    private val _isKeyboardVisible = MutableStateFlow(false)
    val isKeyboardVisible: StateFlow<Boolean> = _isKeyboardVisible.asStateFlow()

    private val _fontSize = MutableStateFlow(14f)
    val fontSize: StateFlow<Float> = _fontSize.asStateFlow()

    private val _backgroundColor = MutableStateFlow(0xFF000000.toInt())
    val backgroundColor: StateFlow<Int> = _backgroundColor.asStateFlow()

    private val _foregroundColor = MutableStateFlow(0xFFFFFFFF.toInt())
    val foregroundColor: StateFlow<Int> = _foregroundColor.asStateFlow()

    private val _cursorColor = MutableStateFlow(0xFFFFFFFF.toInt())
    val cursorColor: StateFlow<Int> = _cursorColor.asStateFlow()

    // Activity visibility state
    private val _isActivityVisible = MutableStateFlow(false)
    val isActivityVisible: StateFlow<Boolean> = _isActivityVisible.asStateFlow()

    init {
        Log.d(TAG, "TerminalViewModel initialized")

        // Collect preferences
        viewModelScope.launch {
            preferences.showExtraKeys.collect { _showExtraKeys.value = it }
        }

        viewModelScope.launch {
            preferences.fontSize.collect { _fontSize.value = it }
        }

        viewModelScope.launch {
            preferences.backgroundColor.collect { _backgroundColor.value = it }
        }

        viewModelScope.launch {
            preferences.foregroundColor.collect { _foregroundColor.value = it }
        }

        viewModelScope.launch {
            preferences.cursorColor.collect { _cursorColor.value = it }
        }
    }

    //region Service Integration

    /**
     * Set the TXService reference.
     */
    fun setService(service: TXService?) {
        txService = service
        if (service != null) {
            syncWithService()
        }
    }

    /**
     * Sync sessions with the service.
     */
    fun syncWithService() {
        txService?.let { service ->
            val serviceSessions = service.getAllSessions()
            _sessions.value = serviceSessions

            // If no active session, set first one
            if (_activeSessionId.value == null && serviceSessions.isNotEmpty()) {
                _activeSessionId.value = serviceSessions.first().id
            }
        }
    }

    /**
     * Restore sessions after activity restart.
     */
    fun restoreSessions(restoredSessions: List<TerminalSession>) {
        _sessions.value = restoredSessions
        if (_activeSessionId.value == null && restoredSessions.isNotEmpty()) {
            _activeSessionId.value = restoredSessions.first().id
        }
    }

    //endregion

    //region Activity Lifecycle

    /**
     * Called when activity becomes visible.
     */
    fun onActivityVisible() {
        _isActivityVisible.value = true
        // Refresh all sessions
        _sessions.value.forEach { session ->
            session.onScreenUpdate?.invoke()
        }
    }

    /**
     * Called when activity is hidden.
     */
    fun onActivityHidden() {
        _isActivityVisible.value = false
    }

    /**
     * Store current session for restoration.
     */
    fun storeCurrentSession() {
        // Store in preferences if needed
        _activeSessionId.value?.let { sessionId ->
            viewModelScope.launch {
                preferences.setLastSessionId(sessionId)
            }
        }
    }

    //endregion

    //region Session Management

    /**
     * Create a new terminal session.
     */
    fun createSession(name: String = "Terminal"): TerminalSession {
        val shellPath = preferences.getShellPathSync()

        // Create through service if available
        val session = txService?.createSession(name, shellPath)
            ?: createLocalSession(name, shellPath)

        // Add to local list
        _sessions.value += session

        // Make it active
        _activeSessionId.value = session.id

        // Setup callbacks
        setupSessionCallbacks(session)

        return session
    }

    /**
     * Create a session locally (fallback if service not available).
     */
    private fun createLocalSession(name: String, shellPath: String): TerminalSession {
        val session = TerminalSession(
            id = "session_${System.currentTimeMillis()}",
            name = name,
            shellPath = shellPath
        )
        session.initialize()
        return session
    }

    /**
     * Setup session callbacks.
     */
    private fun setupSessionCallbacks(session: TerminalSession) {
        session.onScreenUpdate = {
            if (_isActivityVisible.value) {
                // Trigger UI update
            }
        }

        session.onSessionFinished = { exitCode ->
            viewModelScope.launch {
                Log.d(TAG, "Session ${session.id} finished with exit code: $exitCode")
                // Auto-remove session if exit code is 0 or 130 (Ctrl+C)
                if (exitCode == 0 || exitCode == 130) {
                    closeSession(session.id)
                }
            }
        }
    }

    /**
     * Switch to a session.
     */
    fun switchToSession(sessionId: String) {
        if (_sessions.value.any { it.id == sessionId }) {
            _activeSessionId.value = sessionId
        }
    }

    /**
     * Close a session.
     */
    fun closeSession(sessionId: String) {
        val session = _sessions.value.find { it.id == sessionId }
        session?.let {
            // Finish the session
            it.finish()

            // Remove from list
            _sessions.value = _sessions.value.filter { s -> s.id != sessionId }

            // Close through service if available
            txService?.closeSession(sessionId)

            // Switch to another session if this was active
            if (_activeSessionId.value == sessionId) {
                _activeSessionId.value = _sessions.value.firstOrNull()?.id
            }
        }
    }

    /**
     * Close all sessions.
     */
    fun closeAllSessions() {
        _sessions.value.forEach { it.finish() }
        _sessions.value = emptyList()
        _activeSessionId.value = null

        txService?.killAllSessions()
    }

    /**
     * Rename a session.
     */
    fun renameSession(sessionId: String, newName: String) {
        // Note: Session name is immutable in current implementation
        // Would need to add setter to TerminalSession
    }

    /**
     * Get session count.
     */
    fun getSessionCount(): Int = _sessions.value.size

    //endregion

    //region Input

    /**
     * Send text to active session.
     */
    fun sendText(text: String) {
        activeSession?.sendText(text)
    }

    /**
     * Send key event to active session.
     */
    fun sendKey(keyCode: Int, modifiers: Int = 0, pressed: Boolean = true) {
        activeSession?.sendKey(keyCode, modifiers, pressed)
    }

    /**
     * Send character to active session.
     */
    fun sendChar(codepoint: Int) {
        activeSession?.sendChar(codepoint)
    }

    /**
     * Send Ctrl+C (interrupt).
     */
    fun sendInterrupt() {
        activeSession?.sendInterrupt()
    }

    /**
     * Send Ctrl+D (EOF).
     */
    fun sendEOF() {
        activeSession?.sendEOF()
    }

    /**
     * Clear screen.
     */
    fun clearScreen() {
        activeSession?.clearScreen()
    }

    //endregion

    //region Clipboard

    /**
     * Copy selection to clipboard.
     */
    fun copyToClipboard(): String {
        val text = activeSession?.copySelection() ?: ""
        if (text.isNotEmpty()) {
            val clipboard = getApplication<Application>().getSystemService(
                Context.CLIPBOARD_SERVICE
            ) as ClipboardManager
            clipboard.setPrimaryClip(ClipData.newPlainText("Terminal", text))
        }
        return text
    }

    /**
     * Paste from clipboard.
     */
    fun pasteFromClipboard() {
        val clipboard = getApplication<Application>().getSystemService(
            Context.CLIPBOARD_SERVICE
        ) as ClipboardManager

        clipboard.primaryClip?.getItemAt(0)?.text?.let { text ->
            activeSession?.paste(text.toString())
        }
    }

    //endregion

    //region UI Settings

    /**
     * Toggle extra keys visibility.
     */
    fun toggleExtraKeys() {
        _showExtraKeys.value = !_showExtraKeys.value
        viewModelScope.launch {
            preferences.setShowExtraKeys(_showExtraKeys.value)
        }
    }

    /**
     * Set keyboard visibility.
     */
    fun setKeyboardVisible(visible: Boolean) {
        _isKeyboardVisible.value = visible
    }

    /**
     * Increase font size.
     */
    fun increaseFontSize() {
        val newSize = (_fontSize.value + 1f).coerceAtMost(32f)
        _fontSize.value = newSize
        viewModelScope.launch {
            preferences.setFontSize(newSize)
        }
    }

    /**
     * Decrease font size.
     */
    fun decreaseFontSize() {
        val newSize = (_fontSize.value - 1f).coerceAtLeast(8f)
        _fontSize.value = newSize
        viewModelScope.launch {
            preferences.setFontSize(newSize)
        }
    }

    /**
     * Reset font size.
     */
    fun resetFontSize() {
        val defaultSize = com.tx.terminal.data.TerminalPreferences.Defaults.FONT_SIZE
        _fontSize.value = defaultSize
        viewModelScope.launch {
            preferences.setFontSize(defaultSize)
        }
    }

    /**
     * Resize terminal.
     */
    fun resize(columns: Int, rows: Int) {
        activeSession?.resize(columns, rows)
    }

    //endregion

    override fun onCleared() {
        super.onCleared()
        Log.d(TAG, "TerminalViewModel cleared")
        // Note: Don't close sessions here - they should persist in the service
    }
}
