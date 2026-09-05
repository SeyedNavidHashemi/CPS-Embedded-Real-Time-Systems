SELECT 
  name AS Task_Name,
  COUNT(*) AS Execution_Count,
  AVG(dur) / 1000000.0 AS Average_Duration_ms,
  MAX(dur) / 1000000.0 AS Max_Duration_ms
FROM slice
WHERE name = 'AirMouse_Filtering'
GROUP BY name;

WITH SensorDeltas AS (
  SELECT 
    name,
    ts,
    (ts - LAG(ts) OVER (PARTITION BY name ORDER BY ts)) / 1000000.0 AS delta_ms
  FROM slice
  WHERE name IN ('AirMouse_Gyro', 'AirMouse_Accel', 'AirMouse_Magneto')
)
SELECT 
  name,
  COUNT(*) AS Total_Samples,
  AVG(delta_ms) AS Average_Sampling_Period_ms,
  1000.0 / AVG(delta_ms) AS Average_Frequency_Hz,
  MIN(delta_ms) AS Min_Delta_ms,
  MAX(delta_ms) AS Max_Delta_ms
FROM SensorDeltas
WHERE delta_ms IS NOT NULL
GROUP BY name;

SELECT 
  name AS Sensor_Task,
  SUM(dur) / 1000000.0 AS Total_CPU_Time_ms,
  AVG(dur) / 1000000.0 AS Avg_Time_Per_Execution_ms
FROM slice
WHERE name LIKE '%Sensor%' OR name LIKE '%Gyro%' OR name LIKE '%Accel%' OR name LIKE '%Magneto%'
GROUP BY name
ORDER BY Total_CPU_Time_ms DESC;

SELECT 
  thread.name AS Thread_Name,
  thread_state.state AS State,
  SUM(thread_state.dur) / 1000000.0 AS Total_Duration_ms
FROM thread_state
JOIN thread USING (utid)
JOIN process USING (upid)
WHERE process.name = 'com.example.airmouse'
GROUP BY Thread_Name, State
ORDER BY Thread_Name, Total_Duration_ms DESC;

SELECT 
  name,
  ts / 1000000000.0 AS Start_Time_Seconds,
  dur / 1000000.0 AS Duration_ms
FROM slice
WHERE name = 'AirMouse_SocketSend'
ORDER BY dur DESC
LIMIT 20;