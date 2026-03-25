package com.tx.terminal;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.util.Log;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "TX_MainActivity";
    
    private SurfaceView mSurfaceView;
    private long mNativeHandle = 0;
    private RenderThread mRenderThread;
    private InputMethodManager mInputMethodManager;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Create surface view for rendering
        mSurfaceView = new SurfaceView(this);
        mSurfaceView.getHolder().addCallback(this);
        
        // Set up input handling
        mSurfaceView.setFocusable(true);
        mSurfaceView.setFocusableInTouchMode(true);
        mSurfaceView.requestFocus();
        
        // Set content view
        FrameLayout layout = new FrameLayout(this);
        layout.addView(mSurfaceView);
        setContentView(layout);
        
        // Get input method manager
        mInputMethodManager = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        
        // Show keyboard
        mSurfaceView.setOnClickListener(v -> showKeyboard());
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        
        if (mRenderThread != null) {
            mRenderThread.stopRendering();
            try {
                mRenderThread.join();
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted while waiting for render thread", e);
            }
        }
        
        if (mNativeHandle != 0) {
            TXNative.destroyTerminal(mNativeHandle);
            mNativeHandle = 0;
        }
    }
    
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "Surface created");
        
        // Create terminal
        mNativeHandle = TXNative.createTerminal(80, 24, "/system/bin/sh");
        
        if (mNativeHandle == 0) {
            Log.e(TAG, "Failed to create terminal");
            finish();
            return;
        }
        
        // Set surface
        TXNative.setSurface(mNativeHandle, holder.getSurface());
        
        // Start render thread
        mRenderThread = new RenderThread(mNativeHandle);
        mRenderThread.start();
    }
    
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, "Surface changed: " + width + "x" + height);
        
        if (mNativeHandle != 0) {
            TXNative.onResize(mNativeHandle, width, height);
        }
    }
    
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "Surface destroyed");
        
        if (mRenderThread != null) {
            mRenderThread.stopRendering();
        }
        
        if (mNativeHandle != 0) {
            TXNative.destroySurface(mNativeHandle);
        }
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mNativeHandle != 0) {
            int mods = 0;
            if (event.isShiftPressed()) mods |= 1;
            if (event.isCtrlPressed()) mods |= 2;
            if (event.isAltPressed()) mods |= 4;
            
            TXNative.sendKey(mNativeHandle, keyCode, mods, true);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
    
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mNativeHandle != 0) {
            int mods = 0;
            if (event.isShiftPressed()) mods |= 1;
            if (event.isCtrlPressed()) mods |= 2;
            if (event.isAltPressed()) mods |= 4;
            
            TXNative.sendKey(mNativeHandle, keyCode, mods, false);
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Handle touch events for selection/scrolling
        return super.onTouchEvent(event);
    }
    
    private void showKeyboard() {
        mInputMethodManager.showSoftInput(mSurfaceView, InputMethodManager.SHOW_IMPLICIT);
    }
    
    private void hideKeyboard() {
        mInputMethodManager.hideSoftInputFromWindow(mSurfaceView.getWindowToken(), 0);
    }
    
    // Render thread for continuous rendering
    private static class RenderThread extends Thread {
        private final long mHandle;
        private volatile boolean mRunning = true;
        
        RenderThread(long handle) {
            mHandle = handle;
        }
        
        void stopRendering() {
            mRunning = false;
        }
        
        @Override
        public void run() {
            while (mRunning && TXNative.isRunning(mHandle)) {
                TXNative.render(mHandle);
                
                try {
                    Thread.sleep(16);  // ~60 FPS
                } catch (InterruptedException e) {
                    break;
                }
            }
        }
    }
}
