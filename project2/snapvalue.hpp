#pragma once

class SnapValue {
public:
    SnapValue(size_t label_ = 0, int value_ = 0, int* snap_ = nullptr)
        : label(label_), value(value_), snap(snap_) {}

    // time stamp
    size_t label;
    // used for random value update
    int value;
    // clean collect
    int* snap;

    ~SnapValue() {
        if (snap != nullptr) {
            delete[] snap;    
        }
    }
};
