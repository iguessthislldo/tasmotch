#include <PubSubClient.h>

struct Input {
    static const unsigned long debounce_delay = 100;

    const int pin;
    bool active_high = true;
    unsigned long last_change = 0;
    bool value = false;

    bool raw_value() {
        return active_high == (digitalRead(pin) == HIGH);
    }

    void init() {
        pinMode(pin, INPUT);
        value = raw_value();
        last_change = millis();
    }

    bool changed() {
        const unsigned long now = millis();
        const bool value_now = raw_value();
        bool has_changed = false;
        if (value_now != value) {
            if ((now - last_change) > debounce_delay) {
                value = value_now;
                has_changed = true;
            }
            last_change = now;
        }
        return has_changed;
    }
};

struct Output {
    const int pin;
    bool is_high;

    void init() {
        pinMode(pin, OUTPUT);
        set(is_high);
    }

    bool set(bool is_high) {
        this->is_high = is_high;
        digitalWrite(pin, is_high ? HIGH : LOW);
        return is_high;
    }

    bool toggle() {
        return set(!is_high);
    }

    void flash(unsigned long period, unsigned count = 1) {
        const auto p = period / 2;
        for (unsigned i = 0; i < count; i++) {
            set(true);
            delay(p);
            set(false);
            delay(p);
        }
    }
};

template<typename Type>
struct List {
    Type* head = nullptr;
    Type* tail = nullptr;

    void clear() {
        for (Type* value = head; value;) {
            Type* next = value->next;
            delete value;
            value = next;
        }
        head = nullptr;
        tail = nullptr;
    }

    ~List() {
        clear();
    }

    void add(Type* new_value) {
        if (!head) {
            head = new_value;
        }
        if (tail) {
            tail->next = new_value;
        }
        tail = new_value;
    }
};

size_t prefix_length(const char* what, const char* prefix) {
    size_t i;
    for (i = 0; what[i] == prefix[i] && what[i]; ++i) {
    }
    return i;
}

bool starts_with(const char* what, const char* prefix) {
    return prefix[prefix_length(what, prefix)] == '\0';
}

bool equal_strings(const char* a, const char* b) {
    const size_t i = prefix_length(a, b);
    return a[i] == '\0' && b[i] == '\0';
}

char* append_to_string(char* d, const char* s, char omit = '\0') {
    for (; *s; s++) {
        if (*s != omit) {
            *d++ = *s;
        }
    }
    *d = '\0';
    return d;
}

enum class ControlMode {
    OnOff,
    OnOnly,
    OffOnly,
};

struct Device {
    String name;
    ControlMode control_mode;
    Device* next = nullptr;
};

struct DeviceList : public List<Device> {
    Device* find(const char* name) {
        for (Device* d = head; d; d = d->next) {
            if (equal_strings(d->name.c_str(), name)) {
                return d;
            }
        }
        return nullptr;
    }

    Device* add(const char* name, ControlMode control_mode) {
        Device* d = find(name);
        if (d) {
            Serial.print("Already have ");
            Serial.println(name);
            return d;
        }
        Serial.print("Adding ");
        Serial.println(name);
        d = new Device{name, control_mode};
        List<Device>::add(d);
        return d;
    }
};

char topic_buffer[128] = {0};

class Switch;

struct SwitchList : public List<Switch> {
    void init();
    void found_device(const char* device_name, const char* topic_name);
    void check(PubSubClient& mqtt);
} all_switches;

class Switch {
public:
    enum Type {
        Latching,
        Momentary,
    };

    Switch* next;
    Input input;

private:
    Type type;
    const char* name;
    DeviceList find_device_list;
    DeviceList found_device_list;

public:
    Switch(Type type, const char* name, int pin)
    : input{pin}
    , type(type)
    , name(name)
    {
        all_switches.add(this);
    }

    void init() {
        input.init();
    }

    void find_device(const char* device_prefix, ControlMode control_mode = ControlMode::OnOff) {
        find_device_list.add(device_prefix, control_mode);
    }

    void found_device(const char* device_name, const char* topic_name) {
        for (Device* d = find_device_list.head; d; d = d->next) {
            if (starts_with(device_name, d->name.c_str())) {
                found_device_list.add(topic_name, d->control_mode);
            }
        }
    }

    enum class Cmd {
        On,
        Off,
        Toggle,
    };

    static const char* cmd_to_str(Cmd cmd) {
        switch (cmd) {
        case Cmd::On:
            return "On";
        case Cmd::Off:
            return "Off";
        case Cmd::Toggle:
            return "TOGGLE";
        }
        return nullptr;
    }

    static bool cmd_matches_control_mode(Cmd cmd, ControlMode control_mode) {
        switch (control_mode) {
        case ControlMode::OnOff:
            return true;
        case ControlMode::OnOnly:
            return cmd == Cmd::On;
        case ControlMode::OffOnly:
            return cmd == Cmd::Off;
        }
        return false;
    }

    void check(PubSubClient& mqtt) {
        if (input.changed() && (type == Latching || input.value)) {
            Cmd cmd = type == Latching ? (input.value ? Cmd::On : Cmd::Off) : Cmd::Toggle;
            const char* const cmd_str = cmd_to_str(cmd);
            Serial.print(name);
            Serial.println(" changed, publishing:");
            for (Device* d = found_device_list.head; d; d = d->next) {
                if (!cmd_matches_control_mode(cmd, d->control_mode)) {
                    continue;
                }

                topic_buffer[0] = '\0';
                append_to_string(
                    append_to_string(
                        append_to_string(topic_buffer, "cmnd/"),
                        d->name.c_str()),
                    "/Power");
                Serial.print(topic_buffer);
                Serial.print(" ");
                Serial.println(cmd_str);
                mqtt.publish(topic_buffer, cmd_str);
            }
        }
    }
};

void SwitchList::init() {
    for (Switch* s = head; s; s = s->next) {
        s->init();
    }
}

void SwitchList::found_device(const char* device_name, const char* topic_name) {
    for (Switch* s = head; s; s = s->next) {
        s->found_device(device_name, topic_name);
    }
}

void SwitchList::check(PubSubClient& mqtt) {
    for (Switch* s = head; s; s = s->next) {
        s->check(mqtt);
    }
}
