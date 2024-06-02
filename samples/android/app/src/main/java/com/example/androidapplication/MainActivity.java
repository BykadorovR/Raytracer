package com.example.androidapplication;

import android.os.Bundle;
import android.view.View;

import com.google.androidgamesdk.GameActivity;

public class MainActivity extends GameActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
    static {
        System.loadLibrary("AndroidApplication");
    }
}