package com.example.airmouse;

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Trace;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity implements SensorEventListener {

    private static final int DEFAULT_PORT = 5000;

    private static final int GYROSCOPE_CALIBRATION_SAMPLES = 100;
    private static final int MAGNETOMETER_CALIBRATION_SAMPLES = 1000;
    private static final int ACCEL_SAMPLES_PER_STEP = 100;
    private static final float GRAVITY_EARTH = 9.81f;

    // Gesture detection thresholds and timing values
    private static final float CLICK_GYROSCOPE_THRESHOLD = 2.5f;
    private static final long CLICK_TIMEOUT_MS = 600;
    private static final long SCROLL_COOLDOWN_MS = 80;

    // Acceleration thresholds used to detect push/pull scrolling gestures
    private static final float SCROLL_ACCEL_THRESHOLD = 1.8f;
    private static final float PUSH_PULL_THRESHOLD = 1.5f;
    private static final float ALPHA_GRAVITY = 0.8f;

    // Mouse movement sensitivity gains
    private static final double GAIN_X = 3500.0;
    private static final double GAIN_Y = 3500.0;

    // Android sensor components
    private SensorManager sensorManager;
    private Sensor accelerometer;
    private Sensor gyroscope;
    private Sensor magnetometer;

    private boolean hasAccelerometer = false;
    private boolean hasGyroscope = false;
    private boolean hasMagnetometer = false;

    private boolean hasAccelerometerData = false;
    private boolean hasGyroscopeData = false;
    private boolean hasMagnetometerData = false;

    // Calibration state flags
    private boolean isGyroscopeCalibrating = false;
    private boolean isMagnetometerCalibrating = false;

    //Six-position accelerometer calibration state
    private boolean isAccelerometerCalibrating = false;
    private int accelCalibStep = 0;
    private boolean isCollectingAccel = false;
    private int accelCollectionCount = 0;
    private final float[] accelStepSums = new float[3];
    private final float[][] accelStepAverages = new float[6][3];

    // Gyroscope calibration data
    private final List<float[]> gyroscopeSamples = new ArrayList<>();
    private final float[] gyroscopeBias = new float[3];

    // Magnetometer calibration data
    private int magnetometerSampleCount = 0;
    private final float[] magnetometerMin = {Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE};
    private final float[] magnetometerMax = {-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE};
    private final float[] magnetometerOffset = new float[3];
    private final float[] magnetometerScale = {1.0f, 1.0f, 1.0f};

    // Accelerometer calibration data
    private final float[] accelerometerOffset = new float[3];
    private final float[] accelerometerScale = {1.0f, 1.0f, 1.0f};

    // Latest accelerometer and gravity-filtered values
    private final float[] accelerometerCurrent = new float[3];
    private final float[] gravity = new float[3];

    // Runtime timing state
    private long lastTimestamp = 0;
    private long lastClickTime = 0;
    private long scrollCooldownTime = 0;

    // Temporarily blocks cursor movement after click or scroll gestures
    private long gestureLockoutTime = 0;

    // Prevents accidental opposite-direction scroll during hand recovery
    private long oppositeDirectionLockTime = 0;
    private int lastScrollDirection = 0;
    private static final long OPPOSITE_DIRECTION_TIMEOUT_MS = 350;

    private float currentVisualX = 0.0f;
    private float currentVisualY = 0.0f;

    private NetworkClient networkClient;
    private boolean isMouseRunning = false;

    // UI Elements
    private View mainScreenView;
    private View calibrationScreenView;
    private View visualPointerView;
    private EditText ipInput;
    private EditText portInput;
    private Button openCalibrationButton;
    private Button closeCalibrationButton;
    private Button startMouseButton;
    private Button calibrateGyroscopeButton;
    private Button calibrateAccelerometerButton;
    private Button calibrateMagnetometerButton;
    private TextView gyroscopeTextView;
    private TextView accelerometerTextView;
    private TextView magnetometerTextView;
    private TextView actionIndicatorTextView;
    private TextView calibrationStatusTextView;

    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private Runnable resetVisualRunnable;

    // Android lifecycle
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        bindViews();
        setupSensors();
        setupUiActions();
        checkAvailableSensors();
        resetActionIndicator();
    }

    @Override
    protected void onResume() {
        super.onResume();
        registerAvailableSensors();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
        stopMouseClient();
    }

    // Connect Java fields to their corresponding UI elements.
    private void bindViews() {
        mainScreenView = findViewById(R.id.mainScreen);
        calibrationScreenView = findViewById(R.id.calibrationScreen);
        visualPointerView = findViewById(R.id.visualPointer);

        ipInput = findViewById(R.id.ipInput);
        portInput = findViewById(R.id.portInput);

        openCalibrationButton = findViewById(R.id.btnOpenCalibration);
        closeCalibrationButton = findViewById(R.id.btnCloseCalibration);
        startMouseButton = findViewById(R.id.btnStartMouse);

        calibrateGyroscopeButton = findViewById(R.id.btnCalibrateGyro);
        calibrateAccelerometerButton = findViewById(R.id.btnCalibrateAccel);
        calibrateMagnetometerButton = findViewById(R.id.btnCalibrateMag);

        gyroscopeTextView = findViewById(R.id.txtGyro);
        accelerometerTextView = findViewById(R.id.txtAccel);
        magnetometerTextView = findViewById(R.id.txtMag);

        actionIndicatorTextView = findViewById(R.id.txtActionIndicator);
        calibrationStatusTextView = findViewById(R.id.txtCalibrationStatus);

        resetVisualRunnable = this::resetActionIndicator;
    }

    // Initialize the required raw Android sensors.
    private void setupSensors() {
        sensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);

        if (sensorManager == null) {
            setCalibrationStatus("Status: Sensor service unavailable", R.color.error_red);
            disableAllSensorButtons();
            return;
        }

        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);

        hasAccelerometer = accelerometer != null;
        hasGyroscope = gyroscope != null;
        hasMagnetometer = magnetometer != null;
    }

    // Register all button actions used by the main and calibration screens.
    private void setupUiActions() {
        openCalibrationButton.setOnClickListener(v -> {
            mainScreenView.setVisibility(View.GONE);
            calibrationScreenView.setVisibility(View.VISIBLE);
        });

        closeCalibrationButton.setOnClickListener(v -> {
            calibrationScreenView.setVisibility(View.GONE);
            mainScreenView.setVisibility(View.VISIBLE);
        });

        startMouseButton.setOnClickListener(v -> {
            if (!isMouseRunning) {
                startMouseClient();
            } else {
                stopMouseClient();
            }
        });

        calibrateGyroscopeButton.setOnClickListener(v -> startGyroscopeCalibration());
        calibrateMagnetometerButton.setOnClickListener(v -> startMagnetometerCalibration());

        calibrateAccelerometerButton.setOnClickListener(v -> {
            if (!hasAccelerometer) return;
            if (!isAccelerometerCalibrating) {
                resetAllCalibrationStates();
                isAccelerometerCalibrating = true;
                accelCalibStep = 1;
                isCollectingAccel = false;
                calibrateAccelerometerButton.setText("RECORD 1/6");
                setCalibrationStatus("Step 1/6: Place screen UP. Keep stable & press RECORD.", R.color.warning_orange);
            } else {
                if (!isCollectingAccel) {
                    isCollectingAccel = true;
                    accelCollectionCount = 0;
                    accelStepSums[0] = accelStepSums[1] = accelStepSums[2] = 0;
                    setCalibrationStatus("Recording Step " + accelCalibStep + " ... Hold still!", R.color.warning_orange);
                }
            }
        });
    }

    // Check whether the minimum required sensors are available.
    private void checkAvailableSensors() {
        if (!hasGyroscope || !hasAccelerometer) {
            setCalibrationStatus("Status: Missing essential sensors", R.color.error_red);
            startMouseButton.setEnabled(false);
            startMouseButton.setAlpha(0.5f);
        } else {
            setCalibrationStatus("Status: Ready", R.color.success_green);
        }
    }

    // Register all available sensors using GAME delay for smoother motion tracking.
    private void registerAvailableSensors() {
        if (sensorManager == null) return;
        if (hasAccelerometer) sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
        if (hasGyroscope) sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME);
        if (hasMagnetometer) sensorManager.registerListener(this, magnetometer, SensorManager.SENSOR_DELAY_GAME);
    }

    // Disable all sensor-related buttons when required sensors are missing.
    private void disableAllSensorButtons() {
        calibrateGyroscopeButton.setEnabled(false);
        calibrateAccelerometerButton.setEnabled(false);
        calibrateMagnetometerButton.setEnabled(false);
        startMouseButton.setEnabled(false);
    }

    // Parse the port entered by the user and falls back to the default port if invalid.
    private int parsePortOrSetDefault() {
        try {
            int port = Integer.parseInt(portInput.getText().toString().trim());
            return (port > 0 && port <= 65535) ? port : DEFAULT_PORT;
        } catch (Exception e) {
            return DEFAULT_PORT;
        }
    }

    // Validate a simple IPv4 address format
    private boolean isIPv4Valid(String ipAddress) {
        if (ipAddress == null || ipAddress.isEmpty()) return false;
        String[] parts = ipAddress.split("\\.");
        if (parts.length != 4) return false;
        for (String part : parts) {
            try {
                int value = Integer.parseInt(part);
                if (value < 0 || value > 255) return false;
            } catch (NumberFormatException e) {
                return false;
            }
        }
        return true;
    }

    // Start the UDP client that sends mouse commands to the laptop
    private void startMouseClient() {
        String ipAddress = ipInput.getText().toString().trim();
        int port = parsePortOrSetDefault();

        if (!isIPv4Valid(ipAddress)) {
            setAction("Action: Invalid laptop IP", R.color.error_red);
            setCalibrationStatus("Status: Enter a valid IPv4 address", R.color.error_red);
            return;
        }

        networkClient = new NetworkClient(ipAddress, port);
        networkClient.start();
        isMouseRunning = true;

        startMouseButton.setText("STOP MOUSE");
        startMouseButton.setBackgroundResource(R.drawable.bg_button_error);
        setAction("Action: Mouse started", R.color.success_green);
    }

    // Stop the UDP client and resets the mouse state
    private void stopMouseClient() {
        if (networkClient != null) {
            networkClient.stop();
            networkClient = null;
        }
        isMouseRunning = false;

        startMouseButton.setText("START MOUSE");
        startMouseButton.setBackgroundResource(R.drawable.bg_button_success);
        setAction("Action: Mouse stopped", R.color.warning_orange);
    }

    // Ensure that only one calibration process runs at a time
    private void resetAllCalibrationStates() {
        isGyroscopeCalibrating = false;
        isAccelerometerCalibrating = false;
        isMagnetometerCalibrating = false;
    }

    // Start gyroscope bias calibration while the phone is kept still.
    private void startGyroscopeCalibration() {
        resetAllCalibrationStates();
        gyroscopeSamples.clear();
        isGyroscopeCalibrating = true;
        setCalibrationStatus("Status: Calibrating Gyroscope... Keep phone still", R.color.warning_orange);
    }

    // Start magnetometer calibration using a figure-eight movement
    private void startMagnetometerCalibration() {
        resetAllCalibrationStates();
        magnetometerSampleCount = 0;
        for(int i=0; i<3; i++) {
            magnetometerMin[i] = Float.MAX_VALUE;
            magnetometerMax[i] = -Float.MAX_VALUE;
        }
        isMagnetometerCalibrating = true;
        setCalibrationStatus("Status: Move phone in Figure-8 shape", R.color.warning_orange);
    }

    // Receive raw sensor events and forwards each event to the proper handler.
    @Override
    public void onSensorChanged(SensorEvent event) {
        float[] sensorValues = event.values.clone();
        int sensorType = event.sensor.getType();

        if (sensorType == Sensor.TYPE_ACCELEROMETER) {
            Trace.beginSection("AirMouse_Accel");
            handleAccelerometer(sensorValues);
            Trace.endSection();
        } else if (sensorType == Sensor.TYPE_MAGNETIC_FIELD) {
            Trace.beginSection("AirMouse_Magneto");
            handleMagnetometer(sensorValues);
            Trace.endSection();
        } else if (sensorType == Sensor.TYPE_GYROSCOPE) {
            Trace.beginSection("AirMouse_Gyro");
            handleGyroscope(sensorValues, event.timestamp);
            Trace.endSection();
        }
    }

    // Handle accelerometer calibration and stores the corrected acceleration values
    private void handleAccelerometer(float[] sensorValues) {
        hasAccelerometerData = true;

        if (isAccelerometerCalibrating) {
            if (isCollectingAccel) {
                accelStepSums[0] += sensorValues[0];
                accelStepSums[1] += sensorValues[1];
                accelStepSums[2] += sensorValues[2];
                accelCollectionCount++;

                if (accelCollectionCount >= ACCEL_SAMPLES_PER_STEP) {
                    isCollectingAccel = false;
                    int stepIndex = accelCalibStep - 1;
                    accelStepAverages[stepIndex][0] = accelStepSums[0] / ACCEL_SAMPLES_PER_STEP;
                    accelStepAverages[stepIndex][1] = accelStepSums[1] / ACCEL_SAMPLES_PER_STEP;
                    accelStepAverages[stepIndex][2] = accelStepSums[2] / ACCEL_SAMPLES_PER_STEP;

                    accelCalibStep++;
                    if (accelCalibStep > 6) {
                        finishAccelerometerCalibration();
                    } else {
                        calibrateAccelerometerButton.setText("RECORD " + accelCalibStep + "/6");
                        String[] instructions = {
                                "Place screen UP & press RECORD",
                                "Place screen DOWN & press RECORD",
                                "Stand phone VERTICALLY & press RECORD",
                                "Hold phone UPSIDE DOWN & press RECORD",
                                "Rest on RIGHT EDGE & press RECORD",
                                "Rest on LEFT EDGE & press RECORD"
                        };
                        setCalibrationStatus("Step " + accelCalibStep + "/6: " + instructions[accelCalibStep-1], R.color.warning_orange);
                    }
                }
            }
            return;
        }

        accelerometerCurrent[0] = (sensorValues[0] - accelerometerOffset[0]) / accelerometerScale[0];
        accelerometerCurrent[1] = (sensorValues[1] - accelerometerOffset[1]) / accelerometerScale[1];
        accelerometerCurrent[2] = (sensorValues[2] - accelerometerOffset[2]) / accelerometerScale[2];

        accelerometerTextView.setText(String.format(Locale.US, "Accelerometer: [%.2f, %.2f, %.2f]", accelerometerCurrent[0], accelerometerCurrent[1], accelerometerCurrent[2]));
    }

    // Compute accelerometer offset and scale using six static orientations
    private void finishAccelerometerCalibration() {
        for (int axis = 0; axis < 3; axis++) {
            float min = Float.MAX_VALUE;
            float max = -Float.MAX_VALUE;
            for (int step = 0; step < 6; step++) {
                if (accelStepAverages[step][axis] < min) min = accelStepAverages[step][axis];
                if (accelStepAverages[step][axis] > max) max = accelStepAverages[step][axis];
            }
            accelerometerOffset[axis] = (max + min) / 2.0f;
            accelerometerScale[axis] = (max - min) / (2.0f * GRAVITY_EARTH);
            if (Math.abs(accelerometerScale[axis]) < 0.0001f) accelerometerScale[axis] = 1.0f;
        }
        isAccelerometerCalibrating = false;
        calibrateAccelerometerButton.setText("CALIBRATE ACCEL");
        setCalibrationStatus("Status: Accelerometer 6-Point Calibrated!", R.color.success_green);
    }

    // Handle magnetometer calibration and displays corrected magnetic field values
    private void handleMagnetometer(float[] sensorValues) {
        hasMagnetometerData = true;

        if (isMagnetometerCalibrating) {
            magnetometerSampleCount++;
            for (int i = 0; i < 3; i++) {
                if (sensorValues[i] < magnetometerMin[i]) magnetometerMin[i] = sensorValues[i];
                if (sensorValues[i] > magnetometerMax[i]) magnetometerMax[i] = sensorValues[i];
            }

            if (magnetometerSampleCount >= MAGNETOMETER_CALIBRATION_SAMPLES) {
                isMagnetometerCalibrating = false;
                for (int i = 0; i < 3; i++) {
                    magnetometerOffset[i] = (magnetometerMax[i] + magnetometerMin[i]) / 2.0f;
                    magnetometerScale[i] = (magnetometerMax[i] - magnetometerMin[i]) / 2.0f;
                    if (Math.abs(magnetometerScale[i]) < 0.0001f) magnetometerScale[i] = 1.0f;
                }
                setCalibrationStatus("Status: Magnetometer Calibrated!", R.color.success_green);
            }
            return;
        }

        float correctedX = (sensorValues[0] - magnetometerOffset[0]) / magnetometerScale[0];
        float correctedY = (sensorValues[1] - magnetometerOffset[1]) / magnetometerScale[1];
        float correctedZ = (sensorValues[2] - magnetometerOffset[2]) / magnetometerScale[2];

        magnetometerTextView.setText(String.format(Locale.US, "Magnetometer: [%.2f, %.2f, %.2f]", correctedX, correctedY, correctedZ));
    }

    // Apply gyroscope bias correction and forwards the result to motion processing.
    private void handleGyroscope(float[] sensorValues, long timestamp) {
        hasGyroscopeData = true;

        if (isGyroscopeCalibrating) {
            gyroscopeSamples.add(sensorValues.clone());
            if (gyroscopeSamples.size() >= GYROSCOPE_CALIBRATION_SAMPLES) {
                isGyroscopeCalibrating = false;
                float[] sum = new float[3];
                for (float[] sample : gyroscopeSamples) {
                    sum[0] += sample[0]; sum[1] += sample[1]; sum[2] += sample[2];
                }
                gyroscopeBias[0] = sum[0] / gyroscopeSamples.size();
                gyroscopeBias[1] = sum[1] / gyroscopeSamples.size();
                gyroscopeBias[2] = sum[2] / gyroscopeSamples.size();
                setCalibrationStatus("Status: Gyroscope Calibrated!", R.color.success_green);
            }
            return;
        }

        float[] calibratedGyroscope = new float[3];
        calibratedGyroscope[0] = sensorValues[0] - gyroscopeBias[0];
        calibratedGyroscope[1] = sensorValues[1] - gyroscopeBias[1];
        calibratedGyroscope[2] = sensorValues[2] - gyroscopeBias[2];

        gyroscopeTextView.setText(String.format(Locale.US, "Gyroscope: [%.2f, %.2f, %.2f]", calibratedGyroscope[0], calibratedGyroscope[1], calibratedGyroscope[2]));

        processMotion(calibratedGyroscope, timestamp);
    }

    // Convert filtered sensor data into mouse movement, click, and scroll commands.
    private void processMotion(float[] gyroscopeValues, long timestamp) {
        if (!hasAccelerometerData) return;

        if (lastTimestamp == 0) {
            lastTimestamp = timestamp;
            return;
        }

        float deltaTime = (timestamp - lastTimestamp) / 1_000_000_000.0f;
        lastTimestamp = timestamp;

        if (deltaTime <= 0.0f || deltaTime > 1.0f) return;

        int clickEvent = 0;
        int scrollEvent = 0;

        Trace.beginSection("AirMouse_Filtering");

        try {
            // Low-pass filter for estimating gravity from accelerometer data
            gravity[0] = ALPHA_GRAVITY * gravity[0] + (1.0f - ALPHA_GRAVITY) * accelerometerCurrent[0];
            gravity[1] = ALPHA_GRAVITY * gravity[1] + (1.0f - ALPHA_GRAVITY) * accelerometerCurrent[1];
            gravity[2] = ALPHA_GRAVITY * gravity[2] + (1.0f - ALPHA_GRAVITY) * accelerometerCurrent[2];

            float linearAccelerationY = accelerometerCurrent[1] - gravity[1];
            long currentTime = System.currentTimeMillis();

            boolean isPitching = Math.abs(gyroscopeValues[0]) > 0.4f;
            boolean isPushPullGesture = Math.abs(linearAccelerationY) > PUSH_PULL_THRESHOLD;

            // Detect left-click using fast rotation around the Y axis
            if (currentTime > lastClickTime) {
                if (gyroscopeValues[1] < -CLICK_GYROSCOPE_THRESHOLD) {
                    clickEvent = 1;
                    lastClickTime = currentTime + CLICK_TIMEOUT_MS;
                    gestureLockoutTime = currentTime + 300; 
                    triggerVisualAction("CLICK!", R.color.error_red, 600);
                }
            }

            // Detect scroll gestures using strong push/pull motion on the Y axis
            if (currentTime > scrollCooldownTime && !isPitching) {
                int currentDirection = 0;
                if (linearAccelerationY > SCROLL_ACCEL_THRESHOLD) currentDirection = 1;
                else if (linearAccelerationY < -SCROLL_ACCEL_THRESHOLD) currentDirection = -1;

                if (currentDirection != 0 && (currentDirection == lastScrollDirection || currentTime > oppositeDirectionLockTime)) {
                    scrollEvent = currentDirection;
                    lastScrollDirection = currentDirection;
                    oppositeDirectionLockTime = currentTime + OPPOSITE_DIRECTION_TIMEOUT_MS;
                    scrollCooldownTime = currentTime + SCROLL_COOLDOWN_MS;
                    
                    // Lock mouse while scrolling
                    gestureLockoutTime = currentTime + 400; 
                    triggerVisualAction(scrollEvent == 1 ? "SCROLL UP" : "SCROLL DOWN", R.color.primary_blue, 80);
                }
            }

            double deltaX = 0.0;
            double deltaY = 0.0;

            // Calculate cursor movement only when no gesture lockout is active
            if (currentTime > gestureLockoutTime && !isPushPullGesture) {
                float rateX = gyroscopeValues[0]; 
                float rateZ = gyroscopeValues[2]; 
                // Deadzone removes small hand tremors when the phone is almost still
                final float DEADZONE = 0.06f;
                if (Math.abs(rateX) < DEADZONE) rateX = 0.0f;
                if (Math.abs(rateZ) < DEADZONE) rateZ = 0.0f;

                deltaX = -rateZ * GAIN_X * deltaTime;
                deltaY = -rateX * GAIN_Y * deltaTime;
            }


            // A click packet must not contain cursor movement
            if (clickEvent == 1) {
                deltaX = 0.0;
                deltaY = 0.0;
            }

            // Send movement and gesture commands to the laptop
            if (isMouseRunning && networkClient != null) {
                networkClient.sendMouseData(deltaX, deltaY, clickEvent, scrollEvent);
            }

            // Update the visual pointer on the phone screen independently from networking
            updateVisualPointer((float) deltaX, (float) deltaY);

        } finally {
            Trace.endSection();
        }
    }

    // Update the small visual pointer shown inside the Android UI.
    private void updateVisualPointer(float deltaX, float deltaY) {
        uiHandler.post(() -> {
            View parentView = (View) visualPointerView.getParent();
            if (parentView == null) return;

            float maxX = (parentView.getWidth() - visualPointerView.getWidth()) / 2.0f;
            float maxY = (parentView.getHeight() - visualPointerView.getHeight()) / 2.0f;

            currentVisualX += deltaX;
            currentVisualY += deltaY;

            if (currentVisualX > maxX) currentVisualX = maxX;
            if (currentVisualX < -maxX) currentVisualX = -maxX;
            if (currentVisualY > maxY) currentVisualY = maxY;
            if (currentVisualY < -maxY) currentVisualY = -maxY;

            visualPointerView.setTranslationX(currentVisualX);
            visualPointerView.setTranslationY(currentVisualY);
        });
    }

    // Show a temporary click or scroll action on the UI.
    private void triggerVisualAction(String actionLabel, int colorResId, long durationMs) {
        uiHandler.post(() -> {
            setAction("Action: " + actionLabel, colorResId);
            visualPointerView.setBackgroundResource(R.drawable.bg_pointer_dot);
            uiHandler.removeCallbacks(resetVisualRunnable);
            uiHandler.postDelayed(resetVisualRunnable, durationMs);
        });
    }

    // Reset the action label to the idle state.
    private void resetActionIndicator() {
        setAction("Action: NONE", R.color.success_green);
        if (visualPointerView != null) {
            visualPointerView.setBackgroundResource(R.drawable.bg_pointer_dot);
        }
    }

    // Update the action text and color.
    private void setAction(String text, int colorResId) {
        if (actionIndicatorTextView == null) return;
        actionIndicatorTextView.setText(text);
        actionIndicatorTextView.setTextColor(getColor(colorResId));
    }

    // Update the calibration status text and color.
    private void setCalibrationStatus(String text, int colorResId) {
        if (calibrationStatusTextView == null) return;
        calibrationStatusTextView.setText(text);
        calibrationStatusTextView.setTextColor(getColor(colorResId));
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}
}