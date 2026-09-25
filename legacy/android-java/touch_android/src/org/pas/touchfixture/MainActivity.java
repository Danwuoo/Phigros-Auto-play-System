package org.pas.touchfixture;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Locale;

/** Isolated touch evidence. The colored cells encode Android-observed events in pixels. */
public final class MainActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        setContentView(new TouchView(this));
    }

    private static final class Trace {
        final int id;
        final float x, y;
        Trace(int id, float x, float y) { this.id=id; this.x=x; this.y=y; }
    }

    private static final class TouchView extends View {
        private final Paint paint = new Paint(3);
        private final HashMap<Integer, Trace> active = new HashMap<>();
        private final ArrayDeque<Trace> trail = new ArrayDeque<>();
        private int sequence, downs, ups, moves, cancels, maxConcurrent;
        private int pairs, movesWithOther, lastX, lastY, lastId, lastAction;

        TouchView(Context context) { super(context); setBackgroundColor(Color.rgb(15, 18, 28)); }

        private void addTrace(int id, float x, float y) {
            trail.addLast(new Trace(id, x, y));
            while (trail.size() > 96) trail.removeFirst();
            lastId=id; lastX=Math.round(x); lastY=Math.round(y);
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            int action = event.getActionMasked();
            int index = event.getActionIndex();
            lastAction=action;
            sequence++;
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                int id=event.getPointerId(index);
                addTrace(id, event.getX(index), event.getY(index));
                if (!active.isEmpty()) pairs++;
                active.put(id, new Trace(id, event.getX(index), event.getY(index)));
                downs++;
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
                int id=event.getPointerId(index);
                addTrace(id, event.getX(index), event.getY(index));
                active.remove(id);
                ups++;
            } else if (action == MotionEvent.ACTION_MOVE) {
                boolean changed=false;
                for (int i=0; i<event.getPointerCount(); i++) {
                    int id=event.getPointerId(i);
                    Trace old=active.get(id);
                    float x=event.getX(i), y=event.getY(i);
                    if (old == null || Math.abs(old.x-x)+Math.abs(old.y-y) > 1f) {
                        addTrace(id,x,y);
                        active.put(id,new Trace(id,x,y));
                        changed=true;
                    }
                }
                if (changed) {
                    moves++;
                    if (active.size() > 1) movesWithOther++;
                }
            } else if (action == MotionEvent.ACTION_CANCEL) {
                cancels++;
                active.clear();
            }
            maxConcurrent=Math.max(maxConcurrent,active.size());
            invalidate();
            return true;
        }

        private void cell(Canvas canvas, int index, int value) {
            paint.setColor(Color.rgb((value >>> 16)&255,(value >>> 8)&255,value&255));
            int x=12+index*30;
            canvas.drawRect(x,100,x+24,124,paint);
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            paint.setColor(Color.WHITE);
            paint.setTextSize(28);
            paint.setTypeface(android.graphics.Typeface.MONOSPACE);
            canvas.drawText("PAS TOUCH FIXTURE - Android observed (not RPC ack)",12,35,paint);
            canvas.drawText(String.format(Locale.US,
                "seq %d down %d up %d move %d cancel %d active %d max %d pair %d bothMove %d",
                sequence,downs,ups,moves,cancels,active.size(),maxConcurrent,pairs,movesWithOther),12,75,paint);
            int[] values = {0x504153,sequence,downs,ups,moves,cancels,maxConcurrent,
                            active.size(),lastX,lastY,lastId,pairs,movesWithOther,lastAction};
            for (int i=0;i<values.length;i++) cell(canvas,i,values[i]);
            paint.setColor(Color.rgb(90,90,100));
            paint.setStrokeWidth(1);
            for (int x=0;x<getWidth();x+=getWidth()/4) canvas.drawLine(x,145,x,getHeight(),paint);
            for (int y=145;y<getHeight();y+=Math.max(1,(getHeight()-145)/3)) canvas.drawLine(0,y,getWidth(),y,paint);
            paint.setColor(Color.YELLOW);
            for (Trace trace:trail) canvas.drawCircle(trace.x,trace.y,5,paint);
            for (Trace trace:active.values()) {
                paint.setColor(Color.CYAN);
                canvas.drawCircle(trace.x,trace.y,19,paint);
                paint.setColor(Color.WHITE);
                canvas.drawText("pointer " + trace.id,trace.x+22,trace.y-15,paint);
            }
        }
    }
}
