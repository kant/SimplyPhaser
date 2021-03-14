// Copyright © 2021 Brad Howes. All rights reserved.

#pragma once

#include <cmath>
#include <limits>
#include <os/log.h>

namespace Biquad {

/**
 Read-only filter coefficients.
 */
template <typename T>
struct Coefficients {
    Coefficients()
    : a0{0.0}, a1{0.0}, a2{0.0}, b1{0.0}, b2{0.0}, c0{1.0}, d0{0.0} {}

    Coefficients(double _a0, double _a1, double _a2, double _b1, double _b2)
    : a0{_a0}, a1{_a1}, a2{_a2}, b1{_b1}, b2{_b2}, c0{1.0}, d0{0.0} {}

    Coefficients(double _a0, double _a1, double _a2, double _b1, double _b2, double _c0, double _d0)
    : a0{_a0}, a1{_a1}, a2{_a2}, b1{_b1}, b2{_b2}, c0{_c0}, d0{_d0} {}

    Coefficients A0(double VV) { return Coefficients(VV, a1, a2, b1, b2, c0, d0); }
    Coefficients A1(double VV) { return Coefficients(a0, VV, a2, b1, b2, c0, d0); }
    Coefficients A2(double VV) { return Coefficients(a0, a1, VV, b1, b2, c0, d0); }
    Coefficients B1(double VV) { return Coefficients(a0, a1, a2, VV, b2, c0, d0); }
    Coefficients B2(double VV) { return Coefficients(a0, a1, a2, b1, VV, c0, d0); }
    Coefficients C0(double VV) { return Coefficients(a0, a1, a2, b1, b2, VV, d0); }
    Coefficients D0(double VV) { return Coefficients(a0, a1, a2, b1, b2, c0, VV); }

    T a0;
    T a1;
    T a2;
    T b1;
    T b2;
    T c0;
    T d0;

    // 1-pole low-pass coefficients generator
    static Coefficients<T> LPF1(T sampleRate, T frequency) {
        double theta = 2.0 * M_PI * frequency / sampleRate;
        double gamma = std::cos(theta) / (1.0 + std::sin(theta));
        return Coefficients()
        .A0((1.0 - gamma) / 2.0)
        .A1((1.0 - gamma) / 2.0)
        .A2(0.0)
        .B1(-gamma)
        .B2(0.0);
    }

    // 1-pole high-pass coefficients generator
    static Coefficients<T> HPF1(T sampleRate, T frequency) {
        double theta = 2.0 * M_PI * frequency / sampleRate;
        double gamma = std::cos(theta) / (1.0 + std::sin(theta));
        return Coefficients()
        .A0((1.0 + gamma) / 2.0)
        .A1((1.0 + gamma) / -2.0)
        .A2(0.0)
        .B1(-gamma)
        .B2(0.0);
    }

    // 2-pole low-pass coefficients generator
    static Coefficients<T> LPF2(T sampleRate, T frequency, T resonance) {
        double theta = 2.0 * M_PI * frequency / sampleRate;
        double d = 1.0 / resonance;
        double beta = 0.5 * (1 - d / 2.0 * std::sin(theta)) / (1 + d / 2.0 * std::sin(theta));
        double gamma = (0.5 + beta) * std::cos(theta);
        double alpha = (0.5 + beta - gamma) / 2.0;
        return Coefficients()
        .A0(alpha)
        .A1(2.0 * alpha)
        .A2(alpha)
        .B1(-2.0 * gamma)
        .B2(2.0 * beta);
    }

    // 2-pole high-pass coefficients generator
    static Coefficients<T> HPF2(T sampleRate, T frequency, T resonance) {
        double theta = 2.0 * M_PI * frequency / sampleRate;
        double d = 1.0 / resonance;
        double beta = 0.5 * (1 - d / 2.0 * std::sin(theta)) / (1 + d / 2.0 * std::sin(theta));
        double gamma = (0.5 + beta) * std::cos(theta);
        return Coefficients()
        .A0((0.5 + beta + gamma) / 2.0)
        .A1(-1.0 * (0.5 + beta + gamma))
        .A2((0.5 + beta + gamma) / 2.0)
        .B1(-2.0 * gamma)
        .B2(2.0 * beta);
    }

    // 1-pole all-pass coefficients generator
    static Coefficients<T> APF1(T sampleRate, T frequency) {
        double tangent = std::tan(M_PI * frequency / sampleRate);
        double alpha = (tangent - 1.0) / (tangent + 1.0);
        return Coefficients(alpha, 1.0, 0.0, alpha, 0.0);
    }

    // 2-pole all-pass coefficients generator
    static Coefficients<T> APF2(T sampleRate, T frequency, T resonance) {
        double bandwidth = frequency / resonance;
        double argTan = M_PI * bandwidth / sampleRate;
        if (argTan >= 0.95 * M_PI / 2.0) argTan = 0.95 * M_PI / 2.0;
        double tangent = std::tan(argTan);
        double alpha = (tangent - 1.0) / (tangent + 1.0);
        double beta = -std::cos(2.0 * M_PI * frequency / sampleRate);
        return Coefficients()
        .A0(-alpha)
        .A1(beta * (1.0 - alpha))
        .A2(1.0)
        .B1(beta * (1.0 - alpha))
        .B2(-alpha);
    }

    inline static os_log_t log_{os_log_create("DSP.Biquad", "Coefficients")};
};

/**
 Mutable filter state.
 */
template <typename T>
struct State {
    State() : x_z1{0}, x_z2{0}, y_z1{0}, y_z2{0} {}
    T x_z1;
    T x_z2;
    T y_z1;
    T y_z2;
};

namespace Transform {

template <typename T>
struct Base {

    /// If value is too small to be represented in a `float`, force it to be zero.
    static T forceMinToZero(T value) {
        return ((value > 0.0 && value < std::numeric_limits<float>::min()) ||
                (value < 0.0 && value > -std::numeric_limits<float>::min())) ? 0.0 : value;
    }
};

// Transform for the 'direct' biquad structure
template <typename T>
struct Direct : Base<T> {
    static T transform(T input, State<T>& state, const Coefficients<T>& coefficients) {
        T output = coefficients.a0 * input + coefficients.a1 * state.x_z1 + coefficients.a2 * state.x_z2 -
        coefficients.b1 * state.y_z1 - coefficients.b2 * state.y_z2;
        output = Base<T>::forceMinToZero(output);
        state.x_z2 = state.x_z1;
        state.x_z1 = input;
        state.y_z2 = state.y_z1;
        state.y_z1 = output;
        return output;
    }

    static T storageComponent(const State<T>& state, const Coefficients<T>& coefficients) {
        return coefficients.a1 * state.x_z1 + coefficients.a2 * state.x_z2 - coefficients.b1 * state.y_z1 -
        coefficients.b2 * state.y_z2;
    }
};

// Transform for the 'canonical' biquad structure (min state)
template <typename T>
struct Canonical : Base<T> {
    static T transform(T input, State<T>& state, const Coefficients<T>& coefficients) {
        T theta = input - coefficients.b1 * state.x_z1 - coefficients.b2 * state.x_z2;
        T output = coefficients.a0 * theta + coefficients.a1 * state.x_z1 + coefficients.a2 * state.x_z2;
        output = Base<T>::forceMinToZero(output);
        state.x_z2 = state.x_z1;
        state.x_z1 = theta;
        return output;
    }

    static T storageComponent(const State<T>& state, const Coefficients<T>& coefficients) { return 0.0; }
};

// Transform for the transposed 'direct' biquad structure
template <typename T>
struct DirectTranspose : Base<T> {
    static T transform(T input, State<T>& state, const Coefficients<T>& coefficients) {
        T theta = input + state.y_z1;
        T output = coefficients.a0 * theta + state.x_z1;
        output = Base<T>::forceMinToZero(output);
        state.y_z1 = state.y_z2 - coefficients.b1 * theta;
        state.y_z2 = -coefficients.b2 * theta;
        state.x_z1 = state.x_z2 + coefficients.a1 * theta;
        state.x_z2 = coefficients.a2 * theta;
        return output;
    }

    static T storageComponent(const State<T>& state, const Coefficients<T>& coefficients) { return 0.0; }
};

// Transform for the transposed 'canonical' biquad structure (min state)
template <typename T>
struct CanonicalTranspose : Base<T> {
    static T transform(T input, State<T>& state, const Coefficients<T>& coefficients) {
        T output = Base<T>::forceMinToZero(coefficients.a0 * input + state.x_z1);
        state.x_z1 = coefficients.a1 * input - coefficients.b1 * output + state.x_z2;
        state.x_z2 = coefficients.a2 * input - coefficients.b2 * output;
        return output;
    }

    static T storageComponent(const State<T>& state, const Coefficients<T>& coefficients) { return state.x_z1; }
};

} // namespace Op

/**
 Generic biquad filter setup. Only knows how to reset its internal state and to transform (filter)
 values.
 */
template <typename T, typename Transformer>
class Filter {
public:
    using CoefficientsType = Coefficients<T>;
    using StateType = State<T>;

    /**
     Create a new filter using the given biquad coefficients.
     */
    explicit Filter(const CoefficientsType& coefficients) : coefficients_{coefficients}, state_{} {}

    Filter() : Filter(Coefficients<T>()) {}

    /**
     Use a new set of biquad coefficients.
     */
    void setCoefficients(const Coefficients<T>& coefficients) {
        coefficients_ = coefficients;
    }

    /**
     Reset internal state.
     */
    void reset() { state_ = State<T>(); }

    /**
     Apply the filter to a given value.
     */
    T transform(T input) { return Transformer::transform(input, state_, coefficients_); }

    /**
     Obtain the `gain` value from the coefficients.
     */
    T gainValue() const { return coefficients_.a0; }

    /**
     Obtain a calculated state value.
     */
    T storageComponent() const { return Transformer::storageComponent(state_, coefficients_); }

private:
    Coefficients<T> coefficients_;
    State<T> state_;
    os_log_t log_{os_log_create("DSP.Biquad", "Filter")};
};

template <typename T>
using Direct = Filter<T, Transform::Direct<T>>;

template <typename T>
using DirectTranspose = Filter<T, Transform::DirectTranspose<T>>;

template <typename T>
using Canonical = Filter<T, Transform::Canonical<T>>;

template <typename T>
using CanonicalTranspose = Filter<T, Transform::CanonicalTranspose<T>>;

} // namespace Biquad
