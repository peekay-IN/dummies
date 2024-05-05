/* --- Internal Functions --- */

static int generate_telemetry_payload(
    uint8_t* payload_buffer,
    size_t payload_buffer_size,
    size_t* payload_buffer_length)
{
  az_json_writer jw;
  az_result rc;
  az_span payload_buffer_span = az_span_create(payload_buffer, payload_buffer_size);
  az_span json_span;
  float temperature, humidity, light, pressure, altitude;
  int32_t magneticFieldX, magneticFieldY, magneticFieldZ;
  int32_t pitch, roll, accelerationX, accelerationY, accelerationZ;

  // Acquiring data from Espressif's ESP32 Azure IoT Kit sensors.
  temperature = esp32_azureiotkit_get_temperature();
  humidity = esp32_azureiotkit_get_humidity();
  light = esp32_azureiotkit_get_ambientLight();
  esp32_azureiotkit_get_pressure_altitude(&pressure, &altitude);
  esp32_azureiotkit_get_magnetometer(&magneticFieldX, &magneticFieldY, &magneticFieldZ);
  esp32_azureiotkit_get_pitch_roll_accel(
      &pitch, &roll, &accelerationX, &accelerationY, &accelerationZ);

  rc = az_json_writer_init(&jw, payload_buffer_span, NULL);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed initializing json writer for telemetry.");

  rc = az_json_writer_append_begin_object(&jw);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed setting telemetry json root.");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_TEMPERATURE));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding temperature property name to telemetry payload.");
  rc = az_json_writer_append_double(&jw, temperature, DOUBLE_DECIMAL_PLACE_DIGITS);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding temperature property value to telemetry payload. ");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_HUMIDITY));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding humidity property name to telemetry payload.");
  rc = az_json_writer_append_double(&jw, humidity, DOUBLE_DECIMAL_PLACE_DIGITS);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding humidity property value to telemetry payload. ");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_LIGHT));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding light property name to telemetry payload.");
  rc = az_json_writer_append_double(&jw, light, DOUBLE_DECIMAL_PLACE_DIGITS);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding light property value to telemetry payload.");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_PRESSURE));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding pressure property name to telemetry payload.");
  rc = az_json_writer_append_double(&jw, pressure, DOUBLE_DECIMAL_PLACE_DIGITS);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding pressure property value to telemetry payload.");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_ALTITUDE));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding altitude property name to telemetry payload.");
  rc = az_json_writer_append_double(&jw, altitude, DOUBLE_DECIMAL_PLACE_DIGITS);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding altitude property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_MAGNETOMETERX));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(X) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, magneticFieldX);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(X) property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_MAGNETOMETERY));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(Y) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, magneticFieldY);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(Y) property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_MAGNETOMETERZ));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(Z) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, magneticFieldZ);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding magnetometer(Z) property value to telemetry payload.");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_PITCH));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding pitch property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, pitch);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding pitch property value to telemetry payload.");

  rc = az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_ROLL));
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding roll property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, roll);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed adding roll property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_ACCELEROMETERX));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(X) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, accelerationX);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(X) property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_ACCELEROMETERY));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(Y) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, accelerationY);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(Y) property value to telemetry payload.");

  rc = az_json_writer_append_property_name(
      &jw, AZ_SPAN_FROM_STR(TELEMETRY_PROP_NAME_ACCELEROMETERZ));
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(Z) property name to telemetry payload.");
  rc = az_json_writer_append_int32(&jw, accelerationZ);
  EXIT_IF_AZ_FAILED(
      rc, RESULT_ERROR, "Failed adding acceleration(Z) property value to telemetry payload.");

  rc = az_json_writer_append_end_object(&jw);
  EXIT_IF_AZ_FAILED(rc, RESULT_ERROR, "Failed closing telemetry json payload.");

  payload_buffer_span = az_json_writer_get_bytes_used_in_destination(&jw);

  if ((payload_buffer_size - az_span_size(payload_buffer_span)) < 1)
  {
    LogError("Insufficient space for telemetry payload null terminator.");
    return RESULT_ERROR;
  }

  payload_buffer[az_span_size(payload_buffer_span)] = null_terminator;
  *payload_buffer_length = az_span_size(payload_buffer_span);

  return RESULT_OK;
}
