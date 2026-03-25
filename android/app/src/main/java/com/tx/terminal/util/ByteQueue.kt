package com.tx.terminal.util

import java.nio.charset.StandardCharsets

/**
 * A circular byte buffer queue for producer-consumer I/O.
 *
 * Based on Termux's ByteQueue - provides thread-safe byte queueing between
 * the PTY reader thread and the terminal emulator/parser.
 *
 * This class is thread-safe for single producer / single consumer scenarios.
 */
class ByteQueue(size: Int) {

    private val buffer: ByteArray = ByteArray(size)
    private val capacity: Int = size

    @Volatile
    private var head: Int = 0

    @Volatile
    private var tail: Int = 0

    @Volatile
    private var isClosed: Boolean = false

    /**
     * Get the number of bytes available to read.
     */
    fun getBytesAvailable(): Int {
        return if (tail >= head) {
            tail - head
        } else {
            capacity - head + tail
        }
    }

    /**
     * Get the free space available for writing.
     */
    fun getSpaceAvailable(): Int {
        return capacity - getBytesAvailable() - 1 // -1 to distinguish empty from full
    }

    /**
     * Read bytes from the queue into the destination buffer.
     *
     * @param dst Destination buffer
     * @param block If true, blocks until at least one byte is available
     * @return Number of bytes read, or -1 if queue is closed
     */
    @Synchronized
    fun read(dst: ByteArray, block: Boolean): Int {
        while (true) {
            val available = getBytesAvailable()

            if (available > 0) {
                // Read as much as we can
                val toRead = minOf(available, dst.size)

                for (i in 0 until toRead) {
                    dst[i] = buffer[head]
                    head = (head + 1) % capacity
                }

                (this as Object).notify()
                return toRead
            }

            if (isClosed) return -1

            if (!block) return 0

            // Wait for data
            try {
                (this as Object).wait()
            } catch (e: InterruptedException) {
                Thread.currentThread().interrupt()
                return -1
            }
        }
    }

    /**
     * Write bytes to the queue from the source buffer.
     *
     * @param src Source buffer
     * @param offset Offset in source buffer
     * @param count Number of bytes to write
     * @return true if write succeeded, false if queue is closed or full
     */
    @Synchronized
    fun write(src: ByteArray, offset: Int, count: Int): Boolean {
        if (isClosed) return false

        var written = 0
        var srcOffset = offset
        var remaining = count

        while (remaining > 0) {
            val space = getSpaceAvailable()

            if (space == 0) {
                // Buffer full - we wrote what we could
                if (written > 0) {
                    (this as Object).notify()
                    return true
                }
                return false
            }

            val toWrite = minOf(space, remaining)

            for (i in 0 until toWrite) {
                buffer[tail] = src[srcOffset + i]
                tail = (tail + 1) % capacity
            }

            written += toWrite
            srcOffset += toWrite
            remaining -= toWrite
        }

        (this as Object).notify()
        return true
    }

    /**
     * Write a string to the queue as UTF-8 bytes.
     */
    fun writeString(str: String): Boolean {
        val bytes = str.toByteArray(StandardCharsets.UTF_8)
        return write(bytes, 0, bytes.size)
    }

    /**
     * Close the queue, waking any waiting threads.
     */
    @Synchronized
    fun close() {
        isClosed = true
        (this as Object).notifyAll()
    }

    /**
     * Check if the queue is closed.
     */
    fun isClosed(): Boolean = isClosed

    /**
     * Peek at the next byte without removing it.
     * @return The next byte, or -1 if queue is empty
     */
    @Synchronized
    fun peek(): Int {
        return if (getBytesAvailable() > 0) {
            buffer[head].toInt() and 0xFF
        } else {
            -1
        }
    }
}
