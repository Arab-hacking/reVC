package com.revc.game;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Translucent multi-touch gameplay HUD for Android phones.
 * Untouched areas pass straight through to SDLSurface so menu taps and
 * free-look camera swipes work naturally alongside the virtual controls.
 */
public class TouchControlsView extends View {

    private static final class PadButton {
        final String label;
        final int keyCode;
        final boolean isHudToggle;
        final RectF rect = new RectF();
        int activePointers = 0;

        PadButton(String label, int keyCode, boolean isHudToggle) {
            this.label = label;
            this.keyCode = keyCode;
            this.isHudToggle = isHudToggle;
        }
    }

    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint activePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final List<PadButton> buttons = new ArrayList<>();
    private final PadButton hudToggle = new PadButton("HUD", 0, true);
    private final Map<Integer, PadButton> pointerButtons = new HashMap<>();
    private final Map<Integer, Boolean> passthroughPointers = new HashMap<>();

    private boolean hudVisible = true;
    private int stickPointerId = MotionEvent.INVALID_POINTER_ID;
    private float stickCenterX, stickCenterY, stickRadius;
    private float stickKnobX, stickKnobY;
    private boolean stickUp, stickDown, stickLeft, stickRight;

    private final PadButton btnGas   = new PadButton("GAS",   KeyEvent.KEYCODE_W, false);
    private final PadButton btnBrake = new PadButton("BRAKE", KeyEvent.KEYCODE_S, false);
    private final PadButton btnCar   = new PadButton("CAR",   KeyEvent.KEYCODE_F, false);
    private final PadButton btnJump  = new PadButton("JUMP",  KeyEvent.KEYCODE_SPACE, false);
    private final PadButton btnRun   = new PadButton("RUN",   KeyEvent.KEYCODE_SHIFT_LEFT, false);
    private final PadButton btnFire  = new PadButton("FIRE",  KeyEvent.KEYCODE_CTRL_LEFT, false);

    private final PadButton btnMenu  = new PadButton("MENU",  KeyEvent.KEYCODE_ESCAPE, false);
    private final PadButton btnCam   = new PadButton("CAM",   KeyEvent.KEYCODE_V, false);
    private final PadButton btnTime  = new PadButton("TIME+", KeyEvent.KEYCODE_NUMPAD_ADD, false);
    private final PadButton btnFly   = new PadButton("FLY",   KeyEvent.KEYCODE_F10, false);

    public TouchControlsView(Context context) {
        super(context);
        float density = context.getResources().getDisplayMetrics().density;

        fillPaint.setStyle(Paint.Style.FILL);
        fillPaint.setColor(Color.argb(68, 18, 22, 30));

        activePaint.setStyle(Paint.Style.FILL);
        activePaint.setColor(Color.argb(140, 68, 138, 255));

        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(Math.max(1.5f, 1.5f * density));
        strokePaint.setColor(Color.argb(135, 225, 235, 255));

        textPaint.setColor(Color.argb(215, 245, 248, 255));
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTextSize(11.5f * density);
        textPaint.setFakeBoldText(true);

        buttons.add(btnGas);
        buttons.add(btnBrake);
        buttons.add(btnCar);
        buttons.add(btnJump);
        buttons.add(btnRun);
        buttons.add(btnFire);
        buttons.add(btnMenu);
        buttons.add(btnCam);
        buttons.add(btnTime);
        buttons.add(btnFly);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        layoutControls(w, h);
    }

    private void layoutControls(int w, int h) {
        float minDim = Math.min(w, h);
        float pad = minDim * 0.035f;

        stickRadius = minDim * 0.145f;
        stickCenterX = pad + stickRadius * 1.15f;
        stickCenterY = h - pad - stickRadius * 1.10f;
        stickKnobX = stickCenterX;
        stickKnobY = stickCenterY;

        float btnSize = minDim * 0.125f;
        float gap = minDim * 0.022f;
        float rightX = w - pad - btnSize;
        float col2X = rightX - btnSize - gap;
        float col3X = col2X - btnSize - gap;
        float botY = h - pad - btnSize;
        float row2Y = botY - btnSize - gap;

        btnGas.rect.set(rightX, botY, rightX + btnSize, botY + btnSize);
        btnBrake.rect.set(col2X, botY, col2X + btnSize, botY + btnSize);
        btnJump.rect.set(col3X, botY, col3X + btnSize, botY + btnSize);

        btnFire.rect.set(rightX, row2Y, rightX + btnSize, row2Y + btnSize);
        btnRun.rect.set(col2X, row2Y, col2X + btnSize, row2Y + btnSize);
        btnCar.rect.set(col3X, row2Y, col3X + btnSize, row2Y + btnSize);

        float pillW = minDim * 0.115f;
        float pillH = minDim * 0.072f;
        float topY = pad * 0.75f;
        hudToggle.rect.set(w - pad - pillW, topY, w - pad, topY + pillH);

        float startX = pad;
        PadButton[] topRow = { btnMenu, btnCam, btnTime, btnFly };
        for (int i = 0; i < topRow.length; i++) {
            float x = startX + i * (pillW + gap * 0.8f);
            topRow[i].rect.set(x, topY, x + pillW, topY + pillH);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        drawPill(canvas, hudToggle);
        if (!hudVisible) {
            return;
        }

        // Left analog movement ring + thumb knob
        canvas.drawCircle(stickCenterX, stickCenterY, stickRadius, fillPaint);
        canvas.drawCircle(stickCenterX, stickCenterY, stickRadius, strokePaint);
        Paint knobFill = (stickPointerId != MotionEvent.INVALID_POINTER_ID) ? activePaint : fillPaint;
        canvas.drawCircle(stickKnobX, stickKnobY, stickRadius * 0.42f, knobFill);
        canvas.drawCircle(stickKnobX, stickKnobY, stickRadius * 0.42f, strokePaint);

        for (PadButton b : buttons) {
            drawPill(canvas, b);
        }
    }

    private void drawPill(Canvas canvas, PadButton b) {
        float r = Math.min(b.rect.width(), b.rect.height()) * 0.36f;
        Paint bg = b.activePointers > 0 ? activePaint : fillPaint;
        canvas.drawRoundRect(b.rect, r, r, bg);
        canvas.drawRoundRect(b.rect, r, r, strokePaint);
        Paint.FontMetrics fm = textPaint.getFontMetrics();
        float textY = b.rect.centerY() - (fm.ascent + fm.descent) * 0.5f;
        canvas.drawText(b.label, b.rect.centerX(), textY, textPaint);
    }

    private PadButton hitTestButton(float x, float y) {
        if (hudToggle.rect.contains(x, y)) {
            return hudToggle;
        }
        if (!hudVisible) {
            return null;
        }
        for (PadButton b : buttons) {
            if (b.rect.contains(x, y)) {
                return b;
            }
        }
        return null;
    }

    private boolean hitTestStick(float x, float y) {
        if (!hudVisible) {
            return false;
        }
        float dx = x - stickCenterX;
        float dy = y - stickCenterY;
        float hitR = stickRadius * 1.28f;
        return (dx * dx + dy * dy) <= (hitR * hitR);
    }

    private void updateStickDirections(float x, float y) {
        float dx = x - stickCenterX;
        float dy = y - stickCenterY;
        float dist = (float) Math.hypot(dx, dy);
        if (dist > stickRadius && dist > 1e-4f) {
            stickKnobX = stickCenterX + (dx / dist) * stickRadius;
            stickKnobY = stickCenterY + (dy / dist) * stickRadius;
        } else {
            stickKnobX = x;
            stickKnobY = y;
        }
        float dead = stickRadius * 0.28f;
        setStickKey(KeyEvent.KEYCODE_W, dy < -dead, 0);
        setStickKey(KeyEvent.KEYCODE_S, dy >  dead, 1);
        setStickKey(KeyEvent.KEYCODE_A, dx < -dead, 2);
        setStickKey(KeyEvent.KEYCODE_D, dx >  dead, 3);
    }

    private void resetStick() {
        stickPointerId = MotionEvent.INVALID_POINTER_ID;
        stickKnobX = stickCenterX;
        stickKnobY = stickCenterY;
        setStickKey(KeyEvent.KEYCODE_W, false, 0);
        setStickKey(KeyEvent.KEYCODE_S, false, 1);
        setStickKey(KeyEvent.KEYCODE_A, false, 2);
        setStickKey(KeyEvent.KEYCODE_D, false, 3);
    }

    private void setStickKey(int keyCode, boolean down, int dirIdx) {
        boolean prev = (dirIdx == 0) ? stickUp :
                       (dirIdx == 1) ? stickDown :
                       (dirIdx == 2) ? stickLeft : stickRight;
        if (prev == down) {
            return;
        }
        if (dirIdx == 0) stickUp = down;
        else if (dirIdx == 1) stickDown = down;
        else if (dirIdx == 2) stickLeft = down;
        else stickRight = down;

        if (down) {
            SDLActivity.onNativeKeyDown(keyCode);
        } else {
            SDLActivity.onNativeKeyUp(keyCode);
        }
    }

    private void pressButton(PadButton b) {
        if (b.isHudToggle) {
            return;
        }
        if (b.activePointers == 0) {
            SDLActivity.onNativeKeyDown(b.keyCode);
        }
        b.activePointers++;
    }

    private void releaseButton(PadButton b) {
        if (b == null || b.isHudToggle) {
            return;
        }
        if (b.activePointers > 0) {
            b.activePointers--;
            if (b.activePointers == 0) {
                SDLActivity.onNativeKeyUp(b.keyCode);
            }
        }
    }

    private void releaseAllControls() {
        resetStick();
        for (PadButton b : buttons) {
            if (b.activePointers > 0) {
                b.activePointers = 0;
                SDLActivity.onNativeKeyUp(b.keyCode);
            }
        }
        pointerButtons.clear();
        passthroughPointers.clear();
    }

    private void forwardTouchToSDL(MotionEvent event, int pointerIndex, int action) {
        int w = getWidth();
        int h = getHeight();
        if (w <= 0 || h <= 0) {
            return;
        }
        int touchDevId = event.getDeviceId();
        if (touchDevId < 0) {
            touchDevId -= 1;
        }
        int fingerId = event.getPointerId(pointerIndex);
        float nx = event.getX(pointerIndex) / (float) w;
        float ny = event.getY(pointerIndex) / (float) h;
        float p = Math.min(1.0f, event.getPressure(pointerIndex));
        SDLActivity.onNativeTouch(touchDevId, fingerId, action, nx, ny, p);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int idx = event.getActionIndex();

        switch (action) {
            case MotionEvent.ACTION_DOWN: {
                float x = event.getX(0);
                float y = event.getY(0);
                int pid = event.getPointerId(0);
                PadButton b = hitTestButton(x, y);
                if (b != null) {
                    if (b.isHudToggle) {
                        releaseAllControls();
                        hudVisible = !hudVisible;
                        invalidate();
                        return true;
                    }
                    pointerButtons.put(pid, b);
                    pressButton(b);
                    invalidate();
                    return true;
                }
                if (stickPointerId == MotionEvent.INVALID_POINTER_ID && hitTestStick(x, y)) {
                    stickPointerId = pid;
                    updateStickDirections(x, y);
                    invalidate();
                    return true;
                }
                // Let SDLSurface handle untouched areas directly (menu taps, single-finger look).
                return false;
            }

            case MotionEvent.ACTION_POINTER_DOWN: {
                float x = event.getX(idx);
                float y = event.getY(idx);
                int pid = event.getPointerId(idx);
                PadButton b = hitTestButton(x, y);
                if (b != null) {
                    if (b.isHudToggle) {
                        releaseAllControls();
                        hudVisible = !hudVisible;
                        invalidate();
                        return true;
                    }
                    pointerButtons.put(pid, b);
                    pressButton(b);
                    invalidate();
                    return true;
                }
                if (stickPointerId == MotionEvent.INVALID_POINTER_ID && hitTestStick(x, y)) {
                    stickPointerId = pid;
                    updateStickDirections(x, y);
                    invalidate();
                    return true;
                }
                passthroughPointers.put(pid, Boolean.TRUE);
                forwardTouchToSDL(event, idx, MotionEvent.ACTION_POINTER_DOWN);
                return true;
            }

            case MotionEvent.ACTION_MOVE: {
                boolean dirty = false;
                int count = event.getPointerCount();
                for (int i = 0; i < count; i++) {
                    int pid = event.getPointerId(i);
                    float x = event.getX(i);
                    float y = event.getY(i);
                    if (pid == stickPointerId) {
                        updateStickDirections(x, y);
                        dirty = true;
                    } else if (passthroughPointers.containsKey(pid)) {
                        forwardTouchToSDL(event, i, MotionEvent.ACTION_MOVE);
                    }
                }
                if (dirty) {
                    invalidate();
                }
                return true;
            }

            case MotionEvent.ACTION_POINTER_UP: {
                int pid = event.getPointerId(idx);
                if (pid == stickPointerId) {
                    resetStick();
                    invalidate();
                } else if (pointerButtons.containsKey(pid)) {
                    releaseButton(pointerButtons.remove(pid));
                    invalidate();
                } else if (passthroughPointers.remove(pid) != null) {
                    forwardTouchToSDL(event, idx, MotionEvent.ACTION_POINTER_UP);
                }
                return true;
            }

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                int count = event.getPointerCount();
                for (int i = 0; i < count; i++) {
                    int pid = event.getPointerId(i);
                    if (passthroughPointers.remove(pid) != null) {
                        forwardTouchToSDL(event, i, action);
                    }
                }
                releaseAllControls();
                invalidate();
                return true;
            }
        }
        return false;
    }
}
