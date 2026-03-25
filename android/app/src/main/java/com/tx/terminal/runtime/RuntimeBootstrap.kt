package com.tx.terminal.runtime

import android.content.Context
import android.os.Build
import android.system.Os
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

/**
 * TX Runtime Bootstrap
 *
 * Manages the app-private Linux-style runtime environment including:
 * - HOME directory
 * - PREFIX directory (usr-like structure)
 * - TMP directory
 * - Binary/tool directories
 * - Environment variable setup
 * - BusyBox/tool bootstrap
 */
class RuntimeBootstrap(private val context: Context) {

    companion object {
        private const val TAG = "TXRuntime"

        // Directory names
        const val DIR_HOME = "home"
        const val DIR_PREFIX = "usr"
        const val DIR_TMP = "tmp"
        const val DIR_BIN = "bin"
        const val DIR_LIB = "lib"
        const val DIR_ETC = "etc"
        const val DIR_VAR = "var"
        const val DIR_SHARE = "share"

        // BusyBox binary name
        const val BUSYBOX_BINARY = "busybox"

        // Supported ABIs in priority order
        val SUPPORTED_ABIS = listOf("arm64-v8a", "armeabi-v7a", "x86_64", "x86")

        @Volatile
        private var instance: RuntimeBootstrap? = null

        fun getInstance(context: Context): RuntimeBootstrap {
            return instance ?: synchronized(this) {
                instance ?: RuntimeBootstrap(context.applicationContext).also { instance = it }
            }
        }
    }

    // Bootstrap state
    private val _bootstrapState = MutableStateFlow<BootstrapState>(BootstrapState.NotStarted)
    val bootstrapState: StateFlow<BootstrapState> = _bootstrapState

    // Runtime directories
    lateinit var homeDir: File
        private set
    lateinit var prefixDir: File
        private set
    lateinit var tmpDir: File
        private set
    lateinit var binDir: File
        private set
    lateinit var libDir: File
        private set
    lateinit var etcDir: File
        private set
    lateinit var varDir: File
        private set

    // Current ABI
    val currentAbi: String by lazy { detectAbi() }

    // Bootstrap log for diagnostics
    private val bootstrapLog = StringBuilder()

    /**
     * Detect the device's ABI
     */
    private fun detectAbi(): String {
        // Check if we're on an ARM64 device
        return when {
            Build.SUPPORTED_ABIS.contains("arm64-v8a") -> "arm64-v8a"
            Build.SUPPORTED_ABIS.contains("armeabi-v7a") -> "armeabi-v7a"
            Build.SUPPORTED_ABIS.contains("x86_64") -> "x86_64"
            Build.SUPPORTED_ABIS.contains("x86") -> "x86"
            else -> "arm64-v8a" // Default fallback
        }
    }

    /**
     * Check if the current ABI is supported
     */
    fun isAbiSupported(abi: String = currentAbi): Boolean {
        return SUPPORTED_ABIS.contains(abi)
    }

    /**
     * Bootstrap the runtime environment
     */
    suspend fun bootstrap(): Boolean = withContext(Dispatchers.IO) {
        if (_bootstrapState.value is BootstrapState.Completed) {
            log("Runtime already bootstrapped")
            return@withContext true
        }

        _bootstrapState.value = BootstrapState.InProgress(0, "Starting bootstrap...")

        try {
            // Step 1: Create directory structure
            updateProgress(10, "Creating directory structure...")
            if (!createDirectoryStructure()) {
                throw RuntimeException("Failed to create directory structure")
            }

            // Step 2: Setup environment files
            updateProgress(30, "Setting up environment files...")
            setupEnvironmentFiles()

            // Step 3: Extract bundled tools (if any)
            updateProgress(50, "Extracting bundled tools...")
            extractBundledTools()

            // Step 4: Setup BusyBox applets (if BusyBox exists)
            updateProgress(70, "Setting up BusyBox...")
            setupBusyBox()

            // Step 5: Setup shell configuration
            updateProgress(90, "Configuring shell...")
            setupShellConfiguration()

            // Step 6: Verify installation
            updateProgress(95, "Verifying installation...")
            verifyInstallation()

            updateProgress(100, "Bootstrap complete!")
            _bootstrapState.value = BootstrapState.Completed

            log("Runtime bootstrap completed successfully")
            log("HOME=$homeDir")
            log("PREFIX=$prefixDir")
            log("TMP=$tmpDir")
            log("ABI=$currentAbi")

            true
        } catch (e: Exception) {
            log("Bootstrap failed: ${e.message}")
            Log.e(TAG, "Bootstrap failed", e)
            _bootstrapState.value = BootstrapState.Failed(e.message ?: "Unknown error")
            false
        }
    }

    /**
     * Create the directory structure
     */
    private fun createDirectoryStructure(): Boolean {
        val filesDir = context.filesDir

        homeDir = File(filesDir, DIR_HOME)
        prefixDir = File(filesDir, DIR_PREFIX)
        tmpDir = File(filesDir, DIR_TMP)
        binDir = File(prefixDir, DIR_BIN)
        libDir = File(prefixDir, DIR_LIB)
        etcDir = File(prefixDir, DIR_ETC)
        varDir = File(prefixDir, DIR_VAR)

        // Create all directories
        val dirs = listOf(homeDir, prefixDir, tmpDir, binDir, libDir, etcDir, varDir)

        for (dir in dirs) {
            if (!dir.exists() && !dir.mkdirs()) {
                log("Failed to create directory: ${dir.absolutePath}")
                return false
            }
            log("Created directory: ${dir.absolutePath}")
        }

        // Set TMPDIR environment variable
        try {
            Os.setenv("TMPDIR", tmpDir.absolutePath, true)
        } catch (e: Exception) {
            log("Warning: Could not set TMPDIR: ${e.message}")
        }

        return true
    }

    /**
     * Setup environment files (e.g., profile, bashrc)
     */
    private fun setupEnvironmentFiles() {
        // Create .profile
        val profileFile = File(homeDir, ".profile")
        if (!profileFile.exists()) {
            val profileContent = buildString {
                appendLine("# TX Terminal profile")
                appendLine("export HOME=\"${homeDir.absolutePath}\"")
                appendLine("export PREFIX=\"${prefixDir.absolutePath}\"")
                appendLine("export TMPDIR=\"${tmpDir.absolutePath}\"")
                appendLine("export PATH=\"${binDir.absolutePath}:\$PATH\"")
                appendLine("export TERM=\"xterm-256color\"")
                appendLine("export COLORTERM=\"truecolor\"")
                appendLine("export SHELL=\"/system/bin/sh\"")
                appendLine("")
                appendLine("# Aliases")
                appendLine("alias ls='ls --color=auto' 2>/dev/null || alias ls='ls'")
                appendLine("alias ll='ls -la' 2>/dev/null || alias ll='ls -la'")
                appendLine("alias la='ls -A' 2>/dev/null || alias la='ls -A'")
            }
            profileFile.writeText(profileContent)
            log("Created .profile")
        }

        // Create .bashrc (if bash is available)
        val bashrcFile = File(homeDir, ".bashrc")
        if (!bashrcFile.exists()) {
            val bashrcContent = buildString {
                appendLine("# TX Terminal bash configuration")
                appendLine("[ -f ~/.profile ] && source ~/.profile")
                appendLine("")
                appendLine("# Prompt")
                appendLine("export PS1='\\[\\e[0;32m\\]\\u@\\h\\[\\e[0m\\]:\\[\\e[0;34m\\]\\w\\[\\e[0m\\]\\$ '")
            }
            bashrcFile.writeText(bashrcContent)
            log("Created .bashrc")
        }
    }

    /**
     * Extract bundled tools from assets
     */
    private fun extractBundledTools(): Boolean {
        val assetManager = context.assets

        // Check for tools in assets/tools/<abi>/
        val toolsPath = "tools/$currentAbi"

        return try {
            val tools = assetManager.list(toolsPath) ?: emptyArray()

            if (tools.isEmpty()) {
                log("No bundled tools found for ABI: $currentAbi")
                return true // Not a failure, just no tools
            }

            for (tool in tools) {
                val assetPath = "$toolsPath/$tool"
                val destFile = File(binDir, tool)

                extractAsset(assetPath, destFile)
                setExecutable(destFile)

                log("Extracted tool: $tool")
            }

            true
        } catch (e: Exception) {
            log("Warning: Could not extract bundled tools: ${e.message}")
            true // Not a critical failure
        }
    }

    /**
     * Setup BusyBox and create applet symlinks
     */
    private fun setupBusyBox(): Boolean {
        val busyBoxFile = File(binDir, BUSYBOX_BINARY)

        // Check if BusyBox exists in bin directory
        if (!busyBoxFile.exists()) {
            // Try to extract from assets
            val assetPath = "tools/$currentAbi/$BUSYBOX_BINARY"
            try {
                extractAsset(assetPath, busyBoxFile)
                setExecutable(busyBoxFile)
                log("Extracted BusyBox from assets")
            } catch (e: Exception) {
                log("BusyBox not found in assets, skipping applet setup")
                return true // Not a critical failure
            }
        }

        if (!busyBoxFile.exists() || !busyBoxFile.canExecute()) {
            log("BusyBox not available or not executable")
            return true // Not a critical failure
        }

        // Create symlinks for common applets
        val applets = listOf(
            "ls", "cat", "echo", "pwd", "cd", "mkdir", "rm", "cp", "mv",
            "ps", "top", "kill", "grep", "awk", "sed", "tar", "gzip",
            "wget", "curl", "ping", "ifconfig", "netstat", "chmod", "chown"
        )

        for (applet in applets) {
            val appletFile = File(binDir, applet)
            if (!appletFile.exists()) {
                try {
                    Os.symlink(busyBoxFile.absolutePath, appletFile.absolutePath)
                    log("Created symlink: $applet -> busybox")
                } catch (e: Exception) {
                    log("Warning: Could not create symlink for $applet: ${e.message}")
                }
            }
        }

        return true
    }

    /**
     * Setup shell configuration files in etc/
     */
    private fun setupShellConfiguration() {
        // Create terminfo directory structure
        val terminfoDir = File(shareDir, "terminfo")
        terminfoDir.mkdirs()

        // Create a basic terminfo entry for xterm-256color
        val xtermDir = File(terminfoDir, "x")
        xtermDir.mkdirs()

        log("Created terminfo directory structure")
    }

    /**
     * Verify the installation
     */
    private fun verifyInstallation(): Boolean {
        var success = true

        // Check critical directories
        val criticalDirs = listOf(homeDir, prefixDir, tmpDir, binDir)
        for (dir in criticalDirs) {
            if (!dir.exists()) {
                log("Critical directory missing: ${dir.absolutePath}")
                success = false
            }
        }

        // Log PATH for debugging
        log("PATH will be: ${binDir.absolutePath}:/system/bin")

        return success
    }

    /**
     * Get the shell path to use
     */
    fun getShellPath(): String {
        // Prefer system shell for now, but could be enhanced to use bundled shell
        return "/system/bin/sh"
    }

    /**
     * Build environment variables for a new session
     */
    fun buildEnvironment(): Map<String, String> {
        return buildMap {
            put("HOME", homeDir.absolutePath)
            put("PREFIX", prefixDir.absolutePath)
            put("TMPDIR", tmpDir.absolutePath)
            put("PATH", "${binDir.absolutePath}:/system/bin:/system/xbin")
            put("TERM", "xterm-256color")
            put("COLORTERM", "truecolor")
            put("SHELL", getShellPath())
            put("ANDROID", "1")
            put("TX_VERSION", "1.0.0")
            put("TX_ABI", currentAbi)
        }
    }

    /**
     * Get environment variables as a list of "KEY=VALUE" strings
     */
    fun getEnvironmentList(): List<String> {
        return buildEnvironment().map { (key, value) -> "$key=$value" }
    }

    /**
     * Get the bootstrap log for diagnostics
     */
    fun getBootstrapLog(): String = bootstrapLog.toString()

    /**
     * Check if bootstrap is complete
     */
    fun isBootstrapped(): Boolean = _bootstrapState.value is BootstrapState.Completed

    /**
     * Get diagnostics information
     */
    fun getDiagnostics(): String {
        return buildString {
            appendLine("=== TX Runtime Diagnostics ===")
            appendLine("ABI: $currentAbi")
            appendLine("Bootstrap State: ${_bootstrapState.value}")
            appendLine("HOME: ${homeDir.absolutePath} (exists=${homeDir.exists()})")
            appendLine("PREFIX: ${prefixDir.absolutePath} (exists=${prefixDir.exists()})")
            appendLine("TMP: ${tmpDir.absolutePath} (exists=${tmpDir.exists()})")
            appendLine("BIN: ${binDir.absolutePath} (exists=${binDir.exists()})")
            appendLine("LIB: ${libDir.absolutePath} (exists=${libDir.exists()})")
            appendLine()
            appendLine("=== Bootstrap Log ===")
            append(bootstrapLog)
        }
    }

    // Helper methods

    private fun updateProgress(percent: Int, message: String) {
        _bootstrapState.value = BootstrapState.InProgress(percent, message)
        log("[$percent%] $message")
    }

    private fun log(message: String) {
        val logLine = "[${System.currentTimeMillis()}] $message"
        bootstrapLog.appendLine(logLine)
        Log.d(TAG, message)
    }

    private fun extractAsset(assetPath: String, destFile: File) {
        context.assets.open(assetPath).use { input ->
            FileOutputStream(destFile).use { output ->
                input.copyTo(output)
            }
        }
    }

    private fun setExecutable(file: File) {
        file.setExecutable(true, false)
    }

    // Additional directories
    val shareDir: File
        get() = File(prefixDir, DIR_SHARE)
}

/**
 * Bootstrap state sealed class
 */
sealed class BootstrapState {
    object NotStarted : BootstrapState()
    data class InProgress(val percent: Int, val message: String) : BootstrapState()
    object Completed : BootstrapState()
    data class Failed(val error: String) : BootstrapState()
}
