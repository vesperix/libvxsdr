// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <complex>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

void init_int(std::vector<std::complex<int16_t>>& x) {
    for (unsigned i = 0; i < x.size(); i++) {
        x[i] = std::complex<int16_t>((int16_t)(i % 65519) - 32768, (int16_t)(i % 65521) - 32768);
    }
}

void init_float(std::vector<std::complex<float>>& x) {
    for (unsigned i = 0; i < x.size(); i++) {
        x[i] = std::complex<float>(((int16_t)(i % 65519) - 32768) / 32768.0F, ((int16_t)(i % 65521) - 32768) / 32768.0F);
    }
}

void random_float(std::vector<std::complex<float>>& x) {
    std::random_device dev;
    std::mt19937_64 gen(dev());
    std::uniform_real_distribution<float> u(-1.0, 1.0);
    for (unsigned i = 0; i < x.size(); i++) {
        x[i] = std::complex<float>(u(gen), u(gen));
    }
}

void int_to_float(const std::vector<std::complex<int16_t>>& v_int, std::vector<std::complex<float>>& v_float) {
    const float scale = 1.0 / 32'767.0;
    v_float.clear();
    for (size_t i = 0; i < v_int.size(); i++) {
        v_float[i] = std::complex<float>(scale * (float)v_int[i].real(), scale * (float)v_int[i].imag());
    }
}

void float_to_int_round(const std::vector<std::complex<float>>& v_float, std::vector<std::complex<int16_t>>& v_int) {
    const float scale = 32'767.0;
    for (size_t i = 0; i < v_float.size(); i++) {
        v_int[i] = std::complex<int16_t>((int16_t)std::lroundf(scale * v_float[i].real()),
                                         (int16_t)std::lroundf(scale * v_float[i].imag()));
    }
}

void float_to_int_truncate(const std::vector<std::complex<float>>& v_float, std::vector<std::complex<int16_t>>& v_int) {
    const float scale = 32'767.0;
    for (size_t i = 0; i < v_float.size(); i++) {
        v_int[i] = std::complex<int16_t>((int16_t)(scale * v_float[i].real()), (int16_t)(scale * v_float[i].imag()));
    }
}

void float_to_int_test(const std::vector<std::complex<float>>& v_float, std::vector<std::complex<int16_t>>& v_int) {
    const float scale = 32'767.0;
    for (size_t i = 0; i < v_float.size(); i++) {
        float re = scale * v_float[i].real();
        if (re > 0.0F) {
            re += 0.5F;
        } else {
            re -= 0.5F;
        }
        float im = scale * v_float[i].imag();
        if (im > 0.0F) {
            im += 0.5F;
        } else {
            im -= 0.5F;
        }
        v_int[i] = std::complex<int16_t>((int16_t)(re), (int16_t)(im));
    }
}

template <typename T>
double diff(const std::vector<std::complex<T>>& x, std::vector<std::complex<T>>& y) {
    double diff = 0.0;
    for (size_t i = 0; i < x.size(); i++) {
        diff += std::abs(x[i] - y[i]);
    }
    return diff / (double)x.size();
}

float int_diff(const std::vector<std::complex<int16_t>>& x, std::vector<std::complex<int16_t>>& y) {
    float diff = 0.0;
    for (size_t i = 0; i < x.size(); i++) {
        diff += std::abs(x[i] - y[i]);
    }
    return diff / (float)x.size();
}

std::vector<int> vec_int_diff(const std::vector<std::complex<int16_t>>& x, std::vector<std::complex<int16_t>>& y) {
    std::vector<int> diff(x.size());
    for (size_t i = 0; i < x.size(); i++) {
        diff[i] = std::abs(x[i].real() - y[i].real()) + std::abs(x[i].imag() - y[i].imag());
        ;
    }
    return diff;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: test_float_convert <number of seconds of data> <minimum sample rate>" << std::endl;
        return -1;
    }

    std::cout << "testing speed of int16_t - float conversions for complex<float> inputs and outputs" << std::endl;

    double n_seconds    = std::strtod(argv[1], nullptr);
    double minimum_rate = std::strtod(argv[2], nullptr);

    size_t n = std::ceil(n_seconds * minimum_rate);

    std::vector<std::complex<int16_t>> x_int(n);
    std::vector<std::complex<int16_t>> y_int(n);
    std::vector<std::complex<float>> x_float(n);
    std::vector<std::complex<float>> y_float(n);

    init_int(x_int);
    random_float(x_float);

    auto t0 = std::chrono::steady_clock::now();
    int_to_float(x_int, y_float);
    auto t1                          = std::chrono::steady_clock::now();
    std::chrono::duration<double> d1 = t1 - t0;
    double rate_i_f                  = (double)n / d1.count();
    std::cout << "complex<int16_t> to complex<float>:                " << rate_i_f << " samples/s"
              << (rate_i_f > minimum_rate ? "" : " (SLOW)") << std::endl;

    t0 = std::chrono::steady_clock::now();
    float_to_int_round(x_float, x_int);
    t1                    = std::chrono::steady_clock::now();
    d1                    = t1 - t0;
    double rate_f_i_round = (double)n / d1.count();
    std::cout << "complex<float> to complex<int16_t> (std::lroundf): " << rate_f_i_round << " samples/s"
              << (rate_f_i_round > minimum_rate ? "" : " (SLOW)") << std::endl;

    t0 = std::chrono::steady_clock::now();
    float_to_int_truncate(x_float, y_int);
    t1                       = std::chrono::steady_clock::now();
    d1                       = t1 - t0;
    double rate_f_i_truncate = (double)n / d1.count();
    double err_f_i_truncate  = diff(x_int, y_int);
    std::cout << "complex<float> to complex<int16_t> (truncating):   " << rate_f_i_truncate << " samples/s"
              << (rate_f_i_truncate > minimum_rate ? "" : " (SLOW)") << " err = " << err_f_i_truncate << std::endl;
    t0 = std::chrono::steady_clock::now();
    float_to_int_test(x_float, y_int);
    t1                   = std::chrono::steady_clock::now();
    d1                   = t1 - t0;
    double rate_f_i_test = (double)n / d1.count();
    double err_f_i_test  = diff(x_int, y_int);
    std::cout << "complex<float> to complex<int16_t> (test):         " << rate_f_i_test << " samples/s"
              << (rate_f_i_test > minimum_rate ? "" : " (SLOW)") << " err = " << err_f_i_test << std::endl;

    t0 = std::chrono::steady_clock::now();
#ifndef VXSDR_LIB_TRUNCATE_FLOAT_CONVERSION
#if defined(VXSDR_COMPILER_CLANG) || defined(VXSDR_COMPILER_APPLECLANG) || defined(VXSDR_COMPILER_GCC)
    float_to_int_test(x_float, y_int);
#else  // neither VXSDR_COMPILER_CLANG, VXSDR_COMPILER_APPLECLANG, nor VXSDR_COMPILER_GCC defined
    float_to_int_round(x_float, x_int);
#endif
#else  // VXSDR_LIB_TRUNCATE_FLOAT_CONVERSION is defined
    float_to_int_truncate(x_float, y_int);
#endif
    t1                      = std::chrono::steady_clock::now();
    d1                      = t1 - t0;
    double rate_f_i_default = (double)n / d1.count();
    double err_f_i_default  = diff(x_int, y_int);
    std::cout << "complex<float> to complex<int16_t> (default):      " << rate_f_i_default << " samples/s"
              << (rate_f_i_default > minimum_rate ? "" : " (SLOW)") << " err = " << err_f_i_default << std::endl;

    bool pass = (rate_i_f > minimum_rate) and (rate_f_i_default > minimum_rate) and (err_f_i_default < 1e-3);

    std::cout << (pass ? "passed" : "failed") << std::endl;

    return (pass ? 0 : 1);
}
