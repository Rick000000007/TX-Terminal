package com.tx.terminal.ui.components

import android.view.KeyEvent
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.ContentPaste
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowLeft
import androidx.compose.material.icons.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp

@Composable
fun ExtraKeysBar(
    onKeyPressed: (Int, Int) -> Unit,
    onCtrlC: () -> Unit,
    onCtrlD: () -> Unit,
    onCopy: () -> Unit,
    onPaste: () -> Unit
) {
    val scrollState = rememberScrollState()

    Surface(
        tonalElevation = 3.dp,
        modifier = Modifier.fillMaxWidth()
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(scrollState)
                .padding(horizontal = 4.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            ExtraKeyButton(
                text = "ESC",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_ESCAPE, 0) }
            )
            ExtraKeyButton(
                text = "TAB",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_TAB, 0) }
            )

            VerticalSeparator(modifier = Modifier.height(24.dp))

            ExtraKeyIcon(
                icon = Icons.Default.KeyboardArrowUp,
                onClick = { onKeyPressed(KeyEvent.KEYCODE_DPAD_UP, 0) }
            )
            ExtraKeyIcon(
                icon = Icons.Default.KeyboardArrowDown,
                onClick = { onKeyPressed(KeyEvent.KEYCODE_DPAD_DOWN, 0) }
            )
            ExtraKeyIcon(
                icon = Icons.Default.KeyboardArrowLeft,
                onClick = { onKeyPressed(KeyEvent.KEYCODE_DPAD_LEFT, 0) }
            )
            ExtraKeyIcon(
                icon = Icons.Default.KeyboardArrowRight,
                onClick = { onKeyPressed(KeyEvent.KEYCODE_DPAD_RIGHT, 0) }
            )

            VerticalSeparator(modifier = Modifier.height(24.dp))

            ExtraKeyButton(
                text = "HOME",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_MOVE_HOME, 0) }
            )
            ExtraKeyButton(
                text = "END",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_MOVE_END, 0) }
            )
            ExtraKeyButton(
                text = "PGUP",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_PAGE_UP, 0) }
            )
            ExtraKeyButton(
                text = "PGDN",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_PAGE_DOWN, 0) }
            )

            VerticalSeparator(modifier = Modifier.height(24.dp))

            ExtraKeyButton(
                text = "^C",
                onClick = onCtrlC,
                isAccent = true
            )
            ExtraKeyButton(
                text = "^D",
                onClick = onCtrlD,
                isAccent = true
            )
            ExtraKeyButton(
                text = "^Z",
                onClick = { onKeyPressed(KeyEvent.KEYCODE_Z, KeyEvent.META_CTRL_ON) }
            )

            VerticalSeparator(modifier = Modifier.height(24.dp))

            ExtraKeyIcon(
                icon = Icons.Default.ContentCopy,
                onClick = onCopy
            )
            ExtraKeyIcon(
                icon = Icons.Default.ContentPaste,
                onClick = onPaste
            )
        }
    }
}

@Composable
private fun ExtraKeyButton(
    text: String,
    onClick: () -> Unit,
    isAccent: Boolean = false
) {
    Button(
        onClick = onClick,
        modifier = Modifier.height(36.dp),
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 0.dp),
        shape = MaterialTheme.shapes.small,
        colors = ButtonDefaults.buttonColors(
            containerColor = if (isAccent) {
                MaterialTheme.colorScheme.primaryContainer
            } else {
                MaterialTheme.colorScheme.surfaceVariant
            },
            contentColor = if (isAccent) {
                MaterialTheme.colorScheme.onPrimaryContainer
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            }
        )
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelMedium
        )
    }
}

@Composable
private fun ExtraKeyIcon(
    icon: ImageVector,
    onClick: () -> Unit
) {
    FilledTonalIconButton(
        onClick = onClick,
        modifier = Modifier.size(36.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(20.dp)
        )
    }
}

@Composable
private fun VerticalSeparator(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .width(1.dp)
            .background(MaterialTheme.colorScheme.outlineVariant)
    )
}
