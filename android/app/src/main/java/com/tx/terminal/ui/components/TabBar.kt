package com.tx.terminal.ui.components

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.tx.terminal.data.TerminalSession

/**
 * Tab bar for switching between terminal sessions
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun TabBar(
    sessions: List<TerminalSession>,
    activeSessionId: String?,
    onSessionSelected: (String) -> Unit,
    onSessionClosed: (String) -> Unit
) {
    val listState = rememberLazyListState()
    
    // Scroll to active session
    LaunchedEffect(activeSessionId) {
        val index = sessions.indexOfFirst { it.id == activeSessionId }
        if (index >= 0) {
            listState.animateScrollToItem(index)
        }
    }
    
    Surface(
        tonalElevation = 2.dp,
        modifier = Modifier.fillMaxWidth()
    ) {
        LazyRow(
            state = listState,
            modifier = Modifier.fillMaxWidth(),
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            items(sessions, key = { it.id }) { session ->
                val isActive = session.id == activeSessionId
                val title by session.title.collectAsState()
                val isRunning by session.isRunning.collectAsState()
                
                TabItem(
                    title = title,
                    isActive = isActive,
                    isRunning = isRunning,
                    onClick = { onSessionSelected(session.id) },
                    onClose = { onSessionClosed(session.id) }
                )
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TabItem(
    title: String,
    isActive: Boolean,
    isRunning: Boolean,
    onClick: () -> Unit,
    onClose: () -> Unit
) {
    val containerColor = if (isActive) {
        MaterialTheme.colorScheme.primaryContainer
    } else {
        MaterialTheme.colorScheme.surfaceVariant
    }
    
    val contentColor = if (isActive) {
        MaterialTheme.colorScheme.onPrimaryContainer
    } else {
        MaterialTheme.colorScheme.onSurfaceVariant
    }
    
    Surface(
        color = containerColor,
        contentColor = contentColor,
        shape = MaterialTheme.shapes.small,
        modifier = Modifier
            .height(40.dp)
            .clip(MaterialTheme.shapes.small)
            .combinedClickable(
                onClick = onClick,
                onLongClick = { /* Show context menu */ }
            )
    ) {
        Row(
            modifier = Modifier.padding(start = 12.dp, end = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Running indicator
            if (!isRunning) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .background(
                            color = MaterialTheme.colorScheme.outline,
                            shape = MaterialTheme.shapes.extraSmall
                        )
                )
            }
            
            // Title
            Text(
                text = title,
                style = MaterialTheme.typography.labelLarge,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.widthIn(max = 120.dp)
            )
            
            // Close button
            IconButton(
                onClick = onClose,
                modifier = Modifier.size(24.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Close,
                    contentDescription = "Close",
                    modifier = Modifier.size(16.dp)
                )
            }
        }
    }
}
