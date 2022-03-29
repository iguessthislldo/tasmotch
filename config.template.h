const char * const wifi_ssid = "REPLACE THIS";
const char * const wifi_password = "REPLACE THIS";
const char * const mqtt_broker_address = "REPLACE THIS";
const unsigned mqtt_broker_port = 1883;

Switch control_d1(Switch::Latching, "flip", D1);
Switch control_d2(Switch::Momentary, "push", D2);

void control_init() {
    control_d1.seek_device_list.add("LIGHT PREFIX", ControlMode::OnOff);
    control_d2.seek_device_list.add("LIGHT PREFIX", ControlMode::OnOff);
}
