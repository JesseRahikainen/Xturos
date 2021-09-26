package com.xturos.game;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

public class Game extends SDLActivity {

    private static final String TAG = "AA";

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ) {
            WindowInsets windowInsets = getWindow().getDecorView().getRootWindowInsets();
            if( windowInsets == null ) return;
            DisplayCutout cutout = windowInsets.getDisplayCutout();
            if( cutout == null ) return;

            int insetTop = cutout.getSafeInsetTop( );
            int insetBottom = cutout.getSafeInsetBottom( );
            int insetLeft = cutout.getSafeInsetLeft( );
            int insetRight = cutout.getSafeInsetRight( );

            // notify the engine code that the safe area has changed
            safeAreaChanged( insetLeft, insetTop, insetRight, insetBottom );

            //Log.d(TAG, "cutout: " + cutout + "  insetBottom: " + cutout.getSafeInsetBottom() + "  insetTop: " + cutout.getSafeInsetTop() + "  insetLeft: " + cutout.getSafeInsetLeft() + "  insetRight: " + cutout.getSafeInsetRight( ) );
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate( savedInstanceState );

        // draw over the cutouts, looks nicer, just need to keep track of it in our code
        if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ) {
            getWindow( ).getAttributes( ).layoutInDisplayCutoutMode  = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ) {
            // only works for some cutouts
            getWindow( ).getAttributes( ).layoutInDisplayCutoutMode  = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        // base safe area, everything is safe
        safeAreaChanged( 0, 0, 0, 0 );
    }

    public static native void safeAreaChanged( int leftInset, int topInset, int rightInset, int bottomInset );
}