package com.tx.terminal.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.tx.terminal.ui.components.*
import com.tx.terminal.viewmodel.TerminalViewModel
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(viewModel: TerminalViewModel) {
    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)
    val scope = rememberCoroutineScope()

    var showNewSessionDialog by remember { mutableStateOf(false) }
    var showSettingsDialog by remember { mutableStateOf(false) }

    val sessions by viewModel.sessions.collectAsState()
    val activeSessionId by viewModel.activeSessionId.collectAsState()
    val showExtraKeys by viewModel.showExtraKeys.collectAsState()
    val activeSession = viewModel.activeSession
    val debugScreen = activeSession?.getScreenContent() ?: ""

    // ✅ FIXED: Proper StateFlow handling
    val activeSessionTitle =
        activeSession?.title?.collectAsState()?.value ?: "TX Terminal"

    ModalNavigationDrawer(
        drawerState = drawerState,
        drawerContent = {
            ModalDrawerSheet {
                Text(
                    "TX Terminal",
                    modifier = Modifier.padding(16.dp),
                    style = MaterialTheme.typography.titleLarge
                )
                Divider()

                NavigationDrawerItem(
                    icon = { Icon(Icons.Default.Add, contentDescription = null) },
                    label = { Text("New Session") },
                    selected = false,
                    onClick = {
                        showNewSessionDialog = true
                        scope.launch { drawerState.close() }
                    }
                )

                NavigationDrawerItem(
                    icon = { Icon(Icons.Default.Settings, contentDescription = null) },
                    label = { Text("Settings") },
                    selected = false,
                    onClick = {
                        showSettingsDialog = true
                        scope.launch { drawerState.close() }
                    }
                )

                Divider(modifier = Modifier.padding(vertical = 8.dp))

                Text(
                    "Sessions",
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    style = MaterialTheme.typography.labelSmall
                )

                sessions.forEach { session ->
                    val sessionTitle by session.title.collectAsState()
                    val sessionRunning by session.isRunning.collectAsState()

                    NavigationDrawerItem(
                        icon = {
                            Icon(
                                if (session.id == activeSessionId)
                                    Icons.Default.Terminal
                                else
                                    Icons.Default.Circle,
                                contentDescription = null
                            )
                        },
                        label = { Text(sessionTitle) },
                        selected = session.id == activeSessionId,
                        onClick = {
                            viewModel.switchToSession(session.id)
                            scope.launch { drawerState.close() }
                        },
                        badge = {
                            if (!sessionRunning) {
                                Text("")
                            }
                        }
                    )
                }
            }
        }
    ) {
        Scaffold(
            modifier = Modifier.nestedScroll(scrollBehavior.nestedScrollConnection),
            topBar = {
                TopAppBar(
                    title = {
                        Text(text = activeSessionTitle) // ✅ FIXED
                    },
                    navigationIcon = {
                        IconButton(onClick = { scope.launch { drawerState.open() } }) {
                            Icon(Icons.Default.Menu, contentDescription = "Menu")
                        }
                    },
                    actions = {
                        if (sessions.size > 1) {
                            IconButton(onClick = {
                                val currentIndex = sessions.indexOfFirst { it.id == activeSessionId }
                                val nextIndex = (currentIndex + 1) % sessions.size
                                viewModel.switchToSession(sessions[nextIndex].id)
                            }) {
                                Icon(Icons.Default.SwitchLeft, contentDescription = "Switch Tab")
                            }
                        }

                        var menuExpanded by remember { mutableStateOf(false) }
                        IconButton(onClick = { menuExpanded = true }) {
                            Icon(Icons.Default.MoreVert, contentDescription = "More")
                        }

                        DropdownMenu(
                            expanded = menuExpanded,
                            onDismissRequest = { menuExpanded = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text("New Session") },
                                onClick = {
                                    showNewSessionDialog = true
                                    menuExpanded = false
                                },
                                leadingIcon = { Icon(Icons.Default.Add, null) }
                            )

                            DropdownMenuItem(
                                text = { Text("Close Session") },
                                onClick = {
                                    activeSessionId?.let { viewModel.closeSession(it) }
                                    menuExpanded = false
                                },
                                leadingIcon = { Icon(Icons.Default.Close, null) }
                            )
                        }
                    }
                )
            }
        ) { paddingValues ->

            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(paddingValues)
            ) {

                if (sessions.size > 1) {
                    TabBar(
                        sessions = sessions,
                        activeSessionId = activeSessionId,
                        onSessionSelected = { viewModel.switchToSession(it) },
                        onSessionClosed = { viewModel.closeSession(it) }
                    )
                }

                Box(modifier = Modifier.weight(1f)) {
                    TerminalSurface(
                        viewModel = viewModel,
                        modifier = Modifier.fillMaxSize()
                    )

                    // Debug overlay
                    Text(
                        text = debugScreen.ifEmpty { "<empty screen buffer>" },
                        color = Color.White,
                        modifier = Modifier
                            .fillMaxSize()
                            .background(Color.Black.copy(alpha = 0.85f))
                            .verticalScroll(rememberScrollState())
                            .padding(8.dp)
                    )
                }

                if (showExtraKeys) {
                    ExtraKeysBar(
                        onKeyPressed = { key, modifiers ->
                            viewModel.sendKey(key, modifiers, true)
                        },
                        onCtrlC = { viewModel.sendInterrupt() },
                        onCtrlD = { viewModel.sendEOF() },
                        onCopy = { viewModel.copyToClipboard() },
                        onPaste = { viewModel.pasteFromClipboard() }
                    )
                }
            }
        }
    }

    if (showNewSessionDialog) {
        NewSessionDialog(
            onDismiss = { showNewSessionDialog = false },
            onConfirm = {
                viewModel.createSession(it)
                showNewSessionDialog = false
            }
        )
    }

    if (showSettingsDialog) {
        SettingsDialog(
            viewModel = viewModel,
            onDismiss = { showSettingsDialog = false }
        )
    }
}
