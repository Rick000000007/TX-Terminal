package com.tx.terminal.ui

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.core.view.WindowCompat
import com.tx.terminal.service.TXService
import com.tx.terminal.ui.screens.MainScreen
import com.tx.terminal.ui.theme.TXTerminalTheme
import com.tx.terminal.viewmodel.TerminalViewModel

/**
 * Main Activity for TX Terminal.
 *
 * Based on Termux's TermuxActivity architecture:
 * - Binds to TXService for session management
 * - Handles activity lifecycle properly (onStart/onStop)
 * - Survives configuration changes
 * - Reconnects to existing sessions after restart
 */
class MainActivity : ComponentActivity() {

    private val viewModel: TerminalViewModel by viewModels()

    // Service connection
    private var txService: TXService? = null
    private var serviceBound = false

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as TXService.LocalBinder
            txService = binder.service
            serviceBound = true

            // Connect ViewModel to service
            viewModel.setService(txService)

            // Restore sessions if any exist
            restoreSessions()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            txService = null
            serviceBound = false
            viewModel.setService(null)
        }
    }

    // Activity state
    private var isVisible = false
    private var isOnResumeAfterOnCreate = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        isOnResumeAfterOnCreate = true

        // Enable edge-to-edge
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Start and bind to service
        startAndBindService()

        setContent {
            TXTerminalTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen(viewModel = viewModel)
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()

        // Bind to service if not already bound
        if (!serviceBound) {
            bindService()
        }

        // Restore current session
        txService?.let { service ->
            if (service.getSessionCount() > 0) {
                // Reconnect to existing sessions
                viewModel.syncWithService()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        isVisible = true
        isOnResumeAfterOnCreate = false

        // Notify ViewModel that activity is visible
        viewModel.onActivityVisible()
    }

    override fun onPause() {
        super.onPause()
        isVisible = false

        // Notify ViewModel that activity is not visible
        viewModel.onActivityHidden()
    }

    override fun onStop() {
        super.onStop()

        // Store current session for restoration
        viewModel.storeCurrentSession()
    }

    override fun onDestroy() {
        super.onDestroy()

        // Unbind from service
        if (serviceBound) {
            unbindService(serviceConnection)
            serviceBound = false
        }

        // Note: We don't stop the service here - it should continue running
        // The service will stop itself when all sessions are closed or user requests stop
    }

    /**
     * Check if activity is currently visible (between onResume and onPause).
     */
    fun isActivityVisible(): Boolean = isVisible

    /**
     * Start and bind to the TXService.
     */
    private fun startAndBindService() {
        TXService.start(this)
        bindService()
    }

    /**
     * Bind to the TXService.
     */
    private fun bindService() {
        val intent = Intent(this, TXService::class.java)
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
    }

    /**
     * Restore sessions from service after activity restart.
     */
    private fun restoreSessions() {
        txService?.let { service ->
            val sessions = service.getAllSessions()
            if (sessions.isNotEmpty()) {
                viewModel.restoreSessions(sessions)
            } else if (viewModel.getSessionCount() == 0) {
                // Create initial session if none exist
                viewModel.createSession("Terminal 1")
            }
        }
    }

    /**
     * Get the bound TXService instance.
     */
    fun getTXService(): TXService? = txService
}
