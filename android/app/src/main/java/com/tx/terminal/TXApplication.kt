package com.tx.terminal

import android.app.Application
import android.content.Context
import android.util.Log
import com.tx.terminal.data.TerminalPreferences
import com.tx.terminal.runtime.RuntimeBootstrap
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class TXApplication : Application() {

    companion object {
        private const val TAG = "TXApplication"
        lateinit var instance: TXApplication
            private set
    }

    val applicationScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    lateinit var preferences: TerminalPreferences
        private set
    lateinit var runtime: RuntimeBootstrap
        private set

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        instance = this
    }

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "TX Terminal starting...")
        Log.i(TAG, "ABI: ${android.os.Build.SUPPORTED_ABIS.joinToString()}")

        // Initialize preferences
        preferences = TerminalPreferences(this)

        // Initialize runtime bootstrap
        runtime = RuntimeBootstrap.getInstance(this)

        // Initialize native library
        try {
            System.loadLibrary("tx_jni")
            Log.i(TAG, "Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library", e)
        }

        // Bootstrap runtime in background
        applicationScope.launch {
            Log.i(TAG, "Starting runtime bootstrap...")
            val success = runtime.bootstrap()
            if (success) {
                Log.i(TAG, "Runtime bootstrap completed successfully")
                Log.i(TAG, "HOME: ${runtime.homeDir.absolutePath}")
                Log.i(TAG, "PREFIX: ${runtime.prefixDir.absolutePath}")
            } else {
                Log.e(TAG, "Runtime bootstrap failed")
                Log.e(TAG, runtime.getBootstrapLog())
            }
        }
    }
}
