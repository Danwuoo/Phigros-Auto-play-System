package org.pas.capturefixture;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Bundle;
import android.view.View;

/** Pure-pixel test source. No network, storage, game reads, or input handling. */
public final class MainActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        setContentView(new FixtureView(this));
    }

    private static final class FixtureView extends View {
        private final Paint paint = new Paint();
        private int counter;

        FixtureView(Context context) { super(context); }

        private void rectangle(Canvas canvas, int color, float x, float y, float w, float h) {
            paint.setColor(color);
            canvas.drawRect(x, y, x + w, y + h, paint);
        }

        @Override protected void onDraw(Canvas canvas) {
            int width = getWidth();
            int height = getHeight();
            canvas.drawColor(Color.rgb(16, 16, 16));
            rectangle(canvas, Color.RED, 0, 0, 50, 50);
            rectangle(canvas, Color.GREEN, width - 50, 0, 50, 50);
            rectangle(canvas, Color.BLUE, 0, height - 50, 50, 50);
            rectangle(canvas, Color.WHITE, width - 50, height - 50, 50, 50);
            paint.setColor(Color.WHITE);
            paint.setTextSize(48);
            paint.setTypeface(android.graphics.Typeface.MONOSPACE);
            canvas.drawText(String.format(java.util.Locale.US, "PAS %08d", counter), 70, 65, paint);
            for (int row = 0; row < 2; row++) {
                int y = 90 + row * 26;
                rectangle(canvas, Color.RED, 10, y, 12, 12);
                for (int bit = 0; bit < 12; bit++) {
                    int color = ((counter & (1 << (row * 12 + bit))) != 0) ? Color.WHITE : Color.BLACK;
                    rectangle(canvas, color, 30 + bit * 14, y, 12, 12);
                }
                rectangle(canvas, Color.RED, 200, y, 12, 12);
            }
            int travel = Math.max(1, width - 80);
            rectangle(canvas, Color.YELLOW, 40 + (counter * 7) % travel,
                      height / 2f - 25, 40, 50);
            counter = (counter + 1) & 0xFFFFFF;
            postInvalidateOnAnimation();
        }
    }
}
