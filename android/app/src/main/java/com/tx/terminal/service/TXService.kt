package com.tx.terminal.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import com.tx.terminal.ui.MainActivity
import com.tx.terminal.R
import com.tx.terminal.data.TerminalSession
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import java.util.concurrent.CopyOnWriteArrayList

/**
 * TX Terminal Service - Foreground service that manages terminal sessions
 *
 * Based on Termux's TermuxService architecture:
 * - Runs as foreground service to prevent being killed
 * - Survives Activity configuration changes and restarts
 * - Manages session lifecycle independently of UI
 * - Provides notification for user control
 */
class TXService : Service() {

    companion object {
        private const val TAG = "TXService"
        private const val NOTIFICATION_ID = 1337
        private const val CHANNEL_ID = "tx_service_channel"

        // Actions
        const val ACTION_START = "com.tx.terminal.action.START"
        const val ACTION_STOP = "com.tx.terminal.action.STOP"
        const val ACTION_WAKE_LOCK = "com.tx.terminal.action.WAKE_LOCK"
        const val ACTION_WAKE_UNLOCK = "com.tx.terminal.action.WAKE_UNLOCK"

        @Volatile
        private var instance: TXService? = null

        fun getInstance(): TXService? = instance

        fun start(context: Context) {
            val intent = Intent(context, TXService::class.java).apply {
                action = ACTION_START
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, TXService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }

    // Binder for local clients (Activity)
    inner class LocalBinder : Binder() {
        val service: TXService = this@TXService
    }
    private val binder = LocalBinder()

    // Service scope for coroutines
    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    // Main thread handler for callbacks
    private val mainHandler = Handler(Looper.getMainLooper())

    // Session management
    private val sessions = CopyOnWriteArrayList<TerminalSession>()
    private var nextSessionId = 1

    // Wake locks
    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    // Service state
    private var wantsToStop = false
    private var isForeground = false

    override fun onCreate() {
        super.onCreate()
        instance = this
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> startForeground()
            ACTION_STOP -> stopService()
            ACTION_WAKE_LOCK -> acquireWakeLock()
            ACTION_WAKE_UNLOCK -> releaseWakeLock()
        }

        // If service is killed, don't restart automatically - let user restart
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        return true
    }

    override fun onDestroy() {
        super.onDestroy()
        killAllSessions()
        releaseWakeLock()
        stopForeground()
        instance = null
    }

    //region Foreground Service

    private fun startForeground() {
        if (isForeground) return

        val notification = buildNotification()
        startForeground(NOTIFICATION_ID, notification)
        isForeground = true
    }

    private fun stopForeground() {
        if (!isForeground) return

        stopForeground(true)
        isForeground = false
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "TX Terminal Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Keeps terminal sessions running"
                setShowBadge(false)
            }

            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this,
            0,
            Intent(this, TXService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("TX Terminal")
            .setContentText("${sessions.size} session(s) running")
            .setSmallIcon(R.drawable.ic_menu_terminal)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Stop", stopIntent)
            .build()
    }

    fun updateNotification() {
        if (!isForeground) return

        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.notify(NOTIFICATION_ID, buildNotification())
    }

    //endregion

    //region Session Management

    fun createSession(name: String? = null, shellPath: String = "/system/bin/sh"): TerminalSession {
        val sessionId = "session_${nextSessionId++}"
        val sessionName = name ?: "Terminal $nextSessionId"

        val session = TerminalSession(
            id = sessionId,
            name = sessionName,
            shellPath = shellPath
        )

        sessions.add(session)
        session.initialize()

        updateNotification()
        return session
    }

    fun getSession(id: String): TerminalSession? {
        return sessions.find { it.id == id }
    }

    fun getAllSessions(): List<TerminalSession> = sessions.toList()

    fun getSessionCount(): Int = sessions.size

    fun closeSession(id: String) {
        val session = sessions.find { it.id == id }
        session?.let {
            it.finish()
            sessions.remove(it)
            updateNotification()
        }
    }

    fun killAllSessions() {
        sessions.forEach { it.finish() }
        sessions.clear()
        updateNotification()
    }

    private fun stopService() {
        wantsToStop = true
        killAllSessions()
        stopSelf()
    }

    //endregion

    //region Wake Lock

    fun acquireWakeLock() {
        if (wakeLock == null) {
            val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
            wakeLock = powerManager.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK,
                "TX::WakeLock"
            ).apply {
                setReferenceCounted(false)
            }
        }

        if (wifiLock == null) {
            val wifiManager = getSystemService(Context.WIFI_SERVICE) as WifiManager
            wifiLock = wifiManager.createWifiLock(
                WifiManager.WIFI_MODE_FULL_HIGH_PERF,
                "TX::WifiLock"
            ).apply {
                setReferenceCounted(false)
            }
        }

        wakeLock?.acquire(10 * 60 * 1000L) // 10 minutes timeout
        wifiLock?.acquire()

        updateNotification()
    }

    fun releaseWakeLock() {
        wakeLock?.let {
            if (it.isHeld) it.release()
        }
        wifiLock?.let {
            if (it.isHeld) it.release()
        }
        updateNotification()
    }

    fun isWakeLockHeld(): Boolean {
        return wakeLock?.isHeld == true
    }

    //endregion

    //region State

    fun wantsToStop(): Boolean = wantsToStop

    //endregion
}
